project(appimagelib)
cmake_minimum_required(VERSION 3.5)

SET (STATIC_BUILD NO)
###
# Registering external targets
###
add_library(squashfuse STATIC IMPORTED)
set_property(TARGET squashfuse PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../squashfuse/.libs/libsquashfuse.a")

add_library(fuseprivate STATIC IMPORTED)
set_property(TARGET fuseprivate PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../squashfuse/.libs/libfuseprivate.a")


if( ${STATIC_BUILD} )
    add_library(archive STATIC IMPORTED)
    set_property(TARGET archive
        PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../libarchive-3.3.1/.libs/libarchive.a")

    add_library(lzma STATIC IMPORTED)
    set_property(TARGET lzma 
        PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../xz-5.2.3/build/lib/liblzma.a")
endif( ${STATIC_BUILD} )


###
# Looking for dependencies
###
find_package(PkgConfig)

execute_process(
    COMMAND git describe --tags --always --abbrev=7
    OUTPUT_VARIABLE VERSION_NUMBER
)
string(STRIP ${VERSION_NUMBER} VERSION_NUMBER)

pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

pkg_check_modules(GIO REQUIRED gio-2.0)
include_directories(${GIO_INCLUDE_DIRS})


pkg_check_modules(CAIRO REQUIRED cairo)
include_directories(${CAIRO_INCLUDE_DIRS})

###
# Sources
###
include_directories("../squashfuse/")
include_directories("../libarchive-3.3.1/libarchive")

SET(LIBAPPIMAGE_SRC
    libappimage.h

    ../shared.c
    ../getsection.c
    ../notify.c
    ../elf.c
)

###
# Build
###
add_library(appimage ${LIBAPPIMAGE_SRC})
target_compile_definitions(appimage
    PRIVATE _FILE_OFFSET_BITS=64
    PRIVATE HAVE_LIBARCHIVE3=0
    PRIVATE VERSION_NUMBER="${VERSION_NUMBER}"
)
set_target_properties(appimage PROPERTIES VERSION ${VERSION_NUMBER})
set_target_properties(appimage PROPERTIES PUBLIC_HEADER ./libappimage.h)

target_link_libraries(appimage
    ssl
    squashfuse
    crypto
    z
    lzma
    pthread
    archive
    inotifytools
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

###
# Deploy
###
install(TARGETS appimage DESTINATION lib/appimage)


###
# Tests 
###
enable_testing()

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${CMAKE_BINARY_DIR})

# Link runTests with what we want to test and the GTest and pthread library
add_executable(test_appimagelib
    test_appimagelib.cpp
)

target_link_libraries(test_appimagelib 
    appimage    
    pthread  
    ${GTEST_LIBRARIES}
    ssl
    crypto
    lzma
    pthread
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

add_dependencies(test_appimagelib appimage)

add_test(test_appimagelib test_appimagelib)