#!/bin/bash

#######################################################################

# Build appimagetool.AppDir

if [ ! -d ./build ] ; then
  echo "You need to run build.sh first"
fi

rm -rf appimagetool.AppDir/ || true
mkdir -p appimagetool.AppDir/usr/bin
cp -f build/appimagetool appimagetool.AppDir/usr/bin

cp resources/AppRun appimagetool.AppDir/
cp build/appimagetool appimagetool.AppDir/usr/bin/
cp build/mksquashfs appimagetool.AppDir/usr/bin/

cp $(which zsyncmake) appimagetool.AppDir/usr/bin/

cp resources/appimagetool.desktop appimagetool.AppDir/
cp resources/appimagetool.svg appimagetool.AppDir/appimagetool.svg
( cd appimagetool.AppDir/ ; ln -s appimagetool.svg .DirIcon )
mkdir -p appimagetool.AppDir/usr/share/metainfo
cp resources/usr/share/metainfo/appimagetool.appdata.xml appimagetool.AppDir/usr/share/metainfo/

#######################################################################

# Build appimaged.AppDir

rm -rf appimaged.AppDir/ || true
mkdir -p appimaged.AppDir/usr/bin
mkdir -p appimaged.AppDir/usr/lib
cp -f build/appimaged appimaged.AppDir/usr/bin
cp -f build/validate appimaged.AppDir/usr/bin
mkdir -p appimaged.AppDir/usr/share/metainfo
cp resources/usr/share/metainfo/appimaged.appdata.xml appimaged.AppDir/usr/share/metainfo/

cp resources/AppRun appimaged.AppDir/
find /usr -name "libarchive.so.*.*" -exec cp {} appimaged.AppDir/usr/lib/ \; > /dev/null 2>&1

cp resources/appimaged.desktop appimaged.AppDir/
cp resources/appimagetool.svg appimaged.AppDir/appimaged.svg
( cd appimaged.AppDir/ ; ln -s appimaged.svg .DirIcon )

#######################################################################
