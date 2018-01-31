Section: misc
Priority: optional
Homepage: http://appimage.org/
Standards-Version: 3.9.2

Package: appimaged
Version: 1.0
Maintainer: probonopd <probonopd@users.noreply.github.com>
Depends: libarchive13, libc6 (>= 2.4), libglib2.0-0, zlib1g, fuse
# Also including dependencies of the AppImages as assumed per excludedeblist
# generated with
# BLACKLISTED_FILES=$(wget -q https://github.com/probonopd/AppImages/raw/master/excludedeblist -O - | sed '/^\s*$/d' | sed '/^#.*$/d' | cut -d "#" -f 1 | sort | uniq)
# echo $BLACKLISTED_FILES | sed -e 's| |, |g'
Recommends: apt, apt-transport-https, dbus, debconf, dictionaries-common, dpkg, fontconfig, fontconfig-config, gksu, glib-networking, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-ugly, gstreamer1.0-pulseaudio, gtk2-engines-pixbuf, gvfs-backends, kde-runtime, libasound2, libatk1.0-0, libc6, libc6-dev, libcairo2, libcups2, libdbus-1-3, libdrm2, libegl1-mesa, libfontconfig1, libgbm1, libgcc1, libgdk-pixbuf2.0-0, libgl1, libgl1-mesa, libgl1-mesa-dri, libgl1-mesa-glx, libglib2.0-0, libglu1-mesa, libgpg-error0, libgtk2.0-0, libgtk-3-0, libnss3, libpango-1.0-0, libpango1.0-0, libpangocairo-1.0-0, libpangoft2-1.0-0, libstdc++6, libtasn1-6, libwayland-egl1-mesa, libxcb1, lsb-base, mime-support, passwd, udev, uuid-runtime
Architecture: %ARCH%
Copyright: ../../LICENSE
# Changelog: <changelog file; defaults to a generic changelog>
Readme: ../../README.md
# Extra-Files: <comma-separated list of additional files for the doc directory>
Files: appimaged /usr/bin
File: /usr/lib/systemd/user/appimaged.service
 [Unit]
 Description=AppImage daemon
 After=basic.target
 
 [Service]
 ExecStart=/usr/bin/appimaged
 Restart=always
 RestartSec=30
 StartLimitIntervalSec=300
 StartLimitBurst=3
 
 [Install]
 WantedBy=graphical.target
#  <more pairs, if there's more than one file to include. Notice the starting space>
Description: Optional AppImage daemon for desktop integration
 Integrates AppImages into the desktop, e.g., installs icons and menu entries
