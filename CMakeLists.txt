cmake_minimum_required(VERSION 3.16)
project(SCAPMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SCAPMS_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(scapms ${SCAPMS_SOURCES})
target_include_directories(scapms PRIVATE include)
find_package(Threads REQUIRED)
target_link_libraries(scapms PRIVATE Threads::Threads)
