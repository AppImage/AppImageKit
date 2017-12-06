#! /bin/bash

if which clang >/dev/null; then
    CC=clang
else
    CC=gcc
fi

set -x

build_dir=$(mktemp -d)
orig_cwd=$(pwd)
repo_root=$(dirname $(readlink -f $0))

cd $repo_root
git_version=$(git describe --tags --always --abbrev=7)

cleanup() {
    [ -d $build_dir ] && rm -r "$build_dir"
}

trap cleanup EXIT

cd "$build_dir"

cp "$repo_root"/squashfs-tools/squashfs-tools/mksquashfs .

# Compile runtime but do not link

$CC -DVERSION_NUMBER=\"$git_version\" -I"$repo_root"/squashfuse/ -D_FILE_OFFSET_BITS=64 -g -Os -c "$repo_root"/runtime.c

# Prepare 1024 bytes of space for updateinformation
printf '\0%.0s' {0..1023} > 1024_blank_bytes

objcopy --add-section .upd_info=1024_blank_bytes \
        --set-section-flags .upd_info=noload,readonly runtime.o runtime2.o

objcopy --add-section .sha256_sig=1024_blank_bytes \
        --set-section-flags .sha256_sig=noload,readonly runtime2.o runtime3.o

# Now statically link against libsquashfuse_ll, libsquashfuse and liblzma
# and embed .upd_info and .sha256_sig sections
$CC -o runtime "$repo_root"/elf.c "$repo_root"/notify.c "$repo_root"/getsection.c runtime3.o \
    "$repo_root"/squashfuse/.libs/libsquashfuse_ll.a "$repo_root"/squashfuse/.libs/libsquashfuse.a \
    "$repo_root"/squashfuse/.libs/libfuseprivate.a -L"$repo_root"/xz-5.2.3/build/lib \
    -Wl,-Bdynamic -lpthread -lz -Wl,-Bstatic -llzma -Wl,-Bdynamic -ldl

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

# Test if we can read it back
readelf -x .upd_info runtime # hexdump
readelf -p .upd_info runtime || true # string

# The raw updateinformation data can be read out manually like this:
HEXOFFSET=$(objdump -h runtime | grep .upd_info | awk '{print $6}')
HEXLENGTH=$(objdump -h runtime | grep .upd_info | awk '{print $3}')
dd bs=1 if=runtime skip=$(($(echo 0x$HEXOFFSET)+0)) count=$(($(echo 0x$HEXLENGTH)+0)) | xxd

# Convert runtime into a data object that can be embedded into appimagetool

cp runtime "$orig_cwd"/
