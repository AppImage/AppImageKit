#! /bin/bash

cd /build/out

./appimagetool.AppDir/AppRun ./appimagetool.AppDir/ -s -v \
    -u "gh-releases-zsync|AppImage|AppImageKit|continuous|appimagetool-$ARCH.AppImage.zsync" \
    appimagetool-"$ARCH".AppImage

./appimagetool-"$ARCH".AppImage ./appimaged.AppDir/ -s -v \
    -u "gh-releases-zsync|AppImage|AppImageKit|continuous|appimaged-$ARCH.AppImage.zsync" \
    appimaged-"$ARCH".AppImage
