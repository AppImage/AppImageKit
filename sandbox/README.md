# AppImage Sandbox

The files in this directory are __purely optional__.

They integrate AppImage into Linux desktops so that when an AppImage is double-clicked without the executable bit set, it is executed in a sandbox. When the executable bit is set on the AppImage, then it is ran outside of the sandbox. (This behavior might change in the future depending on user feedback).

## TODO

* Have a suid binary safely loop-mount the AppImage
