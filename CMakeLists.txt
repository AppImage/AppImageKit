cmake_minimum_required(VERSION 3.2)

project(AppImageKit)

include(ExternalProject)

# enable testing globally
include(CTest)


#####################
# build information #
#####################

# determine Git commit ID
execute_process(
    COMMAND git describe --tags --always --abbrev=7
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# set version and build number
set(VERSION 1-alpha)
if("$ENV{TRAVIS_BUILD_NUMBER}" STREQUAL "")
    set(BUILD_NUMBER "<local dev build>")
else()
    set(BUILD_NUMBER "$ENV{TRAVIS_BUILD_NUMBER}")
endif()

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

# first of all, make sure required programs are available
function(check_program name)
    string(TOUPPER ${name} name_upper)
    find_program(${name_upper} ${name})

    message(STATUS "Checking for program ${name}")

    if(NOT ${name_upper})
        message(FATAL_ERROR "Could not find required program ${name}.")
    endif()

    mark_as_advanced(${name_upper})
endfunction()

check_program(aclocal)
check_program(autoheader)
check_program(automake)
check_program(autoreconf)
check_program(libtoolize)
check_program(patch)
check_program(sed)
check_program(wget)
# TODO: add checks for remaining commands


# configure dependencies
include(cmake/dependencies.cmake)


# include directories globally
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${CAIRO_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${xz_INCLUDE_DIR})
include_directories(${inotify-tools_INCLUDE_DIR})
include_directories(${squashfuse_INCLUDE_DIR})
include_directories(${libarchive_INCLUDE_DIR})


################
# Source files #
################

# sanitizer support
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})

add_subdirectory(src)


################################
# unit and functionality tests #
################################

add_subdirectory(tests)
