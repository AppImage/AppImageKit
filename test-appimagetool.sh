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

# create a sample AppDir
mkdir appimagetool.AppDir
cp -R "$thisdir"/resources/{appimagetool.*,AppRun} appimagetool.AppDir/
cp -R "$appimagetool" appimagetool.AppDir/

# create a file that should be ignored
touch appimagetool.AppDir/to-be-ignored

# create an AppImage without an ignore file to make sure it is bundled
$appimagetool appimagetool.AppDir appimagetool.AppImage
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored || exit 1

# now set up the ignore file, and check that the file is properly ignored
echo "to-be-ignored" > .appimageignore
$appimagetool appimagetool.AppDir appimagetool.AppImage
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored && exit 1 || true

# check if disabling the ignore file feature works by passing an empty string as argument
$appimagetool appimagetool.AppDir appimagetool.AppImage --exclude-file ""
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored || exit 1

# remove the default ignore file, and check if an explicitly passed one works, too
rm .appimageignore
echo "to-be-ignored" > ignore
$appimagetool appimagetool.AppDir appimagetool.AppImage --exclude-file ignore
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored && exit 1 || true
