#!/bin/bash
echo $KEY | md5sum
set -e
set -x

HERE="$(dirname "$(readlink -f "${0}")")"

#
# This script installs the required build-time dependencies
# and builds AppImage
#

HERE="$(dirname "$(readlink -f "${0}")")"

which git 2>&1 >/dev/null || . "$HERE/install-build-deps.sh"

# Fetch git submodules
git submodule init
git submodule update

# Clean up from previous run
rm -rf build/ || true

# Build inotify-tools; the one from CentOS does not have .a
if [ ! -e "./inotify-tools-3.14/libinotifytools/src/.libs/libinotifytools.a" ] ; then
    wget -c http://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz
    tar xf inotify-tools-3.14.tar.gz
    cd inotify-tools-3.14
    mkdir -p build/lib
    ./configure --prefix=`pwd`/build --libdir=`pwd`/build/lib
    make
    make install
    rm inotify-tools-3.14/build/lib/*.so*
    cd -
fi

# Build lzma
if [ ! -e "./xz-5.2.3/build/lib/liblzma.a" ] ; then
  wget -c http://tukaani.org/xz/xz-5.2.3.tar.gz
  tar xf xz-5.2.3.tar.gz
  cd xz-5.2.3
  mkdir -p build/lib
  ./configure --prefix=`pwd`/build --libdir=`pwd`/build/lib --enable-static
  make && make install
  rm xz-5.2.3/build/lib/*.so*
  cd -
fi

# Build openssl
if [ ! -e "./openssl-1.1.0c/build/lib/libssl.a" ] ; then
  wget -c https://www.openssl.org/source/openssl-1.1.0c.tar.gz
  tar xf openssl-1.1.0c.tar.gz
  cd openssl-1.1.0c
  mkdir -p build/lib
  ./config --prefix=`pwd`/build
  make && make install
  rm openssl-1.1.0c/build/lib/*.so*
  cd -
fi

# Patch squashfuse_ll to be a library rather than an executable

cd squashfuse
if [ ! -e ./ll.c.orig ]; then patch -p1 --backup < ../squashfuse.patch ; fi

# Build libsquashfuse_ll library

if [ ! -e ./Makefile ] ; then
  export ACLOCAL_FLAGS="-I /usr/share/aclocal"
  libtoolize --force
  aclocal
  autoheader
  automake --force-missing --add-missing
  autoreconf -fi || true # Errors out, but the following succeeds then?
  autoconf
  sed -i '/PKG_CHECK_MODULES.*/,/,:./d' configure # https://github.com/vasi/squashfuse/issues/12
  ./configure --disable-demo --disable-high-level --without-lzo --without-lz4 --with-xz=`pwd`/../xz-5.2.3/build
fi

# Patch make file to use static lzma
sed -i "s|XZ_LIBS = -llzma  -L$(pwd)/../xz-5.2.3/build/lib|XZ_LIBS = -Bstatic -llzma  -L$(pwd)/../xz-5.2.3/build/lib|g" Makefile

bash --version

make

cd ..

# Build mksquashfs with -offset option to skip n bytes
# https://github.com/plougher/squashfs-tools/pull/13
cd squashfs-tools/squashfs-tools

# Patch squashfuse-tools Makefile to link against static llzma
sed -i "s|CFLAGS += -DXZ_SUPPORT|CFLAGS += -DXZ_SUPPORT -I../../xz-5.2.3/build/include|g" Makefile
sed -i "s|LIBS += -llzma|LIBS += -Bstatic -llzma  -L../../xz-5.2.3/build/lib|g" Makefile

make XZ_SUPPORT=1 mksquashfs # LZ4_SUPPORT=1 did not build yet on CentOS 6
strip mksquashfs

cd ../../

pwd

mkdir build
cd build

cp ../squashfs-tools/squashfs-tools/mksquashfs .

# Compile runtime but do not link

cc -DVERSION_NUMBER=\"$(git describe --tags --always --abbrev=7)\" -I../squashfuse/ -D_FILE_OFFSET_BITS=64 -g -Os -c ../runtime.c

# Prepare 1024 bytes of space for updateinformation
printf '\0%.0s' {0..1023} > 1024_blank_bytes

objcopy --add-section .upd_info=1024_blank_bytes \
          --set-section-flags .upd_info=noload,readonly runtime.o runtime2.o

objcopy --add-section .sha256_sig=1024_blank_bytes \
          --set-section-flags .sha256_sig=noload,readonly runtime2.o runtime3.o

# Now statically link against libsquashfuse_ll, libsquashfuse and liblzma
# and embed .upd_info and .sha256_sig sections
cc ../elf.c ../notify.c ../getsection.c runtime3.o ../squashfuse/.libs/libsquashfuse_ll.a ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -L../xz-5.2.3/build/lib -Wl,-Bdynamic -lfuse -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -ldl -o runtime
strip runtime

# Test if we can read it back
readelf -x .upd_info runtime # hexdump
readelf -p .upd_info runtime || true # string

# The raw updateinformation data can be read out manually like this:
HEXOFFSET=$(objdump -h runtime | grep .upd_info | awk '{print $6}')
HEXLENGTH=$(objdump -h runtime | grep .upd_info | awk '{print $3}')
dd bs=1 if=runtime skip=$(($(echo 0x$HEXOFFSET)+0)) count=$(($(echo 0x$HEXLENGTH)+0)) | xxd

# Insert AppImage magic bytes

printf '\x41\x49\x02' | dd of=runtime bs=1 seek=8 count=3 conv=notrunc

# Convert runtime into a data object that can be embedded into appimagetool

ld -r -b binary -o data.o runtime

# Compile and link digest tool

cc -o digest ../getsection.c ../digest.c -I../openssl-1.1.0c/build/include -L../openssl-1.1.0c/build/lib -Wl,-Bstatic -lssl -lcrypto -Wl,-Bdynamic -lz -ldl
strip digest

# Compile and link validate tool

cc -o validate ../getsection.c ../validate.c -I../openssl-1.1.0c/build/include -L../openssl-1.1.0c/build/lib -Wl,-Bstatic -lssl -lcrypto -Wl,-Bdynamic -lglib-2.0 $(pkg-config --cflags glib-2.0) -lz -ldl
strip validate

# Test if we can read it back
readelf -x .upd_info runtime # hexdump
readelf -p .upd_info runtime || true # string

# The raw updateinformation data can be read out manually like this:
HEXOFFSET=$(objdump -h runtime | grep .upd_info | awk '{print $6}')
HEXLENGTH=$(objdump -h runtime | grep .upd_info | awk '{print $3}')
dd bs=1 if=runtime skip=$(($(echo 0x$HEXOFFSET)+0)) count=$(($(echo 0x$HEXLENGTH)+0)) | xxd

# Convert runtime into a data object that can be embedded into appimagetool

ld -r -b binary -o data.o runtime

# Compile appimagetool but do not link - glib version

cc -DVERSION_NUMBER=\"$(git describe --tags --always --abbrev=7)\" -D_FILE_OFFSET_BITS=64 -I../squashfuse/ $(pkg-config --cflags glib-2.0) -g -Os ../getsection.c  -c ../appimagetool.c

# Now statically link against libsquashfuse and liblzma - glib version

cc data.o appimagetool.o ../elf.c ../getsection.c -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -L../xz-5.2.3/build/lib -Wl,-Bdynamic -lfuse -lpthread -lglib-2.0 $(pkg-config --cflags glib-2.0) -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetool # liblz4

# Version without glib
# cc -D_FILE_OFFSET_BITS=64 -I ../squashfuse -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os -c ../appimagetoolnoglib.c
# cc data.o appimagetoolnoglib.o -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetoolnoglib

# AppRun
cc ../AppRun.c -o AppRun

# check for libarchive name
have_libarchive3=0
archive_n=
if printf "#include <archive3.h>\nint main(){return 0;}" | cc -w -O0 -xc - -Wl,--no-as-needed -larchive3 2>/dev/null ; then
  have_libarchive3=1
  archive_n=3
fi
rm -f a.out

# appimaged, an optional component
cc -std=gnu99 -D_FILE_OFFSET_BITS=64 -DHAVE_LIBARCHIVE3=$have_libarchive3 -DVERSION_NUMBER=\"$(git describe --tags --always --abbrev=7)\" ../getsection.c ../notify.c -Wl,-Bdynamic ../elf.c ../appimaged.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -I../squashfuse/ -L../xz-5.2.3/build/lib -I../inotify-tools-3.14/build/include -L../inotify-tools-3.14/build/lib -Wl,-Bstatic -linotifytools -Wl,-Bdynamic -larchive${archive_n} $(pkg-config --cflags --libs glib-2.0) $(pkg-config --cflags gio-2.0) $(pkg-config --libs gio-2.0) $(pkg-config --libs --cflags cairo) -ldl -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimaged # liblz4

cd ..

# Strip and check size and dependencies

rm build/*.o build/1024_blank_bytes
strip build/* 2>/dev/null
chmod a+x build/*
ls -lh build/*
for FILE in $(ls build/*) ; do
  echo "$FILE"
  ldd "$FILE" || true
done

bash -ex "$HERE/build-appdirs.sh"

ls -lh

mkdir -p out
cp -r build/* ./*.AppDir out/
