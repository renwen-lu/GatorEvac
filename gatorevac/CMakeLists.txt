cmake_minimum_required(VERSION 3.16)
project(GatorEvac LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(gatorevac_core
  src/graph.cpp
  src/kd_tree.cpp
  src/router.cpp
  src/scenario.cpp
  src/engine.cpp)

add_executable(gatorevac-cli src/cli.cpp)
target_link_libraries(gatorevac-cli PRIVATE gatorevac_core)

find_package(Threads REQUIRED)
add_executable(gatorevac-web src/main.cpp)
target_include_directories(gatorevac-web PRIVATE third_party)
target_link_libraries(gatorevac-web PRIVATE gatorevac_core Threads::Threads)
if(WIN32)
  target_link_libraries(gatorevac-web PRIVATE ws2_32)
endif()
