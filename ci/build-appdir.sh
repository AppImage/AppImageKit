#!/bin/bash

set -e
set -x
set -o functrace

if [[ "$3" == "" ]]; then
    echo "Usage: bash $0 <repo root> <install-prefix> <appimagetool AppDir>"
    exit 2
fi

# using shift makes adding/removing parameters easier
repo_root="$1"
shift
install_prefix="$1"
shift
appimagetool_appdir="$1"

if [[ ! -d "$install_prefix" ]]; then
    echo "Error: could not find install-prefix directory: $install_prefix"
    exit 3
fi

if [[ ! -d "$repo_root" ]]; then
    echo "Error: could not find repo root directory: $repo_root"
    exit 4
fi

if [[ -d "$appimagetool_appdir" ]]; then
    echo "Warning: removing existing appimagetool AppDir: $appimagetool_appdir"
    rm -rf "$appimagetool_appdir"
fi

# Run make install only for the appimagetool component to deploy appimagetools files to
# the $appimagetool_appdir
DESTDIR="$appimagetool_appdir" cmake -DCOMPONENT=appimagetool -P cmake_install.cmake

mkdir -p "$appimagetool_appdir"/usr/lib/appimagekit/

# Copy AppDir specific files
cp "$repo_root"/resources/AppRun "$appimagetool_appdir"
cp "$install_prefix"/usr/lib/appimagekit/mksquashfs "$appimagetool_appdir"/usr/lib/appimagekit/

# prefer binaries from /deps, if available
export PATH=/deps/bin:"$PATH"
cp "$(which desktop-file-validate)" "$appimagetool_appdir"/usr/bin/
cp "$(which zsyncmake)" "$appimagetool_appdir"/usr/bin/

cp "$repo_root"/resources/appimagetool.desktop "$appimagetool_appdir"
cp "$repo_root"/resources/appimagetool.png "$appimagetool_appdir"/appimagetool.png
cp "$appimagetool_appdir"/appimagetool.png "$appimagetool_appdir"/.DirIcon

if [ -d /deps/ ]; then
    # deploy GLib
    mkdir -p "$appimagetool_appdir"/usr/lib/
    cp /deps/lib/lib*.so* "$appimagetool_appdir"/usr/lib/

    # libffi is a runtime dynamic dependency
    # see this thread for more information on the topic:
    # https://mail.gnome.org/archives/gtk-devel-list/2012-July/msg00062.html
    if [ "$ARCH" == "x86_64" ]; then
        cp "$(ldconfig -p | grep libffi.so.6 | grep x86-64 | cut -d'>' -f2 | tr -d ' ')" "$appimagetool_appdir"/usr/lib/
    elif [ "$ARCH" == "i686" ]; then
        cp "$(ldconfig -p | grep libffi.so.6 | head -n1 | cut -d'>' -f2 | tr -d ' ')" "$appimagetool_appdir"/usr/lib/
    elif [ "$ARCH" == "armhf" ]; then
        cp "$(ldconfig -p | grep libffi.so.6 | grep arm | grep hf | cut -d'>' -f2 | tr -d ' ')" "$appimagetool_appdir"/usr/lib/
    elif [ "$ARCH" == "aarch64" ]; then
        cp "$(ldconfig -p | grep libffi.so.6 | grep aarch64 | cut -d'>' -f2 | tr -d ' ')" "$appimagetool_appdir"/usr/lib/
    else
        echo "WARNING: unknown architecture, not bundling libffi"
    fi
fi
