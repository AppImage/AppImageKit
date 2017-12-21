#! /bin/bash

set -x
# important to allow for checking the exit code of timeout
set +e

TIMEOUT=3
ARCH=${ARCH:-$(uname -p)}

# fix architecture name
if [ "$ARCH" == "i686" ]; then
    export ARCH="i386"
fi

error() {
    echo "Error: command failed" >&2
    exit 1
}

# first of all, try to run appimagetool
out/appimagetool-"$ARCH".AppImage || error
out/appimagetool-"$ARCH".AppImage -h || error

# now check appimaged
timeout "$TIMEOUT" out/appimaged-"$ARCH".AppImage --no-install

if [ $? -ne 124 ]; then
    echo "Error: appimaged was not terminated by timeout as expected" >&2
    exit 1
fi

echo "" >&2
echo "Tests successful!" >&2
