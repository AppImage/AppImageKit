cmake_minimum_required(VERSION 3.2)

include(ExternalProject)

#####################
# build information #
#####################

execute_process(
    COMMAND git describe --tags --always --abbrev=7
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)


##########################
# configure dependencies #
##########################

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LZMA REQUIRED liblzma)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(CAIRO REQUIRED cairo)

set(USE_SYSTEM_XZ OFF CACHE BOOL "Use system xz (well, liblzma) instead of bundled one")

if(NOT USE_SYSTEM_XZ)
    message(STATUS "Using bundled libxz")

    ExternalProject_Add(xz
        URL http://tukaani.org/xz/xz-5.2.3.tar.gz
        URL_MD5 ef68674fb47a8b8e741b34e429d86e9d
        CONFIGURE_COMMAND ./configure --enable-static --prefix=${CMAKE_INSTALL_PREFIX} --libdir=${CMAKE_INSTALL_PREFIX}/lib
        BUILD_COMMAND make -j8
        BUILD_IN_SOURCE true
        INSTALL_COMMAND true
    )

    ExternalProject_Get_Property(xz SOURCE_DIR)
    set(xz_SOURCE_DIR "${SOURCE_DIR}")
    mark_as_advanced(SOURCE_DIR)

    set(xz_INCLUDE_DIR "${SOURCE_DIR}")
    set(xz_LIBRARY_DIR "${xz_SOURCE_DIR}/src/liblzma/.libs")
    set(xz_LIBRARY "${xz_LIBRARY_DIR}/liblzma.a")

    set(xz_PREFIX "${xz_SOURCE_DIR}/build")
else()
    message(STATUS "Using system libxz")

    find_package(LibLZMA)
    if(NOT LIBLZMA_FOUND)
        message(FATAL_ERROR "liblzma could not be found on the system. You will have to either install it, or use the bundled xz.")
    endif()

    # should be a single directory, although the variable name is plural, see FindLibLZMA.cmake
    set(xz_INCLUDE_DIR "${LIBLZMA_INCLUDE_DIRS}")
    # same as with the include dir
    set(xz_LIBRARY "${LIBLZMA_LIBRARIES}")
    # see https://github.com/vasi/squashfuse README, should use /usr/lib when using the system wide libxz
    set(xz_PREFIX "/usr/lib")
endif()

mark_as_advanced(xz_SOURCE_DIR xz_INCLUDE_DIR xz_LIBRARY xz_PREFIX)

# as distros don't provide suitable squashfuse and squashfs-tools, those dependencies are bundled in, can, and should
# be used from this repository
ExternalProject_Add(squashfuse
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/squashfuse
    PATCH_COMMAND bash -xe ${PROJECT_SOURCE_DIR}/patch.sh
    CONFIGURE_COMMAND libtoolize --force
              COMMAND env ACLOCAL_FLAGS="-I /usr/share/aclocal" aclocal
              COMMAND autoheader
              COMMAND automake --force-missing --add-missing
              COMMAND autoreconf -fi || true
              COMMAND sed -i "/PKG_CHECK_MODULES.*/,/,:./d" configure  # https://github.com/vasi/squashfuse/issues/12
              COMMAND ./configure --disable-demo --disable-high-level --without-lzo --without-lz4 --prefix=${CMAKE_INSTALL_PREFIX} --libdir=${CMAKE_INSTALL_PREFIX}/lib --with-xz=${xz_PREFIX}
              COMMAND sed -i "s|XZ_LIBS = -llzma |XZ_LIBS = -Bstatic -llzma  -L${xz_LIBRARY_DIR}/|g" Makefile
    BUILD_COMMAND make -j8
    BUILD_IN_SOURCE ON
    INSTALL_COMMAND true
)

ExternalProject_Get_Property(squashfuse SOURCE_DIR)
mark_as_advanced(SOURCE_DIR)

set(squashfuse_SOURCE_DIR ${SOURCE_DIR})
set(squashfuse_LIBRARY_DIR ${squashfuse_SOURCE_DIR}/.libs)

set(libsquashfuse "${squashfuse_LIBRARY_DIR}/libsquashfuse.a")
set(libfuseprivate "${squashfuse_LIBRARY_DIR}/libfuseprivate.a")
set(squashfuse_LIBRARIES ${libsquashfuse} ${libfuseprivate})
mark_as_advanced(libsquashfuse libfuseprivate)

mark_as_advanced(squashfuse_SOURCE_DIR squashfuse_LIBRARY_DIR squashfuse_LIBRARIES)


ExternalProject_Add(mksquashfs
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/squashfs-tools/squashfs-tools
    CONFIGURE_COMMAND sed -i "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += -DXZ_SUPPORT -I${xz_INCLUDE_DIR}|g" <SOURCE_DIR>/Makefile
              COMMAND sed -i "s|LIBS += -llzma|LIBS += -Bstatic -l${xz_LIBRARY} -L${xz_LIBRARY_DIR}|g" <SOURCE_DIR>/Makefile
    BUILD_COMMAND make XZ_SUPPORT=1 mksquashfs -j8
    BUILD_IN_SOURCE ON
    INSTALL_COMMAND true
)

include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${xz_INCLUDE_DIR})
include_directories(${squashfuse_SOURCE_DIR})

# only have to build custom xz when not using system libxz
if(NOT USE_SYSTEM_XZ)
    add_dependencies(squashfuse xz)
    add_dependencies(mksquashfs xz)
endif()

if(xz_LIBRARY_DIR)
    link_directories(${xz_LIBRARY_DIR})
endif()


######################
# main build targets #
######################

add_custom_command(OUTPUT data.o
    COMMAND bash ${PROJECT_SOURCE_DIR}/build-runtime.sh
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    MAIN_DEPENDENCY runtime.c
)

add_executable(appimagetool
    appimagetool.c
    getsection.c
    binreloc.c
    elf.c
    data.o
)

target_link_libraries(appimagetool
    ${GLIB_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${squashfuse_LIBRARIES}
)

target_link_libraries(appimagetool
    dl
    pthread
    z
    ${xz_LIBRARY}
)

target_compile_definitions(appimagetool
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DVERSION_NUMBER="${GIT_VERSION}"
    PRIVATE -DENABLE_BINRELOC
)

add_dependencies(appimagetool mksquashfs squashfuse)


################################
# unit and functionality tests #
################################

enable_testing()
add_test(test_appimagetool
    bash ${PROJECT_SOURCE_DIR}/test-appimagetool.sh ${PROJECT_BINARY_DIR}/appimagetool
)
