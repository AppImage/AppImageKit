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

cp -f /usr/bin/file "$APPIMAGETOOL_APPDIR"/usr/bin

cp ../resources/appimagetool.desktop "$APPIMAGETOOL_APPDIR"
cp ../resources/appimagetool.svg "$APPIMAGETOOL_APPDIR"/appimagetool.svg
ln -s "$APPIMAGETOOL_APPDIR"/appimagetool.svg "$APPIMAGETOOL_APPDIR"/.DirIcon
mkdir -p "$APPIMAGETOOL_APPDIR"/usr/share/metainfo
cp ../resources/usr/share/metainfo/appimagetool.appdata.xml "$APPIMAGETOOL_APPDIR"/usr/share/metainfo/

if [ -d /deps/ ]; then
    # deploy glib
    mkdir -p "$APPIMAGETOOL_APPDIR"/usr/lib/
    cp /deps/lib/lib*.so* "$APPIMAGETOOL_APPDIR"/usr/lib/
    # libffi is a runtime dynamic dependency
    # see this thread for more information on the topic:
    # https://mail.gnome.org/archives/gtk-devel-list/2012-July/msg00062.html
    if [ "$ARCH" == "x86_64" ]; then
        cp /usr/lib64/libffi.so.5 "$APPIMAGETOOL_APPDIR"/usr/lib/
    else
        cp /usr/lib/libffi.so.5 "$APPIMAGETOOL_APPDIR"/usr/lib/
    fi
fi
