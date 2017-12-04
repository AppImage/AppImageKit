cmake_minimum_required(VERSION 3.2)

include(ExternalProject)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LZMA REQUIRED liblzma)
pkg_check_modules(ZLIB REQUIRED zlib)

execute_process(
    COMMAND git describe --tags --always --abbrev=7
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

ExternalProject_Add(libxz
    URL http://tukaani.org/xz/xz-5.2.3.tar.gz
    URL_MD5 ef68674fb47a8b8e741b34e429d86e9d
    CONFIGURE_COMMAND ./configure --enable-static --prefix=${CMAKE_INSTALL_PREFIX} --libdir=${CMAKE_INSTALL_PREFIX}/lib
    BUILD_COMMAND make -j8
    BUILD_IN_SOURCE 1
#    INSTALL_COMMAND make install
    INSTALL_COMMAND true
)
ExternalProject_Get_Property(libxz SOURCE_DIR)
set(xz_SOURCE_DIR ${SOURCE_DIR})

add_custom_target(patch_squashfuse ALL
    COMMAND bash patch.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

ExternalProject_Add(squashfuse
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/squashfuse
    CONFIGURE_COMMAND libtoolize --force
              COMMAND env ACLOCAL_FLAGS="-I /usr/share/aclocal" aclocal
              COMMAND autoheader
              COMMAND automake --force-missing --add-missing
              COMMAND autoreconf -fi || true
              COMMAND sed -i "/PKG_CHECK_MODULES.*/,/,:./d" configure  # https://github.com/vasi/squashfuse/issues/12
              COMMAND ./configure --disable-demo --disable-high-level --without-lzo --without-lz4 --prefix=${CMAKE_INSTALL_PREFIX} --libdir=${CMAKE_INSTALL_PREFIX}/lib --with-xz=${SOURCE_DIR}/build
              COMMAND sed -i "s|XZ_LIBS = -llzma  -L${xz_SOURCE_DIR}/src/liblzma/.libs/|XZ_LIBS = -Bstatic -llzma  -L${xz_SOURCE_DIR}/src/liblzma/.libs/|g" Makefile
    BUILD_COMMAND make -j8
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND true
)

ExternalProject_Add(mksquashfs
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/squashfs-tools/squashfs-tools
    CONFIGURE_COMMAND true
    CONFIGURE_COMMAND sed -i "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += -DXZ_SUPPORT -I${xz_SOURCE_DIR}/build/include|g" <SOURCE_DIR>/Makefile
              COMMAND sed -i "s|LIBS += -llzma|LIBS += -Bstatic -llzma -L${xz_SOURCE_DIR}/build/lib|g" <SOURCE_DIR>/Makefile
    BUILD_COMMAND make XZ_SUPPORT=1 mksquashfs -j8
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND true
)

include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${xz_SOURCE_DIR}/)
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(squashfuse)

add_custom_command(OUTPUT data.o
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/build-runtime.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    MAIN_DEPENDENCY runtime.c
)

add_executable(appimagetool
    appimagetool.c
    getsection.c
    binreloc.c
    elf.c
    data.o
)

add_dependencies(squashfuse patch_squashfuse)
add_dependencies(squashfuse libxz)
add_dependencies(appimagetool mksquashfs squashfuse)

link_directories(${xz_SOURCE_DIR}/src/liblzma/.libs/)

find_library(LIBSQUASHFUSE libsquashfuse.a PATHS squashfuse/.libs)
find_library(LIBFUSEPRIVATE libfuseprivate.a PATHS squashfuse/.libs)
find_library(LIBLZMA libzma.a ${xz_SOURCE_DIR}/src/liblzma/.libs/)


#add_library(data STATIC ${CMAKE_CURRENT_BINARY_DIR}/data.o)
#set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/data.o
#    PROPERTIES EXTERNAL_OBJECT true GENERATED true
#)

target_link_libraries(appimagetool
    ${GLIB_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LIBFUSEPRIVATE}
    ${LIBSQUASHFUSE}
)
target_link_libraries(appimagetool dl pthread lzma z)

target_compile_definitions(appimagetool
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DVERSION_NUMBER="${GIT_VERSION}"
    PRIVATE -DENABLE_BINRELOC
)

enable_testing()
add_test(test_appimagetool
    bash ${CMAKE_CURRENT_SOURCE_DIR}/test-appimagetool.sh ${CMAKE_CURRENT_BINARY_DIR}/appimagetool
)
