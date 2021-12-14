project(gummyd LANGUAGES CXX)

set(DAEMON_TARGET ${PROJECT_NAME})

file(GLOB COMMON ../common/*.cpp)
file(GLOB SOURCES *.cpp)

add_executable(${DAEMON_TARGET} ${SOURCES} ${COMMON})

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

target_include_directories(${DAEMON_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/include)

install(TARGETS ${DAEMON_TARGET} RUNTIME DESTINATION ${BINARY_DIR})
