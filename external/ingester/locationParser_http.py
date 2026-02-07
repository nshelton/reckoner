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

def _build_entity_data(
    timestamp: str,
    latitude: float,
    longitude: float,
    name: Optional[str] = None,
    color: str = "#FF0000",
    source: str = "location_parser",
    external_id: Optional[str] = None,
    payload: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Build an entity dict for the API."""
    iso_timestamp = parse_timestamp(timestamp)
    return {
        "type": "location.gps",
        "t_start": iso_timestamp,
        "t_end": None,
        "lat": latitude,
        "lon": longitude,
        "name": name,
        "color": color,
        "render_offset": None,
        "source": source,
        "external_id": external_id,
        "payload": payload or {},
    }


def batch_insert_locations(samples, api_key: str, batch_size: int = 500):
    """
    Insert multiple location samples using the batch endpoint.

    Args:
        samples: List of location samples from your parser
        api_key: API key for authentication
        batch_size: Number of entities per HTTP request (max 1000)
    """
    inserted_count = 0
    updated_count = 0
    error_count = 0

    headers = {
        "Content-Type": "application/json",
        "X-API-Key": api_key,
    }

    # Build all entity dicts first, skipping samples without location
    entity_batch = []
    total_samples = 0

    for sample in samples:
        location = sample.get("location")
        if location is None:
            continue

        total_samples += 1
        try:
            entity_batch.append(
                _build_entity_data(
                    timestamp=location["timestamp"],
                    latitude=location["latitude"],
                    longitude=location["longitude"],
                    external_id=location["timestamp"],
                    payload={"original_data": sample},
                )
            )
        except Exception as e:
            error_count += 1
            print(f"Error building entity: {e}")
            continue

        # Send batch when full
        if len(entity_batch) >= batch_size:
            result = _send_batch(entity_batch, headers)
            inserted_count += result["inserted"]
            updated_count += result["updated"]
            error_count += result["errors"]
            processed = inserted_count + updated_count + error_count
            print(f"Processed {processed}/{total_samples}: {inserted_count} inserted, {updated_count} updated, {error_count} errors")
            entity_batch = []

    # Send remaining entities
    if entity_batch:
        result = _send_batch(entity_batch, headers)
        inserted_count += result["inserted"]
        updated_count += result["updated"]
        error_count += result["errors"]

    print(f"\nFinal stats:")
    print(f"  Inserted: {inserted_count}")
    print(f"  Updated: {updated_count}")
    print(f"  Errors: {error_count}")
    print(f"  Total processed: {inserted_count + updated_count}")


def _send_batch(entities: list, headers: dict) -> Dict[str, Any]:
    """Send a batch of entities to the batch endpoint."""
    response = requests.post(
        f"{BACKEND_URL}/v1/entities/batch",
        json=entities,
        headers=headers,
    )
    if response.status_code == 200:
        return response.json()
    else:
        raise Exception(f"Batch insert failed: {response.status_code} - {response.text}")

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
