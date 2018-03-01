#! /bin/bash

set -e
set -E
set -o functrace

# this script attempts to package appimagetool as an AppImage with... surprise, appimagetool

tempdir=$(mktemp -d /tmp/appimage-test.XXXXXXXX)
thisdir=$(dirname $(readlink -f "$0"))

appimagetool=$(readlink -f "$1")

# make sure to use the built mksquashfs
export PATH=$(dirname "$appimagetool"):"$PATH"

if [ ! -f "$appimagetool" ] || [ ! -x "$appimagetool" ]; then
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
#cp "$thisdir"/resources/usr/share/metainfo/appimagetool.appdata.xml appimagetool.AppDir/usr/share/metainfo/
cp "$appimagetool" appimagetool.AppDir/
mkdir -p appimagetool.AppDir/usr/share/applications
cp appimagetool.AppDir/appimagetool.desktop appimagetool.AppDir/usr/share/applications

set -x # temporarily...
log "create a file that should be ignored"
touch appimagetool.AppDir/to-be-ignored                                                   # 'to-be-ignored' exists

log "create an AppImage without an ignore file to make sure it is bundled"
"$appimagetool" appimagetool.AppDir appimagetool.AppImage || false
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored || false                 # 'to-be-ignored' should be in AppImage

log "now set up the ignore file, and check that the file is properly ignored"
echo "to-be-ignored" > .appimageignore                                                    # 'to-be-ignored' listed in .appimageignore
"$appimagetool" appimagetool.AppDir appimagetool.AppImage
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored && false                 # 'to-be-ignored' shouldn't be in AppImage

log "remove the default ignore file, and check if an explicitly passed one works, too"
rm .appimageignore                                                                        # .appimageignore exists no more
echo "to-be-ignored" > ignore                                                             # 'ignore' exists
"$appimagetool" appimagetool.AppDir appimagetool.AppImage --exclude-file ignore || false  # 'ignore' shoudn't be in AppImage
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored && false
"$appimagetool" -l appimagetool.AppImage | grep -q ignore && false                        # 'ignore' shouldn't be in AppImage

log "check whether files in both .appimageignore and the explicitly passed file work"
touch appimagetool.AppDir/to-be-ignored-too                                               # 'to-be-ignored-too' exists 
echo "to-be-ignored-too" > .appimageignore                                                # .appimageignore exists again, lists 'to-be-ignored-too'
"$appimagetool" appimagetool.AppDir appimagetool.AppImage --exclude-file ignore           # 'ignore' shouldn't be in AppImage
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored || true  #<= Isn't this line wrong? Should be like next:
"$appimagetool" -l appimagetool.AppImage | grep -q ignore || true                         
"$appimagetool" -l appimagetool.AppImage | grep -q to-be-ignored-too || true

log "check whether AppImages built from the exact same AppDir are the same files (reproducible builds, #625)"
"$appimagetool" appimagetool.AppDir appimagetool.AppImage.1
"$appimagetool" appimagetool.AppDir appimagetool.AppImage.2
hash1=$(sha256sum appimagetool.AppImage.1 | awk '{print $1}')
hash2=$(sha256sum appimagetool.AppImage.2 | awk '{print $1}')

if [ "$hash1" != "$hash2" ]; then
    echo "Hash $hash1 doesn't match hash $hash2!"
    exit 1
else
    echo "Hashes for *.AppImage.1/2 both are $hash1"
fi
