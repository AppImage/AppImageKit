#!/bin/bash

set -e
set -x


#######################################################################

# build AppImageTool AppDir
APPIMAGETOOL_APPDIR=appdirs/appimagetool.AppDir

rm -rf "$APPIMAGETOOL_APPDIR" || true

# Run make install only for the 'appimagetool.AppImage' component to deploy appimagetools files to
# the $APPIMAGETOOL_APPDIR
DESTDIR="$APPIMAGETOOL_APPDIR" cmake -DCOMPONENT=appimagetool -P cmake_install.cmake

mkdir -p "$APPIMAGETOOL_APPDIR"/usr/lib/appimagekit/

# Copy AppDir specific files
cp ../resources/AppRun "$APPIMAGETOOL_APPDIR"
cp install_prefix/usr/lib/appimagekit/mksquashfs "$APPIMAGETOOL_APPDIR"/usr/lib/appimagekit/
cp $(which desktop-file-validate) "$APPIMAGETOOL_APPDIR"/usr/bin/
cp $(which zsyncmake) "$APPIMAGETOOL_APPDIR"/usr/bin/

cp ../resources/appimagetool.desktop "$APPIMAGETOOL_APPDIR"
cp ../resources/appimagetool.svg "$APPIMAGETOOL_APPDIR"/appimagetool.svg
ln -s "$APPIMAGETOOL_APPDIR"/appimagetool.svg "$APPIMAGETOOL_APPDIR"/.DirIcon

if [ -d /deps/ ]; then
    # deploy glib
    mkdir -p "$APPIMAGETOOL_APPDIR"/usr/lib/
    cp /deps/lib/lib*.so* "$APPIMAGETOOL_APPDIR"/usr/lib/
    # libffi is a runtime dynamic dependency
    # see this thread for more information on the topic:
    # https://mail.gnome.org/archives/gtk-devel-list/2012-July/msg00062.html
    if [ "$ARCH" == "x86_64" ]; then
        cp /usr/lib64/libffi.so.5 "$APPIMAGETOOL_APPDIR"/usr/lib/
    elif [ "$ARCH" == "i686" ]; then
        cp /usr/lib/libffi.so.5 "$APPIMAGETOOL_APPDIR"/usr/lib/
    elif [ "$ARCH" == "armhf" ]; then
        cp /deps/lib/libffi.so.6 "$APPIMAGETOOL_APPDIR"/usr/lib/
    else
        echo "WARNING: unknown architecture, not bundling libffi"
    fi
fi
