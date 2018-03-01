#! /bin/bash

set -x
# important to allow for checking the exit code of timeout
set +e

TIMEOUT=3
ARCH=${ARCH:-$(uname -m)}

error() {
    echo "Error: command failed" >&2
    exit 1
}

# first of all, try to run appimagetool
out/appimagetool-"$ARCH".AppImage && error  # should fail due to missing parameter
out/appimaged-"$ARCH".AppImage && error  # should fail due to missing parameter
out/appimagetool-"$ARCH".AppImage -h || error  # should not fail
out/appimaged-"$ARCH".AppImage -h || error  # should not fail

# print versions, updateinfo and other info, if present
# first, appimagetool
out/appimagetool-"$ARCH".AppImage --version || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-version || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-updateinformation || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-updateinfo || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-signature || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-offset || error  # should not fail
# next, appimaged
out/appimaged-"$ARCH".AppImage --version || error  # should not fail
out/appimaged-"$ARCH".AppImage --appimage-version || error  # should not fail
out/appimaged-"$ARCH".AppImage --appimage-updateinformation || error  # should not fail
out/appimaged-"$ARCH".AppImage --appimage-updateinfo || error  # should not fail
out/appimaged-"$ARCH".AppImage --appimage-signature || error  # should not fail
out/appimaged-"$ARCH".AppImage --appimage-offset || error  # should not fail

# check creation of .home and .config files (even if never used by these tools)
out/appimagetool-"$ARCH".AppImage --appimage-portable-home || error  # should not fail
out/appimagetool-"$ARCH".AppImage --appimage-portable-config || error  # should not fail
out/appimaged-"$ARCH".AppImage --appimage-portable-home || error  # should not fail
out/appimaged-"$ARCH".AppImage --appimage-portable-config || error  # should not fail
#[ -d out/appimagetool-"$ARCH".AppImage.home ] || error   # should not fail
#[ -d out/appimagetool-"$ARCH".AppImage.config ] || error   # should not fail
#[ -d out/appimaged-"$ARCH".AppImage.home ] || error   # should not fail
#[ -d out/appimaged-"$ARCH".AppImage.config ] || error   # should not fail
# Don't care cleaning up out/appimage{d,tool}-"$ARCH".AppImage.{config,home}... Or should we?

# now test --appimage-mount
timeout "$TIMEOUT" out/appimaged-"$ARCH".AppImage    --appimage-mount ; ret_of_appimaged_mount="$?"
timeout "$TIMEOUT" out/appimagetool-"$ARCH".AppImage --appimage-mount ; ret_of_appimagetool_mount="$?"
echo ret_of_appimaged_mount=$ret_of_appimaged_mount
echo ret_of_appimagetool_mount=$ret_of_appimagetool_mount

if [[ x$ret_of_appimaged_mount != x124 ]] || [[ x$ret_of_appimagetool_mount != x124 ]] ; then
    echo "Error: mounting of appimaged or appimagetool did not terminate by timeout as expected" >&2
    echo "Error: return code of appimaged mount: $ret_of_appimaged_mount"       >&2
    echo "Error: return code of appimagetool mount: $ret_of_appimagetool_mount" >&2
    exit 1
fi

# now check appimaged
timeout "$TIMEOUT" out/appimaged-"$ARCH".AppImage --no-install

if [ $? -ne 124 ]; then
    echo "Error: appimaged was not terminated by timeout as expected" >&2
    exit 1
fi

echo "" >&2
echo "Tests successful!" >&2
