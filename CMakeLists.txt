cmake_minimum_required(VERSION 3.5)

project(gummyd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SOURCES src/main.cpp
	src/defs.h
	src/cfg.h src/cfg.cpp
	src/utils.h src/utils.cpp)

add_executable(gummyd ${SOURCES})
