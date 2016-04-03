#!/bin/bash
set -e

cd "$(dirname "$0")"

bindir="AppImageAssistant.AppDir/arch"
outdir="out"
rm -rvf "$bindir" "$outdir"

for container in docker/*; do
	tag="appimagekit/$(basename "$container" | tr "A-Z" "a-z")"

	echo "* Building  image $tag"

	cp -v build.sh "$container"
	docker build -t "$tag" "$container"
	rm -v "$container/build.sh"
done

for container in docker/*; do
	tag="appimagekit/$(basename "$container" | tr "A-Z" "a-z")"
	arch=$(docker run -v "${PWD}:/AppImageKit" "$tag" uname -m)

	echo "* Building binaries for architecture $arch on image $tag"

	[ -f Makefile ] && make clean
	sudo rm -rf CMakeFiles */CMakeFiles CMakeCache.txt */CMakeCache.txt Makefile */Makefile
	docker run -v "${PWD}:/AppImageKit" "$tag" /AppImageKit/build.sh runtime AppRun
	sudo chown -R $UID:$UID out/

	mkdir -p "$bindir/$arch"
	for bin in runtime; do
		cp -v out/${bin}_*-${arch} "$bindir/$arch/$bin"
	done
done

for container in docker/*; do
	tag="appimagekit/$(basename "$container" | tr "A-Z" "a-z")"
	arch=$(docker run -v "${PWD}:/AppImageKit" "$tag" uname -m)

	echo "* Building AppImages for architecture $arch on image $tag"

	[ -f Makefile ] && make clean
	sudo rm -rf CMakeFiles */CMakeFiles CMakeCache.txt */CMakeCache.txt Makefile */Makefile
	docker run -v "${PWD}:/AppImageKit" "$tag" /AppImageKit/build.sh AppImageAssistant AppImageExtract AppImageMonitor AppImageUpdate
	sudo chown -R $UID:$UID out/
done
