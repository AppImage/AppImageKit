cmake_minimum_required(VERSION 3.2)

project(AppImageKit)

include(ExternalProject)

# enable testing globally
include(CTest)

# support for ccache
# call CMake with -DUSE_CCACHE=ON to make use of it
set(USE_CCACHE OFF CACHE BOOL "")
if(USE_CCACHE)
    find_program(CCACHE ccache)
    if(CCACHE)
        message(STATUS "Using ccache")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE})
    else()
        message(WARNING "USE_CCACHE set, but could not find ccache")
    endif()
endif()


#####################
# build information #
#####################

# determine Git commit ID
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# set version and build number
set(VERSION 1-alpha)
mark_as_advanced(VERSION)
if("$ENV{TRAVIS_BUILD_NUMBER}" STREQUAL "")
    set(BUILD_NUMBER "<local dev build>")
else()
    set(BUILD_NUMBER "$ENV{TRAVIS_BUILD_NUMBER}")
endif()
mark_as_advanced(BUILD_NUMBER)

# get current date
execute_process(
    COMMAND env LC_ALL=C date -u "+%Y-%m-%d %H:%M:%S %Z"
    OUTPUT_VARIABLE DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# C and C++ versions
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


##########################
# configure dependencies #
##########################

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

# configure dependencies
add_subdirectory(lib)

include(lib/libappimage/cmake/tools.cmake)
include(cmake/dependencies.cmake)


################
# Source files #
################

# sanitizer support
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})

add_subdirectory(src)

################################
#           packaging          #
################################
set(APPIMAGEKIT_PACKAGE_DEBS FALSE CACHE BOOL "")

if(APPIMAGEKIT_PACKAGE_DEBS)
    include(cmake/cpack_debs.cmake)
endif()
