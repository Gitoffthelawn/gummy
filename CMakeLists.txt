cmake_minimum_required(VERSION 3.5)
project(gummy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT BINARY_DIR)
	set(BINARY_DIR "bin")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})

add_subdirectory(src/gummyd)
add_subdirectory(src/gummy)

install(FILES 90-backlight.rules DESTINATION /usr/lib/udev/rules.d)

#include(CPack)
