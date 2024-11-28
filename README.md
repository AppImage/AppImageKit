# AppImageKit  [![irc](https://img.shields.io/badge/IRC-%23AppImage%20on%20libera.chat-blue.svg)](https://web.libera.chat/#AppImage) [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=ZT9CL8M5TJU72)

The __AppImage__ format is a format for packaging applications in a way that allows them to
run on a variety of different target systems (base operating systems, distributions) without further modification. 

Using the AppImage format you can package desktop applications as AppImages that run on common Linux-based operating systems, such as RHEL, CentOS, Ubuntu, Fedora, Debian and derivatives.

Copyright (c) 2004-24 Simon Peter <probono@puredarwin.org> and contributors.

https://en.wikipedia.org/wiki/AppImage

Providing an [AppImage](http://appimage.org/) for distributing application has, among others, these advantages:
- Applications packaged as an AppImage can run on many distributions (including Debian, Ubuntu, Fedora, openSUSE, Linux Mint, and others)
- One app = one file = super simple for users: just download one AppImage file, [make it executable](http://discourse.appimage.org/t/how-to-make-an-appimage-executable/80), and run
- No unpacking or installation necessary
- No root needed
- No system libraries changed
- Works out of the box, no installation of runtimes needed
- Optional desktop integration with [appimaged](https://github.com/probonopd/go-appimage/tree/master/src/appimaged#appimaged)
- Optional binary delta updates, e.g., for continuous builds (only download the binary diff) using AppImageUpdate
- Can optionally GPG2-sign your AppImages (inside the file)
- Works on Live ISOs
- Can use the same AppImages when dual-booting multiple distributions
- Can be listed in the [AppImageHub](https://appimage.github.io/apps) central directory of available AppImages
- Can double as a self-extracting compressed archive with the `--appimage-extract` parameter

[Here is an overview](https://appimage.github.io/apps) of projects that are distributing AppImages.

If you have questions, AppImage developers are on #AppImage on irc.libera.chat.

## AppImage usage

Running an AppImage mounts the filesystem image and transparently runs the contained application. So the usage of an AppImage normally should equal the usage of the application contained in it. However, there is special functionality, as described here. If an AppImage you have received does not support these options, ask the author of the AppImage to recreate it using the latest tooling).

### Command line arguments

If you invoke an AppImage built with a recent version of AppImageKit with one of these special command line arguments, then the AppImage will behave differently:

- `--appimage-help` prints the help options
- `--appimage-offset` prints the offset at which the embedded filesystem image starts, and then exits. This is useful in case you would like to loop-mount the filesystem image using the `mount -o loop,offset=...` command 
- `--appimage-extract` extracts the contents from the embedded filesystem image, then exits. This is useful if you are using an AppImage on a system on which FUSE is not available
- `--appimage-mount` mounts the embedded filesystem image and prints the mount point, then waits until it is killed. This is useful if you would like to inspect the contents of an AppImage without executing the contained payload application
- `--appimage-version` prints the version of AppImageKit, then exits. This is useful if you would like to file issues
- `--appimage-updateinformation` prints the update information embedded into the AppImage, then exits. This is useful for debugging binary delta updates
- `--appimage-signature` prints the digital signature embedded into the AppImage, then exits. This is useful for debugging binary delta updates. If you would like to validate the embedded signature, you should use the `validate` command line tool that is part of AppImageKit

### Special directories

Normally the application contained inside an AppImage will store its configuration files wherever it normally stores them (most frequently somewhere inside `$HOME`). If you invoke an AppImage built with a recent version of AppImageKit and have one of these special directories in place, then the configuration files will be stored alongside the AppImage. This can be useful for portable use cases, e.g., carrying an AppImage on a USB stick, along with its data.

- If there is a directory with the same name as the AppImage plus `.home`, then `$HOME` will automatically be set to it before executing the payload application
- If there is a directory with the same name as the AppImage plus `.config`, then `$XDG_CONFIG_HOME` will automatically be set to it before executing the payload application

Example: Imagine you want to use the Leafpad text editor, but carry its settings around with the executable. You can do the following:

```bash
# Download Leafpad AppImage and make it executable
chmod a+x Leafpad-0.8.18.1.glibc2.4-x86_64.AppImage

# Create a directory with the same name as the AppImage plus the ".config" extension
# in the same directory as the AppImage
mkdir Leafpad-0.8.18.1.glibc2.4-x86_64.AppImage.config

# Run Leafpad, change some setting (e.g., change the default font size) then close Leafpad
./Leafpad-0.8.18.1.glibc2.4-x86_64.AppImage

# Now, check where the settings were written:
linux@linux:~> find Leafpad-0.8.18.1.glibc2.4-x86_64.AppImage.config
(...)
Leafpad-0.8.18.1.glibc2.4-x86_64.AppImage.config/leafpad/leafpadrc
```

Note that the file `leafpadrc` was written in the directory we have created before.

## appimagetool

`appimagetool` is a low-level tool used to convert a valid AppDir into an AppImage. It us usually used by [higher-level tools](https://github.com/AppImageCommunity/awesome-appimage?tab=readme-ov-file#appimage-developer-tools) that can be used by application developers to provide AppImages of their applications to end users. `appimagetool` itself is not needed by end users, and is normally not used directly by developers.
Please see https://github.com/AppImage/appimagetool.

## AppImage runtime

The AppImage runtime is a small piece of code that becomes part of every AppImage. It mounts the AppImage and executes the application contained in it.
Please see https://github.com/AppImage/type2-runtime.

## AppImageSpec

The AppImageSpec defines the AppImage format.
Please see https://github.com/AppImage/AppImageSpec.
