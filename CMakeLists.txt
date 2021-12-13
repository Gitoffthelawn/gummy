cmake_minimum_required(VERSION 3.5)

project(gummy LANGUAGES CXX)
set(DAEMON_TARGET gummyd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

file(GLOB COMMONS src/commons/*.cpp)
file(GLOB SOURCES src/gummyd/*.cpp)
add_executable(${DAEMON_TARGET} ${SOURCES} ${COMMONS})

find_library(XCB_LIB "xcb" REQUIRED)
target_link_libraries(${DAEMON_TARGET} PRIVATE "${XCB_LIB}")

find_library(XCB_RANDR_LIB "xcb-randr" REQUIRED)
target_link_libraries(${DAEMON_TARGET} PRIVATE "${XCB_RANDR_LIB}")

find_package(sdbus-c++ REQUIRED)
target_link_libraries(${DAEMON_TARGET} PRIVATE SDBusCpp::sdbus-c++)

find_package(X11 REQUIRED)
target_link_libraries(${DAEMON_TARGET} PRIVATE ${X11_LIBRARIES})

find_package(Threads REQUIRED)
target_link_libraries(${DAEMON_TARGET} PRIVATE Threads::Threads)

file(GLOB CLI_SOURCES src/gummy/*.cpp)
add_executable(${PROJECT_NAME} ${CLI_SOURCES} ${COMMONS})

install(TARGETS ${DAEMON_TARGET} ${PROJECT_NAME} RUNTIME DESTINATION bin)

install(FILES 90-backlight.rules DESTINATION /usr/lib/udev/rules.d)
