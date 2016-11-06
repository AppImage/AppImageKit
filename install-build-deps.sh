#!/bin/bash

# Install build dependencies; TODO: Support systems that do not use apt-get (Pull Requests welcome!)

if [ -e /usr/bin/apt-get ] ; then
  apt-get update
  sudo apt-get -y install zsync git libarchive-dev autoconf libtool make gcc libtool libfuse-dev \
  liblzma-dev libglib2.0-dev libssl-dev libinotifytools0-dev liblz4-dev
  # libtool-bin might be required in newer distributions but is not available in precise
  sudo cp resources/liblz4.pc /usr/lib/x86_64-linux-gnu/pkgconfig/
fi

if [ -e /usr/bin/yum ] ; then
  # Install and enable EPEL and Devtoolset-4 by Software Collections
  # https://www.softwarecollections.org/en/scls/rhscl/devtoolset-4/
  yum -y install centos-release-scl-rh epel-release
  yum -y install devtoolset-4-gcc.x86_64
  # Install and enable Autotools by Pavel Raiskup
  # https://www.softwarecollections.org/en/scls/praiskup/autotools/
  rpm -ivh https://www.softwarecollections.org/en/scls/praiskup/autotools/epel-6-x86_64/download/praiskup-autotools-epel-6-x86_64.noarch.rpm
  yum -y install autotools-latest # 19 MB

  yum -y install epel-release
  yum -y install zsync git wget make binutils fuse glibc-devel glib2-devel libarchive3-devel fuse-devel zlib-devel patch openssl-devel vim-common # inotify-tools-devel lz4-devel
  . /opt/rh/devtoolset-4/enable
  . /opt/rh/autotools-latest/enable

  # Unlike Ubuntu, CentOS does not provide .a, so we need to build it
  wget http://tukaani.org/xz/xz-5.2.2.tar.gz
  tar xzfv xz-5.2.2.tar.gz
  cd xz-5.2.2
  ./configure --enable-static && make && make install
  rm /usr/local/lib/liblzma.so* /usr/*/liblzma.so || true # Don't want the dynamic one
  cd -
fi

# Install dependencies for Arch Linux
if [ -e /usr/bin/pacman ] ; then
  echo "Please submit a pull request if you would like to see Arch Linux support."
  exit 1
fi
