# appimageruntime

Experimental runtime for next-generation AppImage format based on squashfs.

Not for productive use yet. For now use [AppImageKit](https://github.com/probonopd/AppImageKit).

## Usage

There will be a tool that does this for you, but for now

```
mksquashfs Your.AppDir Your.squashfs -root-owned -noappend
dd if=Your.squashfs of=Your.AppImage bs=1024 seek=128
dd conv=notrunc if=runtime of=Your.AppImage
chmod a+x Your.AppImage
```
