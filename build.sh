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
  $SUDO apt-get -y install libfuse-dev libglib2.0-dev cmake git libc6-dev binutils fuse python

fi

if [ -e /usr/bin/yum ] ; then
  $SUDO yum -y install git cmake binutils fuse glibc-devel glib2-devel fuse-devel gcc zlib-devel libpng12
fi

# Install dependencies for Arch Linux
if [ -e /usr/bin/pacman ] ; then
  echo "Checking for existence of build dependencies."

  builddeps=('git' 'cmake' 'binutils' 'fuse' 'glibc' 'glib2' 'gcc' 'zlib')
  for i in "${builddeps[@]}" ; do
    pacman -Q "$i"
    if [ $? != 0 ] ; then
      $SUDO pacman -S "$i"
    else
      echo "$i is already installed."
    fi
  done
  
  echo "Checking for existence of dependencies from AUR."

  declare -a builddepsaur=('libpng12' 'ncurses5-compat-libs')
  for i in "${builddepsaur[@]}" ; do
    pacman -Q "$i"
    if [ $? != 0 ] ; then
      echo "Fetching $i from Arch Linux User Repo"
      git clone https://aur.archlinux.org/"$i".git && cd "$i" && \
      makepkg -si --skippgpcheck && \
      echo "Removing build directory." && \
      cd .. && rm -rf $i
    else
      echo "$i is already installed."
    fi
  done
fi

# Minimum required CMake is 3.1, which implements .tar.xz support
min_cmake_version=3.1
version_gt() { test "$(echo "$@" | tr " " "\n" | sort -V | head -n 1)" != "$1"; }
cmake_version=$(cmake --version | head -n1 | cut -d" " -f3)
if ! version_gt $cmake_versiaon $min_cmake_version; then
	new_cmake_version=3.4
	echo "Manually updating CMake from ${cmake_version} to ${new_cmake_version}..."
	case "$(uname -m)" in
		x86_64) cmake_arch="x86_64" ;;
		i?86) cmake_arch="i386" ;;
	esac
	if [ -n "$cmake_arch" ]; then
		pkgname="cmake-$new_cmake_version.0-Linux-${cmake_arch}.tar.gz"
		#wget --no-check-certificate "https://cmake.org/files/v$new_cmake_version/$pkgname"
		curl -O --insecure "https://cmake.org/files/v$new_cmake_version/$pkgname"
		tar -xf "$pkgname" -C /tmp/
		export PATH="/tmp/$(basename "$pkgname" .tar.gz)/bin/:$PATH"
	else
		echo "!! Could not find official CMake binaries for architecture $(uname -m)" >&2
	fi
fi

if [ "$1" == "--fetch-dependencies-only" ] ; then
  echo "Fetched dependencies.  Exiting now."
  exit 0
fi


cd "${HERE}"
cmake .
make clean
make

version="$(git describe --tags)"
outdir="$PWD/out"

mkdir -p "$outdir"

for i in AppRun; do
	[ -f "$i" ] && cp -v "$i" "${outdir}/${i}_${version}-$(uname -m)"
done

for i in AppImageAssistant AppImageExtract AppImageMonitor AppImageUpdate; do
	[ -f "$i" ] && cp -v "$i" "${outdir}/${i}_${version}-$(uname -m).AppImage"
done

cd -

# Note: This does not build AppDirAssistant by default since AppDirAssistant
# is no longer the recommended way to generate AppImages. If you still want to build it:
# cp AppRun AppDirAssistant.AppDir/
# ./AppImageAssistant AppDirAssistant.AppDir/ AppDirAssistant.AppImage
