Copyright (c) 2004-13 Simon Peter <probono@puredarwin.org>
All rights reserved. 
Redistribution of this document is permitted only in unchanged form.
Version 2013-04-19

Building
========

Use an old system for building (at least 2-3 years old) to ensure the binaries run on older systems too.

```bash
sudo apt-get update ; sudo apt-get -y install libfuse-dev libglib2.0-dev cmake git libc6-dev binutils fuse
git clone https://github.com/probonopd/AppImageKit.git
cd AppImageKit
cmake .
make
```

Once you have built AppImageKit, try making an AppImage, e.g., of Leafpad:

    export APP=leafpad && ./apt-appdir/apt-appdir $APP && ./AppImageAssistant $APP.AppDir $APP.AppImage && ./$APP.AppImage
    
(This is just a proof-of-concept, of in reality you should use AppDirAssistant to create proper AppDirs)

TODO
====

* Include AppDirAssistant tool
* Update http://www.portablelinuxapps.org/docs/1.0/ to remove all references to elficon; include changelog section

Changelog
=========

-11
* Builds on x86-64
* Follow http://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html (Version 0.8.0 introduced new location for the thumbnails)
* Remove elficon as no one uses it; use thumbnails instead
* Include AppImageExtract tool
* Update AppImageAssistant.AppDir so that it runs on latest Ubuntu
* Use AppDir suffix
* Factor out dependency binaries to be bundled into ./bundled-dependencies-i386

-9
* Runtime extracts .DirIcon to $HOME/.thumbnails/normal/
* When called with "--icon", only extracts the icon, prints a message and exits
* Runtime sets environment variables APPIMAGE, APPDIR that the "inside" app may use
* AppRun appends to environment variables XDG_DATA_DIRS, QT_PLUGIN_PATH, PERLLIB

AppImage format definition
==========================

The AppImage format has the following properties:
* The AppImage is an ISO9660 file
* The contents of the ISO9660 file can be compressed with zf
* In the first 32k of the ISO9660 file, an ELF executable is embedded
  which mounts the AppImage, executes the application from the 
  mounted AppImage, and afterwards unmounts the AppImage again
* The ISO9660 file contains an AppDir as per the ROX AppDir specification
* The AppDir contains one desktop file as per the freedesktop.org specification

```
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+                                                                                  +
+   ISO9660                                                                        +
+                                                                                  +
+   ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++   +
+   +                              +                                           +   +
+   +                              +                AppDir                     +   +
+   +                              +                 |_ .DirIcon               +   +
+   +                              +                 |_ appname.desktop        +   +
+   +             ELF              +                 |_ AppRun                 +   +
+   +                              +             (   |_ usr/                   +   +
+   +                              +                     |_ bin/               +   +
+   +                              +                     |_ lib/               +   +
+   +                              +                     |_ share/  )          +   +
+   +                              +                                           +   +
+   ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++   +
+                                                                                  +
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
```

Creating portable AppImages
===========================

For an AppImage to run on most systems, the following conditions need to be met:
1. The AppImage needs to include all libraries and other dependencies that are not part of all of the base systems that the AppImage is intended to run on
2. The binaries contained in the AppImage need to be compiled on a system not newer than the oldest base system that the AppImage is intended to run on
3. The AppImage should actually be tested on the base systems that it is intended to run on

Libraries and other dependencies
--------------------------------

Binaries compiled on old enough base system
--------------------------------------------

The ingredients used in your AppImage should not be built on a more recent base system than the oldest base system your AppImage is intended to run on. Some core libaries, such as glibc, tend to break compatibility with older base systems quite frequently, which means that binaries will run on newer, but not on older base systems than the one the binaries were compiled on.

If you run into errors like this

    failed to initialize: /lib/tls/i686/cmov/libc.so.6: version `GLIBC_2.11' not found

then the binary is compiled on a newer system than the one you are trying to run it on. You should use a binary that has been compiled on an older system. Unfortunately, the complication is that distributions usually compile the latest versions of applications only on the latest systems, which means that you will have a hard time finding binaries of bleeding-edge softwares that run on older systems.

Testing
-------

To ensure that the AppImage runs on the intended base systems, it should be thoroughly tested on each of them. The following testing procedure is both efficient and effective: Get the previous version of Ubuntu, Fedora, and openSUSE Live CDs and test your AppImage there. Using the three largest distributions increases the chances that your AppImage will run on other distributions as well. Using the previous (current minus one) version ensures that your end users who might not have upgraded to the latest version yet can still run your AppImage. Using Live CDs has the advantage that unlike installed systems, you always have a system that is in a factory-fresh condition that can be easily reproduced. Most developers just test their software on their main working systems, which tend to be heavily customized through the installation of additional packages. By testing on Live CDs, you can be sure that end users will get the best experience possible.

I use ISOs of Live CDs, loop-mount them, chroot into them, and run the AppImage there. This way, I need approximately 700 MB per supported base system (distribution) and can easily upgrade to never versions by just exchanging one ISO file. The following script automates this for Ubuntu-like (Casper-based) and Fedora-like (Dract-based) Live ISOs:

    sudo ./AppImageAssistant.AppDir/testappimage /path/to/elementary-0.2-20110926.iso ./AppImageAssistant.AppImage

Support
=======

I support open source projects that wish to distribute their software as an AppImage. For closed source applications, I offer AppImage packaging and testing as a service.
