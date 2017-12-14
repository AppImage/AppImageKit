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
    -e TRAVIS -i \
    -v "${PWD}":/AppImageKit \
    -v "${PWD}"/travis/:/travis
    -v "${PWD}"/out:/out \
    "$DOCKER_IMAGE" \
    /bin/bash -x "/travis/build-binaries.sh" --no-install-dependencies

# inspect AppDirs
find ./out/appimagetool.AppDir/
find ./out/appimaged.AppDir/

# build AppImages
docker run \
    --cap-add SYS_ADMIN \
    --device /dev/fuse:mrw \
    -i \
    -v "${PWD}"/travis/:/travis
    -v "${PWD}"/out:/out \
    -v $HOME/.gnupg:/root/.gnupg \
    "$DOCKER_IMAGE" \
    /bin/bash -x "/travis/build-appimages.sh"

# install more tools
# (vim-common contains xxd)
sudo apt-get install equivs vim-common

# build .deb
(cd out ; equivs-build ../appimaged.ctl)

# remove binaries from output directory
rm -rf out/appimaged out/appimagetool out/validate out/digest out/mksquashfs

# inspect runtime
xxd out/runtime | head -n 1
# fix filename for upload
mv out/runtime out/runtime-"$ARCH"

# remove unused files
sudo rm -rf out/*.AppDir out/*.AppImage.digest

# fix filename for upload
sudo mv out/AppRun out/AppRun-"$ARCH"
