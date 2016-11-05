# AppImageKit [![discourse](https://img.shields.io/badge/forum-discourse-orange.svg)](http://discourse.appimage.org) [![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/probonopd/AppImageKit?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge) [![irc](https://img.shields.io/badge/IRC-%23AppImage%20on%20freenode-blue.svg)](https://webchat.freenode.net/?channels=AppImage) [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=ZT9CL8M5TJU72)

Copyright (c) 2004-16 Simon Peter <probono@puredarwin.org> and contributors.

Using AppImageKit you can package desktop applications as AppImages that run on common Linux-based operating systems, such as RHEL, CentOS, Ubuntu, Fedora, Debian and derivatives.

The __AppImage__ format is a format for packaging applications in a way that allows them to
run on a variety of different target systems (base operating systems, distributions) without further modification. 

__AppImageKit__  is  a  concrete  implementation  of  the  AppImage  format  and  provides  tools  for
conveniently handling AppImages.

https://en.wikipedia.org/wiki/AppImage
`appimagetool` uses a next-generation AppImage format based on squashfs and embeds a runtime for it. `appimaged` is a daemon that handles registering and unregistering AppImages with the system (e.g., menu entries, icons, MIME types, binary delta updates, and such).

## appimagetool usage

A precompiled version can be found in the last successful Travis CI build, you can get it with:

```
# Get the ID of the last successful build on Travis CI
ID=$(wget -q https://api.travis-ci.org/repos/probonopd/AppImageKit/builds -O - | head -n 1 | sed -e 's|}|\n|g' | grep '"result":0' | head -n 1 | sed -e 's|,|\n|g' | grep '"id"' | cut -d ":" -f 2)

# Get the transfer.sh URL from the logfile of the last successful build on Travis CI
URL=$(wget -q "https://s3.amazonaws.com/archive.travis-ci.org/jobs/$((ID+1))/log.txt" -O - | grep "https://transfer.sh/.*/appimagetool" | tail -n 1 | sed -e 's|\r||g')

wget "$URL"
```
Usage in a nutshell:
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
## appimaged usage

`appimaged` is an optional daemon that watches locations like `~/bin` and `~/Downloads` for AppImages and if it detects some, registers them with the system, so that they show up in the menu, have their icons show up, MIME types associated, etc. It also unregisters AppImages again from the system if they are deleted. If [firejail](https://github.com/netblue30/firejail) is installed, it runs the AppImages with it.

A precompiled version can be found in the last successful Travis CI build, you can get it with:

```
# Get the ID of the last successful build on Travis CI
ID=$(wget -q https://api.travis-ci.org/repos/probonopd/AppImageKit/builds -O - | head -n 1 | sed -e 's|}|\n|g' | grep '"result":0' | head -n 1 | sed -e 's|,|\n|g' | grep '"id"' | cut -d ":" -f 2)

# Get the transfer.sh URL from the logfile of the last successful build on Travis CI
URL=$(wget -q "https://s3.amazonaws.com/archive.travis-ci.org/jobs/$((ID+1))/log.txt" -O - | grep "https://transfer.sh/.*/appimaged" | tail -n 1 | sed -e 's|\r||g')

wget "$URL"
```
Usage in a nutshell:

```
./appimaged
```
It will register the AppImages in with your system from the following places:
* $HOME/Downloads
* $HOME/bin
* /Applications
* /isodevice/Applications
* /opt
* /usr/local/bin

Run `appimaged -v` for increased verbosity.

__NOTE:__ It may be necessary to restart (or `xkill`) dash, nautilus, to recognize new directories that didn't exist prior to the first run of `appimaged`. Alternatively, it should be sufficient to log out of the session and log in again after having run appimaged once.

If you have `AppImageUpdate` on your `$PATH`, then it can also do this neat trick:

![screenshot from 2016-10-15 16-37-05](https://cloud.githubusercontent.com/assets/2480569/19410850/0390fe9c-92f6-11e6-9882-3ca6d360a190.jpg)

Here is an easy way to get the latest AppImageUpdate onto your `$PATH`:

```
APP=AppImageUpdate
nodeFileName=$(wget -q "https://bintray.com/package/files/probono/AppImages/$APP?order=desc&sort=fileLastModified&basePath=&tab=files" -O - | grep -e '-x86_64.AppImage">' | cut -d '"' -f 6 | head -n 1)
wget -c "https://bintray.com/$nodeFileName" -O "$APP"
chmod a+x "$APP"
sudo mv "$APP" /usr/local/bin/
```

## Building

On a not too recent Ubuntu:
```
git clone --recursive https://github.com/probonopd/AppImageKit.git
cd AppImageKit/
bash -ex install-build-deps.sh
bash -ex build.sh
```

It might be necessary to rename `-larchive3` to `-larchive` and change the libarchive3 includes in `shared.c` since CentOS (which is used to compile AppImageKit on Travis CI) and Ubuntu cannot agree on library names.
