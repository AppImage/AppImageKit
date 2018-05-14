#! /bin/bash

set -e

cd /AppImageKit/build

cpack -V
mv *.deb out/

cd out/

# || true -- temporary workaround

./appimagetool.AppDir/AppRun ./appimagetool.AppDir/ -s -v \
    -u "gh-releases-zsync|AppImage|AppImageKit|continuous|appimagetool-$ARCH.AppImage.zsync" \
    appimagetool-"$ARCH".AppImage || true
