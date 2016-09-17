# appimagetool [![Build Status](https://travis-ci.org/probonopd/appimagetool.svg?branch=master)](https://travis-ci.org/probonopd/appimagetool)

Not for productive use yet. For now use [AppImageKit](https://github.com/probonopd/AppImageKit).

## Building

On a not too recent Ubuntu:
```
git clone --recursive https://github.com/probonopd/appimagetool.git
cd appimagetool/
bash -ex build.sh
```
A precompiled version can be found on the [Releases](https://github.com/probonopd/appimagetool/releases) page.

## Usage

```
Usage:
  appimagetool [OPTION...] SOURCE [DESTINATION] - Generate, extract, and inspect AppImages

Help Options:
  -h, --help                  Show help options

Application Options:
  -l, --list                  List files in SOURCE AppImage
  -u, --updateinformation     Embed update information STRING; if zsyncmake is installed, generate zsync file
  -v, --verbose               Produce verbose output
```

`appimagetool` uses an experimental next-generation AppImage format based on squashfs and embeds an experimental runtime for it.
If you want to use this squashfs-based runtime manually, you can:

```
mksquashfs Your.AppDir Your.squashfs -root-owned -noappend
dd if=Your.squashfs of=Your.AppImage bs=1024 seek=128
dd conv=notrunc if=runtime of=Your.AppImage
chmod a+x Your.AppImage
```
