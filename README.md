# appimagetool [![Build Status](https://travis-ci.org/probonopd/appimagetool.svg?branch=master)](https://travis-ci.org/probonopd/appimagetool)

Not for productive use yet. For now use [AppImageKit](https://github.com/probonopd/AppImageKit). `appimagetool` uses an experimental next-generation AppImage format based on squashfs and embeds an experimental runtime for it.

## Getting it
A precompiled version can be found in the last successful Travis CI build, you can get it with:

```
# Get the ID of the last successful build on Travis CI
ID=$(wget -q https://api.travis-ci.org/repos/probonopd/appimagetool/builds -O - | head -n 1 | sed -e 's|}|\n|g' | grep '"result":0' | head -n 1 | sed -e 's|,|\n|g' | grep '"id"' | cut -d ":" -f 2)

# Get the transfer.sh URL from the logfile of the last successful build on Travis CI
URL=$(wget -q "https://s3.amazonaws.com/archive.travis-ci.org/jobs/$((ID+1))/log.txt" -O - | grep "https://transfer.sh/.*/appimagetool" | tail -n 1 | sed -e 's|\r||g')

wget "$URL"
```

## Building

On a not too recent Ubuntu:
```
git clone --recursive https://github.com/probonopd/appimagetool.git
cd appimagetool/
bash -ex install-build-deps.sh
bash -ex build.sh
```

## Usage

```
chmod a+x appimagetool

./appimagetool some.AppDir
```

Detailed usage:
```
Usage:
  appimagetool [OPTION...] SOURCE [DESTINATION] - Generate, extract, and inspect AppImages

Help Options:
  -h, --help                  Show help options

Application Options:
  -l, --list                  List files in SOURCE AppImage
  -u, --updateinformation     Embed update information STRING; if zsyncmake is installed, generate zsync file
  --bintray-user              Bintray user name
  --bintray-repo              Bintray repository
  --version                   Show version number
  -v, --verbose               Produce verbose output
  -s, --sign                  Sign with gpg2
  -n, --no-appstream          Do not check AppStream metadata
```

If you want to generate an AppImage manually, you can:

```
mksquashfs Your.AppDir Your.squashfs -root-owned -noappend
cat runtime >> Your.AppImage
cat Your.squashfs >> Your.AppImage
chmod a+x Your.AppImage
```
