include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(reckoner_tests
  test_vec2.cpp
  test_mat3.cpp
  test_entity.cpp
  test_time_utils.cpp
  test_solar.cpp
  test_ring_buffer.cpp
  test_tile_math.cpp
  test_camera.cpp
  test_timeline_camera.cpp
  test_picking_logic.cpp
  test_entity_picker.cpp
  test_fake_backend.cpp
  test_backend_factory.cpp
  test_fetch_orchestrator.cpp
  test_fps_tracker.cpp
)

target_link_libraries(reckoner_tests PRIVATE
  reckoner_core
  reckoner_http
  Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(reckoner_tests)
