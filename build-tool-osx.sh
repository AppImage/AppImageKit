#!/bin/bash
#
# This script installs the required build-time dependencies
# and builds AppImageTool on OSX
#

small_FLAGS="-Os -ffunction-sections -fdata-sections"
CC="cc -O2 -Wall -Wno-deprecated-declarations -Wno-unused-result"

STRIP="strip"
JOBS=${JOBS:-1}

echo $KEY | md5sum

set -e
set -x

HERE="$(dirname "$(readlink -f "${0}")")"
cd "$HERE"

# Fetch git submodules
git submodule init
git submodule update

# Clean up from previous run
rm -rf build/ || true

# Build lzma always static because the runtime gets distributed with
# the generated .AppImage file.
if [ ! -e "./xz-5.2.3/build/lib/liblzma.a" ] ; then
  wget -c http://tukaani.org/xz/xz-5.2.3.tar.gz
  tar xf xz-5.2.3.tar.gz
  cd xz-5.2.3
  mkdir -p build/lib
  CFLAGS="-Wall $small_FLAGS" ./configure --prefix=$(pwd)/build --libdir=$(pwd)/build/lib --enable-static --disable-shared
  make -j$JOBS && make install
  cd -
fi

# Patch squashfuse_ll to be a library rather than an executable

cd squashfuse
if [ ! -e ./ll.c.orig ]; then
  patch -p1 --backup < ../squashfuse.patch
  patch -p1 --backup < ../squashfuse_dlopen.patch
fi
if [ ! -e ./squashfuse_dlopen.c ]; then
  cp ../squashfuse_dlopen.c .
fi
if [ ! -e ./squashfuse_dlopen.h ]; then
  cp ../squashfuse_dlopen.h .
fi

# Build libsquashfuse_ll library

if [ ! -e ./Makefile ] ; then
  export ACLOCAL_FLAGS="-I /usr/share/aclocal"
  glibtoolize --force
  aclocal
  autoheader
  automake --force-missing --add-missing
  autoreconf -fi || true # Errors out, but the following succeeds then?
  autoconf
  sed -i "" '/PKG_CHECK_MODULES.*/,/,:./d' configure # https://github.com/vasi/squashfuse/issues/12
  CFLAGS="-Wall $small_FLAGS" ./configure --disable-demo --disable-high-level --without-lzo --without-lz4 --with-xz=$(pwd)/../xz-5.2.3/build

  # Patch Makefile to use static lzma
  sed -i "" "s|XZ_LIBS = -llzma  -L$(pwd)/../xz-5.2.3/build/lib|XZ_LIBS = -Bstatic -llzma  -L$(pwd)/../xz-5.2.3/build/lib|g" Makefile
fi

bash --version

make -j$JOBS

cd ..

# Build mksquashfs with -offset option to skip n bytes
# https://github.com/plougher/squashfs-tools/pull/13
cd squashfs-tools
if [ ! -e squashfs-tools/action.c.orig ] ; then
    patch -p1 --backup < ../squashfs_osx.patch
fi
cd squashfs-tools

# Patch squashfuse-tools Makefile to link against static llzma
sed -i "" "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += -DXZ_SUPPORT -I../../xz-5.2.3/build/include|g" Makefile
sed -i "" "s|LIBS += -llzma|LIBS += -Bstatic -llzma  -L../../xz-5.2.3/build/lib|g" Makefile

make -j$JOBS XZ_SUPPORT=1 mksquashfs # LZ4_SUPPORT=1 did not build yet on CentOS 6
$STRIP mksquashfs

cd ../../

pwd

mkdir build
cd build

cp ../squashfs-tools/squashfs-tools/mksquashfs .


# Compile appimagetool but do not link - glib version

$CC -DGIT_COMMIT=\"$(git describe --tags --always --abbrev=7)\" -D_FILE_OFFSET_BITS=64 -I../squashfuse/ \
    $(pkg-config --cflags glib-2.0) -g -Os ../getsection.c  -c ../appimagetool.c

# Now statically link against libsquashfuse - glib version

  # statically link against liblzma
  $CC -o appimagetool appimagetool.o ../elf.c ../getsection.c -DENABLE_BINRELOC ../binreloc.c \
    ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a \
    -L../xz-5.2.3/build/lib \
     -ldl -lpthread \
     $(pkg-config --cflags --libs glib-2.0) -lz  -llzma 


