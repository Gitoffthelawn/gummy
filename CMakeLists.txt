cmake_minimum_required(VERSION 3.5)
project(gummy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

add_subdirectory(src/gummyd)
add_subdirectory(src/gummy)

#include(CPack)
