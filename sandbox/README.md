# Depreciation notice

This sandbox is deprecated and will be replaced by the optional `appimaged` daemon which will use [Firejail](https://github.com/netblue30/firejail/) for sandboxing.

Also, `appimaged` will be moved from its current temporary location to this repository once we think it's ready to roll. Please see http://discourse.appimage.org/t/65 for more information.

## Legacy AppImage sandbox

Demo video here: https://www.youtube.com/watch?v=7C9thHXPZd8

The AppImage sandbox is __purely optional__. It integrates AppImage into Linux desktops so that when an AppImage is double-clicked without the executable bit set, it is executed in a sandbox. When the executable bit is set on the AppImage, then it is ran outside of the sandbox. (This behavior might change in the future depending on user feedback).

### Philosophy

In order to run "untrusted" AppImages, the sandbox part cannot come inside the AppImage (because the AppImage cannot be trusted). Hence, for an effective sandboxing mechanism to work, we need to install the infrastructure for it into the base system. This violates the AppImage principle that AppImages must be able to run without any support from the base system, hence the whole sandbox part is purely optional.

You need to install a deb or rpm into your system to enable the  AppImage sandbox. While we are at it, we assume that who can install the sandbox part into the base system has root rights, which opens new possibilities.

### Implementation

Here is a very alpha [appimage-sandbox_0.1_amd64.deb](https://github.com/probonopd/AppImageKit/releases/download/5/appimage-sandbox_0.1_amd64.deb)

Right now this is basically just a [trivial, tiny shell script](https://github.com/probonopd/AppImageKit/blob/master/sandbox/src/usr/bin/runappimage) around https://github.com/projectatomic/bubblewrap and a tiny setuid helper written in C called "loopmounter".

### Building

To build .deb and .rpm packages:

```
sudo apt-get install git
git clone https://github.com/probonopd/AppImageKit.git
cd AppImageKit/sandbox/
bash package.sh 

# Install with
sudo dpkg -i appimage-sandbox_0.1_amd64.deb 
```
Note that these packages are done quick-and-dirty as long as the sandbox is not yet properly packaged by distributions.
The rpm needs some rework because the `alien` command appears to have a bug, see https://ask.fedoraproject.org/en/question/37185/file-conflict-for-installing-a-package-with-filesystem/

**Security warning:** A suid helper called "loopmounter" is installed. Please peer-review it carefully before use.
