cmake_minimum_required(VERSION 3.5)
project(gummy VERSION 0.1.1 LANGUAGES CXX)

add_compile_definitions(VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options("$<$<CONFIG:RELEASE>:-W;-Wall;-O3;-pedantic>")
add_compile_options("$<$<CONFIG:DEBUG>:-W;-Wall;-Og;-g;-foptimize-sibling-calls;-pedantic>")

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

add_subdirectory(src/gummyd)
add_subdirectory(src/gummy)

set(
    CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)

set(CPACK_GENERATOR "DEB")
set(CPACK_STRIP_FILES YES)
set(CPACK_PACKAGE_CONTACT "fushkodev@outlook.it")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_PACKAGE_DESCRIPTION "automatic and manual adjustment of screen brightness and temperature.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "X11 screen manager")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Francesco Fusco <fushkodev@outlook.it>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Fushko/gummy")
set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libxext-dev libxcb-randr0-dev libsdbus-c++-dev")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)
