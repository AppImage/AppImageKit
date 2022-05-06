#! /bin/bash

set -e
set -x
set -o functrace

# make sure the prebuilt libraries in the container will be found
# (in case we're building in an AppImageBuild container)
export LD_LIBRARY_PATH=/deps/lib:"$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH=/deps/lib/pkgconfig/
export PATH=/deps/bin:"$PATH"

# note: signing is not working at the moment
# see ci-build.sh for more information
echo "$KEY" | md5sum

# we always build in a temporary directory
# use RAM disk if possible and if enough space available
USE_SHM=0
if [ -d /dev/shm ] && mount | grep /dev/shm | grep -v -q noexec; then
    SHM_FREE_KIB_MIN=$((1 * 1024 * 1024))
    SHM_FREE_KIB=$(df -P -k /dev/shm | tail -n 1 | sed -e 's/ \+/ /g' | cut -d ' ' -f 4)
    if [[ "$SHM_FREE_KIB" != "" ]] && [ $SHM_FREE_KIB -ge $SHM_FREE_KIB_MIN ]; then
        USE_SHM=1
    fi
fi
if [[ "$USE_SHM" = "1" ]]; then
    TEMP_BASE=/dev/shm
elif [ -d /docker-ramdisk ]; then
    TEMP_BASE=/docker-ramdisk
else
    TEMP_BASE=/tmp
fi

BUILD_DIR="$(mktemp -d -p "$TEMP_BASE" AppImageKit-build-XXXXXX)"

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

REPO_ROOT="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")"/..)"
OLD_CWD="$(readlink -f .)"

pushd "$BUILD_DIR"

# configure build and generate build files
cmake "$REPO_ROOT" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_TESTING=ON \
    -DAPPIMAGEKIT_PACKAGE_DEBS=ON

# run build
if [[ "$CI" != "" ]]; then
    nproc="$(nproc)"
else
    nproc="$(nproc --ignore=1)"
fi

make -j"$nproc"

# install so-far-built binaries into some prefix
# this way, we can test them, use them to build the AppDir later, copy stuff from there into said AppDir, ...
install_prefix="install-prefix"
make install DESTDIR="$install_prefix"

# print first few bytes in runtime
# allows checking whether the magic bytes are in place
xxd src/runtime | head -n1

# strip everything *but* runtime
find "$install_prefix"/usr/bin/ -not -iname runtime -print -exec "$STRIP" "{}" \; 2>/dev/null

# more debugging
ls -lh "$install_prefix"/usr/bin
for FILE in "$install_prefix"/usr/bin/*; do
  echo "$FILE"
  ldd "$FILE" || true
done

# continue building AppDir
appdir="appimagetool.AppDir"
bash "$REPO_ROOT"/ci/build-appdir.sh "$REPO_ROOT" "$install_prefix" "$appdir"

# list result
ls -lh
find "$install_prefix"

# test built appimagetool
"$REPO_ROOT"/ci/test-appimagetool.sh "$REPO_ROOT" "$install_prefix"/usr/bin/appimagetool

# seems to work -- let's build an AppImage
# we start by putting the rest of the files into the AppDir

"$appdir"/AppRun ./appimagetool.AppDir/ -v \
    -u "gh-releases-zsync|AppImage|AppImageKit|continuous|appimagetool-$ARCH.AppImage.zsync" \
    appimagetool-"$ARCH".AppImage

mv -v appimagetool-"$ARCH".AppImage* "$OLD_CWD"
mv -v "$install_prefix"/usr/lib/appimagekit/runtime "$OLD_CWD"
mv -v "$install_prefix"/usr/bin/* "$OLD_CWD"
