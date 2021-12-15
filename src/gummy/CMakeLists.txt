project(gummy LANGUAGES CXX)

file(GLOB SOURCES *.cpp)
file(GLOB COMMON ../common/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES} ${COMMON})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_compile_definitions(CMAKE_INSTALL_DAEMON_PATH="${CMAKE_INSTALL_FULL_LIBEXECDIR}/gummyd")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
