mock-appdir
-------------

```mock-appdir``` is a modification of ```apt-appdir``` for RHEL/Fedora-like
systems. It uses [Mock](https://fedoraproject.org/wiki/Mock), a Fedora package
building tool, to run inside a chroot.

It does exactly the same thing as ```apt-appdir```, you run it as follows:

```
mock-appdir/mock-appdir firefox
```

The Mock chroot is first populated with the Fedora @buildsys-build and @base-x
groups, along with systemd, so that these do not get included as dependencies.

Then (for example) the Firefox package is installed, along with all of its
dependencies, and these are extracted using ```rpm2cpio``` and turned into an
AppDir.

The default chroot is currently **fedora-20-x86_64**. You can change what the
target is by setting the variable MOCK_TARGET before running, for example:

```
MOCK_TARGET=fedora-21-x86_64 mock-appdir/mock-appdir firefox
```

Note that $MOCK_TARGET is just the name of a mock config file in 
```/etc/mock```.

Ben Rosser <rosser.bjr@gmail.com>
