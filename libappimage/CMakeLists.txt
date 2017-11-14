project(appimagelib)
cmake_minimum_required(VERSION 3.5)

set(APPIMAGE_MAJOR_VERSION 0)
set(APPIMAGE_MINOR_VERSION 1)
set(APPIMAGE_PATCH_VERSION 0)
set(APPIMAGE_VERSION
  ${APPIMAGE_MAJOR_VERSION}.${APPIMAGE_MINOR_VERSION}.${APPIMAGE_PATCH_VERSION})
  
SET (STATIC_BUILD NO CACHE PATH "Link against static libs.")

include(cmake/ImportedTargets.cmake)


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
include(cmake/ConfigureDeploy.cmake)

enable_testing()
add_subdirectory(test)