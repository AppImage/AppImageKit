# AppImageKit [![Build Status](https://travis-ci.org/AppImage/AppImageKit.svg?branch=appimagetool/master)](https://travis-ci.org/AppImage/AppImageKit) ![Downloads](https://img.shields.io/github/downloads/AppImage/AppImageKit/total.svg) [![Codacy Badge](https://api.codacy.com/project/badge/Grade/ff60338a447941afb17c30f777314bee)](https://www.codacy.com/app/probonopd/AppImageKit?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=AppImage/AppImageKit&amp;utm_campaign=Badge_Grade) [![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/probonopd/AppImageKit?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge) [![irc](https://img.shields.io/badge/IRC-%23AppImage%20on%20freenode-blue.svg)](https://webchat.freenode.net/?channels=AppImage) [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=ZT9CL8M5TJU72)

The __AppImage__ format is a format for packaging applications in a way that allows them to
run on a variety of different target systems (base operating systems, distributions) without further modification. 

![appimagekitr](https://user-images.githubusercontent.com/2480569/34471155-5b53131a-ef41-11e7-90a0-8c57b3c21551.png)

Using AppImageKit you can package desktop applications as AppImages that run on common Linux-based operating systems, such as RHEL, CentOS, Ubuntu, Fedora, Debian and derivatives.

Copyright (c) 2004-19 Simon Peter <probono@puredarwin.org> and contributors.

https://en.wikipedia.org/wiki/AppImage

__AppImageKit__  is  a  concrete  implementation  of  the  AppImage  format  and  provides  tools such as `appimagetool` and `appimaged` for conveniently handling AppImages.

`appimagetool` converts an AppDir into a self-mounting filesystem image. `appimaged` is a daemon that handles registering and unregistering AppImages with the system (e.g., menu entries, icons, MIME types, binary delta updates, and such).

Providing an [AppImage](http://appimage.org/) for distributing application has, among others, these advantages:
- Applications packaged as an AppImage can run on many distributions (including Ubuntu, Fedora, openSUSE, CentOS, elementaryOS, Linux Mint, and others)
- One app = one file = super simple for users: just download one AppImage file, [make it executable](http://discourse.appimage.org/t/how-to-make-an-appimage-executable/80), and run
- No unpacking or installation necessary
- No root needed
- No system libraries changed
- Works out of the box, no installation of runtimes needed
- Optional desktop integration with [appimaged](https://github.com/AppImage/appimaged)
- Optional binary delta updates, e.g., for continuous builds (only download the binary diff) using AppImageUpdate
- Can optionally GPG2-sign your AppImages (inside the file)
- Works on Live ISOs
- Can use the same AppImages when dual-booting multiple distributions
- Can be listed in the [AppImageHub](https://appimage.github.io/apps) central directory of available AppImages
- Can double as a self-extracting compressed archive with the `--appimage-extract` parameter

[Here is an overview](https://appimage.github.io/apps) of projects that are already distributing upstream-provided, official AppImages.

If you have questions, AppImage developers are on #AppImage on irc.freenode.net.

## Motivation

Linus addresses some core issues of Linux on the desktop in his [DebConf 14_ QA with Linus Torvalds talk](https://www.youtube.com/watch?v=5PmHRSeA2c8). At 05:40 Linus highlights application packaging: 

> I'm talking about actual application writers that want to make a package of their application for Linux. And I've seen this firsthand with the other project I've been involved with, which is my divelog application.

Obviously Linus is talking about [Subsurface](https://subsurface-divelog.org). 

> We make binaries for Windows and OS X. 

Both bundle not only the application itself, but also the required Qt libraries that the application needs to run. Also included are dependency libraries like `libssh2.1.dylib`and `libzip.2.dylib`.

> We basically don't make binaries for Linux. Why? Because binaries for Linux desktop applications is a major f*ing pain in the ass. Right. You don't make binaries for Linux. You make binaries for Fedora 19, Fedora 20, maybe there's even like RHEL 5 from ten years ago, you make binaries for debian stable.

So why not use the same approach as on Windows and OS X, namely, treat the base operating system as a _platform_ on top of which we run the application we care about. This means that we have to bundle the application with all their dependencies that are _not_ part of the base operating system. Welcome [application bundles](https://blogs.gnome.org/tvb/2013/12/10/application-bundles-for-glade/).

> Or actually you don't make binaries for debian stable because debian stable has libraries that are so old that anything that was built in the last century doesn't work. But you might make binaries for debian... whatever the codename is for unstable. And even that is a major pain because (...) debian has those rules that you are supposed to use shared libraries. Right. 

This is why binaries going into an AppImage should be built against the oldest still-supported LTS or Enterprise distributions.

> And if you don't use shared libraries, getting your package in, like, is just painful. 

"Getting your package in" means that the distribution accepts the package as part of the base operating system. For an application, that might not be desired at all. As long as we can package the application in a way that it seamlessly runs on top of the base operating system. 

> But using shared libraries is not an option when the libraries are experimental and the libraries are used by two people and one of them is crazy, so every other day some ABI breaks. 

One simple way to achieve this is to bundle private copies of the libraries in question with the application that uses them. Preferably in a way that does not interfere with anything else that is running on the base operating system. Note that this does not have to be true for all libraries; core libraries that are matured, have stable interfaces and can reasonably expected to be present in all distributions do not necessarily have to be bundled with the application.

> So you actually want to just compile one binary and have it work. Preferably forever. And preferably across all Linux distributions. 

That is actually possible, as long as you stay away from any distribution-specific packaging, and as long as you do not use a too recent build system. The same will probably be true for Windows and OS X - if you compile on OS X 10.11 then I would not expect the resulting build products to run on OS X 10.5.

> And I actually think distributions have done a horribly, horribly bad job. 

Distributions are all about building the base operating system. But I don't think distributions are a good way to get applications. Rather, I would prefer to get the latest versions of applications directly from the people who write them. And this is already a reality for software like Google Chrome, Eclipse, Arduino and other applications. Who uses the (mostly outdated and changed) versions that are part of the distributions? Probably most people don't.

> One of the things that I do on the kernel - and I have to fight this every single release and I think it's sad - we have one rule in the kernel, one rule: we don't break userspace. (...) People break userspace, I get really, really angry. (...) 

Excellent. Thank you for this policy! This is why I can still run the Mosaic browser from over a decade ago on modern Linux-based operating systems. (I tried and it works.)

> And then all the distributions come in and they screw it all up. Because they break binary compatibility left and right. 

Luckily, binaries built on older distributions tend to still work on newer distributions. At least that has been my experience over the last decade with building application bundles using AppImageKit, and before that, klik.

> They update glibc and everything breaks. (...) 

There is a [way around this](https://blogs.gnome.org/tvb/2013/12/14/application-bundles-revisited/), although not many people actually care to use the workaround (yet).

> So that's my rant. And that's what I really fundamentally think needs to change for Linux to work on the desktop because you can't have applications writers to do fifteen billion different versions.

AppImage to the rescue!

## AppImage usage

Running an AppImage mounts the filesystem image and transparently runs the contained application. So the usage of an AppImage normally should equal the usage of the application contained in it. However, there is special functionality, as described here. If an AppImage you have received does not support these options, ask the author of the AppImage to recreate it using the latest `appimagetool` (or `linuxdeployqt`).

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
wget -c "https://bintray.com/probono/AppImages/download_file?file_path=Leafpad-0.8.18.1.glibc2.4-x86_64.AppImage" -O Leafpad-0.8.18.1.glibc2.4-x86_64.AppImage
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

## appimagetool usage

`appimagetool` is used to generate an AppImage from an existing `AppDir`. Higher-level tools such as [`linuxdeployqt`](https://github.com/probonopd/linuxdeployqt) use it internally. A precompiled version can be found on [GitHub Releases](https://github.com/AppImage/AppImageKit/releases).

```
wget "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage
```

Usage in a nutshell, assuming that you already have an [AppDir](https://github.com/AppImage/AppImageSpec/blob/master/draft.md#appdir) in place:

```
./appimagetool-x86_64.AppImage some.AppDir
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
  -g, --guess                 Guess update information based on Travis CI or GitLab environment variables
  --bintray-user              Bintray user name
  --bintray-repo              Bintray repository
  --version                   Show version number
  -v, --verbose               Produce verbose output
  -s, --sign                  Sign with gpg[2]
  --comp                      Squashfs compression
  -n, --no-appstream          Do not check AppStream metadata
  --exclude-file              Uses given file as exclude file for mksquashfs, in addition to .appimageignore.
  --runtime-file              Runtime file to use
  --sign-key                  Key ID to use for gpg[2] signatures
  --sign-args                 Extra arguments to use when signing with gpg[2]
```

If you want to generate an AppImage manually, you can:

```
mksquashfs Your.AppDir Your.squashfs -root-owned -noappend
cat runtime >> Your.AppImage
cat Your.squashfs >> Your.AppImage
chmod a+x Your.AppImage
```


## Building

__NOTE:__ The AppImage project supplies [binaries](https://github.com/AppImage/AppImageKit/releases/tag/continuous) that application developers can use. These binaries are built using the CentOS 6 Docker on Travis CI build system in this repository. As an application developer, you do not have to use the build system. You only have to use the build systems when contributing to AppImageKit, when needing another architecture than `x86_64`, or when trying to reproduce our binaries.

Our build system is based on Docker. To build your own binaries, please install Docker first. Then, follow the following steps:

```
git clone --single-branch --recursive https://github.com/AppImage/AppImageKit
cd AppImageKit/
bash ci/build.sh
```

This will create the binaries in a directory called `out/`.

Please note: It is not recommended nor supported to build AppImageKit on any newer build system than the oldest still-supported versions of major distributions for reasons outlined [here](https://github.com/AppImage/AppImageKit/wiki/Creating-AppImages#creating-appimages-that-are-compatible-with-many-systems). Currently we are targeting CentOS 6.x and Ubuntu 14.04 as build systems and we are not interested to build AppImageKit on newer versions anytime soon. Binaries built on those systems will run just fine on newer (later) target systems (distributions).
