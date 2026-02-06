cmake_minimum_required(VERSION 3.21)
project(reckoner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch nlohmann_json for JSON parsing
include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Find dependencies
find_package(glfw3 REQUIRED)
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

# Collect all source files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  src/*.cpp
)

# Create executable
add_executable(reckoner ${SRC_FILES})

# Include directories
target_include_directories(reckoner PRIVATE src)

# Link libraries
target_link_libraries(reckoner PRIVATE
  glfw
  OpenGL::GL
  imgui
  CURL::libcurl
  nlohmann_json::nlohmann_json
)
