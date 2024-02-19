#! /bin/bash

set -e
set -E
set -o functrace

if [[ "$2" == "" ]]; then
    echo "Usage: bash $0 <repo root> <appimagetool>"
    exit 2
fi

# using shift makes adding/removing parameters easier
repo_root="$(readlink -f "$1")"
shift
appimagetool="$(readlink -f "$1")"

if [[ ! -x "$appimagetool" ]]; then
    echo "Error: appimagetool $appimagetool is not an executable"
    exit 3
fi

# this script attempts to package appimagetool as an AppImage with... surprise, appimagetool

# we always build in a temporary directory
# use RAM disk if possible
if [ -d /dev/shm ] && mount | grep /dev/shm | grep -v -q noexec; then
    TEMP_BASE=/dev/shm
elif [ -d /docker-ramdisk ]; then
    TEMP_BASE=/docker-ramdisk
else
    TEMP_BASE=/tmp
fi

BUILD_DIR="$(mktemp -d -p "$TEMP_BASE" appimagetool-test-XXXXXX)"

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

REPO_ROOT="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")"/..)"
OLD_CWD="$(readlink -f .)"

pushd "$BUILD_DIR"

# make sure to use the built mksquashfs
export PATH="$(dirname "$appimagetool")":"$PATH"

if [ "$CI" == true ]; then
  # TODO: find way to get colored log on Travis
  log() { echo -e "\n$*\n"; }
else
  log() { echo -e "\n$(tput setaf 2)$(tput bold)$*$(tput sgr0)\n"; }
fi

# debug log
trap '[[ $FUNCNAME = exithook ]] || { last_lineno=$real_lineno; real_lineno=$LINENO; }' DEBUG

exithook() {
    local exitcode="$1"
    local lineno="${last_lineno:-$2}"

    if [ "$exitcode" -ne 0 ]; then
        echo "$(tput setaf 1)$(tput bold)Test run failed: error in line $lineno$(tput sgr0)"
    else
        log "Tests succeeded!"
    fi

    exit "$exitcode"
}

trap 'exithook $? $LINENO ${BASH_LINENO[@]}' EXIT

# real script begins here
log "create a sample AppDir"
mkdir -p appimagetool.AppDir/usr/share/metainfo/
cp "$repo_root"/resources/{appimagetool.*,AppRun} appimagetool.AppDir/
#cp "$repo_root"/resources/usr/share/metainfo/appimagetool.appdata.xml appimagetool.AppDir/usr/share/metainfo/
cp "$appimagetool" appimagetool.AppDir/
mkdir -p appimagetool.AppDir/usr/share/applications
cp appimagetool.AppDir/appimagetool.desktop appimagetool.AppDir/usr/share/applications

log "create a file that should be ignored"
touch appimagetool.AppDir/to-be-ignored

log "create an AppImage without an ignore file to make sure it is bundled"
"$appimagetool" appimagetool.AppDir appimagetool.AppImage || false
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored || false

log "now set up the ignore file, and check that the file is properly ignored"
echo "to-be-ignored" > .appimageignore
"$appimagetool" appimagetool.AppDir appimagetool.AppImage
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored && false

log "remove the default ignore file, and check if an explicitly passed one works, too"
rm .appimageignore
echo "to-be-ignored" > ignore
"$appimagetool" appimagetool.AppDir appimagetool.AppImage --exclude-file ignore || false
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored && false

log "check whether files in both .appimageignore and the explicitly passed file work"
touch appimagetool.AppDir/to-be-ignored-too
echo "to-be-ignored-too" > .appimageignore
"$appimagetool" appimagetool.AppDir appimagetool.AppImage --exclude-file ignore
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored || true
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored-too || true

log "check whether AppImages built from the exact same AppDir are the same files (reproducible builds, #625)"
"$appimagetool" appimagetool.AppDir appimagetool.AppImage.1
"$appimagetool" appimagetool.AppDir appimagetool.AppImage.2
hash1=$(sha256sum appimagetool.AppImage.1 | awk '{print $1}')
hash2=$(sha256sum appimagetool.AppImage.2 | awk '{print $1}')
if [ "$hash1" != "$hash2" ]; then
    echo "Hash $hash1 doesn't match hash $hash2!"
    exit 1
fi

log "check --mksquashfs-opt forwarding"
"$appimagetool" appimagetool.AppDir appimagetool.AppImage.1
"$appimagetool" appimagetool.AppDir appimagetool.AppImage.2 --mksquashfs-opt "-mem" --mksquashfs-opt "100M"
"$appimagetool" appimagetool.AppDir appimagetool.AppImage.3 --mksquashfs-opt "-all-time" --mksquashfs-opt "12345"
hash1=$(sha256sum appimagetool.AppImage.1 | awk '{print $1}')
hash2=$(sha256sum appimagetool.AppImage.2 | awk '{print $1}')
hash3=$(sha256sum appimagetool.AppImage.3 | awk '{print $1}')
if [ "$hash1" != "$hash2" ]; then
    echo "Hashes of regular and mem-restricted AppImages differ"
    exit 1
fi
if [ "$hash1" == "$hash3" ]; then
    echo "Hashes of regular and mtime-modified AppImages don't differ"
    exit 1
fi

log "check appimagetool dies when mksquashfs fails"
set +e # temporarily disable error trapping as next line is supposed to fail
out=$("$appimagetool" appimagetool.AppDir appimagetool.AppImage --mksquashfs-opt "-misspelt-option" 2>&1)
rc=$?
set -e
test ${rc} == 1
echo "${out}" | grep -q "invalid option"
echo "${out}" | grep -qP 'mksquashfs \(pid \d+\) exited with code 1'
echo "${out}" | grep -q "sfs_mksquashfs error"
