#! /bin/bash

set -x
# important to allow for checking the exit code of timeout
set +e

TIMEOUT=3
ARCH=${ARCH:-$(uname -p)}


# first of all, try to run appimagetool
out/appimagetool-"$ARCH".AppImage
out/appimagetool-"$ARCH".AppImage -h

# now check appimaged
timeout "$TIMEOUT" out/appimaged-"$ARCH".AppImage

if [ $? -ne 124 ]; then
    echo "Error: appimaged was not terminated by timeout as expected" >&2
    exit 1
fi

echo "" >&2
echo "Tests successful!" >&2
