#!/bin/bash

set -e
set -x

# preparations
mkdir -p appdirs/

#######################################################################

# build AppImageTool AppDir
APPIMAGETOOL_APPDIR=appdirs/appimagetool.AppDir

rm -rf "$APPIMAGETOOL_APPDIR" || true
mkdir -p "$APPIMAGETOOL_APPDIR"/usr/{bin,lib/appimagekit}
cp -f install_prefix/usr/bin/appimagetool "$APPIMAGETOOL_APPDIR"/usr/bin

cp ../resources/AppRun "$APPIMAGETOOL_APPDIR"
cp install_prefix/usr/bin/appimagetool "$APPIMAGETOOL_APPDIR"/usr/bin/
cp install_prefix/usr/lib/appimagekit/mksquashfs "$APPIMAGETOOL_APPDIR"/usr/lib/appimagekit/
cp $(which desktop-file-validate) "$APPIMAGETOOL_APPDIR"/usr/bin/
cp $(which zsyncmake) "$APPIMAGETOOL_APPDIR"/usr/bin/

cp ../resources/appimagetool.desktop "$APPIMAGETOOL_APPDIR"
cp ../resources/appimagetool.svg "$APPIMAGETOOL_APPDIR"/appimagetool.svg
ln -s "$APPIMAGETOOL_APPDIR"/appimagetool.svg "$APPIMAGETOOL_APPDIR"/.DirIcon
mkdir -p "$APPIMAGETOOL_APPDIR"/usr/share/metainfo
cp ../resources/usr/share/metainfo/appimagetool.appdata.xml "$APPIMAGETOOL_APPDIR"/usr/share/metainfo/

#######################################################################

# build appimaged AppDir

APPIMAGED_APPDIR=appdirs/appimaged.AppDir

rm -rf "$APPIMAGED_APPDIR"/ || true
mkdir -p "$APPIMAGED_APPDIR"/usr/bin
mkdir -p "$APPIMAGED_APPDIR"/usr/lib
cp -f install_prefix/usr/bin/appimaged "$APPIMAGED_APPDIR"/usr/bin
cp -f install_prefix/usr/bin/validate "$APPIMAGED_APPDIR"/usr/bin
mkdir -p "$APPIMAGED_APPDIR"/usr/share/metainfo
cp ../resources/usr/share/metainfo/appimaged.appdata.xml "$APPIMAGED_APPDIR"/usr/share/metainfo/

cp ../resources/AppRun "$APPIMAGED_APPDIR"/

cp ../resources/appimaged.desktop "$APPIMAGED_APPDIR"/
cp ../resources/appimagetool.svg "$APPIMAGED_APPDIR"/appimaged.svg
( cd "$APPIMAGED_APPDIR"/ ; ln -s appimaged.svg .DirIcon )
