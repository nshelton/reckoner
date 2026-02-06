# Updated Location Parser for Jupyter Notebook
# Replace your SQLite code with these cells

## Cell 1: Setup and Configuration

```python
import requests
from datetime import datetime
from typing import Dict, Any, Optional
from pathlib import Path


print(f"✓ Loaded API key (length: {len(API_KEY)})")
```

## Cell 2: Helper Functions

```python
def parse_timestamp(timestamp_str: str) -> str:
    """Convert timestamp to ISO 8601 format"""
    # Try parsing common formats
    for fmt in [
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%dT%H:%M:%S",
        "%Y-%m-%d %H:%M:%S.%f",
        "%Y-%m-%dT%H:%M:%S.%f",
    ]:
        try:
            dt = datetime.strptime(timestamp_str, fmt)
            return dt.strftime("%Y-%m-%dT%H:%M:%SZ")
        except ValueError:
            continue

    # If already ISO 8601, return as-is
    if "T" in timestamp_str and ("Z" in timestamp_str or "+" in timestamp_str):
        return timestamp_str

    raise ValueError(f"Could not parse timestamp: {timestamp_str}")

def create_location_entity(
    timestamp: str,
    latitude: float,
    longitude: float,
    name: Optional[str] = None,
    external_id: Optional[str] = None,
    payload: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """Create a location.gps entity via HTTP POST"""

    # Convert timestamp to ISO 8601
    iso_timestamp = parse_timestamp(timestamp)

    # Build request body
    entity_data = {
        "type": "location.gps",
        "t_start": iso_timestamp,
        "t_end": None,  # Instantaneous event
        "lat": latitude,
        "lon": longitude,
        "name": name,
        "color": "#FF0000",  # Red color
        "render_offset": None,
        "source": "location_parser",
        "external_id": external_id,
        "payload": payload or {}
    }

    # Make HTTP POST request
    headers = {
        "Content-Type": "application/json",
        "X-API-Key": API_KEY
    }

    response = requests.post(
        f"{BACKEND_URL}/v1/entity",
        json=entity_data,
        headers=headers
    )

    if response.status_code == 200:
        return response.json()
    else:
        raise Exception(f"Failed to create entity: {response.status_code} - {response.text}")

print("✓ Helper functions loaded")
```

## Cell 3: Insert Data (replaces your SQLite code)

```python
# Insert all location samples
inserted_count = 0
updated_count = 0
error_count = 0

for i, sample in enumerate(samples):
    location = sample.get('location')

    if location is None:
        continue

    try:
        # Use timestamp as external_id for idempotent inserts
        # This means re-running won't create duplicates
        external_id = location['timestamp']

        result = create_location_entity(
            timestamp=location['timestamp'],
            latitude=location['latitude'],
            longitude=location['longitude'],
            external_id=external_id,
            payload={
                "source_file": "location_parser",
                "raw_data": sample  # Store original sample data
            }
        )

        if result['status'] == 'inserted':
            inserted_count += 1
        else:
            updated_count += 1

        # Print progress every 100 samples
        if (i + 1) % 100 == 0:
            print(f"Processed {i + 1}/{len(samples)}: {inserted_count} inserted, {updated_count} updated")

    except Exception as e:
        error_count += 1
        print(f"Error at sample {i}: {e}")

# Final summary
print(f"\n{'='*50}")
print(f"FINAL STATS:")
print(f"  ✓ Inserted: {inserted_count}")
print(f"  ↻ Updated: {updated_count}")
print(f"  ✗ Errors: {error_count}")
print(f"  Total processed: {inserted_count + updated_count}")
print(f"{'='*50}")
```

## Cell 4: Verify Data (optional)

```python
# Query the backend to verify your data was inserted
response = requests.post(
    f"{BACKEND_URL}/v1/query/time",
    headers={
        "Content-Type": "application/json",
        "X-API-Key": API_KEY
    },
    json={
        "types": ["location.gps"],
        "start": "2024-01-01T00:00:00Z",  # Adjust to your data range
        "end": "2024-12-31T23:59:59Z",
        "limit": 10
    }
)

if response.status_code == 200:
    data = response.json()
    print(f"Found {len(data['entities'])} location entities")

    # Show first few
    for entity in data['entities'][:5]:
        print(f"  - {entity['t_start']}: ({entity['lat']:.6f}, {entity['lon']:.6f})")
else:
    print(f"Query failed: {response.status_code} - {response.text}")
```

---

## Key Differences from SQLite Version:

1. **No database connection needed** - Uses HTTP requests instead
2. **API Key authentication** - Loads from `.env` file automatically
3. **Idempotent inserts** - Using `external_id` means re-running won't create duplicates
4. **ISO 8601 timestamps** - Automatically converts your timestamp format
5. **Proper data model** - Follows backend's `EntityIn` schema
6. **Error handling** - Continues processing even if some samples fail

## Usage:

1. Make sure your backend is running: `uvicorn app.main:app --host 0.0.0.0 --port 8000`
2. Paste Cell 1, 2, and 3 into your notebook in order
3. Run them - your data will be sent to the PostgreSQL backend
4. Optionally run Cell 4 to verify the data was inserted

Your data will now be stored in PostgreSQL with PostGIS spatial indexing! 🎉
