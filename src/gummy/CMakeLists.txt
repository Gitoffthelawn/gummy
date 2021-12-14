project(gummy LANGUAGES CXX)

file(GLOB SOURCES *.cpp)
file(GLOB COMMON ../common/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES} ${COMMON})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${BINARY_DIR})
