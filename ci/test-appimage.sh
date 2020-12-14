#! /bin/bash

set -x
# important to allow for checking the exit code of timeout
set +e

TIMEOUT=3
ARCH="${ARCH:-"$(uname -m)"}"

error() {
    echo "Error: command failed" >&2
    exit 1
}

if [[ "$1" == "" ]] || [[ ! -x "$1" ]]; then
    echo "Usage: env [PATCH_OUT_MAGIC_BYTES=1] bash $0 <appimagetool.AppImage>"
    exit 2
fi

appimagetool="$1"

if [[ "$PATCH_OUT_MAGIC_BYTES" != "" ]]; then
    tmpdir="$(mktemp -d /tmp/appimage-test-XXXXX)"
    cleanup() {
        [[ -d "$tmpdir" ]] && rm -r "$tmpdir"
    }
    trap cleanup EXIT

    echo "Copying appimagetool and patching out magic bytes"
    cp "$appimagetool" "$tmpdir/"
    ls -al "$tmpdir"
    appimagetool="$tmpdir"/"$(basename "$appimagetool")"
    dd if=/dev/zero of="$appimagetool" bs=1 count=3 seek=8 conv=notrunc
fi

# first of all, try to run appimagetool
"$appimagetool" && error  # should fail due to missing parameter
"$appimagetool" -h || error  # should not fail

# print version and update information
"$appimagetool" --version || error  # should not fail
"$appimagetool" --appimage-version || error  # should not fail
"$appimagetool" --appimage-updateinformation || error  # should not fail

echo "" >&2
echo "Tests successful!" >&2
