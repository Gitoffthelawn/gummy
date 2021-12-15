cmake_minimum_required(VERSION 3.5)
project(gummy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CACHEFILE_DIR}/${CMAKE_INSTALL_BINDIR})

add_subdirectory(src/gummyd)
add_subdirectory(src/gummy)

#include(CPack)
