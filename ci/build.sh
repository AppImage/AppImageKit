#! /bin/bash

set -e
set -x

if [[ "$ARCH" == "" ]]; then
    echo "Usage: env ARCH=... bash $0"
    exit 2
fi

# TODO: fix inconsistency in architecture naming *everywhere*
docker_arch=

case "$ARCH" in
    "x86_64")
        export ARCH="x86_64"
        docker_dist=centos7
        ;;
    "i386"|"i686")
        export ARCH="i686"
        docker_arch="i386"
        docker_dist=centos7
        ;;
    armhf|aarch64)
        docker_dist=bionic
        ;;
    *)
        echo "Unknown architecture: $ARCH"
        exit 3
        ;;
esac


#### NOTE ####
# Signing is currently broken, as the secret to decrypt the key is not available at the moment.
# Even worse, putting key material in a third-party execution environment is not a good idea
# to begin with.
# The signing doesn't add anything security wise anyway, so it doesn't really matter if it works
# at the moment or not.
#### END NOTE ####

# clean up and download data from GitHub
#rm -rf data.tar.g* .gnu* || true
#wget https://github.com/AppImage/AppImageKit/files/584665/data.zip -O data.tar.gz.gpg
#( set +x ; echo $KEY | gpg2 --batch --passphrase-fd 0 --no-tty --skip-verify --output data.tar.gz --decrypt data.tar.gz.gpg ) || true
#( tar xf data.tar.gz ; sudo chown -R $USER .gnu* ; rm -rf $HOME/.gnu* ; mv .gnu* $HOME/ ) || true


# make sure we're in the repository root directory
this_dir="$(readlink -f "$(dirname "$0")")"
repo_root="$this_dir"/..

# docker image name
docker_image=quay.io/appimage/appimagebuild:"$docker_dist"-"${docker_arch:-$ARCH}"
# make sure it's up to date
docker pull "$docker_image"

# prepare output directory
mkdir -p out/

# we run all builds with non-privileged user accounts to make sure the build doesn't depend on such features
uid="$(id -u)"

# When running under Podman (on hosts where `docker` is a shim that invokes `podman`),
# UID/GID mappings may lead to permission errors when copying artifacts to `/out`.
# We set the user namespace mode to `keep-id` to make sure that the host UID/GID
# are mapped to the same values inside the container, but using this environment
# variable (a) to not affect builds using Docker, and (b) to allow overriding the
# user namespace mode easily if necessary.
export PODMAN_USERNS="${PODMAN_USERNS:-keep-id}"

# note: we cannot just use '-e ARCH', as this wouldn't overwrite the value set via ENV ARCH=... in the image
common_docker_opts=(
    -e TERM="$TERM"
    -e ARCH="$ARCH"
    -i
    -v "$repo_root":/ws
    -v "$(readlink -f out/)":/out
)

# make ctrl-c work
if [[ "$CI" == "" ]] && [[ "$TERM" != "" ]]; then
    common_docker_opts+=("-t")
fi

# build AppImageKit and appimagetool-"$ARCH".AppImage
# TODO: make gnupg home available, e.g., through "-v" "$HOME"/.gnupg:/root/.gnupg
# TODO: this ^ won't work since we don't build as root any more
# note: we enforce using the same UID in the container as outside, so that the created files are owned by the caller
docker run --rm \
    --user "$uid" \
    "${common_docker_opts[@]}" \
    "-v" "$HOME"/.gnupg:/root/.gnupg \
    "$docker_image" \
    /bin/bash -xc "cd /out && /ws/ci/build-binaries-and-appimage.sh --run-tests"

# test appimagetool-"$ARCH".AppImage
# note: if we're in a CI system, we allow the use of FUSE in the container, to make sure that this functionality works as intended
# outside CI environments, we use APPIMAGE_EXTRACT_AND_RUN instead, which is safer, but gives less meaningful results
# note: FUSE and QEMU don't like each other, so if we're running in emulated mode, we can't run these tests
# therefore, by default, on ARM, these tests are not run
if [[ "$ARCH" != "arm"* ]] && [[ "$ARCH" != "aarch"* ]]; then
    docker_test_opts=("${common_docker_opts[@]}")

    if [[ "$CI" != "" ]]; then
        echo "Warning: assuming this is running in a CI environment, allowing the use of FUSE in the container"
        docker_test_opts+=(
            "--device" "/dev/fuse:mrw"
            "--cap-add" "SYS_ADMIN"
            "--security-opt" "apparmor:unconfined"
        )
    else
        echo "Note: this is not a CI environment, using APPIMAGE_EXTRACT_AND_RUN and patching out magic bytes"
        docker_test_opts+=(
            "-e" "APPIMAGE_EXTRACT_AND_RUN=1"
            "-e" "PATCH_OUT_MAGIC_BYTES=1"
        )
    fi

    # to make fuse happy, we need to use a "real" user
    # as we don't want to use root, we use the user "build" we created in AppImageBuild
    docker run --rm \
        --user build \
        "${docker_test_opts[@]}" \
        "$docker_image" \
        /bin/bash -xc "cd /out && bash /ws/ci/test-appimage.sh ./appimagetool-\"$ARCH\".AppImage"
fi

# remove binaries from output directory
ls -al out/
rm out/appimagetool
rm out/{validate,digest}

# make sure the runtime contains the magic bytes
hexdump -Cv out/runtime | head -n 1 | grep "41 49 02 00"
# fix filename for upload
mv out/runtime out/runtime-"$ARCH"

# fix filename for upload
mv out/AppRun out/AppRun-"$ARCH"
