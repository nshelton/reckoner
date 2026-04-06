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

# Collect all sources, excluding the old hot-reload plumbing
file(GLOB_RECURSE ALL_SRCS CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM ALL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HotLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/PluginMain.cpp
)

add_executable(reckoner ${ALL_SRCS})

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
    imgui
    glfw
    OpenGL::GL
    CURL::libcurl
    nlohmann_json::nlohmann_json
)
