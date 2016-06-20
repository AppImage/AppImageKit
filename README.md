# AppImageKit [![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/probonopd/AppImageKit?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

Copyright (c) 2004-16 Simon Peter <probono@puredarwin.org> and contributors.

Using AppImageKit you can package desktop applications as AppImages that run on common Linux-based operating systems, such as RHEL, CentOS, Ubuntu, Fedora, Debian and derivatives.

The __AppImage__ format is a format for packaging applications in a way that allows them to
run on a variety of different target systems (base operating systems, distributions) without further modification. 

__AppImageKit__  is  a  concrete  implementation  of  the  AppImage  format  and  provides  tools  for
conveniently handling AppImages.

https://en.wikipedia.org/wiki/AppImage

This document describes the AppImage format and AppImageKit. It is intended to describe the philosophy behind the AppImage format and the concrete implementation. This document is not a formal specification, since the AppImage format is not frozen yet but in the process of being specified more formally at https://github.com/AppImage/AppImageSpec. Contributors are encouraged to comment.

Significant upstream projects have started providing AppImages of releases and/or nightly/continuous builds, see this [list of upstream-provided AppImages](https://github.com/probonopd/AppImageKit/wiki/AppImages#upstream-appimages).

## Motivation

### Use cases

Here are some concrete use cases for the AppImage format and the AppImageKit tools to create AppImages:

 * "As a user, I want to go to an upstream download page, download an application from the original author, and run it on my Linux desktop system just like I would do with a Windows or Mac application."
 * "As a tester, I want to be able to get the latest bleeding-edge version of an application from a continuous build server and test it on my system, without needing to compile and without having to worry that I might mess up my system."
 * "As an application author or ISV, I want to provide packages for Linux desktop systems just as I do for Windows and OS X, without the need to get it 'into' a distribution and without having to build for gazillions of different distributions."

### The problem space

Historically, UNIX and Linux systems have made it easy to procure source code, however they have made it comparably difficult to use ready-made software in binary form. In contrast, other systems, most prominently Windows and the classic Macintosh operating system, have made it relatively simple for independent software publishers (ISPs) to distribute software and for end-users to procure and install software from said ISPs, without any instances (such as distributors or App Stores) between the two parties.


Package managers were introduced to mitigate the complexities of dealing with source code by providing libraries of precompiled packages from repositories maintained by distributors or third parties. However, the introduction of package managers did not drastically reduce complexities or provide robustness - they merely stacked a management layer on top of an already complex system, effectively preventing the user from manipulating installed software directly.

With the introduction of Mac OS X, arguably the first UNIX-based operating system with widespread mass adoption to a non-technical user base, Apple blended traditional UNIX aspects (such as maintaining a traditional filesystem hierarchy, including `/bin`, `/usr`, `/lib` directories) with common "desktop" approaches (such as "installing" an application by dragging it to the hard disk). While Apple _does_ use a package manager-like approach for managing the base operating system and its updates, it does _not_ do so for the user applications.

Open Source operating systems, such as the most prominent Linux distributions, mostly use package managers for everything. While this is perceived superior to Windows and the Mac by many Linux enthusiasts, it also creates a number of disadvantages:

 1. __Centralization__ Some organization decides what is "in" a distribution and what is not. By definition, software "in" a distribution is easier to install and manage than software that is not.
 2. __Duplication of effort__ In traditional systems, each application is compiled specifically for each target operating system. This means that one piece of software has to be compiled many, many times on many, many systems using much, much power and time
 3. __Need to be online__ Most package managers are created with connected computers in the mind, making it really cumbersome to "just fetch an app" on an online system, and copy it over to another system that is not connected to the Internet.
 
A critical distinction between the approach known from Windows and the Mac and the one known from UNIX and Linux is the "platform": While Windows and the Mac are seen as platforms to run software on, most Linux distributions see themselves as the system that includes the applications.

While this leads to a number of advantages that have been frequently reiterated, it also poses a significant number of challenges:

 1. __No recent apps on mature operating systems__. In most distributions, you get only the version that was recent at the time when the distribution was created. For example, if you use Ubuntu Gutsy then you are stuck forever with the software that was recent at the time when Ubuntu Gutsy was compiled. Even if Firefox might have progressed by several versions in the meantime, you cannot get more recent apps than what was available back when the distribution was put together. That is like if you'd get only software from 2001 when you use Windows XP. In the traditional model, the user has to decide: Either use a mature base operating system but be locked out of recent apps (e.g., using Ubuntu LTS), or be forced to update the base operating system to the latest bleeding edge version in order to get the recent apps (e.g., using Debian sid). This situation is clearly not optimal, since the common desktop user would prefer to hardly touch the base operating system (maybe update it every other year or so) but always get the latest apps.
 2. __No simple way to use multiple versions in parallel__. Most package managers do not allow you to have more than one version of an app installed in parallel. Hence you have no way to simply try out the latest version of an app without running the risk that it might not be easy to switch back to the older version, especially if the older version is no longer available in your distribution (e.g, old versions get removed from Debian sid as soon as a newer version appears). This is especially annoying if you would simply like to try out a few things before you decide whether to use the old or the new version.
 3. __Not easy to move an app from one machine to another__. If you've used an app on one machine and decide that you would like to use the same app either under a different base operating system (say, you want to use OpenOffice on Fedora after having used it on Ubuntu) or if you would simply take the app from one machine to another (say from the desktop computer to the netbook), you have to download and install the app again (if you did not keep around the installation files and if the two operating systems don't share the exact same package format - both of which is rather unlikely).

Linus Torvalds addresses some core issues that inhibit Linux from truly conquering the desktop in his [DebConf 14_ QA with Linus Torvalds talk](https://www.youtube.com/watch?v=5PmHRSeA2c8). At 05:40 Linus highlights application packaging: 

> I'm talking about actual application writers that want to make a package of their application for Linux. And I've seen this firsthand with the other project I've been involved with, which is my divelog application. We make binaries for Windows and OS X. We basically don't make binaries for Linux. Why? Because binaries for Linux desktop applications is a major f*ing pain in the ass. Right. You don't make binaries for Linux. You make binaries for Fedora 19, Fedora 20, maybe there's even like RHEL 5 from ten years ago, you make binaries for debian stable, or actually you don't make binaries for debian stable because debian stable has libraries that are so old that anything that was built in the last century doesn't work. But you might make binaries for debian... whatever the codename is for unstable. And even that is a major pain because (...) debian has those rules that you are supposed to use shared libraries.

More on this [here](https://github.com/probonopd/AppImages/blob/master/README.md).

AppImage to the rescue. 

While here are more [sophisticated](http://0pointer.net/blog/revisiting-how-we-put-together-linux-systems.html) [approaches](https://wiki.gnome.org/Projects/SandboxedApps) in the works, in the meantime here is something reasonably simple, universal, and robust that works today.

### Objectives

The AppImage format has been created with specific objectives in mind.

 1. __Be Simple__. AppImage is intended to be a very simple format that is easy to understand, create, and manage.
 2. __Maintain binary compatibility__. AppImage is a format for binary software distribution. Software packaged as AppImage is intended to be as binary-compatible as possible with as many systems as possible. The need for (re-)compilation of software should be greatly reduced.
 3. __Be distribution-agostic__. An AppImage should run on all base operating systems (distributions) that it was created for (and later versions). For example, you could target Ubuntu 9.10, openSUSE 11.2, and Fedora 13 (and later versions) at the same time, without having to create and maintain separate packages for each target system.
 4. __Remove the need for installation__. AppImages contain the app in a format that allows it to run directly from the archive, without having to be installed first. This is comparable to a Live CD. Before Live CDs, operating systems had to be installed first before they could be used.
 5. __Keep apps compressed all the time__. Since the application remains packaged all the time, it is never uncompressed on the hard disk. The computer uncompresses the application on-the-fly while accessing it. Since decompression is faster than reading from hard disk on most systems, this has a speed advantage in addition to saving space. Also, the time needed for installation is entirely removed.
 6. __Allow to put apps anywhere__. AppImages are "relocatable", thus allowing the user to store and execute them from any location (including CD-ROMs, DVDs, removable disks, USB sticks).
 7. __Make applications read-only__. Since AppImages are read-only by design, the user can be reasonably sure that an app does not modify itself during operation.
 8. __Do not require recompilation__. AppImages must be possible to create from already-existing binaries, without the need for recompilation. This greatly speeds up the AppImage creation process, since no compiler has to be involved. This also allows third parties to package closed-source applications as AppImages. (Nevertheless, it can be beneficial for upstream application developers to build from source specifically for the purpose of generating an AppImage.)
 9. __Keep base operating system untouched__. Since AppImages are intended to run on plain systems that have not been specially prepared by an administrator, AppImages may not require any unusual preparation of the base operating system. Hence, they cannot rely on special kernel patches, kernel modules, or any applications that do not come with the targeted distributions by default.
 10. __Do not require root__. Since AppImages are intended to be run by end users, they should not reqiure an administrative account (root) to be installed or used. They may, however, be installed by an administrator (e.g., in multi-user scenarios) if so desired.

The key idea of the AppImage format is __one app = one file__. Every AppImage contains an app and all the files the app needs to run. In other words, each AppImage has no dependencies other than what is included in the targeted base operating system(s). While it would theoretically be possible to create rpm or deb packages in the same way, it is hardly ever done. In contrast, doing so is strongly encouraged when dealing with AppImages and is the default use case of the AppImage format.

In short: An AppImage is for an app what a Live CD is for an operating system.

## AppImage Format overview

An AppImage is an ISO 9660 file with zisofs compression containing a minimal AppDir (a directory that contains the app and all the files that it requires to run which are not part of the targeted base operating systems) and a tiny runtime executable embedded into its header. Hence, an AppImage is both an ISO 9660 file (that you can mount and examine) and an ELF executable (that you can execute).

When you execute an AppImage, the tiny embedded runtime mounts the ISO file, and executes the app contained therein.

A minimal AppImage could potentially look like this:

```
----------------------------------------------------------
|            |                                           |
| ELF        | ISO9660 zisofs compressed data containing |
| embedded   | AppRun                                    |
| in ISO9660 | .DirIcon                                  |
| header     | SomeAppFile                               |
|            |                                           |
---------------------------------------------------------
```

 * AppRun is the binary that is executed when the AppImage is run
 * .DirIcon contains a svg icon that is used for the AppImage
 * SomeAppFile could be some random file that the app requires to run

However, in order to allow for automated generation, processing, and richer metadata, the AppImage format follows a somewhat more elaborate convention:

```
----------------------------------------------------------
|            |                                           |
| ELF        | ISO9660 zisofs compressed data containing |
| embedded   | AppRun                                    |
| in ISO9660 | appname.desktop                           |
| header     | usr/bin/appname                           |
|            | usr/lib/libname.so.0                      |
|            | usr/share/icons/.../iconname.svg          |
|            | usr/share/appname/somehelperfile          |
|            | .DirIcon                                  |
|            |                                           |
---------------------------------------------------------
```

 * The `runtime` ELF embedded in the ISO9660 header always executes the file called `AppRun` which is stored inside the ISO9660 file.
 * The file `AppRun` inside the ISO9660 file is not the actual executable, but instead a tiny helper binary that finds and exectues the actual app. Generic `AppRun` files have been implemented in bash and C as parts of AppImageKit. The C version, `AppRun.c`, is generally preferred as it is faster and more portable.
 * AppRun usually does not contain hardcoded information about the app, but instead retrieves it from the file `appname.desktop` that follows the Desktop File Specification.

A minimal appname.desktop file that would be sufficient for AppImage would need to contain

```
[Desktop Entry]
Name=AppName
Exec=appname
Icon=iconname
```
This desktop file would tell the `AppRun` executable to run the executable called `appname`, and would specify `AppName` as the name for the AppImage, and `iconname.png` as its icon.

However, it does not hurt if the desktop file contains additional information. Should it be desired to provide additional metadata with an AppImage, the desktop file could be extended with `X-AppImage-...` fields as per the Desktop File Specification. Usually, desktop files provided in deb or rpm archives are suitable to be used in AppImages. However, absolute paths in the `Exec=` statement are not supported by the AppImage format.

The AppImage contains the usual `usr/` hierarchy (following the File Hierarchy Standard). In the concrete example from the desktop file above, the AppRun executable would look for `usr/bin/appname` and would execute that. Also, AppImageKitAssistant (a tool used to create AppImages easily) would look for an icon and use that as the `.DirIcon` file, effectively making it the icon of the AppImage.

The app must be programmed in a way that allows for relocation. In other words, the app must not have hardcoded paths such as `/usr/bin`, `/usr/share`, `/etc` inside the binary. Instead, it must use relative paths, such as `./bin`. 

Since most binaries contained in deb and rpm archives generally are not created in a way that allows for relocation, they need to be either changed and recompiled (e.g., using the [binreloc](https://github.com/ximion/binreloc) framework), or the binaries need to be patched. As recompiling is not convenient in most cases, AppRun changes to the `usr/` directory prior to executing the app, enabling the app to specify all paths relative to the AppImage's `usr/` directory. This allows one to use patched binaries (where the string `/usr` has been replaced with the same-length string `././`, which means "current directory"). Note that if you use the `././` patch, then your app must not use chdir, or otherwise it will break.

Note that the AppImage format has been conceived to facilitate the conversion of deb and rpm packages into the AppImage format with minimal manual effort. Hence, it contains some conventions in addition to those specified by the AppDir format, to which it is compatible to the extent that an unpacked AppImage can be used as an AppDir with the ROX Filer.

## AppImageKit

The AppImage format is complemented by a suite of tools called AppImageKit that provide a concrete sample implementation of the ideas expressed in the format, and that can greatly simplify dealing with AppImages.

Currently the AppImageKit contains (among others): 
 * __AppImageAssistant__, a CLI and GUI app that turns an AppDir into an AppImage.
 * __AppRun__, the executable that finds and executes the app contained in the AppImage.
 * __runtime__, the tiny ELF binary that is embedded into the header of each AppImage. AppImageKit automatically embeds the runtime into the AppImages it creates.
 * __apt-appdir__, a command line tool running on Ubuntu that turns packaged software into AppDirs. This tool can be used to semi-automatically prepare AppDirs that can be used as the input for AppImageAssistant. Note that while apt-appdir has been written for Ubuntu, it should also run on Debian and could be ported to other distributions as well, then using the respective package managers instead of apt-get.

AppImageKit also contains additional tools and helpers.

## Building AppImageKit

Latest prebuilt packages can be downloaded from [here.](https://github.com/probonopd/AppImageKit/releases)

**Building From Source:**

Use an old system for building (at least 2-3 years old) to ensure the binaries run on older systems too. (Or use LibcWrapGenerator to ensure the build products run on older glibc versions; but to get started it might be the easiest to use a not too recent build system.)

```bash
sudo apt-get update ; sudo apt-get -y install libfuse-dev libglib2.0-dev cmake git libc6-dev binutils fuse # debian, Ubuntu
sudo yum install git cmake binutils fuse glibc-devel glib2-devel fuse-devel gcc zlib-devel libpng12 # Fedora, RHEL, CentOS
git clone https://github.com/probonopd/AppImageKit.git
cd AppImageKit
cmake .
make
```

Once you have built AppImageKit, try making an AppImage, e.g., of Leafpad. The following has been tested on Ubuntu:

    export APP=leafpad && ./apt-appdir/apt-appdir $APP && ./AppImageAssistant.AppDir/package $APP.AppDir $APP.AppImage && ./$APP.AppImage
    
(This is just a proof-of-concept, in reality you should use a proper "recipe" script or AppDirAssistant to create proper AppDirs in order to ensure binary compatibility of your AppImages. See the [Wiki](https://github.com/probonopd/AppImageKit/wiki) for details and for examples on how to bundle real-world software such as LibreOffice, Google Chrome, and others as AppImages.)

## Creating AppImages

The general workflow for creating an AppImage involves the following steps:
 1. __Gather suitable binaries__. If the application has already been compiled, you can use existing binaries (for example, contained in .tar.gz, deb, or rpm archives). Note that the binaries must not be compiled on newer distributions than the ones you are targeting. In other words, if you are targeting Ubuntu 9.10, you should not use binaries compiled on Ubuntu 10.04. For upstream projects, it might be advantageous to compile special builds for use in AppImages, although this is not required.
 2. __Gather suitable binaries of all dependencies__ that are not part of the base operating systems you are targeting. For example, if you are targeting Ubuntu, Fedora, and openSUSE, then you need to gather all libraries and other dependencies that your app requires to run that are not part of Ubuntu, Fedora, and openSUSE.
 3. __Create a working AppDir__ from your binaries. A working AppImage runs your app when you execute its AppRun file.
 4. __Turn your AppDir into an AppImage__. This compresses the contents of your AppDir into a single, self-mounting and self-executable file.
 5. __Test your AppImage__ on all base operating systems you are targeting. This is an important step which you should not skip. Subtle differences in distributions make this a must. While it is possible in most cases to create AppImages that run on various distributions, this does not come automatically, but requires careful hand-tuning.

While it would theoretically be possible to do all these steps by hand, AppImageKit contains tools that greatly simplify the tasks.

See the [Wiki](https://github.com/probonopd/AppImageKit/wiki) for details and for examples on how to bundle real-world software such as LibreOffice, Google Chrome, and others as AppImages.

## Updates

AppImages can be updated using [AppImageUpdate](https://github.com/probonopd/AppImageKit/blob/master/AppImageUpdate.AppDir/README.md). AppImageUpdate lets you update AppImages in a decentral way using information embedded in the AppImage itself. No central repository is involved. This enables upstream application projects to release AppImages that can be updated easily. Since AppImageKit uses delta updates, the downloads are very small and efficient.

For the full story, read [this](https://github.com/probonopd/AppImageKit/blob/master/AppImageUpdate.AppDir/README.md).

## Sandboxing

[Firejail](https://github.com/netblue30/firejail/) is a low-overhead sandbox that provides native support for the AppImage format. Written in C with virtually no dependencies, Firejail runs on any Linux computer with a 3.x kernel version or newer. 

To run an AppImage inside a Firejail sandbox, run it like this:

```
$ firejail --appimage Krita-3.0-x86_64.AppImage
```

or with some basic X11 and network sandboxing:

```
$ firejail --appimage --net=none --x11 Krita-3.0-x86_64.AppImage
```

It is also possible to use the [Bubblewrap](https://github.com/probonopd/AppImageKit/blob/master/sandbox/README.md) sandbox to run AppImages.

## Support

I support open source projects that wish to distribute their software as an AppImage. For closed source applications, I offer AppImage packaging and testing as a service.

## Contributing

You are invited to contribute to the AppImage format, the AppImageKit tools, and the PortableLinuxApps website (which is a showcase of AppImage).

The preferred channel of communication is https://github.com/probonopd/AppImageKit - please file [Issues](https://github.com/probonopd/AppImageKit/issues) (also for wishlist items or discussion topics) or submit [Pull requests](https://github.com/probonopd/AppImageKit/pulls).

Instead of an IRC channel we use [Gitter](https://gitter.im/probonopd/AppImageKit) which has the advantage that one does not have to be online all the time and one can communicate despite not being in the same timezone.

## Acknowledgments

This work stands on the shoulders of giants. The following persons and organizations should specifically be thanked (in no particular order), even though this list can never be exhaustive:
 * Apple Inc. for popularizing the notion of application bundles (even though others have used this concept before). AppImages would not be understood by people as easily if it wasn't for Apple's .app bundles and .dmg disk images.
 * The contributors of the ROX project, which introduced the AppDir format that the AppImage format is conceptually based on. AppImages improve on ROX AppDirs in that they encapsulate the AppDirs in a compressed container file which adds robustness and ease of administration.
 * The contributors of the klik project, which AppImageKit is conceptually based on (with the principal author of AppImageKit being the founder of the klik project). AppImages improve on klik in that they need no runtime to be installed on the base operating system before they can be used.
 * Alexander Larsson for his work on Glick, which AppImageKit is technically based on. AppImageKit improves on Glick in that it uses a compressed filesystem and in that it provides additional tools which simplify creating AppImages.
 * The contributors of the Python project, which gives developers a powerful tool to turn ideas into reality rapidly. AppImageKit would have been much more cumbersome to create if Python would not exist.
