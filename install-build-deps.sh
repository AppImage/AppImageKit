#!/bin/bash

set -e

# Install build dependencies; TODO: Support systems that do not use apt-get (Pull Requests welcome!)

ARCH=$(uname -m)
if [ "$ARCH" == "i686" ]; then
  ARCH=i386
fi


# Install dependencies for openSUSE
if [ -e /usr/bin/zypper ] ; then
    sudo zypper refresh
    sudo zypper in -y build git-core gcc g++ wget make glibc-devel glib2-devel libarchive-devel \
        fuse fuse-devel zlib-devel patch cairo-devel zsync desktop-file-utils cmake
    #for some reason openSUSE Tumbleweed have apt-get.
    return
fi

if [ -e /usr/bin/apt-get ] ; then
  sudo apt-get update
  sudo apt-get -y install zsync git libarchive-dev autoconf libtool make gcc g++ libtool libfuse-dev \
  liblzma-dev libglib2.0-dev libssl-dev libinotifytools0-dev liblz4-dev equivs libcairo-dev desktop-file-utils cmake
  # libtool-bin might be required in newer distributions but is not available in precise
  sudo cp resources/liblz4.pc /usr/lib/$ARCH-linux-gnu/pkgconfig/
  if cat /etc/lsb-release | grep 14.04 2>&1 >/dev/null; then
    export CMAKE_VERSION=3.10.0
    # sometimes, using a crowbar is easier than fiddling with PPAs
    wget https://cmake.org/files/v3.10/cmake-"$CMAKE_VERSION"-Linux-x86_64.tar.gz -O- | tar xz --strip-components=1 -C /usr/local
  fi
fi

if [ -e /usr/bin/yum ] ; then
  # Install and enable EPEL and Devtoolset-4 by Software Collections
  # https://www.softwarecollections.org/en/scls/rhscl/devtoolset-4/
  if [ "$ARCH" == "x86_64" ]; then
    yum -y install centos-release-scl-rh epel-release
    yum -y install devtoolset-4-gcc.$ARCH devtoolset-4-gcc-c++.$ARCH
  fi

  # Install and enable Autotools by Pavel Raiskup
  # https://www.softwarecollections.org/en/scls/praiskup/autotools/
  rpm -ivh https://www.softwarecollections.org/en/scls/praiskup/autotools/epel-6-$ARCH/download/praiskup-autotools-epel-6-$ARCH.noarch.rpm
  yum -y install autotools-latest # 19 MB

  if [ "$ARCH" == "x86_64" ]; then
    rpm -ivh https://github.com/kikitux/blog/raw/master/zsync/zsync-0.6.2-1.el6.rf.x86_64.rpm
  fi
  if [ "$ARCH" == "i386" ]; then
    rpm -ivh https://github.com/kikitux/blog/raw/master/zsync/zsync-0.6.2-1.el6.rf.i686.rpm
  fi

  yum -y install epel-release
  yum -y install git wget make binutils fuse glibc-devel glib2-devel libarchive3-devel fuse-devel zlib-devel patch openssl-static openssl-devel vim-common cairo-devel desktop-file-utils libtool # inotify-tools-devel lz4-devel

  export AUTOMAKE_VERSION=1.15 AUTOCONF_VERSION=2.69 CMAKE_VERSION=3.10.0
  wget https://raw.githubusercontent.com/AppImage/AppImageBuild/master/build-automake.sh -O- | bash
  wget https://raw.githubusercontent.com/AppImage/AppImageBuild/master/build-autoconf.sh -O- | bash
  wget https://cmake.org/files/v3.10/cmake-"$CMAKE_VERSION"-Linux-x86_64.tar.gz -O- | tar xz --strip-components=1 -C /usr
fi

# Install dependencies for Arch Linux
if [ -e /usr/bin/pacman ] ; then
  echo "Checking arch package provides and installed packages" "cmake"
  declare -a arr=("zsync" "git" "libarchive" "autoconf" "libtool" "make"
    "libtool" "fuse2" "xz" "glib2" "openssl" "inotify-tools" "lz4" "gcc" "g++")
  for i in "${arr[@]}"
  do
      if [ ! "$(package-query -Q $i || package-query --qprovides $i -Q)" ]; then
          TO_INSTALL="$TO_INSTALL $i"
      fi
  done
  if [ "$TO_INSTALL" ]; then
      echo "Found the following missing packages:$TO_INSTALL, installing now"
      sudo pacman -S --needed $TO_INSTALL
  fi
fi
