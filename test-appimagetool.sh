#! /bin/bash

set -e
set -E
set -o functrace

# this script attempts to package appimagetool as an AppImage with... surprise, appimagetool

tempdir=$(mktemp -d /tmp/appimage-test.XXXXXXXX)
thisdir=$(dirname $(readlink -f "$0"))

export PATH="$thisdir/squashfs-tools/squashfs-tools:$PATH"

appimagetool=$(readlink -f "$1")

if [ ! -x "$appimagetool" ]; then
    echo "Usage: $0 <path to appimagetool>"
    exit 1
fi

if [ "$TRAVIS" == true ]; then
  # TODO: find way to get colored log on Travis
  log() { echo -e "\n$*\n"; }
else
  log() { echo -e "\n$(tput setaf 2)$(tput bold)$*$(tput sgr0)\n"; }
fi

# debug log
trap '[[ $FUNCNAME = exithook ]] || { last_lineno=$real_lineno; real_lineno=$LINENO; }' DEBUG

exithook() {
    local exitcode="$1"
    local lineno=${last_lineno:-$2}

    if [ $exitcode -ne 0 ]; then
        echo "$(tput setaf 1)$(tput bold)Test run failed: error in line $lineno$(tput sgr0)"
    else
        log "Tests succeeded!"
    fi

    rm -r "$tempdir";

    exit $exitcode
}

trap 'exithook $? $LINENO ${BASH_LINENO[@]}' EXIT

# real script begins here
cd "$tempdir"

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
$appimagetool appimagetool.AppDir appimagetool.AppImage || false
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored || false

log "now set up the ignore file, and check that the file is properly ignored"
echo "to-be-ignored" > .appimageignore
$appimagetool appimagetool.AppDir appimagetool.AppImage
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored && false

log "remove the default ignore file, and check if an explicitly passed one works, too"
rm .appimageignore
echo "to-be-ignored" > ignore
$appimagetool appimagetool.AppDir appimagetool.AppImage --exclude-file ignore || false
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored && false

log "check whether files in both .appimageignore and the explicitly passed file work"
touch appimagetool.AppDir/to-be-ignored-too
echo "to-be-ignored-too" > .appimageignore
$appimagetool appimagetool.AppDir appimagetool.AppImage --exclude-file ignore
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored || true
$appimagetool -l appimagetool.AppImage | grep -q to-be-ignored-too || true
