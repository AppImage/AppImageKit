#!/bin/bash
#
# Usage: ./installBinary.sh <binary> <source libs> <destination>
#
# Copies <binary> to <destination>/usr/bin and its binary dependencies
# from <source libs> to <destination>/usr/lib

find_dependencies() {
	local binary="$1"
	local source_libs="$2"

	for i in $(ldd "$binary" | cut -d" " -f1); do
		find "$source_libs" -name "$(basename "$i")"
	done
}

binary="$1"
source_libs="$2"
destination="$3"

echo "Installing $binary + $source_libs --> $destination"

destination_bin="$destination/usr/bin/"
destination_lib="$destination/usr/lib/"

mkdir -p "$destination_bin" "$destination_lib"

install -vt "$destination_bin" "$binary"
dependencies="$(find_dependencies "$binary" "$source_libs")"
[ -n "$dependencies" ] && install -vt "$destination_lib" $dependencies

exit 0
