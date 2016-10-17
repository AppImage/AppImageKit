#!/bin/bash

#
# This is supposed to be run on Travis CI trusty. Also works for me on Ubuntu 16.04.
# I don't have the time to mess around with autotools and the like.
#

# Clean up from previous run
rm -rf build/ || true

# Patch squashfuse_ll to be a library rather than an executable

cd squashfuse
if [ ! -e ./ll.c.orig ]; then patch -p1 --backup < ../squashfuse.patch ; fi

# Build libsquashfuse_ll library

if [ ! -e ./Makefile ] ; then
  libtoolize --force
  aclocal
  autoheader
  automake --force-missing --add-missing
  autoconf
  ./configure --with-xz=/usr/lib/ --without-lz4 --without-lzo
fi

make

cd ..

mkdir build

# Compile runtime
make -f Makefile.runtime install
make -f Makefile.runtime clean

cd build

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

cc data.o appimagetool.o ../elf.c ../getsection.c -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread  $(pkg-config --libs glib-2.0 liblzma ) -lz -Wl,-Bdynamic -o appimagetool

# Version without glib
# cc -D_FILE_OFFSET_BITS=64 -I ../squashfuse -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os -c ../appimagetoolnoglib.c
# cc data.o appimagetoolnoglib.o -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetoolnoglib

# appimaged, an optional component
cc -std=gnu99 -Wl,-Bdynamic -DVERSION_NUMBER=\"$(git describe --tags --always --abbrev=7)\" ../elf.c ../appimaged.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -I../squashfuse/ -Wl,-Bstatic -linotifytools -Wl,-Bdynamic $(pkg-config --cflags --libs glib-2.0) $(pkg-config --cflags gio-2.0) $(pkg-config --libs gio-2.0) -ldl -lpthread -lz $(pkg-config --libs liblzma )  -o appimaged

cd ..

# Strip and check size and dependencies

rm build/*.o
strip build/* 2>/dev/null
chmod a+x build/*
ldd build/appimagetool
ls -lh build/*
