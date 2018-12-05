#! /bin/bash

set -x
# important to allow for checking the exit code of timeout
set +e

TIMEOUT=3
ARCH=${ARCH:-$(uname -m)}

error() {
    echo "Error: command failed" >&2
    exit 1
}

# first of all, try to run appimagetool
out/appimagetool-"$ARCH".AppImage && error  # should fail due to missing parameter
out/appimagetool-"$ARCH".AppImage -h || error  # should not fail

# print version and update information
out/appimagetool-"$ARCH".AppImage --version || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-version || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-updateinformation || error  # should not fail

echo "" >&2
echo "Tests successful!" >&2
