#! /bin/bash

cd /AppImageKit

./build.sh "$@"

./test-appimagetool.sh build/out/usr/bin/appimagetool
