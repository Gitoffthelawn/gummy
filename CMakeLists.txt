cmake_minimum_required(VERSION 3.5)

project(gummy LANGUAGES CXX)
set(DAEMON_TARGET gummyd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB COMMONS src/commons/*.cpp)
file(GLOB SOURCES src/gummyd/*.cpp)
add_executable(${DAEMON_TARGET} ${SOURCES} ${COMMONS})

find_library(XCB_LIB "xcb" REQUIRED)
find_library(XCB_RANDR_LIB "xcb-randr" REQUIRED)
find_library(XCB_SHM_LIB "xcb-shm" REQUIRED)
target_link_libraries(${DAEMON_TARGET} PRIVATE "${XCB_LIB}")
target_link_libraries(${DAEMON_TARGET} PRIVATE "${XCB_RANDR_LIB}")
target_link_libraries(${DAEMON_TARGET} PRIVATE "${XCB_SHM_LIB}")

file(GLOB CLI_SOURCES src/gummy/*.cpp)
add_executable(${PROJECT_NAME} ${CLI_SOURCES} ${COMMONS})
