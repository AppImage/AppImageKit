# find required system dependencies via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(OPENSSL REQUIRED openssl)


set(USE_SYSTEM_XZ OFF CACHE BOOL "Use system xz/liblzma instead of building our own")

if(NOT USE_SYSTEM_XZ)
    message(STATUS "Downloading and building xz")

    ExternalProject_Add(xz
        URL https://tukaani.org/xz/xz-5.2.3.tar.gz
        URL_HASH SHA512=a5eb4f707cf31579d166a6f95dbac45cf7ea181036d1632b4f123a4072f502f8d57cd6e7d0588f0bf831a07b8fc4065d26589a25c399b95ddcf5f73435163da6
        CONFIGURE_COMMAND CFLAGS=-fPIC CPPFLAGS=-fPIC <SOURCE_DIR>/configure --disable-shared --enable-static --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
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
    message(STATUS "Using system xz")

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
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/patch-squashfuse.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/patch-squashfuse.sh
    @ONLY
)

ExternalProject_Add(squashfuse
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
    GIT_REPOSITORY https://github.com/plougher/squashfs-tools/
    GIT_TAG 5be5d61
    UPDATE_COMMAND ""  # make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
    PATCH_COMMAND patch -N -p1 < ${PROJECT_SOURCE_DIR}/src/mksquashfs-mkfs-fixed-timestamp.patch || true
    CONFIGURE_COMMAND sed -i "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += -DXZ_SUPPORT -I${xz_INCLUDE_DIR}|g" <SOURCE_DIR>/squashfs-tools/Makefile
              COMMAND sed -i "s|LIBS += -llzma|LIBS += -Bstatic ${xz_LIBRARIES}|g" <SOURCE_DIR>/squashfs-tools/Makefile
              COMMAND sed -i "s|install: mksquashfs unsquashfs|install: mksquashfs|g" squashfs-tools/Makefile
              COMMAND sed -i "/cp unsquashfs/d" squashfs-tools/Makefile
    BUILD_COMMAND make -C squashfs-tools/ XZ_SUPPORT=1 mksquashfs
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


set(USE_SYSTEM_INOTIFY_TOOLS OFF CACHE BOOL "Use system libinotifytools instead of building our own")

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    message(STATUS "Downloading and building inotify-tools")

    # TODO: build out of source
    ExternalProject_Add(inotify-tools
        URL https://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz
        URL_HASH SHA512=6074d510e89bba5da0d7c4d86f2562c662868666ba0a7ea5d73e53c010a0050dd1fc01959b22cffdb9b8a35bd1b0b43c04d02d6f19927520f05889e8a9297dfb
        PATCH_COMMAND wget -N --content-disposition "https://git.savannah.gnu.org/gitweb/?p=config.git$<SEMICOLON>a=blob_plain$<SEMICOLON>f=config.guess$<SEMICOLON>hb=HEAD"
              COMMAND wget -N --content-disposition "https://git.savannah.gnu.org/gitweb/?p=config.git$<SEMICOLON>a=blob_plain$<SEMICOLON>f=config.sub$<SEMICOLON>hb=HEAD"
        UPDATE_COMMAND ""  # make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --enable-shared --enable-static --enable-doxygen=no --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
        BUILD_COMMAND make
        BUILD_IN_SOURCE ON
        INSTALL_COMMAND make install
    )

    ExternalProject_Get_Property(inotify-tools INSTALL_DIR)
    set(inotify-tools_INSTALL_DIR "${INSTALL_DIR}")
    mark_as_advanced(inotify-tools_INSTALL_DIR)

    set(inotify-tools_INCLUDE_DIR ${inotify-tools_INSTALL_DIR}/include/)
    set(inotify-tools_LIBRARY_DIR ${inotify-tools_INSTALL_DIR}/lib)
    set(inotify-tools_LIBRARIES ${inotify-tools_LIBRARY_DIR}/libinotifytools.a)
    mark_as_advanced(inotify-tools_LIBRARY_DIR)
else()
    message(STATUS "Using system inotify-tools")

    find_package(INotify REQUIRED)

    set(inotify-tools_INCLUDE_DIR ${INOTIFYTOOLS_INCLUDE_DIRS})
    set(inotify-tools_LIBRARIES ${INOTIFYTOOLS_LIBRARY})
endif()

mark_as_advanced(inotify-tools_LIBRARIES inotify-tools_INCLUDE_DIR)


set(USE_SYSTEM_LIBARCHIVE OFF CACHE BOOL "Use system libarchive instead of building our own")

if(NOT USE_SYSTEM_LIBARCHIVE)
    message(STATUS "Downloading and building libarchive")

    ExternalProject_Add(libarchive
        URL https://www.libarchive.org/downloads/libarchive-3.3.1.tar.gz
        URL_HASH SHA512=90702b393b6f0943f42438e277b257af45eee4fa82420431f6a4f5f48bb846f2a72c8ff084dc3ee9c87bdf8b57f4d8dddf7814870fe2604fe86c55d8d744c164
        CONFIGURE_COMMAND CFLAGS=-fPIC CPPFLAGS=-fPIC <SOURCE_DIR>/configure --disable-shared --enable-static --disable-bsdtar --disable-bsdcat --disable-bsdcpio --with-zlib --without-bz2lib --without-iconv --without-lz4 --without-lzma --without-lzo2 --without-nettle --without-openssl --without-xml2 --without-expat --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
        BUILD_COMMAND make
        INSTALL_COMMAND make install
    )

    ExternalProject_Get_Property(libarchive INSTALL_DIR)
    set(libarchive_INSTALL_DIR "${INSTALL_DIR}")
    mark_as_advanced(libarchive_INSTALL_DIR)

    set(libarchive_INCLUDE_DIR ${libarchive_INSTALL_DIR}/include)
    set(libarchive_LIBRARY_DIR ${libarchive_INSTALL_DIR}/lib)
    set(libarchive_LIBRARIES ${libarchive_LIBRARY_DIR}/libarchive.a)
    mark_as_advanced(libarchive_LIBRARY_DIR libarchive_LIBRARIES libarchive_INCLUDE_DIR)
else()
    message(STATUS "Using system libarchive")

    find_package(LibArchive REQUIRED)

    set(libarchive_INCLUDE_DIR ${LibArchive_INCLUDE_DIR})
    set(libarchive_LIBRARIES ${LibArchive_LIBRARY})
endif()


# only have to build custom xz when not using system libxz
if(NOT USE_SYSTEM_XZ)
    add_dependencies(squashfuse xz)
    add_dependencies(mksquashfs xz)
endif()


set(USE_SYSTEM_GTEST OFF CACHE BOOL "Use system GTest instead of downloading and building GTest")

if(NOT USE_SYSTEM_GTEST)
    message(STATUS "Downloading and building GTest")

    ExternalProject_Add(gtest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.8.0
        UPDATE_COMMAND ""  # make sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -G${CMAKE_GENERATOR} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> <SOURCE_DIR>/googletest
    )

    ExternalProject_Get_Property(gtest SOURCE_DIR)
    ExternalProject_Get_Property(gtest INSTALL_DIR)
    set(gtest_SOURCE_DIR "${SOURCE_DIR}")
    set(gtest_INSTALL_DIR "${INSTALL_DIR}")
    mark_as_advanced(gtest_SOURCE_DIR gtest_INSTALL_DIR)

    set(gtest_INCLUDE_DIRS "${gtest_INSTALL_DIR}/include/")
    set(gtest_LIBRARIES_DIR "${gtest_INSTALL_DIR}/lib")
    set(gtest_LIBRARIES "${gtest_LIBRARIES_DIR}/libgtest.a" "${gtest_LIBRARIES_DIR}/libgtest_main.a")
else()
    message(STATUS "Using system GTest")

    find_package(GTest REQUIRED)

    set(gtest_INCLUDE_DIRS "${GTEST_INCLUDE_DIRS}")
    set(gtest_LIBRARIES "${GTEST_LIBRARIES}")
endif()
