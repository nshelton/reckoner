#pragma once

#include "tiles/TileCache.h" // for TileKey, TileKeyHash, TileState

#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>
#include <deque>
#include <string>
#include <cstdint>

typedef void CURL;

struct RasterTileEntry {
    TileState state = TileState::Empty;
    std::vector<uint8_t> pixels; // RGBA, width*height*4 bytes
    int width  = 0;
    int height = 0;
    uint64_t lastUsedFrame = 0;
};

/// Async raster (PNG) tile downloader. Thread-safe fetch queue, same design as TileCache.
/// Call processCompletedFetches() each frame from the main thread, then requestTile().
class RasterTileCache {
public:
    explicit RasterTileCache(std::string urlTemplate);
    ~RasterTileCache();

    /// Process async fetch results — must be called from the main (GL) thread each frame.
    void processCompletedFetches();

    /// Request a tile. Returns pointer to pixel data when ready, nullptr otherwise.
    /// Kicks off an async fetch on first request.
    const RasterTileEntry* requestTile(const TileKey& key);

    /// Evict least-recently-used ready tiles to stay under maxTiles.
    void evictOldTiles(int maxTiles = 128);

    int cachedCount() const;
    int pendingCount() const;

private:
    struct FetchResult {
        TileKey key;
        std::vector<uint8_t> pixels;
        int width  = 0;
        int height = 0;
        bool success = false;
    };

    std::string m_urlTemplate;

    std::unordered_map<TileKey, RasterTileEntry, TileKeyHash> m_tiles;
    uint64_t m_frameCounter = 0;

    std::mutex m_resultMutex;
    std::deque<FetchResult> m_completedFetches;
    std::vector<std::future<void>> m_inFlight;
    std::deque<TileKey> m_pendingQueue;

    std::mutex m_curlPoolMutex;
    std::vector<CURL*> m_curlPool;
    static constexpr int MaxConcurrentFetches = 6;

    void fetchTileAsync(const TileKey& key);
    void drainPendingQueue();
    FetchResult downloadAndDecode(const TileKey& key);
    CURL* acquireCurl();
    void releaseCurl(CURL* curl);
};
