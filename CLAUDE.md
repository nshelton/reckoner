# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
cmake -B build
cmake --build build
./build/reckoner
```

Requires: C++17, CMake 3.21+, OpenGL 3.3+, curl. On macOS, dependencies are available via system frameworks. On Linux: `sudo apt install libglfw3-dev libgl1-mesa-dev`.

GLFW and nlohmann_json are fetched automatically by CMake (FetchContent). ImGui (docking branch) is vendored in `external/imgui`.

There are no tests or linting tools configured.

## Architecture

Reckoner is a spatiotemporal entity visualizer — it renders personal data (GPS tracks, photos, calendar events) across both a map view and a timeline view using OpenGL + ImGui.

### Data Flow

```
User Input → InteractionController → Camera/TimelineCamera → AppModel
                                                                ↓
Backend (HTTP/Fake) → entities → Layer storage in AppModel
                                                                ↓
MainScreen orchestrates: Renderer (map) + TimelineRenderer (timeline) + ImGui (controls)
```

### Key Concepts

- **Layer = Type**: Each layer corresponds to one backend entity type (e.g., `"location.gps"`, `"photo"`, `"calendar.event"`). Layer name IS the type string. Layers are stored in `AppModel::layers` with fixed indices (0=GPS, 1=photo, 2=calendar, 3=googletimeline).
- **Dual camera system**: `Camera` handles the map view (spatial pan/zoom), `TimelineCamera` handles the timeline view (temporal pan/zoom). Both convert between screen and world coordinates.
- **Backend abstraction**: `Backend` interface (`Backend.h`) with `HttpBackend` (real API) and `FakeBackend` (synthetic data). `MainScreen` owns one backend per layer.
- **Entity**: Core data type with required time extent and optional lat/lon. Defined in `src/core/Entity.h`.

### Rendering Stack

- `Renderer` — map rendering coordinator. Manages `LineRenderer`, `PointRenderer` (one per layer), `TileRenderer` (vector), and `RasterTileRenderer` (OSM/CartoDB tiles).
- `TimelineRenderer` — timeline rendering. Uses the same `PointRenderer` instances from `Renderer`.
- Specialized renderers in `src/renderer/`: `CalendarRenderer`, `HistogramRenderer`, `SolarAltitudeRenderer`, `MoonAltitudeRenderer`, `TextRenderer`. Each wraps an OpenGL shader program.
- Shaders live in `src/shaders/` and are loaded at runtime via the `SHADER_BASE_DIR` compile definition.

### Screen System

`App` manages the GLFW window and ImGui context. It delegates to an `IScreen` implementation (`Screen.h`). `MainScreen` is the only screen — it wires together cameras, renderers, backends, and the ImGui control panel.

### Tile System

`src/tiles/` handles map tile rendering: `TileRenderer` for vector tiles (MVT via `MvtDecoder`), `RasterTileRenderer` for raster tiles (OSM, CartoDB). Both use `TileCache`/`RasterTileCache` for async HTTP fetching and caching.

## Environment

The app reads a `.env` file at startup (`src/core/EnvLoader.h`) for backend API configuration. The `.env` file is gitignored.
