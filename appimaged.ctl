Section: misc
Priority: optional
Homepage: http://appimage.org/
Standards-Version: 3.9.2

Package: appimaged
Version: 1.0
Maintainer: probonopd <probonopd@users.noreply.github.com>
Depends: libarchive13, libc6 (>= 2.4), libglib2.0-0, zlib1g
Architecture: %ARCH%
Copyright: ../LICENSE
# Changelog: <changelog file; defaults to a generic changelog>
Readme: ../README.md
# Extra-Files: <comma-separated list of additional files for the doc directory>
Files: appimaged /usr/bin
File: /usr/lib/systemd/user/appimaged.service
 [Unit]
 Description=AppImage daemon
 After=basic.target
 
 [Service]
 ExecStart=/usr/bin/appimaged
 Restart=always
 RestartSec=5s
 StartLimitInterval=0
 
 [Install]
 WantedBy=graphical.target
#  <more pairs, if there's more than one file to include. Notice the starting space>
Description: Optional AppImage daemon for desktop integration
 Integrates AppImages into the desktop, e.g., installs icons and menu entries
