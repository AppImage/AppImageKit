project(libappimage)
cmake_minimum_required(VERSION 3.5)

set(APPIMAGE_MAJOR_VERSION 0)
set(APPIMAGE_MINOR_VERSION 1)
set(APPIMAGE_PATCH_VERSION 0)
set(APPIMAGE_VERSION
  ${APPIMAGE_MAJOR_VERSION}.${APPIMAGE_MINOR_VERSION}.${APPIMAGE_PATCH_VERSION})


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
add_library(libappimage ${LIBAPPIMAGE_SRC})
target_compile_definitions(libappimage
    PRIVATE _FILE_OFFSET_BITS=64
    PRIVATE HAVE_LIBARCHIVE3=0
    PRIVATE VERSION_NUMBER="${VERSION_NUMBER}"
)
set_target_properties(libappimage PROPERTIES PREFIX "")
set_target_properties(libappimage PROPERTIES VERSION ${VERSION_NUMBER})
set_target_properties(libappimage PROPERTIES PUBLIC_HEADER ./libappimage.h)

target_link_libraries(libappimage
    pthread
    ${squashfuse_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${libarchive_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${inotify-tools_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)
add_dependencies(libappimage squashfuse)

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    add_dependencies(appimaged inotify-tools)
endif()
if(NOT USE_SYSTEM_LIBARCHIVE)
    add_dependencies(appimaged libarchive)
endif()


###
# Deploy
###
include(cmake/ConfigureDeploy.cmake)

enable_testing()
add_subdirectory(test)