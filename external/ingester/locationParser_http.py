"""
Updated location parser that sends data to the backend via HTTP requests
Instead of SQLite, this inserts entities into the PostgreSQL backend
"""

import requests
from datetime import datetime
from typing import Dict, Any, Optional
import os
from pathlib import Path

# Configuration
BACKEND_URL = "http://localhost:8000"
API_KEY_FILE = Path(__file__).parent.parent / "spatiotemporal_db" / ".env"

def load_api_key() -> str:
    """Load API key from .env file"""
    if not API_KEY_FILE.exists():
        raise FileNotFoundError(f"API key file not found: {API_KEY_FILE}")

    with open(API_KEY_FILE) as f:
        for line in f:
            if line.startswith("API_KEY="):
                return line.strip().split("=", 1)[1]

    raise ValueError("API_KEY not found in .env file")

def parse_timestamp(timestamp_str: str) -> str:
    """
    Convert timestamp to ISO 8601 format
    Handles various input formats and converts to UTC
    """
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
    api_key: str,
    name: Optional[str] = None,
    color: str = "#FF0000",
    source: str = "location_parser",
    external_id: Optional[str] = None,
    payload: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """
    Create a location.gps entity via HTTP POST to the backend

    Args:
        timestamp: Timestamp string (will be converted to ISO 8601)
        latitude: Latitude in decimal degrees
        longitude: Longitude in decimal degrees
        api_key: API key for authentication
        name: Optional name for the location
        color: Hex color code (default red)
        source: Source identifier for idempotent upserts
        external_id: External ID for idempotent upserts
        payload: Optional JSON payload with additional data

    Returns:
        Response from the API containing entity ID and status
    """
    # Convert timestamp to ISO 8601
    iso_timestamp = parse_timestamp(timestamp)

    # Build request body according to EntityIn model
    entity_data = {
        "type": "location.gps",
        "t_start": iso_timestamp,
        "t_end": None,  # Instantaneous event
        "lat": latitude,
        "lon": longitude,
        "name": name,
        "color": color,
        "render_offset": None,
        "source": source,
        "external_id": external_id,
        "payload": payload or {}
    }

    # Make HTTP POST request
    headers = {
        "Content-Type": "application/json",
        "X-API-Key": api_key
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

def batch_insert_locations(samples, api_key: str, batch_size: int = 100):
    """
    Insert multiple location samples in batches

    Args:
        samples: List of location samples from your parser
        api_key: API key for authentication
        batch_size: Number of entities to insert before printing progress
    """
    inserted_count = 0
    updated_count = 0
    error_count = 0

    for i, sample in enumerate(samples):
        location = sample.get('location')

        if location is None:
            continue

        try:
            # Use timestamp as external_id for idempotent inserts
            external_id = location['timestamp']

            result = create_location_entity(
                timestamp=location['timestamp'],
                latitude=location['latitude'],
                longitude=location['longitude'],
                api_key=api_key,
                external_id=external_id,
                payload={
                    "original_data": sample  # Store original sample data
                }
            )

            if result['status'] == 'inserted':
                inserted_count += 1
            else:
                updated_count += 1

            # Print progress every batch_size entities
            if (i + 1) % batch_size == 0:
                print(f"Processed {i + 1} samples: {inserted_count} inserted, {updated_count} updated, {error_count} errors")

        except Exception as e:
            error_count += 1
            print(f"Error processing sample {i}: {e}")

    print(f"\nFinal stats:")
    print(f"  Inserted: {inserted_count}")
    print(f"  Updated: {updated_count}")
    print(f"  Errors: {error_count}")
    print(f"  Total processed: {inserted_count + updated_count}")

# Example usage in your notebook:
"""
# Load API key
api_key = load_api_key()

# Single insert example
result = create_location_entity(
    timestamp="2024-01-15 12:30:00",
    latitude=34.0522,
    longitude=-118.2437,
    api_key=api_key,
    name="Downtown LA"
)
print(f"Created entity: {result['id']} ({result['status']})")

# Batch insert example (assuming you have 'samples' list)
# batch_insert_locations(samples, api_key)
"""
