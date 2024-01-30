# FetchContent_MakeAvailable() is only available in CMake 3.14 or newer
cmake_minimum_required(VERSION 3.14)

include(FetchContent)

# Need this patch until https://github.com/AppImage/libappimage/pull/160 is resolved
FetchContent_Declare(libappimage_patch
    URL https://github.com/AppImage/libappimage/commit/ce0a186a5a3cd8f31f4afd216d5322410a0a8e26.patch
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(libappimage_patch)

FetchContent_Declare(libappimage
    # We can not use a URL source with a github-generated source archive: libappimage's gtest submodule would be missing
    # If you update the GIT_TAG and the patch does not apply anymore you need to rebase libappimage_patch (see above)
GIT_REPOSITORY https://github.com/AppImage/libappimage
    GIT_TAG aa7d9fb03d3d64415c37120f20faa05412458e94  # Eventually we may want to use master, once that works reliably.
    PATCH_COMMAND patch -p 1 --forward < ${libappimage_patch_SOURCE_DIR}/ce0a186a5a3cd8f31f4afd216d5322410a0a8e26.patch
)
FetchContent_MakeAvailable(libappimage)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${libappimage_SOURCE_DIR}/cmake)
include(${libappimage_SOURCE_DIR}/cmake/scripts.cmake)
include(${libappimage_SOURCE_DIR}/cmake/tools.cmake)


# the names of the targets need to differ from the library filenames
# this is especially an issue with libcairo, where the library is called libcairo
# therefore, all libs imported this way have been prefixed with lib
import_pkgconfig_target(TARGET_NAME libfuse PKGCONFIG_TARGET fuse)
# openssl is required for optional tools only, and doesn't need to be enforced
# FIXME: remove dependency to openssl by implementing own SHA hashes in libappimage_hashlib
import_pkgconfig_target(TARGET_NAME libssl PKGCONFIG_TARGET openssl OPTIONAL)
import_pkgconfig_target(TARGET_NAME libgio PKGCONFIG_TARGET gio OPTIONAL)


if(USE_CCACHE)
    message(STATUS "Using CCache to build AppImageKit dependencies")
    # TODO: find way to use find_program with all possible paths
    # (might differ from distro to distro)
    # these work on Debian and Ubuntu:
    set(CC "/usr/lib/ccache/gcc")
    set(CXX "/usr/lib/ccache/g++")
else()
    set(CC "${CMAKE_C_COMPILER}")
    set(CXX "${CMAKE_CXX_COMPILER}")
endif()

set(CFLAGS ${DEPENDENCIES_CFLAGS})
set(CPPFLAGS ${DEPENDENCIES_CPPFLAGS})
set(LDFLAGS ${DEPENDENCIES_LDFLAGS})

set(USE_SYSTEM_MKSQUASHFS OFF CACHE BOOL "Use system mksquashfs instead of downloading and building our own. Warning: you need a recent version otherwise it might not work as intended.")

if(NOT USE_SYSTEM_MKSQUASHFS)
    set(mksquashfs_cflags "-DXZ_SUPPORT ${CFLAGS}")

    if(NOT xz_LIBRARIES OR xz_LIBRARIES STREQUAL "")
        message(FATAL_ERROR "xz_LIBRARIES not set")
    elseif(xz_LIBRARIES MATCHES "\\.a$")
        set(mksquashfs_ldflags "${xz_LIBRARIES}")
    else()
        set(mksquashfs_ldflags "-l${xz_LIBRARIES}")
    endif()

    if(xz_INCLUDE_DIRS)
        set(mksquashfs_cflags "${mksquashfs_cflags} -I${xz_INCLUDE_DIRS}")
    endif()
    if(xz_LIBRARY_DIRS)
        set(mksquashfs_ldflags "${mksquashfs_ldflags} -L${xz_LIBRARY_DIRS}")
    endif()

    ExternalProject_Add(mksquashfs
        GIT_REPOSITORY https://github.com/plougher/squashfs-tools/
        GIT_TAG 4.5.1
        UPDATE_COMMAND ""  # Make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
        CONFIGURE_COMMAND ${SED} -i "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += ${mksquashfs_cflags}|g" <SOURCE_DIR>/squashfs-tools/Makefile
        COMMAND ${SED} -i "/INSTALL_MANPAGES_DIR/d" <SOURCE_DIR>/squashfs-tools/Makefile
        COMMAND ${SED} -i "s|LIBS += -llzma|LIBS += -Bstatic ${mksquashfs_ldflags}|g" <SOURCE_DIR>/squashfs-tools/Makefile
        COMMAND ${SED} -i "s|install: mksquashfs unsquashfs|install: mksquashfs|g" squashfs-tools/Makefile
        COMMAND ${SED} -i "/cp unsquashfs/d" squashfs-tools/Makefile
        BUILD_COMMAND env CC=${CC} CXX=${CXX} LDFLAGS=${LDFLAGS} ${MAKE} -C squashfs-tools/ XZ_SUPPORT=1 mksquashfs
        # ${MAKE} install unfortunately expects unsquashfs to be built as well, hence can't install the binary
        # therefore using built file in SOURCE_DIR
        # TODO: implement building out of source
        BUILD_IN_SOURCE ON
        INSTALL_COMMAND ${MAKE} -C squashfs-tools/ install INSTALL_DIR=<INSTALL_DIR>
    )

    ExternalProject_Get_Property(mksquashfs INSTALL_DIR)
    set(mksquashfs_INSTALL_DIR "${INSTALL_DIR}")
    mark_as_advanced(mksquashfs_INSTALL_DIR)

    # for later use when packaging as an AppImage
    set(mksquashfs_BINARY "${mksquashfs_INSTALL_DIR}/mksquashfs")
    mark_as_advanced(mksquashfs_BINARY)
else()
    message(STATUS "Using system mksquashfs")

    set(mksquashfs_BINARY "mksquashfs")
endif()

#### build dependency configuration ####

# only have to build custom xz when not using system libxz
if(TARGET xz-EXTERNAL)
    if(TARGET mksquashfs)
        ExternalProject_Add_StepDependencies(mksquashfs configure xz-EXTERNAL)
    endif()
endif()
