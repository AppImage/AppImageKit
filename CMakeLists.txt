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

# C and C++ versions
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


##########################
# configure dependencies #
##########################

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


# find required system dependencies via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(OPENSSL REQUIRED openssl)

set(USE_SYSTEM_XZ OFF CACHE BOOL "Use system xz (well, liblzma) instead of bundled one")

if(NOT USE_SYSTEM_XZ)
    message(STATUS "Using bundled libxz")

    ExternalProject_Add(xz
        URL https://tukaani.org/xz/xz-5.2.3.tar.gz
        URL_HASH SHA512=a5eb4f707cf31579d166a6f95dbac45cf7ea181036d1632b4f123a4072f502f8d57cd6e7d0588f0bf831a07b8fc4065d26589a25c399b95ddcf5f73435163da6
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --enable-static --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
        BUILD_COMMAND make
        INSTALL_COMMAND make install
    )

    ExternalProject_Get_Property(xz SOURCE_DIR)
    ExternalProject_Get_Property(xz INSTALL_DIR)
    set(xz_SOURCE_DIR "${SOURCE_DIR}")
    set(xz_INSTALL_DIR "${INSTALL_DIR}")
    mark_as_advanced(xz_SOURCE_DIR xz_INSTALL_DIR)

    set(xz_INCLUDE_DIR "${xz_SOURCE_DIR}/src/liblzma/api/")
    set(xz_LIBRARIES_DIR "${xz_INSTALL_DIR}/lib")
    set(xz_LIBRARIES "${xz_LIBRARIES_DIR}/liblzma.a")

    set(xz_PREFIX "${xz_INSTALL_DIR}")
else()
    message(STATUS "Using system libxz")

    find_package(LibLZMA)
    if(NOT LIBLZMA_FOUND)
        message(FATAL_ERROR "liblzma could not be found on the system. You will have to either install it, or use the bundled xz.")
    endif()

    set(xz_INCLUDE_DIR "${LIBLZMA_INCLUDE_DIRS}")
    set(xz_LIBRARIES "${LIBLZMA_LIBRARIES}")
    set(xz_PREFIX "/usr/lib")
endif()

mark_as_advanced(xz_INCLUDE_DIR xz_LIBRARIES_DIR xz_LIBRARIES xz_PREFIX)

# as distros don't provide suitable squashfuse and squashfs-tools, those dependencies are bundled in, can, and should
# be used from this repository
# TODO: implement out-of-source builds for squashfuse, as for the other dependencies
ExternalProject_Add(squashfuse
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/squashfuse
    PATCH_COMMAND bash -xe ${PROJECT_SOURCE_DIR}/patch.sh
    CONFIGURE_COMMAND libtoolize --force
              COMMAND env ACLOCAL_FLAGS="-I /usr/share/aclocal" aclocal
              COMMAND autoheader
              COMMAND automake --force-missing --add-missing
              COMMAND autoreconf -fi || true
              COMMAND sed -i "/PKG_CHECK_MODULES.*/,/,:./d" configure  # https://github.com/vasi/squashfuse/issues/12
              COMMAND ./configure --disable-demo --disable-high-level --without-lzo --without-lz4 --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib --with-xz=${xz_PREFIX}
              COMMAND sed -i "s|XZ_LIBS = -llzma |XZ_LIBS = -Bstatic ${xz_LIBRARIES}/|g" Makefile
    BUILD_COMMAND make
    BUILD_IN_SOURCE ON
    INSTALL_COMMAND make install
)

ExternalProject_Get_Property(squashfuse SOURCE_DIR)
ExternalProject_Get_Property(squashfuse INSTALL_DIR)
set(squashfuse_SOURCE_DIR "${SOURCE_DIR}")
set(squashfuse_INSTALL_DIR "${INSTALL_DIR}")
mark_as_advanced(squashfuse_SOURCE_DIR squashfuse_INSTALL_DIR)

set(squashfuse_LIBRARY_DIR ${squashfuse_SOURCE_DIR}/.libs)
set(squashfuse_INCLUDE_DIR ${squashfuse_SOURCE_DIR})

set(libsquashfuse ${squashfuse_LIBRARY_DIR}/libsquashfuse.a)
set(libsquashfuse_ll ${squashfuse_LIBRARY_DIR}/libsquashfuse_ll.a)
set(libfuseprivate ${squashfuse_LIBRARY_DIR}/libfuseprivate.a)
set(squashfuse_LIBRARIES ${libsquashfuse} ${libsquashfuse_ll} ${libfuseprivate})
mark_as_advanced(libsquashfuse libfuseprivate)

mark_as_advanced(squashfuse_LIBRARY_DIR squashfuse_LIBRARIES squashfuse_INCLUDE_DIR)


# TODO: allow using system wide mksquashfs
ExternalProject_Add(mksquashfs
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/squashfs-tools/squashfs-tools
    CONFIGURE_COMMAND sed -i "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += -DXZ_SUPPORT -I${xz_INCLUDE_DIR}|g" <SOURCE_DIR>/Makefile
              COMMAND sed -i "s|LIBS += -llzma|LIBS += -Bstatic ${xz_LIBRARIES}|g" <SOURCE_DIR>/Makefile
              COMMAND sed -i "s|install: mksquashfs unsquashfs|install: mksquashfs|g" Makefile
              COMMAND sed -i "/cp unsquashfs/d" Makefile
    BUILD_COMMAND make XZ_SUPPORT=1 mksquashfs
    # make install unfortunately expects unsquashfs to be built as well, hence can't install the binary
    # therefore using built file in SOURCE_DIR
    # TODO: implement building out of source
    BUILD_IN_SOURCE ON
    INSTALL_COMMAND make install INSTALL_DIR=<INSTALL_DIR>
)

ExternalProject_Get_Property(mksquashfs SOURCE_DIR)
ExternalProject_Get_Property(mksquashfs INSTALL_DIR)
set(mksquashfs_SOURCE_DIR "${SOURCE_DIR}")
set(mksquashfs_INSTALL_DIR "${INSTALL_DIR}")
mark_as_advanced(mksquashfs_SOURCE_DIR mksquashfs_INSTALL_DIR)

# for later use when packaging as an AppImage
set(mksquashfs_BINARY "${mksquashfs_INSTALL_DIR}/mksquashfs")
mark_as_advanced(mksquashfs_BINARY)


# TODO: allow using system wide inotify-tools, analog to libxz
# TODO: build out of source
ExternalProject_Add(inotify-tools
    URL https://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz
    URL_HASH SHA512=6074d510e89bba5da0d7c4d86f2562c662868666ba0a7ea5d73e53c010a0050dd1fc01959b22cffdb9b8a35bd1b0b43c04d02d6f19927520f05889e8a9297dfb
    PATCH_COMMAND wget -N "https://git.savannah.gnu.org/gitweb/?p=config.git$<SEMICOLON>a=blob_plain$<SEMICOLON>f=config.guess$<SEMICOLON>hb=HEAD" -O config.guess
          COMMAND wget -N "https://git.savannah.gnu.org/gitweb/?p=config.git$<SEMICOLON>a=blob_plain$<SEMICOLON>f=config.sub$<SEMICOLON>hb=HEAD" -O config.sub
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --enable-shared --enable-static --enable-doxygen=no --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
    BUILD_COMMAND make
    BUILD_IN_SOURCE ON
    INSTALL_COMMAND make install
)

ExternalProject_Get_Property(inotify-tools SOURCE_DIR)
ExternalProject_Get_Property(inotify-tools INSTALL_DIR)
set(inotify-tools_SOURCE_DIR "${SOURCE_DIR}")
set(inotify-tools_INSTALL_DIR "${INSTALL_DIR}")
mark_as_advanced(inotify-tools_SOURCE_DIR inotify-tools_INSTALL_DIR)

set(inotify-tools_INCLUDE_DIR ${inotify-tools_SOURCE_DIR}/libinotifytools/src/)
set(inotify-tools_LIBRARY_DIR ${inotify-tools_INSTALL_DIR}/lib)
set(inotify-tools_LIBRARIES ${inotify-tools_LIBRARY_DIR}/libinotifytools.a)
mark_as_advanced(inotify-tools_LIBRARY_DIR inotify-tools_LIBRARIES inotify-tools_INCLUDE_DIR)


# TODO: allow using system wide libarchive, analog to libxz
ExternalProject_Add(libarchive
    URL https://www.libarchive.org/downloads/libarchive-3.3.1.tar.gz
    URL_HASH SHA512=90702b393b6f0943f42438e277b257af45eee4fa82420431f6a4f5f48bb846f2a72c8ff084dc3ee9c87bdf8b57f4d8dddf7814870fe2604fe86c55d8d744c164
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --disable-shared --enable-static --disable-bsdtar --disable-bsdcat --disable-bsdcpio --with-zlib --without-bz2lib --without-iconv --without-lz4 --without-lzma --without-lzo2 --without-nettle --without-openssl --without-xml2 --without-expat --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Get_Property(libarchive SOURCE_DIR)
ExternalProject_Get_Property(libarchive INSTALL_DIR)
set(libarchive_SOURCE_DIR "${SOURCE_DIR}")
set(libarchive_INSTALL_DIR "${INSTALL_DIR}")
mark_as_advanced(libarchive_SOURCE_DIR inotify-tools_INSTALL_DIR)

set(libarchive_INCLUDE_DIR ${libarchive_SOURCE_DIR}/libarchive)
set(libarchive_LIBRARY_DIR ${libarchive_INSTALL_DIR}/lib)
set(libarchive_LIBRARIES ${libarchive_LIBRARY_DIR}/libarchive.a)
mark_as_advanced(libarchive_LIBRARY_DIR libarchive_LIBRARIES libarchive_INCLUDE_DIR)


# include directories globally
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${CAIRO_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${xz_INCLUDE_DIR})
include_directories(${inotify-tools_INCLUDE_DIR})
include_directories(${squashfuse_INCLUDE_DIR})
include_directories(${libarchive_INCLUDE_DIR})


# only have to build custom xz when not using system libxz
if(NOT USE_SYSTEM_XZ)
    add_dependencies(squashfuse xz)
    add_dependencies(mksquashfs xz)
endif()


######################
# main build targets #
######################

# set up build script
configure_file(
    ${PROJECT_SOURCE_DIR}/build-runtime.sh.in
    ${PROJECT_BINARY_DIR}/build-runtime.sh
    @ONLY
)

add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/runtime
    COMMAND bash ${PROJECT_BINARY_DIR}/build-runtime.sh
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/runtime.c
    DEPENDS squashfuse xz
)

add_custom_command(OUTPUT data.o
    COMMAND ld -r -b binary -o data.o runtime
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS ${PROJECT_BINARY_DIR}/runtime
)

add_custom_target(runtime DEPENDS ${PROJECT_BINARY_DIR}/runtime)


add_executable(appimagetool
    appimagetool.c
    getsection.c
    binreloc.c
    elf.c
    data.o
)

target_link_libraries(appimagetool
    ${GLIB_LIBRARIES}
    ${squashfuse_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

target_link_libraries(appimagetool
    dl
    pthread
    ${ZLIB_LIBRARIES}
    ${xz_LIBRARIES}
)

target_compile_definitions(appimagetool
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DVERSION_NUMBER="${GIT_VERSION}"
    PRIVATE -DENABLE_BINRELOC
)

add_dependencies(appimagetool mksquashfs squashfuse)


add_executable(appimaged appimaged.c notify.c elf.c getsection.c)

target_link_libraries(appimaged
    pthread
    dl
    ${CAIRO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${inotify-tools_LIBRARIES}
    ${squashfuse_LIBRARIES}
    ${xz_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${libarchive_LIBRARIES}
)

target_compile_definitions(appimaged
    PRIVATE -DVERSION_NUMBER="${GIT_VERSION}"
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DHAVE_LIBARCHIVE3=0
)

add_dependencies(appimaged squashfuse inotify-tools libarchive)


add_executable(AppRun AppRun.c)


add_executable(validate validate.c getsection.c)

target_link_libraries(validate
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)


add_executable(digest digest.c getsection.c)

target_link_libraries(digest
    ${OPENSSL_LIBRARIES}
)


# install binaries
install(
    PROGRAMS ${mksquashfs_INSTALL_DIR}/mksquashfs ${PROJECT_BINARY_DIR}/runtime
    DESTINATION bin
)
install(
    TARGETS AppRun appimaged appimagetool digest validate
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    INCLUDES DESTINATION include
)


################################
# unit and functionality tests #
################################

enable_testing()
add_test(test_appimagetool
    bash ${PROJECT_SOURCE_DIR}/test-appimagetool.sh ${PROJECT_BINARY_DIR}/appimagetool
)
