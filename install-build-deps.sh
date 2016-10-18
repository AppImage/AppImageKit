#!/bin/bash

# Install build dependencies; TODO: Support systems that do not use apt-get (Pull Requests welcome!)

if [ -e /usr/bin/apt-get ] ; then
  sudo apt-get -y install git autoconf libtool make gcc libtool libfuse-dev \
  liblzma-dev libglib2.0-dev libssl-dev libinotifytools0-dev liblz4-dev
  # libtool-bin might be required in newer distributions but is not available in precise
  sudo cp resource/liblz4.pc /usr/lib/x86_64-linux-gnu/pkgconfig/
fi
