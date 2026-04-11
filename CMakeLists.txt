cmake_minimum_required(VERSION 3.21)
project(reckoner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build test suite" ON)

# Fetch nlohmann_json for JSON parsing
include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Fetch GLFW from source
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Find dependencies
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)

# Build ImGui from source
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# Shader path (can be overridden with -DSHADER_BASE_DIR=<path>)
if(NOT DEFINED SHADER_BASE_DIR)
  set(SHADER_BASE_DIR "${CMAKE_SOURCE_DIR}/src/shaders")
endif()

# ---------------------------------------------------------------------------
# reckoner_core — pure C++17 logic, zero GL/GLFW/ImGui/curl dependencies
# ---------------------------------------------------------------------------
add_library(reckoner_core STATIC
  src/core/EnvLoader.cpp
  src/core/PickingLogic.cpp
  src/core/SolarCalculations.cpp
  src/core/TimeUtils.cpp
  src/Camera.cpp
  src/TimelineCamera.cpp
  src/EntityPicker.cpp
)
target_include_directories(reckoner_core PUBLIC src)
target_link_libraries(reckoner_core PUBLIC nlohmann_json::nlohmann_json)

# ---------------------------------------------------------------------------
# reckoner_http — HTTP/backend code (curl + json)
# ---------------------------------------------------------------------------
add_library(reckoner_http STATIC
  src/http/HttpClient.cpp
  src/http/BackendAPI.cpp
  src/HttpBackend.cpp
  src/BackendFactory.cpp
)
target_include_directories(reckoner_http PUBLIC src)
target_link_libraries(reckoner_http PUBLIC
  reckoner_core
  CURL::libcurl
  nlohmann_json::nlohmann_json
)

# ---------------------------------------------------------------------------
# reckoner executable — GL/ImGui sources (explicit list, no GLOB_RECURSE)
# ---------------------------------------------------------------------------
add_executable(reckoner
  src/main.cpp
  src/app/App.cpp
  src/screens/MainScreen.cpp
  src/Interaction.cpp
  src/Renderer.cpp
  src/TimelineRenderer.cpp
  src/renderer/CalendarRenderer.cpp
  src/renderer/HistogramRenderer.cpp
  src/renderer/LineRenderer.cpp
  src/renderer/MoonAltitudeRenderer.cpp
  src/renderer/PointRenderer.cpp
  src/renderer/Shader.cpp
  src/renderer/SolarAltitudeRenderer.cpp
  src/renderer/TextRenderer.cpp
  src/tiles/TileRenderer.cpp
  src/tiles/TileCache.cpp
  src/tiles/RasterTileRenderer.cpp
  src/tiles/RasterTileCache.cpp
  src/tiles/MvtDecoder.cpp
)

target_include_directories(reckoner PRIVATE
    src
    external/stb
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_compile_definitions(reckoner PRIVATE
    SHADER_BASE_DIR="${SHADER_BASE_DIR}"
)
target_link_libraries(reckoner PRIVATE
    reckoner_core
    reckoner_http
    imgui
    glfw
    OpenGL::GL
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
