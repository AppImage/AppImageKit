#!/bin/bash

# Clean up from previous run
rm -rf build/ || true

# Install build dependencies

sudo apt-get -y install git autoconf libtool make gcc libtool libfuse-dev liblzma-dev
# libtool-bin might be required in newer distributions but is not available in precise

# Patch squashfuse_ll to be a library rather than an executable

cd squashfuse
patch -p1 < ../squashfuse-patches/ll.c.patch
patch -p1 < ../squashfuse-patches/Makefile.am.patch

# Build libsquashfuse_ll library

libtoolize --force
aclocal
autoheader
automake --force-missing --add-missing
autoconf

./configure --with-xz=/usr/lib/

sed -i -e 's|-O2|-Os|g' Makefile # Optimize for size

make

cd ..

mkdir build
cd build

# Compile runtime but do not link

cc -D_FILE_OFFSET_BITS=64 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os -c ../runtime.c

# Now statically link against libsquashfuse_ll, libsquashfuse and liblzma

cc runtime.o ../squashfuse/.libs/libsquashfuse_ll.a ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lglib-2.0 -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o runtime

# Insert AppImage magic bytes

printf '\x41\x49\x02' | dd of=runtime bs=1 seek=8 count=3 conv=notrunc

# Compile appimagetool but do not link

cc -D_FILE_OFFSET_BITS=64 -I ../squashfuse -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os -c ../appimagetool.c

# Now statically link against libsquashfuse and liblzma

cc appimagetool.o ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lglib-2.0 -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetool

cd ..

# Reset squashfuse to its original state

cd squashfuse
git reset --hard
cd -

# Strip runtime and check its size and dependencies

rm build/*.o
strip build/*
ldd build/appimagetool
ldd build/runtime
ls -l build/*
