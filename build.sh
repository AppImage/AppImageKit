#!/bin/bash

#
# This script installs the required build-time dependencies
# and builds AppImage
#

HERE="$(dirname "$(readlink -f "${0}")")"

if [ -e /usr/bin/apt-get ] ; then
  sudo apt-get update
  sudo apt-get -y install libfuse-dev libglib2.0-dev cmake git libc6-dev binutils fuse

fi

if [ -e /usr/bin/yum ] ; then
  sudo yum -y install git cmake binutils fuse glibc-devel glib2-devel fuse-devel gcc zlib-devel libpng12

fi

cmake .
make clean
make
