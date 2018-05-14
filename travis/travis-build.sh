#! /bin/bash

set -e
set -x

case "$ARCH" in
    "x86_64")
        export ARCH="x86_64"
        ;;
    "i386"|"i686")
        export ARCH="i686"
        # sleep so as not to overwrite during uploading (FIXME)
        sleep 60
        ;;
esac

# clean up and download data from GitHub
rm -rf data.tar.g* .gnu* || true
wget https://github.com/AppImage/AppImageKit/files/584665/data.zip -O data.tar.gz.gpg
( set +x ; echo $KEY | gpg2 --batch --passphrase-fd 0 --no-tty --skip-verify --output data.tar.gz --decrypt data.tar.gz.gpg ) || true
( tar xf data.tar.gz ; sudo chown -R $USER .gnu* ; rm -rf $HOME/.gnu* ; mv .gnu* $HOME/ ) || true

# prepare output directory
mkdir -p ./out/

# build AppImageKit
docker run --rm \
    --device /dev/fuse:mrw \
    -e ARCH -e TRAVIS -e TRAVIS_BUILD_NUMBER \
    -i \
    -v "${PWD}":/AppImageKit \
    -v "${PWD}"/travis/:/travis \
    "$DOCKER_IMAGE" \
    /bin/bash -x "/travis/build-binaries.sh" --run-tests

# inspect AppDirs
find build/out/appimagetool.AppDir/

# build AppImage
docker run --rm \
    --cap-add SYS_ADMIN \
    --device /dev/fuse:mrw \
    -e ARCH -e TRAVIS -e TRAVIS_BUILD_NUMBER \
    -i \
    -v "${PWD}":/AppImageKit \
    -v "${PWD}"/travis/:/travis \
    -v $HOME/.gnupg:/root/.gnupg \
    "$DOCKER_IMAGE" \
    /bin/bash -x "/travis/build-appimage.sh"

cd build/

# test AppImages
[ "$ARCH" == "i686" ] && sudo apt-get update && sudo apt-get install -y gcc-multilib lib32z1 libfuse2 libfuse2:i386 libglib2.0-0:i386 libcairo2:i386
bash -x ../travis/test-appimage.sh

# install more tools
# (vim-common contains xxd)
sudo apt-get install vim-common

# fix permissions
sudo chown -R travis.travis .

# remove binaries from output directory
ls -al out/
rm -r out/{appimagetool,validate,digest,mksquashfs,*.AppDir}

# inspect runtime
xxd out/runtime | head -n 1
# fix filename for upload
mv out/runtime out/runtime-"$ARCH"

# remove unused files
sudo rm -rf out/*.AppDir out/*.AppImage.digest

# fix filename for upload
sudo mv out/AppRun out/AppRun-"$ARCH"
