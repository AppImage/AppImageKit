# appimagetool [![Build Status](https://travis-ci.org/probonopd/appimagetool.svg?branch=master)](https://travis-ci.org/probonopd/appimagetool)

Not for productive use yet. For now use [AppImageKit](https://github.com/probonopd/AppImageKit).

## Building

On a not too recent Ubuntu:
```
git clone --recursive https://github.com/probonopd/appimagetool.git
cd appimagetool/
bash -ex build.sh
```

## Usage

```
Usage: appimagetool [OPTION...] SOURCE {DESTINATION}
appimagetool -- Generate, extract, and inspect AppImages

  -d, --dump=FILE            Dump FILE from SOURCE AppImage to stdout
  -l, --list                 List files in SOURCE AppImage
  -v, --verbose              Produce verbose output
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```

`appimagetool` uses an experimental next-generation AppImage format based on squashfs and embeds an experimental runtime for it.
If you want to use this squashfs-based runtime manually, you can:

```
mksquashfs Your.AppDir Your.squashfs -root-owned -noappend
dd if=Your.squashfs of=Your.AppImage bs=1024 seek=128
dd conv=notrunc if=runtime of=Your.AppImage
chmod a+x Your.AppImage
```
