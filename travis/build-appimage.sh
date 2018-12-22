#! /bin/bash

set -e

cd /AppImageKit/build

cd out/

if [ "TRAVIS_PULL_REQUEST" == "false" ] ; then 
  # Sign only when NOT on a pull request
  ./appimagetool.AppDir/AppRun ./appimagetool.AppDir/ -s -v \
    -u "gh-releases-zsync|AppImage|AppImageKit|continuous|appimagetool-$ARCH.AppImage.zsync" \
    appimagetool-"$ARCH".AppImage
else
  # Do not attempt to sign (it would fail ayway)
  ./appimagetool.AppDir/AppRun ./appimagetool.AppDir/ -v \
    -u "gh-releases-zsync|AppImage|AppImageKit|continuous|appimagetool-$ARCH.AppImage.zsync" \
    appimagetool-"$ARCH".AppImage
 fi
