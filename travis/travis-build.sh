#! /bin/bash

set -e
set -x

case "$ARCH" in
    "x86_64")
        export ARCH="x86_64"
        sed -i -e 's|%ARCH%|amd64|g' appimaged.ctl
        ;;
    "i386"|"i686")
        export ARCH="i686"
        sed -i -e 's|%ARCH%|i386|g' appimaged.ctl
        # sleep so as not to overwrite during uploading (FIXME)
        sleep 60
        ;;
esac

# debug print
grep Architecture appimaged.ctl

# clean up and download data from GitHub
rm -rf data.tar.g* .gnu* || true
wget https://github.com/AppImage/AppImageKit/files/584665/data.zip -O data.tar.gz.gpg
( set +x ; echo $KEY | gpg2 --batch --passphrase-fd 0 --no-tty --skip-verify --output data.tar.gz --decrypt data.tar.gz.gpg ) || true
( tar xf data.tar.gz ; sudo chown -R $USER .gnu* ; rm -rf $HOME/.gnu* ; mv .gnu* $HOME/ ) || true

# prepare output directory
mkdir -p ./out/

# build AppImageKit
docker run \
    --device /dev/fuse:mrw \
    -e ARCH -e TRAVIS -e TRAVIS_BUILD_NUMBER \
    -i \
    -v "${PWD}":/AppImageKit \
    -v "${PWD}"/travis/:/travis \
    "$DOCKER_IMAGE" \
    /bin/bash -x "/travis/build-binaries.sh" --run-tests

# inspect AppDirs
find build/out/appimagetool.AppDir/
find build/out/appimaged.AppDir/

# build AppImages
docker run \
    --cap-add SYS_ADMIN \
    --device /dev/fuse:mrw \
    -e ARCH -e TRAVIS -e TRAVIS_BUILD_NUMBER \
    -i \
    -v "${PWD}"/travis/:/travis \
    -v "${PWD}"/build:/build \
    -v $HOME/.gnupg:/root/.gnupg \
    "$DOCKER_IMAGE" \
    /bin/bash -x "/travis/build-appimages.sh"

cd build/

# test AppImages
[ "$ARCH" == "i686" ] && sudo apt-get install -y gcc-multilib lib32z1 libfuse2 libfuse2:i386 libglib2.0-0:i386 libcairo2:i386
bash -x ../travis/test-appimages.sh

# install more tools
# (vim-common contains xxd)
sudo apt-get install equivs vim-common

# fix permissions
sudo chown -R travis.travis .

# build .deb
(cd out ; equivs-build ../../appimaged.ctl)

# remove binaries from output directory
ls -al out/
rm -r out/{appimaged,appimagetool,validate,digest,mksquashfs,*.AppDir}

# inspect runtime
xxd out/runtime | head -n 1
# fix filename for upload
mv out/runtime out/runtime-"$ARCH"

# remove unused files
sudo rm -rf out/*.AppDir out/*.AppImage.digest

# fix filename for upload
sudo mv out/AppRun out/AppRun-"$ARCH"
