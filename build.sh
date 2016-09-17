#!/bin/bash

# Clean up from previous run
rm -rf build/ || true

# Install build dependencies

sudo apt-get -y install git autoconf libtool make gcc libtool libfuse-dev liblzma-dev libglib2.0-dev
# libtool-bin might be required in newer distributions but is not available in precise

# Patch squashfuse_ll to be a library rather than an executable

cd squashfuse
patch -p1 < ../squashfuse.patch

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

cc -D_FILE_OFFSET_BITS=64 -g -Os -c ../runtime.c

# Prepare 1024 bytes of space for updateinformation
# and create a section .updateinformation to be embedded into the ELF
#
# Store updateinformation in PT_NOTE section since if we would store it
# in an own section, then strip would strip the information about it away
# http://www.netbsd.org/docs/kernel/elf-notes.html # How to generate
# https://stackoverflow.com/questions/17637745/can-a-program-read-its-own-elf-section # How to read

# Can be read with
# readelf -p updateinformation runtime

# Let's use assembler to create a ELF PT_NOTE section that contains 1024 # characters as padding
cat > updateinformation.S <<\EOF
        .section ".note.upd-info", "a"
        .p2align 2
        .long   2f-1f # name size (not including padding), gets calculated automatically
        .long   4f-3f # value size (not including padding), gets calculated automatically
        .long   0 # type (readelf -a reports this as "Description"; 0 = "Unknown")
1:      .asciz "updateinformation" # name (readelf -a reports this as "Owner")
2:      .p2align 2
3:      .asciz ">\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0<" # value, 1024 characters, generated in bash using printf '\\0%.0s' {0..1023}
4:      .p2align 2

EOF

gcc -c updateinformation.S

# Now statically link against libsquashfuse_ll, libsquashfuse and liblzma
# and embed updateinformation section

cc updateinformation.o runtime.o ../squashfuse/.libs/libsquashfuse_ll.a ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o runtime
strip runtime

# Test if we can read it back

readelf -p .note.upd-info runtime

readelf -n runtime
# Displaying notes found at file offset 0x00000274 with length 0x00000424:
# Owner                 Data size	Description
# updateinformation     0x00000401	Unknown note type: (0x00000000)

# The raw updateinformation data can be read out manually like this:
HEXOFFSET=$(objdump -h runtime | grep .note.upd-info | awk '{print $6}')
dd bs=1 if=runtime skip=$(($(echo 0x$HEXOFFSET)+32)) count=1024 | xxd

# Insert AppImage magic bytes

printf '\x41\x49\x02' | dd of=runtime bs=1 seek=8 count=3 conv=notrunc

# Convert runtime into a data object that can be embedded into appimagetool

ld -r -b binary -o data.o runtime

# Compile appimagetool but do not link

cc -D_FILE_OFFSET_BITS=64 -I ../squashfuse -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os -c ../appimagetoolnoglib.c

# Now statically link against libsquashfuse and liblzma

cc data.o appimagetoolnoglib.o -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetoolnoglib

# Compile appimagetool but do not link - glib version

cc -D_FILE_OFFSET_BITS=64 -I ../squashfuse $(pkg-config --cflags glib-2.0) -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -g -Os -c ../appimagetool.c

# Now statically link against libsquashfuse and liblzma - glib version

cc data.o appimagetool.o -DENABLE_BINRELOC ../binreloc.c ../squashfuse/.libs/libsquashfuse.a ../squashfuse/.libs/libfuseprivate.a -Wl,-Bdynamic -lfuse -lpthread  $(pkg-config --libs glib-2.0) -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -o appimagetool

cd ..

# Reset squashfuse to its original state

cd squashfuse
git reset --hard
cd -

# Strip and check size and dependencies

strip build/appimage*
ldd build/appimagetool
ls -l build/*
