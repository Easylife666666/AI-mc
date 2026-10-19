cmake_minimum_required(VERSION 3.20)
project(aimc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)
find_package(TBB REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)
pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
pkg_check_modules(MINIAUDIO REQUIRED IMPORTED_TARGET miniaudio)

# ---- vendored Dear ImGui -------------------------------------------------
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)
target_compile_features(imgui PUBLIC cxx_std_17)

# ---- engine core (no window needed for logic tests) ----------------------
add_library(aimc_core STATIC
  src/audio/audio.cpp
  src/game/creative.cpp
  src/game/item.cpp
  src/game/player.cpp
  src/game/settings.cpp
  src/game/raycast.cpp
  src/platform/window.cpp
  src/render/atlas.cpp
  src/render/camera.cpp
  src/render/chunk_mesh.cpp
  src/render/frustum.cpp
  src/render/selection.cpp
  src/render/shader.cpp
  src/render/sky.cpp
  src/render/screenshot.cpp
  src/world/block.cpp
  src/world/chunk.cpp
  src/world/dimension.cpp
  src/world/save.cpp
  src/world/portal.cpp
  src/world/terrain.cpp
  src/world/world.cpp
  src/ui/creative_screen.cpp
  src/ui/debug_ui.cpp
)
target_include_directories(aimc_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(aimc_core PUBLIC
  glm::glm
  imgui
  PkgConfig::GLEW
  SDL2::SDL2
  OpenGL::GL
  spdlog::spdlog
  TBB::tbb
  PkgConfig::MINIAUDIO
  ZLIB::ZLIB
  nlohmann_json::nlohmann_json
)
target_compile_options(aimc_core PRIVATE -Wall -Wextra -Wpedantic)

# ---- game executable -----------------------------------------------------
add_executable(aimc src/main.cpp)
target_link_libraries(aimc PRIVATE aimc_core)
target_compile_options(aimc PRIVATE -Wall -Wextra -Wpedantic)

# ---- tests ---------------------------------------------------------------
enable_testing()
add_executable(aimc_tests tests/test_core.cpp)
target_link_libraries(aimc_tests PRIVATE aimc_core GTest::gtest_main)
target_compile_options(aimc_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME aimc_tests COMMAND aimc_tests)
