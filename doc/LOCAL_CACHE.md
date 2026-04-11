# Local Entity Cache — Design Document

## Problem

Startup currently streams ~5M entities via NDJSON over HTTP (`GET /v1/query/export`), taking ~5 minutes. This is unacceptable for daily use. The data changes infrequently — most entities are historical and immutable, with new data arriving at the recent end of the timeline.

## Goals

1. **Fast startup**: Load 5M entities from local binary file in <2 seconds
2. **Incremental sync**: Only download time ranges that changed on the server
3. **Correct**: Never show stale data without the user knowing
4. **Simple**: One cache file per entity type, stored in a known location

## Non-Goals

- Spatial indexing in the cache (the app already handles this in memory)
- Query filtering from the cache (we load everything at startup)
- Cache eviction (the full dataset fits in memory and on disk)

---

## Cache File Format

### Why binary, not JSON/NDJSON

5M entities as NDJSON ≈ 1.5 GB text, ~400 MB gzipped. Parsing JSON is slow (string allocs, field lookups). A flat binary format eliminates parsing entirely — just `mmap` or `fread` and cast.

### File layout

```
┌──────────────────────────────────────────────────┐
│  File Header (64 bytes)                          │
├──────────────────────────────────────────────────┤
│  String Table (variable length)                  │
├──────────────────────────────────────────────────┤
│  Entity Records (fixed-size, N × 64 bytes)       │
├──────────────────────────────────────────────────┤
│  Bucket Index (K × 24 bytes)                     │
└──────────────────────────────────────────────────┘
```

### File Header (64 bytes)

```cpp
struct CacheHeader {
    char     magic[4];          // "RKCF" (Reckoner Cache File)
    uint32_t version;           // Format version (1)
    char     entity_type[32];   // e.g. "location.gps\0"
    uint64_t entity_count;      // Number of entity records
    uint64_t string_table_offset;
    uint64_t string_table_size;
    uint64_t entity_offset;
    uint64_t bucket_index_offset;
    uint32_t bucket_count;
    uint32_t reserved;
};
```

### Entity Record (64 bytes, fixed)

```cpp
struct CacheEntity {
    // Time (16 bytes)
    double   time_start;        // unix timestamp
    double   time_end;          // unix timestamp

    // Location (16 bytes)
    float    lat;               // NaN if no location
    float    lon;               // NaN if no location
    float    render_offset;
    uint32_t flags;             // bit 0: has_location, bits 1-2: reserved

    // String references into string table (24 bytes)
    uint32_t id_offset;         // offset into string table
    uint16_t id_length;
    uint32_t name_offset;       // 0 = no name
    uint16_t name_length;
    uint32_t color_offset;      // 0 = no color
    uint16_t color_length;
    uint32_t padding;

    // Sort key (8 bytes)
    // Records are sorted by time_start for binary search and bucket alignment
};
// static_assert(sizeof(CacheEntity) == 64);
```

Why 64 bytes: cache-line aligned, easy to seek, easy to memory-map. Strings (id, name, color) are stored separately in the string table to keep the record fixed-size.

### String Table

Contiguous block of UTF-8 strings, referenced by (offset, length) pairs in entity records. No null terminators needed — length is explicit.

### Bucket Index

The entity records are sorted by `time_start`. The time range is divided into fixed-duration buckets (e.g., 1 day). The bucket index stores the byte range and hash of each bucket, enabling incremental sync.

```cpp
struct CacheBucket {
    double   bucket_start;      // unix timestamp (start of time bucket)
    uint32_t entity_start_idx;  // first entity index in this bucket
    uint32_t entity_count;      // number of entities in this bucket
    uint64_t hash;              // xxhash64 of the server's canonical data for this bucket
};
```

---

## Bucket Hashing Protocol

### Concept

Time is divided into fixed buckets (1 day = 86400 seconds). Each bucket has a hash that summarizes its contents. The client and server independently compute hashes for the same buckets. If a hash differs, that bucket is stale and must be re-fetched.

### Bucket size

**1 day** is the default bucket duration. Rationale:
- 5M entities over ~5 years ≈ ~2,700 daily buckets
- Comparing 2,700 hashes is instant
- Re-fetching one day of GPS data (~3,000 entities) takes <1 second
- New data almost always lands in "today" bucket, so typically only 1 bucket needs refresh

### Hash computation

The hash must be deterministic and identical on client and server. The server is the source of truth.

**Hash input per entity** (canonical byte representation):
```
id (UTF-8 bytes) || t_start (ISO8601 UTF-8) || t_end (ISO8601 UTF-8 or "null") || lat (8 bytes LE double or 0x00) || lon (8 bytes LE double or 0x00)
```

**Bucket hash**: xxHash64 of the concatenation of all entity hashes within the bucket, in `(t_start, id)` sort order.

Why xxHash64: fast (>10 GB/s), good distribution, available in C++ (header-only) and Python. Not cryptographic, but we don't need that — this is change detection, not security.

---

## New Server Endpoint

### `GET /v1/cache/bucket-hashes`

Returns the hash of each time bucket, so the client can identify which buckets are stale.

**Query Parameters:**

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `type` | string | yes | — | Entity type (e.g., `location.gps`) |
| `bucket_seconds` | int | no | 86400 | Bucket duration in seconds |

**Response:**

```json
{
  "type": "location.gps",
  "bucket_seconds": 86400,
  "total_entities": 4200000,
  "buckets": [
    {
      "start": "2020-01-01T00:00:00Z",
      "end": "2020-01-02T00:00:00Z",
      "count": 1423,
      "hash": "a1b2c3d4e5f60718"
    },
    {
      "start": "2020-01-02T00:00:00Z",
      "end": "2020-01-03T00:00:00Z",
      "count": 1587,
      "hash": "b2c3d4e5f6071829"
    }
  ]
}
```

**Server implementation** (Python/FastAPI sketch):

```python
import xxhash
from datetime import datetime, timedelta, timezone

@app.get("/v1/cache/bucket-hashes")
async def bucket_hashes(
    type: str,
    bucket_seconds: int = 86400,
    db=Depends(get_db),
    api_key=Depends(verify_api_key),
):
    # Query all entities of this type, ordered by t_start, id
    # Use server-side cursor for constant memory
    rows = await db.fetch_all(
        """
        SELECT id, t_start, t_end, lat, lon
        FROM entities
        WHERE type = :type
        ORDER BY t_start, id
        """,
        {"type": type},
    )

    buckets = []
    current_bucket_start = None
    hasher = None
    count = 0

    for row in rows:
        ts = row["t_start"].timestamp()
        bucket_start = int(ts // bucket_seconds) * bucket_seconds

        if bucket_start != current_bucket_start:
            if hasher is not None:
                buckets.append({
                    "start": datetime.fromtimestamp(current_bucket_start, tz=timezone.utc).isoformat(),
                    "end": datetime.fromtimestamp(current_bucket_start + bucket_seconds, tz=timezone.utc).isoformat(),
                    "count": count,
                    "hash": hasher.hexdigest(),
                })
            current_bucket_start = bucket_start
            hasher = xxhash.xxh64()
            count = 0

        # Canonical hash input
        hasher.update(row["id"].encode())
        hasher.update(row["t_start"].isoformat().encode())
        hasher.update(row["t_end"].isoformat().encode() if row["t_end"] else b"null")
        hasher.update(struct.pack("<d", row["lat"]) if row["lat"] is not None else b"\x00")
        hasher.update(struct.pack("<d", row["lon"]) if row["lon"] is not None else b"\x00")
        count += 1

    if hasher is not None:
        buckets.append({
            "start": datetime.fromtimestamp(current_bucket_start, tz=timezone.utc).isoformat(),
            "end": datetime.fromtimestamp(current_bucket_start + bucket_seconds, tz=timezone.utc).isoformat(),
            "count": count,
            "hash": hasher.hexdigest(),
        })

    return {
        "type": type,
        "bucket_seconds": bucket_seconds,
        "total_entities": sum(b["count"] for b in buckets),
        "buckets": buckets,
    }
```

### `POST /v1/cache/bucket-data`

Fetch the full entity data for specific stale buckets. This replaces the generic export for targeted sync.

**Request:**

```json
{
  "type": "location.gps",
  "buckets": [
    {"start": "2026-04-04T00:00:00Z", "end": "2026-04-05T00:00:00Z"},
    {"start": "2026-04-05T00:00:00Z", "end": "2026-04-06T00:00:00Z"}
  ]
}
```

**Response:** NDJSON stream (same format as `/v1/query/export`), filtered to the requested time ranges, ordered by `(t_start, id)`.

This endpoint lets the client surgically re-fetch only stale buckets instead of re-downloading everything.

---

## Client Sync Algorithm

### Startup flow

```
1. Check for cache file (~/.reckoner/cache/<type>.rkcf)
   ├── No file → full export (GET /v1/query/export), write cache, done
   └── File exists → incremental sync:
       a. Read bucket index from cache file
       b. GET /v1/cache/bucket-hashes?type=<type>
       c. Compare local vs server hashes
       d. Identify stale buckets (hash mismatch, new buckets, deleted buckets)
       e. POST /v1/cache/bucket-data for stale buckets
       f. Merge: replace stale bucket data, keep unchanged buckets
       g. Rewrite cache file
       h. Load entities into AppModel
```

### Typical sync scenarios

| Scenario | Buckets to fetch | Time |
|----------|-----------------|------|
| App reopened same day, no new data | 0 | <0.5s (hash compare only) |
| App reopened, new GPS data today | 1 (today) | ~1s |
| App reopened after a week offline | 7 (one per day) | ~5s |
| Server re-ingested last month | ~30 | ~15s |
| First launch (no cache) | all ~2,700 | ~5 min (full export, same as today) |

### Cold start optimization

On first launch with no cache, use the existing `GET /v1/query/export` streaming path and write the cache file as data arrives. The app can begin rendering before the export completes (same as current behavior), and the cache file is ready for next launch.

---

## Cache File Location

```
~/.reckoner/cache/
├── location.gps.rkcf
├── photo.rkcf
├── calendar.event.rkcf
└── location.googletimeline.rkcf
```

Configurable via `.env`:
```
CACHE_DIR=~/.reckoner/cache
```

---

## Client Implementation

### New files

```
src/cache/
├── CacheFormat.h       // CacheHeader, CacheEntity, CacheBucket structs
├── CacheWriter.h/cpp   // Writes cache file from entity vectors
├── CacheReader.h/cpp   // Reads cache file, returns entity vectors
└── CacheSync.h/cpp     // Orchestrates sync: hash compare, fetch stale, merge
```

### CacheWriter

```cpp
class CacheWriter {
public:
    // Write a complete cache file for one entity type.
    // Entities must be sorted by time_start.
    static bool write(
        const std::string& path,
        const std::string& entity_type,
        const std::vector<Entity>& entities,
        const std::vector<BucketHash>& bucket_hashes,
        int bucket_seconds = 86400
    );
};
```

### CacheReader

```cpp
class CacheReader {
public:
    struct Result {
        std::vector<Entity> entities;
        std::vector<CacheBucket> buckets;  // for sync comparison
        std::string entity_type;
    };

    // Read entire cache file. Returns empty result if file missing/corrupt.
    static Result read(const std::string& path);
};
```

### CacheSync

```cpp
class CacheSync {
public:
    CacheSync(BackendAPI* api, const std::string& cache_dir);

    struct SyncResult {
        std::vector<Entity> entities;
        int buckets_checked;
        int buckets_fetched;    // 0 = fully cached
        double sync_time_ms;
    };

    // Load entities for a type, using cache + incremental sync.
    // Calls progress_callback with (entities_loaded, total_expected).
    SyncResult load(
        const std::string& entity_type,
        std::function<void(size_t loaded, size_t total)> progress_callback = {}
    );
};
```

### Integration with MainScreen

Replace `startFullLoad()` fetch logic:

```cpp
// Before (5 min):
gpsBackend->streamAllEntities(on_total, batch_callback);

// After (<2s typical):
CacheSync sync(&m_api, cache_dir);
auto result = sync.load("location.gps", [this](size_t n, size_t total) {
    // update progress UI
});
m_model->layers[0].entities = std::move(result.entities);
```

---

## Disk Size Estimates

| Type | Entities | Record size | String table est. | Total |
|------|----------|-------------|-------------------|-------|
| location.gps | 4,200,000 | 64B × 4.2M = 268 MB | ~160 MB (36-char UUIDs) | ~430 MB |
| photo | 50,000 | 64B × 50K = 3.2 MB | ~3 MB | ~6 MB |
| calendar.event | 20,000 | 64B × 20K = 1.3 MB | ~2 MB (names) | ~3 MB |
| googletimeline | 500,000 | 64B × 500K = 32 MB | ~19 MB | ~51 MB |
| **Total** | | | | **~490 MB** |

This is manageable. Could compress the string table with LZ4 to reduce by ~60% if needed, but not worth the complexity initially.

### Alternative: shrink IDs

If UUIDs are the dominant string cost (~36 bytes × 5M = 180 MB), consider storing them as 16-byte binary UUIDs in the entity record instead of in the string table. This would cut total size to ~340 MB and simplify the format. Worth doing in v1.

---

## Failure Modes

| Failure | Handling |
|---------|----------|
| Cache file corrupt (bad magic, wrong version) | Delete and full re-export |
| Server unreachable | Load from cache as-is, show "offline / cached data" indicator |
| Hash endpoint not available (old server) | Fall back to full re-export |
| Disk full during write | Write to temp file, rename on success (atomic) |
| Interrupted sync (app killed mid-write) | Temp file is never renamed; next launch retries |

---

## Future Enhancements

- **Materialized hashes on server**: Pre-compute bucket hashes in a `cache_hashes` table, updated by ingest triggers. Avoids scanning the full table on every hash request.
- **Compression**: LZ4 frame compression on the entity record section. ~2x size reduction, decompresses at >4 GB/s.
- **mmap loading**: Memory-map the entity records section instead of reading into `std::vector`. Eliminates the copy and lets the OS manage page faults. Requires the binary layout to match the in-memory struct exactly.
- **Background sync**: After initial cache load, sync stale buckets in the background while the user is already interacting with cached data.
