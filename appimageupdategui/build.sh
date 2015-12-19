#!/bin/bash

if [ "$(which apt-get)" != "" ] ; then
  add-apt-repository -y "deb http://archive.ubuntu.com/ubuntu $(lsb_release -sc) universe"
  add-apt-repository -y ppa:vala-team
  apt-get update
  apt-get -y install libgtk-3-dev valac-0.30 clang cmake libvala-0.30 git
fi

if [ "(which yum)" != "" ] ; then
  yum -y install gtk3-devel vala clang cmake vala-devel
fi

valac --pkg 'gtk+-3.0' --pkg 'gmodule-2.0' --pkg posix main.vala progress.vala -o appimageupdategui -v
strip appimageupdategui
