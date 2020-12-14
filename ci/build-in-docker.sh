#! /bin/bash

if [[ "$DIST" == "" ]] || [[ "$ARCH" == "" ]]; then
    echo "Usage: env ARCH=... DIST=... bash $0"
    exit 1
fi

set -x
set -e

cd "$(readlink -f "$(dirname "$0")")"

# sets variables $image, $dockerfile
source build-docker-image.sh

# figure out which build script to use
if [[ "$BUILD_LITE" == "" ]]; then
    build_script=build.sh
else
    build_script=build-lite.sh
fi

DOCKER_OPTS=()
# fix for https://stackoverflow.com/questions/51195528/rcc-error-in-resource-qrc-cannot-find-file-png
if [ "$CI" != "" ]; then
    DOCKER_OPTS+=("--security-opt" "seccomp:unconfined")
fi

# only if there's more than 3G of free space in RAM, we can build in a RAM disk
if [[ "$GITHUB_ACTIONS" != "" ]]; then
    echo "Building on GitHub actions, which does not support --tmpfs flag -> building on regular disk"
elif [[ "$(free -m  | grep "Mem:" | awk '{print $4}')" -gt 3072 ]]; then
    echo "Host system has enough free memory -> building in RAM disk"
    DOCKER_OPTS+=("--tmpfs /docker-ramdisk:exec,mode=777")
else
    echo "Host system does not have enough free memory -> building on regular disk"
fi

# run the build with the current user to
#   a) make sure root is not required for builds
#   b) allow the build scripts to "mv" the binaries into the /out directory
uid="$(id -u)"
# run build
docker run -e DIST -e ARCH -e GITHUB_RUN_NUMBER -e GITHUB_RUN_ID --rm -i --user "$uid" \
     "${DOCKER_OPTS[@]}" -v "$(readlink -f ..):/ws" \
     "$image" \
     bash -xc "export CI=1 && cd /ws && source ci/$build_script"

