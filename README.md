# Reckoner

A spatiotemporal entity visualizer - view your data across both space and time.

## Features

- **Dual-View Visualization**: Independent Map and Timeline windows
- **ImGui Docking**: Arrange windows however you want - side-by-side, tabbed, or complex layouts
- **Interactive Map**: Pan with left-click drag, zoom with mouse wheel
- **Extensible Architecture**: Ready for layers, backend integration, and more

## Quick Start

### Prerequisites

- C++17 compiler
- CMake 3.21+
- GLFW3
- OpenGL 3.3+
- ImGui (docking branch - included in `external/imgui`)

### Build

```bash
cmake -B build
cmake --build build
./build/reckoner
```

### Install Dependencies (Ubuntu/Debian)

```bash
sudo apt install libglfw3-dev libgl1-mesa-dev
```

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for implementation details.

See [DESIGN.md](DESIGN.md) for the full vision and roadmap.

### Current Structure

```
App (GLFW + ImGui)
 └─ MainScreen
     ├─ Map Window      (interactive spatial view)
     ├─ Timeline Window (temporal view - placeholder)
     └─ Controls Window (settings)
```

### Key Components

- **AppModel**: Central state (SpatialExtent, future: TimeExtent, Layers)
- **Camera**: View transformation (pan/zoom)
- **Interaction**: Mouse/keyboard → Camera updates
- **Renderer**: Draws the scene using OpenGL

## Usage

### Controls

**Map Window:**
- Left-click drag: Pan the view
- Scroll wheel: Zoom in/out
- Window docking: Drag tabs to rearrange

### Window Layout

The app uses ImGui docking. You can:
- Drag windows by their title bar to reposition
- Drag to window edges to split views
- Drop one window onto another to create tabs
- Your layout is remembered between sessions

## Development

### Next Steps

1. **Lat/Lon Grid**: Replace test grid with proper geographic grid
2. **Timeline View**: Add time axis and event rendering
3. **Layer System**: Support multiple entity types
4. **Backend Integration**: Connect to FastAPI + PostGIS backend
5. **Hit Testing**: Click to select entities

### Adding a New Feature

1. Update `AppModel` if new state is needed
2. Add interaction handling in `InteractionController`
3. Add rendering in `Renderer` or create new specialized renderer
4. Update `MainScreen::onGui()` for UI controls

## License

MIT License - see LICENSE file for details

## Credits

Built with:
- [ImGui](https://github.com/ocornut/imgui) (docking branch)
- [GLFW](https://www.glfw.org/)
- OpenGL
