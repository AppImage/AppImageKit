#!/bin/bash

set -e
set -x

#
# This script installs the required build-time dependencies
# and builds AppImage
#

HERE="$(dirname "$(readlink -f "${0}")")"

if [ -e /usr/bin/apt-get ] ; then
  apt-get update
  bash -ex ./install-build-deps.sh
fi

if [ -e /usr/bin/yum ] ; then
  # Install and enable EPEL and Devtoolset-4 by Software Collections
  # https://www.softwarecollections.org/en/scls/rhscl/devtoolset-4/
  yum -y install centos-release-scl-rh epel-release
  yum -y install devtoolset-4-gcc.x86_64
  # Install and enable Autotools by Pavel Raiskup
  # https://www.softwarecollections.org/en/scls/praiskup/autotools/
  rpm -ivh https://www.softwarecollections.org/en/scls/praiskup/autotools/epel-6-x86_64/download/praiskup-autotools-epel-6-x86_64.noarch.rpm
  yum -y install autotools-latest # 19 MB

  yum -y install epel-release
  yum -y install git wget make binutils fuse glibc-devel glib2-devel fuse-devel zlib-devel patch openssl-devel vim-common # inotify-tools-devel lz4-devel
  . /opt/rh/devtoolset-4/enable
  . /opt/rh/autotools-latest/enable

  # Unlike Ubuntu, CentOS does not provide .a, so we need to build it
  wget http://tukaani.org/xz/xz-5.2.2.tar.gz
  tar xzfv xz-5.2.2.tar.gz 
  cd xz-5.2.2
  ./configure --enable-static && make && make install
  rm /usr/local/lib/liblzma.so* /usr/*/liblzma.so || true # Don't want the dynamic one
  cd -
fi

# Install dependencies for Arch Linux
if [ -e /usr/bin/pacman ] ; then
  echo "Please submit a pull request if you would like to see Arch Linux support."
  exit 1
fi

# Fetch git submodules
git submodule init
git submodule update

# Clean up from previous run
rm -rf build/ || true

# Patch squashfuse_ll to be a library rather than an executable

cd squashfuse
if [ ! -e ./ll.c.orig ]; then patch -p1 --backup < ../squashfuse.patch ; fi

# Build inotify-tools; the one from does not have .a
wget -c http://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz
tar xf inotify-tools-3.14.tar.gz
cd inotify-tools-3.14
./configure --prefix=/usr && make && su -c 'make install'
cd -
rm /usr/*/libinotifytools.so* /usr/local/lib/libinotifytools.so* || true # Don't want the dynamic one

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
  ./configure --disable-demo --disable-high-level --without-lzo --with-xz=/usr/lib/
fi

bash --version

make

cd ..

mkdir build
cd build

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
cc ../elf.c ../notify.c ../getsection.c runtime3.o ../squashfuse/.libs/libsquashfuse_ll.a ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -ldl -o runtime
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

cc -o digest ../getsection.c ../digest.c -lssl -lcrypto
# cc -o digest -Wl,-Bdynamic ../digest.c -Wl,-Bstatic -static  -lcrypto -Wl,-Bdynamic -ldl # 1.4 MB
strip digest

# Compile and link validate tool

cc -o validate ../getsection.c ../validate.c -lssl -lcrypto -lglib-2.0 $(pkg-config --cflags glib-2.0)
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

cc -DVERSION_NUMBER=\"$(git describe --tags --always --abbrev=7)\" -D_FILE_OFFSET_BITS=64 -I../squashfuse/ $(pkg-config --cflags glib-2.0) -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os ../getsection.c  -c ../appimagetool.c

# Now statically link against libsquashfuse and liblzma - glib version

cc data.o appimagetool.o ../elf.c ../getsection.c -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lglib-2.0 $(pkg-config --cflags glib-2.0) -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetool # liblz4

# Version without glib
# cc -D_FILE_OFFSET_BITS=64 -I ../squashfuse -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os -c ../appimagetoolnoglib.c
# cc data.o appimagetoolnoglib.o -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetoolnoglib

# appimaged, an optional component
cc -std=gnu99 ../getsection.c -Wl,-Bdynamic -DVERSION_NUMBER=\"$(git describe --tags --always --abbrev=7)\" ../elf.c ../appimaged.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -I../squashfuse/ -Wl,-Bstatic -linotifytools -Wl,-Bdynamic $(pkg-config --cflags --libs glib-2.0) $(pkg-config --cflags gio-2.0) $(pkg-config --libs gio-2.0) -ldl -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimaged # liblz4

cd ..

# Strip and check size and dependencies

rm build/*.o build/1024_blank_bytes
strip build/* 2>/dev/null
chmod a+x build/*
ls -lh build/*
for FILE in $(ls build/*) ; do
  echo "build/$FILE"
  ldd "build/$FILE" || true
done

bash -ex build-appimage.sh
curl --upload-file ./*.AppImage https://transfer.sh/appimagetool

mkdir -p /out/
cp build/* ./*.AppImage /out/
