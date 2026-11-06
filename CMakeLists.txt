cmake_minimum_required(VERSION 3.20)
project(LittleTree VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11 xrender)

add_library(littletree_core
  src/core/Plant.cpp
  src/core/Weather.cpp
  src/core/Simulation.cpp
  src/core/Storage.cpp
)
target_include_directories(littletree_core PUBLIC src)
target_compile_options(littletree_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(LittleTree
  src/main.cpp
  src/graphics/Sprite.cpp
  src/graphics/Renderer.cpp
  src/platform/linux/Window.cpp
)
target_link_libraries(LittleTree PRIVATE littletree_core PkgConfig::X11 m)
target_compile_options(LittleTree PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
  add_executable(little_tree_tests tests/CoreTests.cpp)
  target_link_libraries(little_tree_tests PRIVATE littletree_core m)
  target_compile_options(little_tree_tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME little_tree_tests COMMAND little_tree_tests)
endif()
