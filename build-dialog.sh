#!/bin/bash

# needs: cmake libx11-dev libxft-dev libfontconfig1-dev

common_FLAGS="-fstack-protector -ffunction-sections -fdata-sections -D_FORTIFY_SOURCE=2"
CFLAGS="-Os $common_FLAGS"
CXXFLAGS="-Os -std=c++98 $common_FLAGS -Wno-deprecated-declarations"
LDFLAGS="-Wl,--gc-sections -Wl,--as-needed -Wl,-z,relro"
CXX="g++"
STRIP="strip"
JOBS=${JOBS:-1}

set -e
set -x

HERE="$(dirname "$(readlink -f "${0}")")"
cd "$HERE"

# Fetch git submodules
git submodule init
git submodule update

mkdir -p build

if [ ! -e "./fltk/lib/libfltk_images.a" ]; then
  cd fltk
  CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" \
  ./configure --disable-gl --enable-localzlib --enable-localpng --enable-xft \
    --disable-xinerama --disable-xdbe --disable-xfixes --disable-xcursor --disable-xrender
  make -j$JOBS DIRS='zlib png src'
  cd -
fi

if [ ! -e "./libdesktopenvironments/build/src/libdesktopenvironment.a" ]; then
  mkdir -p libdesktopenvironments/build
  cd libdesktopenvironments/build
  cmake .. -DCMAKE_BUILD_TYPE="MinSizeRel"
  make -j$JOBS
  cd -
fi

$CXX dialog.cpp -o ./build/dialog \
  -I./libdesktopenvironments/include -I. \
  $(./fltk/fltk-config --use-images --cxxflags) \
  $(./fltk/fltk-config --use-images --ldflags | sed 's|-lfltk_jpeg||g') \
  ./libdesktopenvironments/build/src/libdesktopenvironment.a
$STRIP ./build/dialog
(objdump -p ./build/dialog | grep NEEDED) || true
ldd ./build/dialog || true


