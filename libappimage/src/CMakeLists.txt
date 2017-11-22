###
# AppImage deployment utils package
###

include(../cmake/cmake_import_manual_targets.cmake)

find_package(PkgConfig)

pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

pkg_check_modules(GIO REQUIRED gio-2.0)
include_directories(${GIO_INCLUDE_DIRS})

pkg_check_modules(CAIRO REQUIRED cairo)
include_directories(${CAIRO_INCLUDE_DIRS})


include_directories("../../squashfuse/")
include_directories("../../libarchive-3.3.1/libarchive")

SET(LIBAPPIMAGE_DEPLOYMENT_SRC
    appimage.h

    ../../shared.c
    ../../getsection.c
    ../../notify.c
    ../../elf.c
)

add_library(appimage SHARED ${LIBAPPIMAGE_DEPLOYMENT_SRC})

set_target_properties(appimage PROPERTIES 
    VERSION ${APPIMAGE_VERSION}
    PUBLIC_HEADER "appimage.h"
)

target_compile_definitions(appimage
    PRIVATE _FILE_OFFSET_BITS=64
    PRIVATE HAVE_LIBARCHIVE3=0
    PRIVATE VERSION_NUMBER="${APPIMAGE_VERSION}"
)
   
target_link_libraries(appimage 
    PRIVATE ssl
    PRIVATE squashfuse
    PRIVATE crypto
    PRIVATE z
    PRIVATE lzma
    PRIVATE pthread
    PRIVATE archive
    PRIVATE inotifytools
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)
      
install(TARGETS appimage
    EXPORT AppImageTargets
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/appimage" COMPONENT dev
)
