#! /bin/bash

cd /AppImageKit

./build.sh "$@"

./test-appimagetool.sh build/install_prefix/usr/bin/appimagetool
