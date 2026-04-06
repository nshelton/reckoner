# Reckoner Refactoring Plan: Modular, Testable, Agent-Verifiable

## Context

The codebase is a monolithic C++17 OpenGL/ImGui app compiled as a single executable with no tests, no library decomposition, and no CI. `MainScreen` is a ~713-line god object that owns everything. The `Backend` interface is too narrow, forcing `dynamic_cast<HttpBackend*>` in 10+ places. Pure logic (Camera, EntityPicker, TimeUtils, SolarCalculations) is trapped behind GL dependencies and untestable. The goal is to make incremental, non-breaking changes that introduce modularity, test coverage, and CI verification.

---

## Phase 1: Foundation — Test Framework + CMake Libraries

**Add Catch2, restructure CMake into 3 targets, add first tests.**

### CMake restructure (`CMakeLists.txt`)
- Add Catch2 v3 via FetchContent
- Create `reckoner_core` static lib: all pure logic with **zero** GL/GLFW/ImGui/curl deps
  - `src/core/*.cpp` + `src/core/*.h` (Vec2, Mat3, Color, Entity, TimeExtent, RingBuffer, Theme, SolarCalculations, TimeUtils, EnvLoader)
  - `src/EntityPicker.cpp` + `.h`
  - `src/Camera.cpp` + `.h` (after removing gratuitous GLFW include — Camera.cpp has zero GL calls)
  - `src/TimelineCamera.cpp` + `.h`
  - `src/Layer.h`, `src/AppModel.h`
  - `src/tiles/TileMath.h`
  - Links only to: `nlohmann_json::nlohmann_json`
- Create `reckoner_http` static lib: HTTP/backend code
  - `src/http/HttpClient.cpp`, `src/http/BackendAPI.cpp`, `src/HttpBackend.cpp`
  - `src/Backend.h`, `src/FakeBackend.h`
  - Links to: `reckoner_core`, `CURL::libcurl`, `nlohmann_json`
- `reckoner` executable: remaining GL/ImGui sources (explicit list, no GLOB_RECURSE)
  - Links to: `reckoner_core`, `reckoner_http`, `imgui`, `glfw`, `OpenGL::GL`
- `reckoner_tests` target in `tests/CMakeLists.txt`
  - Links to: `reckoner_core`, `Catch2::Catch2WithMain`

### Key fix: `src/Camera.h`
- Remove `#include <GLFW/glfw3.h>` and `#define GL_SILENCE_DEPRECATION` (lines 3-4). Verified: Camera.cpp makes zero GL/GLFW calls.

### Initial tests (`tests/`)
- `test_vec2.cpp` — arithmetic, operator==
- `test_mat3.cpp` — ortho projection, apply/applyInverse roundtrip
- `test_entity.cpp` — time_mid, duration, has_location, spatial_contains
- `test_time_utils.cpp` — parse_iso8601/to_iso8601 roundtrip
- `test_solar.cpp` — solarAltitudeDeg at known epoch/location
- `test_ring_buffer.cpp` — push/overflow, statistics
- `test_tile_math.cpp` — lon/lat to tile conversion
- `test_camera.cpp` — screenToWorld roundtrip, zoomAtPixel anchor
- `test_timeline_camera.cpp` — screenToTime roundtrip, getTimeExtent

### Verification
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/reckoner  # still works identically
```

---

## Phase 2: Extract Pure Logic from Interaction

**Split InteractionController's pure picking/formatting from GL texture code.**

### New files in `reckoner_core`
- `src/core/PickingLogic.h` / `.cpp`
  - Move `PickResult` struct (currently Interaction.h:18-24)
  - Extract formatting helpers: `fmtTimestamp`, `fmtDuration`, `fmtLat`, `fmtLon` (Interaction.cpp anonymous namespace, lines ~293-334)

### Modify `src/Interaction.h` / `.cpp`
- Import PickResult from `core/PickingLogic.h`
- Remove formatter implementations (call through to PickingLogic)
- GL texture code (`drainPhotoTexture`, `createTexture`) stays here

### New tests
- `test_entity_picker.cpp` — pickMap finds nearest, pickTimeline binary search, out-of-radius returns -1
- `test_picking_logic.cpp` — fmtTimestamp, fmtDuration, fmtLat/fmtLon formatting

---

## Phase 3: Fix Backend Interface + Dependency Injection

**Eliminate all dynamic_cast by expanding Backend. Introduce BackendFactory.**

### Expand `src/Backend.h`
Add virtual methods with default no-op implementations:
- `streamAllEntities(on_total, batch_callback)` — default: empty
- `streamAllByType(startTime, endTime, batch_callback)` — default: empty
- `cancelFetch()` — default: no-op
- `fetchPhotoThumb(entityId)` — default: empty vector
- `fetchStats()` — default: empty ServerStats
- `entityType()` — default: empty string

Move `ServerStats` struct from `src/http/BackendAPI.h` to `src/Backend.h`.

FakeBackend inherits defaults with zero changes. HttpBackend adds `override`.

### New `src/BackendFactory.h` / `.cpp`
```cpp
struct BackendConfig { enum class Type { Fake, Http }; Type type; std::string url; };
struct BackendSet { unique_ptr<Backend> gps, photo, calendar, googleTimeline; };
BackendSet createBackends(const BackendConfig& config);
```

### Simplify MainScreen
- Replace all `dynamic_cast<HttpBackend*>(...)->method()` with `m_backend->method()` (10+ sites)
- Use `BackendFactory` in `switchBackend()` instead of inline construction
- Remove `BackendType` enum (use `BackendConfig`)

### New tests
- `test_fake_backend.cpp` — generates correct entity count, entities within extent
- `test_backend_factory.cpp` — factory produces correct types

---

## Phase 4: Decompose MainScreen

**Break the god object into focused, testable classes.**

### Extract `src/FetchOrchestrator.h` / `.cpp`
Owns: `BackendSet`, 4 futures, `PendingBatch` queue, mutex, batch drain logic.
- `startFullLoad()` — the 4 parallel async fetches (MainScreen.cpp:489-658)
- `drainCompletedBatches()` — move from queue to model (MainScreen.cpp:469-487)
- `cancelAllAndWait()` — the cancel+wait pattern repeated 3x in MainScreen
- `fetchServerStats()` — (MainScreen.cpp:660-669)
- Testable with FakeBackend, no GL required

### Extract `src/gui/ControlsPanel.h` / `.cpp`
Move ImGui controls code (MainScreen.cpp:232-376) into a class that returns an `Actions` value type rather than directly mutating state.

### Extract `src/core/FpsTracker.h` (into reckoner_core)
Trivial: tick(dt), fps(), frameMs(). Currently MainScreen.cpp:60-68.

### Resulting MainScreen (~200 lines)
Thin coordinator: owns FetchOrchestrator, cameras, renderers, InteractionController, ControlsPanel. Delegates everything.

### New tests
- `test_fetch_orchestrator.cpp` — with FakeBackend: load populates model, drain moves entities, cancel is safe
- `test_fps_tracker.cpp` — constant dt produces expected FPS

---

## Phase 5: CI / Agent Verification

### `.github/workflows/ci.yml`
- **Ubuntu headless job**: build `reckoner_core` + `reckoner_tests`, run `ctest`
- **macOS job**: build all targets (verifies GL code compiles)

### CMake option
```cmake
option(BUILD_TESTS "Build test suite" ON)
```

### Update `CLAUDE.md` with verification commands
```
## Verification
cmake --build build --target reckoner_core   # build core lib
cmake --build build --target reckoner_tests  # build tests
ctest --test-dir build --output-on-failure   # run tests
cmake --build build                          # build everything
```

---

## Final Dependency Graph

```
reckoner_core  (pure C++17, no GL/curl/ImGui)
    ├── reckoner_http  (+ curl, nlohmann_json)
    ├── reckoner (executable)  (+ GL, GLFW, ImGui)
    └── reckoner_tests  (+ Catch2)
```

## Phase Order & Risk

| Phase | Scope | Risk | Mitigation |
|-------|-------|------|------------|
| 1 | CMake + tests | Low | Camera.h GLFW removal verified safe |
| 2 | Interaction split | Medium | EntityPicker tests verify pick correctness |
| 3 | Backend interface | Low | Default implementations = zero FakeBackend changes |
| 4 | MainScreen decomposition | High | FetchOrchestrator testable with FakeBackend |
| 5 | CI pipeline | Low | Core tests run headless; GL only on macOS |
