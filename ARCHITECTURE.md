# Reckoner Architecture

A spatiotemporal entity visualizer with independent map and timeline views.

## Directory Structure

```
src/
├── app/                    # Application framework
│   ├── App.h/cpp          # Main app with GLFW + ImGui docking
│   └── Screen.h           # Screen interface for app states
├── screens/               # Screen implementations
│   └── MainScreen.h/cpp   # Main visualization screen
├── core/                  # Core types and math
│   ├── Vec2.h            # 2D vector
│   ├── Mat3.h            # 3x3 transformation matrix
│   ├── Color.h           # RGBA color
│   ├── Core.h            # Convenience header (includes all core types)
│   └── Theme.h           # Color theme constants
├── renderer/             # OpenGL rendering
│   └── LineRenderer.h/cpp # Line/point rendering with shaders
├── AppModel.h            # Central application state
├── Camera.h/cpp          # View transformation (pan/zoom)
├── Interaction.h/cpp     # Mouse/keyboard interaction handling
├── Renderer.h/cpp        # High-level rendering coordinator
└── main.cpp              # Entry point
```

## Core Architecture

### AppModel
Central state container. Currently holds:
- `SpatialExtent` - Geographic bounds for the map view

Future additions:
- `TimeExtent` - Temporal bounds for timeline view
- `vector<Layer>` - Data layers to visualize
- Selection state

### Camera
Manages view transformation between world coordinates and screen pixels.
- Pan: Move the view center
- Zoom: Scale the view
- Conversion: `screenToWorld()`, `worldToScreen()`

### Renderer
Coordinates all rendering. Currently:
- Renders a simple grid for testing
- Uses `LineRenderer` for OpenGL drawing

Future: Will delegate to individual layer renderers

### Interaction
Handles user input and maps it to camera/model changes.
- Pan: Left-click drag
- Zoom: Mouse wheel
- Future: Hit testing, selection, timeline scrubbing

### Screen System
- `IScreen` interface defines lifecycle (attach, update, render, GUI, detach)
- `MainScreen` implements the main visualization with docked Map/Timeline windows
- `App` manages the screen, GLFW window, and ImGui context

## Data Flow

```
User Input → Interaction → Camera/AppModel
                ↓
            Renderer → LineRenderer → OpenGL
                ↓
            ImGui Windows (Map, Timeline, Controls)
```

## Key Design Principles

1. **Separation of Concerns**
   - Camera handles view transforms
   - Renderer handles drawing
   - Interaction handles input
   - AppModel holds data

2. **Extensibility**
   - Layer system (future) allows adding new entity types
   - Screen system allows different views/modes
   - Renderer delegates to specialized renderers

3. **Performance**
   - Only update camera on actual size changes
   - Async data fetching (future)
   - Spatial/temporal culling (future)

## Next Steps

See [DESIGN.md](DESIGN.md) for the full vision:
- Add `TimeExtent` and timeline rendering
- Implement Layer system for entity types
- Add backend API integration
- Implement hit testing and selection
