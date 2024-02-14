cmake_minimum_required(VERSION 3.16)
project(server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O3")

include(FetchContent)

FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG  tags/v0.9.6
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(simdjson)

add_executable(server 
              main.cpp 
              simdjson.cpp
              process.cpp
              ws.cpp
              ws.hpp)
target_link_libraries(server simdjson)
target_compile_definitions(simdjson PUBLIC SIMDJSON_EXCEPTIONS=OFF)
