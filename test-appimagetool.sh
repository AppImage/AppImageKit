#! /bin/bash

set -xe

# this script attempts to package appimagetool as an AppImage with... surprise, appimagetool

tempdir=$(mktemp -d /tmp/appimage-test.XXXXXXXX)
thisdir=$(dirname $(readlink -f "$0"))

export PATH="$thisdir/squashfs-tools/squashfs-tools:$PATH"

appimagetool=$(readlink -f "$1")

if [ ! -x "$appimagetool" ]; then
    echo "Usage: $0 <path to appimagetool>"
    exit 1
fi

cleanup() { rm -r "$tempdir"; }
trap cleanup EXIT

cd "$tempdir"

# Does not work on Travis CI
# log() { echo "$(tput setaf 2)$(tput bold)$*$(tput sgr0)"; }

log "create a sample AppDir"
mkdir -p appimagetool.AppDir/usr/share/metainfo/
cp "$thisdir"/resources/{appimagetool.*,AppRun} appimagetool.AppDir/
cp "$thisdir"/resources/usr/share/metainfo/appimagetool.appdata.xml appimagetool.AppDir/usr/share/metainfo/
cp "$appimagetool" appimagetool.AppDir/
mkdir -p appimagetool.AppDir/usr/share/applications
cp appimagetool.AppDir/appimagetool.desktop appimagetool.AppDir/usr/share/applications

log "create a file that should be ignored"
touch appimagetool.AppDir/to-be-ignored

log "create an AppImage without an ignore file to make sure it is bundled"
$appimagetool appimagetool.AppDir appimagetool.AppImage || exit $?
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored || exit 1

log "now set up the ignore file, and check that the file is properly ignored"
echo "to-be-ignored" > .appimageignore
$appimagetool appimagetool.AppDir appimagetool.AppImage
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored && exit 1 || true

log "remove the default ignore file, and check if an explicitly passed one works, too"
rm .appimageignore
echo "to-be-ignored" > ignore
$appimagetool appimagetool.AppDir appimagetool.AppImage --exclude-file ignore || exit $?
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored && exit 1 || true

log "check whether files in both .appimageignore and the explicitly passed file work"
touch appimagetool.AppDir/to-be-ignored-too
echo "to-be-ignored-too" > .appimageignore
$appimagetool appimagetool.AppDir appimagetool.AppImage --exclude-file ignore || exit $?
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored && exit 1 || true
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored-too && exit 1 || true
