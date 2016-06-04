# AppImage Sandbox

The files in this directory are __purely optional__.

They integrate AppImage into Linux desktops so that when an AppImage is double-clicked without the executable bit set, it is executed in a sandbox. When the executable bit is set on the AppImage, then it is ran outside of the sandbox. (This behavior might change in the future depending on user feedback).

## Philosophy

In order to run "untrusted" AppImages, the sandbox part cannot come inside the AppImage (because the AppImage cannot be trusted). Hence, for an effective sandboxing mechanism to work, we need to install the infrastructure for it into the base system. This violates the AppImage principle that AppImages must be able to run without any support from the base system, hence the whole sandbox part is purely optional.

While we are at it, we assume that who can install the sandbox part into the base system has root rights, which opens new possibilities.

## Implementation

Right now this is using https://github.com/projectatomic/bubblewrap

## TODO

* Have a suid binary safely loop-mount the AppImage
