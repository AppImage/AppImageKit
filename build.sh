#!/bin/bash

#
# This script installs the required build-time dependencies
# and builds AppImage
#

HERE="$(dirname "$(readlink -f "${0}")")"

# Figure out whether we should use sudo
SUDO=''
if (( $EUID != 0 )); then
    SUDO='sudo'
fi

if [ -e /usr/bin/apt-get ] ; then
  $SUDO apt-get update
  $SUDO apt-get -y install libfuse-dev libglib2.0-dev cmake git libc6-dev binutils fuse

fi

if [ -e /usr/bin/yum ] ; then
  $SUDO yum -y install git cmake binutils fuse glibc-devel glib2-devel fuse-devel gcc zlib-devel libpng12
fi

cd "${HERE}"
cmake .
make clean
make
cd -
