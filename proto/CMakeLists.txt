cmake_minimum_required(VERSION 3.16)
project(server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O3")
find_package(Boost 1.71 REQUIRED system thread regex)
if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found! Aborting...")
endif()
message("Boost Found at ${Boost_INCLUDE_DIRS}")
message("Boost link directory ${Boost_LIBRARY_DIRS}")


include(FetchContent)

FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG  tags/v0.9.6
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(simdjson)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
add_executable(server server.cpp)
target_link_libraries(server simdjson)
target_link_libraries(server ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
target_compile_definitions(simdjson PUBLIC SIMDJSON_EXCEPTIONS=OFF)
