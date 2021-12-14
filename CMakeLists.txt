cmake_minimum_required(VERSION 3.5)
project(gummy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT BINARY_DIR)
	set(BINARY_DIR "bin")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
set(DAEMON_TARGET gummyd)
set(DAEMON_PATH "${CMAKE_INSTALL_PREFIX}/${BINARY_DIR}/${DAEMON_TARGET}")

add_compile_definitions(DAEMON_PATH="${DAEMON_PATH}")

file(GLOB COMMON src/common/*.cpp)
file(GLOB SOURCES src/gummyd/*.cpp)
file(GLOB SOURCES_CLI src/gummy/*.cpp)

add_executable(${DAEMON_TARGET} ${SOURCES} ${COMMON})
add_executable(${PROJECT_NAME} ${SOURCES_CLI} ${COMMON})

find_package(X11 REQUIRED)
find_package(Threads REQUIRED)
find_package(sdbus-c++ REQUIRED)
find_library(XCB_LIB "xcb" REQUIRED)
find_library(XCB_RANDR_LIB "xcb-randr" REQUIRED)

target_link_libraries(${DAEMON_TARGET} PRIVATE
	${X11_LIBRARIES}
	Threads::Threads
	SDBusCpp::sdbus-c++
	${XCB_LIB}
	${XCB_RANDR_LIB})

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_include_directories(${DAEMON_TARGET} PRIVATE include)

install(TARGETS ${DAEMON_TARGET} ${PROJECT_NAME} RUNTIME DESTINATION ${BINARY_DIR})
install(FILES 90-backlight.rules DESTINATION /usr/lib/udev/rules.d)

include(CPack)
