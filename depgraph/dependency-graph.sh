#!/bin/bash

# Usage: dependency-graph.sh krita

# Dependency grapher
# Show the dependencies of debian packages
# TODO: 
# * Color the packages we assume to be part of the base system
# * Do the same for ldd tree
# * Do the same for rpm/yum
# * Ideally, compare

LOWERAPP=$1

rm log || true

# We get this app and almost all its dependencies via apt-get
# but not using the host system's information about what is
# installed on the system but our own assumptions instead

mkdir -p ./tmp/archives/
mkdir -p ./tmp/lists/partial
touch tmp/pkgcache.bin tmp/srcpkgcache.bin

cat > status <<EOF
Package: libacl1
Status: install ok installed
Architecture: all
Version: 9:9999.9999.9999

Package: multiarch-support
Status: install ok installed
Architecture: all
Version: 9:9999.9999.9999

Package: debconf
Status: install ok installed
Architecture: all
Version: 9:9999.9999.9999

Package: dpkg
Status: install ok installed
Architecture: all
Version: 9:9999.9999.9999
EOF

echo "deb http://archive.ubuntu.com/ubuntu/ xenial main universe
deb http://ppa.launchpad.net/dimula73/krita/ubuntu xenial main 
" > sources.list

OPTIONS="-o Debug::NoLocking=1
-o APT::Cache-Limit=125829120
-o Dir::Etc::sourcelist=./sources.list
-o Dir::State=./tmp
-o Dir::Cache=./tmp
-o Dir::State::status=./status
-o Dir::Etc::sourceparts=-
-o APT::Get::List-Cleanup=0
-o APT::Get::AllowUnauthenticated=1
-o Debug::pkgProblemResolver=true
-o Debug::pkgDepCache::AutoInstall=true
-o APT::Install-Recommends=0
-o APT::Install-Suggests=0
"

# Add local repository so that we can install deb files
# that were downloaded outside of a repository
dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz
echo "deb file:$(readlink -e $PWD) ./" >> sources.list

apt-get $OPTIONS update

apt-get $OPTIONS -y install --print-uris $LOWERAPP 2>&1 | tee -a log

URLS=$(cat log | cut -d "'" -f 2 | grep -e "^http")
cat log | sed -e 's| Installing (.*?) as Depends of (.*?)|\1 \0|g'

TREE=$(cat log | grep "as Depends of" | sed -r -e 's|Installing (.*) as Depends of (.*)|"\2" -- "\1"; |g' | sed -e 's|  ||g' | sort | uniq)

echo "Graph G { minlen=30; splines=line; overlap = false; rankdir=LR ; nodesep=0.1; margin=0; \
node [fontname=Helvetica; fontsize=9; height=0.1; color=lightgray; style=filled; shape=record] \
${TREE} } " | dot -Tsvg > $LOWERAPP.svg

xdg-open $LOWERAPP.svg
