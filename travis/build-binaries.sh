#! /bin/bash

set -e
set -x

cd /AppImageKit

./build.sh "$@"

# make sure the prebuilt libraries in the container will be found
export LD_LIBRARY_PATH=/deps/lib:"$LD_LIBRARY_PATH"

./test-appimagetool.sh build/install_prefix/usr/bin/appimagetool
