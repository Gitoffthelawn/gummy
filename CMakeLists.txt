cmake_minimum_required(VERSION 3.5)

project(gummyd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB SOURCES src/gummyd/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})

find_library(XCB_LIB REQUIRED)
find_library(XCB_RANDR_LIB REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE "${XCB_LIB}")
target_link_libraries(${PROJECT_NAME} PRIVATE "${XCB_RANDR_LIB}")

file(GLOB CLI_SOURCES src/gummy/*.cpp)
add_executable(gummy ${CLI_SOURCES})
