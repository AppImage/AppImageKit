# required for pkg-config's IMPORTED_TARGET
cmake_minimum_required(VERSION 3.6)

# find required system dependencies via pkg-config
find_package(PkgConfig REQUIRED)


# imports a library from the standard set of variables CMake creates when using its pkg-config module or find_package
# this is code shared by import_pkgconfig_target and import_external_project, hence it's been extracted into a separate
# CMake function
#
# partly inspired by https://github.com/Kitware/CMake/blob/master/Modules/FindPkgConfig.cmake#L187
#
# positional parameters:
#  - target_name: name of the target that should be created
#  - variable_prefix: prefix of the variable that should be used to create the target from
function(import_library_from_prefix target_name variable_prefix)
    message(STATUS "Importing target ${target_name} from variable prefix ${variable_prefix}")

    if(TARGET ${target_name})
        message(WARNING "Target exists already, skipping")
        return()
    endif()

    add_library(${target_name} INTERFACE IMPORTED)

    # FIXME: the following line produces CMake errors "directory not found"
    # CMake does, however, not complain if the INCLUDE_DIRECTORIES property contains missing directories
    # possibly related: https://cmake.org/Bug/view.php?id=15052
    #set_property(TARGET ${target_name} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${variable_prefix}_INCLUDE_DIRS)
    include_directories(${${variable_prefix}_INCLUDE_DIRS})

    set_property(TARGET ${target_name} PROPERTY INTERFACE_LINK_LIBRARIES ${${variable_prefix}_LIBRARIES})

    if(${variable_prefix}_CFLAGS_OTHER)
        set_property(TARGET ${target_name} PROPERTY INTERFACE_COMPILE_OPTIONS ${${variable_prefix}_CFLAGS_OTHER})
    endif()

    if(${variable_prefix}_LIBRARY_DIRS)
        link_directories(${${variable_prefix}_LIBRARY_DIRS})
    endif()

    # export some of the imported properties with the target name as prefix
    # this is necessary to allow the other external projects, which are not built with CMake or not within the same
    # CMake context, to link to the libraries built as external projects (or the system ones, depending on the build
    # configuration)
    set(${target_name}_INCLUDE_DIRS ${${variable_prefix}_INCLUDE_DIRS} CACHE INTERNAL "")
    set(${target_name}_LIBRARIES ${${variable_prefix}_LIBRARIES} CACHE INTERNAL "")
    set(${target_name}_LIBRARY_DIRS ${${variable_prefix}_LIBRARY_DIRS} CACHE INTERNAL "")
    # TODO: the following might not always apply
    set(${target_name}_PREFIX ${CMAKE_INSTALL_PREFIX}/lib CACHE INTERNAL "")

    message(STATUS "${variable_prefix}_LIBRARIES: ${${variable_prefix}_LIBRARIES}")
endfunction()


# imports a library using pkg-config
#
# positional parameters:
#  - target_name: name of the target that we shall create for you
#  - pkg_config_target: librar(y name to pass to pkg-config (may include a version)
function(import_pkgconfig_target target_name pkg_config_target)
    message(STATUS "Importing target ${target_name} via pkg-config (${pkg_config_target})")

    pkg_check_modules(${target_name}-IMPORTED REQUIRED ${pkg_config_target})

    import_library_from_prefix(${target_name} ${target_name}-IMPORTED)
endfunction()

function(import_find_pkg_target target_name pkg_name variable_prefix)
    message(STATUS "Importing target ${target_name} via find_package (${pkg_name})")

    find_package(${pkg_name})
    if(NOT ${pkg_name}_FOUND)
        message(FATAL_ERROR "${pkg_name} could not be found on the system. You will have to either install it, or use the bundled package.")
    endif()

    import_library(${prefix})
endfunction()


# imports a library from an existing external project
#
# required parameters:
#  - TARGET_NAME:
function(import_external_project)
    set(oneValueArgs TARGET_NAME EXT_PROJECT_NAME)
    set(multiValueArgs LIBRARIES INCLUDE_DIRS LIBRARY_DIRS)
    cmake_parse_arguments(IMPORT_EXTERNAL_PROJECT "" "${oneValueArgs}" "${multiValueArgs}" "${ARGN}")

    # check whether parameters have been set
    if(NOT IMPORT_EXTERNAL_PROJECT_TARGET_NAME)
        message(FATAL_ERROR "TARGET_NAME parameter missing, but is required")
    endif()
    if(NOT IMPORT_EXTERNAL_PROJECT_EXT_PROJECT_NAME)
        message(FATAL_ERROR "EXT_PROJECT_NAME parameter missing, but is required")
    endif()
    if(NOT IMPORT_EXTERNAL_PROJECT_LIBRARIES)
        message(FATAL_ERROR "LIBRARIES parameter missing, but is required")
    endif()
    if(NOT IMPORT_EXTERNAL_PROJECT_INCLUDE_DIRS)
        message(FATAL_ERROR "INCLUDE_DIRS parameter missing, but is required")
    endif()

    message(STATUS "Importing target ${IMPORT_EXTERNAL_PROJECT_TARGET_NAME} from external project ${IMPORT_EXTERNAL_PROJECT_EXT_PROJECT_NAME}")

    if(TARGET ${target_name})
        message(WARNING "Target exists already, skipping")
        return()
    endif()

    add_library(${IMPORT_EXTERNAL_PROJECT_TARGET_NAME} INTERFACE IMPORTED)

    ExternalProject_Get_Property(${IMPORT_EXTERNAL_PROJECT_EXT_PROJECT_NAME} SOURCE_DIR)
    ExternalProject_Get_Property(${IMPORT_EXTERNAL_PROJECT_EXT_PROJECT_NAME} INSTALL_DIR)

    # "evaluate" patterns in the passed arguments by using some string replacing magic
    # this makes it easier to use this function, as some external project properties don't need to be evaluated and
    # passed beforehand, and should reduce the amount of duplicate code in this file
    foreach(item ITEMS
        IMPORT_EXTERNAL_PROJECT_EXT_PROJECT_NAME
        IMPORT_EXTERNAL_PROJECT_LIBRARIES
        IMPORT_EXTERNAL_PROJECT_INCLUDE_DIRS
        IMPORT_EXTERNAL_PROJECT_LIBRARY_DIRS)

        # create new variable with fixed string...
        string(REPLACE "<SOURCE_DIR>" "${SOURCE_DIR}" ${item}-out "${${item}}")
        # ... and set the original value to the new value
        set(${item} "${${item}-out}")

        # create new variable with fixed string...
        string(REPLACE "<INSTALL_DIR>" "${INSTALL_DIR}" ${item}-out "${${item}}")
        # ... and set the original value to the new value
        set(${item} "${${item}-out}")
    endforeach()

    set_property(TARGET ${IMPORT_EXTERNAL_PROJECT_TARGET_NAME} PROPERTY INTERFACE_LINK_LIBRARIES "${IMPORT_EXTERNAL_PROJECT_LIBRARIES}")

    # FIXME: the following line produces CMake errors "directory not found"
    # CMake does, however, not complain if the INCLUDE_DIRECTORIES property contains missing directories
    # possibly related: https://cmake.org/Bug/view.php?id=15052
    #set_property(TARGET ${IMPORT_EXTERNAL_PROJECT_TARGET_NAME} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${IMPORT_EXTERNAL_PROJECT_INCLUDE_DIRS})
    include_directories(${IMPORT_EXTERNAL_PROJECT_INCLUDE_DIRS})

    if(${IMPORT_EXTERNAL_PROJECT_LIBRARY_DIRS})
        link_directories(${IMPORT_EXTERNAL_PROJECT_LIBRARY_DIRS})
    endif()

    # finally, add a depenceny on the external project to make sure it's built
    add_dependencies(${IMPORT_EXTERNAL_PROJECT_TARGET_NAME} "${IMPORT_EXTERNAL_PROJECT_EXT_PROJECT_NAME}")

    # read external project properties, and export them with the target name as prefix
    # this is necessary to allow the other external projects, which are not built with CMake or not within the same
    # CMake context, to link to the libraries built as external projects (or the system ones, depending on the build
    # configuration)
    set(${IMPORT_EXTERNAL_PROJECT_TARGET_NAME}_INCLUDE_DIRS "${IMPORT_EXTERNAL_PROJECT_INCLUDE_DIRS}" CACHE INTERNAL "")
    set(${IMPORT_EXTERNAL_PROJECT_TARGET_NAME}_LIBRARIES "${IMPORT_EXTERNAL_PROJECT_LIBRARIES}" CACHE INTERNAL "")
    set(${IMPORT_EXTERNAL_PROJECT_TARGET_NAME}_PREFIX ${INSTALL_DIR} CACHE INTERNAL "")
endfunction()


# the names of the targets need to differ from the library filenames
# this is especially an issue with libcairo, where the library is called libcairo
# therefore, all libs imported this way have been prefixed with lib
import_pkgconfig_target(libglib glib-2.0>=2.40)
import_pkgconfig_target(libgio gio-2.0>=2.40)
import_pkgconfig_target(libzlib zlib)
import_pkgconfig_target(libcairo cairo)
import_pkgconfig_target(libopenssl openssl)
import_pkgconfig_target(libfuse fuse)


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


set(USE_SYSTEM_XZ OFF CACHE BOOL "Use system xz/liblzma instead of building our own")

if(NOT USE_SYSTEM_XZ)
    message(STATUS "Downloading and building xz")

    ExternalProject_Add(xz-EXTERNAL
        URL https://tukaani.org/xz/xz-5.2.3.tar.gz
        URL_HASH SHA512=a5eb4f707cf31579d166a6f95dbac45cf7ea181036d1632b4f123a4072f502f8d57cd6e7d0588f0bf831a07b8fc4065d26589a25c399b95ddcf5f73435163da6
        CONFIGURE_COMMAND CC=${CC} CXX=${CXX} CFLAGS=-fPIC CPPFLAGS=-fPIC <SOURCE_DIR>/configure --disable-shared --enable-static --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
        BUILD_COMMAND make
        INSTALL_COMMAND make install
    )

    import_external_project(
        TARGET_NAME xz
        EXT_PROJECT_NAME xz-EXTERNAL
        LIBRARIES "<INSTALL_DIR>/lib/liblzma.a"
        INCLUDE_DIRS "<SOURCE_DIR>/src/liblzma/api/"
    )
else()
    message(STATUS "Using system xz")

    import_find_pkg_target(xz LibLZMA LIBLZMA)
endif()


# as distros don't provide suitable squashfuse and squashfs-tools, those dependencies are bundled in, can, and should
# be used from this repository
# TODO: implement out-of-source builds for squashfuse, as for the other dependencies
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/patch-squashfuse.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/patch-squashfuse.sh
    @ONLY
)

ExternalProject_Add(squashfuse-EXTERNAL
    GIT_REPOSITORY https://github.com/vasi/squashfuse/
    GIT_TAG 1f98030
    UPDATE_COMMAND ""  # make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
    PATCH_COMMAND bash -xe ${CMAKE_CURRENT_BINARY_DIR}/patch-squashfuse.sh
    CONFIGURE_COMMAND libtoolize --force
              COMMAND env ACLOCAL_FLAGS="-I /usr/share/aclocal" aclocal
              COMMAND autoheader
              COMMAND automake --force-missing --add-missing
              COMMAND autoreconf -fi || true
              COMMAND sed -i "/PKG_CHECK_MODULES.*/,/,:./d" configure  # https://github.com/vasi/squashfuse/issues/12
              COMMAND sed -i "s/typedef off_t sqfs_off_t/typedef int64_t sqfs_off_t/g" common.h  # off_t's size might differ, see https://stackoverflow.com/a/9073762
              COMMAND CC=${CC} CXX=${CXX} <SOURCE_DIR>/configure --disable-demo --disable-high-level --without-lzo --without-lz4 --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib --with-xz=${xz_PREFIX}
              COMMAND sed -i "s|XZ_LIBS = -llzma |XZ_LIBS = -Bstatic ${xz_LIBRARIES}/|g" Makefile
    BUILD_COMMAND make
    BUILD_IN_SOURCE ON
    INSTALL_COMMAND make install
)

import_external_project(
    TARGET_NAME squashfuse
    EXT_PROJECT_NAME squashfuse-EXTERNAL
    LIBRARIES "<SOURCE_DIR>/.libs/libsquashfuse.a;<SOURCE_DIR>/.libs/libsquashfuse_ll.a;<SOURCE_DIR>/.libs/libfuseprivate.a"
    INCLUDE_DIRS "<SOURCE_DIR>"
)


set(USE_SYSTEM_INOTIFY_TOOLS OFF CACHE BOOL "Use system libinotifytools instead of building our own")

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    message(STATUS "Downloading and building inotify-tools")

    # TODO: build out of source
    ExternalProject_Add(inotify-tools-EXTERNAL
        URL https://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz
        URL_HASH SHA512=6074d510e89bba5da0d7c4d86f2562c662868666ba0a7ea5d73e53c010a0050dd1fc01959b22cffdb9b8a35bd1b0b43c04d02d6f19927520f05889e8a9297dfb
        PATCH_COMMAND wget -N --content-disposition "https://git.savannah.gnu.org/gitweb/?p=config.git$<SEMICOLON>a=blob_plain$<SEMICOLON>f=config.guess$<SEMICOLON>hb=HEAD"
              COMMAND wget -N --content-disposition "https://git.savannah.gnu.org/gitweb/?p=config.git$<SEMICOLON>a=blob_plain$<SEMICOLON>f=config.sub$<SEMICOLON>hb=HEAD"
        UPDATE_COMMAND ""  # make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
        CONFIGURE_COMMAND CC=${CC} CXX=${CXX} <SOURCE_DIR>/configure --enable-shared --enable-static --enable-doxygen=no --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
        BUILD_COMMAND make
        BUILD_IN_SOURCE ON
        INSTALL_COMMAND make install
    )

    import_external_project(
        TARGET_NAME inotify-tools
        EXT_PROJECT_NAME inotify-tools-EXTERNAL
        LIBRARIES "<INSTALL_DIR>/lib/libinotifytools.a"
        INCLUDE_DIRS "<INSTALL_DIR>/include/"
    )
else()
    message(STATUS "Using system inotify-tools")

    import_find_pkg_target(inotify-tools INotify INOTIFYTOOLS)
endif()


set(USE_SYSTEM_LIBARCHIVE OFF CACHE BOOL "Use system libarchive instead of building our own")

if(NOT USE_SYSTEM_LIBARCHIVE)
    message(STATUS "Downloading and building libarchive")

    ExternalProject_Add(libarchive-EXTERNAL
        URL https://www.libarchive.org/downloads/libarchive-3.3.1.tar.gz
        URL_HASH SHA512=90702b393b6f0943f42438e277b257af45eee4fa82420431f6a4f5f48bb846f2a72c8ff084dc3ee9c87bdf8b57f4d8dddf7814870fe2604fe86c55d8d744c164
        CONFIGURE_COMMAND CC=${CC} CXX=${CXX} CFLAGS=-fPIC CPPFLAGS=-fPIC <SOURCE_DIR>/configure --disable-shared --enable-static --disable-bsdtar --disable-bsdcat --disable-bsdcpio --with-zlib --without-bz2lib --without-iconv --without-lz4 --without-lzma --without-lzo2 --without-nettle --without-openssl --without-xml2 --without-expat --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
        BUILD_COMMAND make
        INSTALL_COMMAND make install
    )

    import_external_project(
        TARGET_NAME libarchive
        EXT_PROJECT_NAME libarchive-EXTERNAL
        LIBRARIES "<INSTALL_DIR>/lib/libarchive.a"
        INCLUDE_DIRS "<INSTALL_DIR>/include/"
    )
else()
    message(STATUS "Using system libarchive")

    import_find_pkg_target(libarchive LibArchive LibArchive)
endif()


set(USE_SYSTEM_GTEST OFF CACHE BOOL "Use system GTest instead of downloading and building GTest")

if(NOT USE_SYSTEM_GTEST)
    message(STATUS "Downloading and building GTest")

    ExternalProject_Add(gtest-EXTERNAL
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.8.0
        UPDATE_COMMAND ""  # make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -G${CMAKE_GENERATOR} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> <SOURCE_DIR>/googletest
    )

    import_external_project(
        TARGET_NAME gtest
        EXT_PROJECT_NAME gtest-EXTERNAL
        LIBRARIES "<INSTALL_DIR>/lib/libgtest.a;<INSTALL_DIR>/lib/libgtest_main.a"
        INCLUDE_DIRS "<INSTALL_DIR>/include/"
    )
else()
    message(STATUS "Using system GTest")

    import_find_pkg_target(gtest GTest GTEST)
endif()


# TODO: allow using system wide mksquashfs
ExternalProject_Add(mksquashfs
    GIT_REPOSITORY https://github.com/plougher/squashfs-tools/
    GIT_TAG 5be5d61
    UPDATE_COMMAND ""  # make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
    PATCH_COMMAND patch -N -p1 < ${PROJECT_SOURCE_DIR}/src/mksquashfs-mkfs-fixed-timestamp.patch || true
    CONFIGURE_COMMAND sed -i "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += -DXZ_SUPPORT -I${xz_INCLUDE_DIRS}|g" <SOURCE_DIR>/squashfs-tools/Makefile
    COMMAND sed -i "s|LIBS += -llzma|LIBS += -Bstatic ${xz_LIBRARIES}|g" <SOURCE_DIR>/squashfs-tools/Makefile
    COMMAND sed -i "s|install: mksquashfs unsquashfs|install: mksquashfs|g" squashfs-tools/Makefile
    COMMAND sed -i "/cp unsquashfs/d" squashfs-tools/Makefile
    BUILD_COMMAND CC=${CC} CXX=${CXX} make -C squashfs-tools/ XZ_SUPPORT=1 mksquashfs
    # make install unfortunately expects unsquashfs to be built as well, hence can't install the binary
    # therefore using built file in SOURCE_DIR
    # TODO: implement building out of source
    BUILD_IN_SOURCE ON
    INSTALL_COMMAND make -C squashfs-tools/ install INSTALL_DIR=<INSTALL_DIR>
)

ExternalProject_Get_Property(mksquashfs INSTALL_DIR)
set(mksquashfs_INSTALL_DIR "${INSTALL_DIR}")
mark_as_advanced(mksquashfs_INSTALL_DIR)

# for later use when packaging as an AppImage
set(mksquashfs_BINARY "${mksquashfs_INSTALL_DIR}/mksquashfs")
mark_as_advanced(mksquashfs_BINARY)


#### build dependency configuration ####

# only have to build custom xz when not using system libxz
if(NOT USE_SYSTEM_XZ)
    add_dependencies(squashfuse xz)
    add_dependencies(mksquashfs xz)
endif()
