#!/bin/sh

# Arrange files inside AppDir like this:
#  <APPDIR>/usr/optional/libgcc_s/libgcc_s.so.1
#  <APPDIR>/usr/optional/libstdc++/libstdc++.so.6
#  <APPDIR>/usr/optional/readelf
#  <APPDIR>/usr/optional/testrt
#  <APPDIR>/usr/optional/testrt.sh

LANG=C

need_cxx_newer="no"
need_gcc_newer="no"

cxx_lib_system=$(ldd ./optional/testrt | grep libstdc++.so.6 | awk '{print $3}')
cxx_lib_bundle="./optional/libstdc++/libstdc++.so.6"
cxx_system=$(./optional/readelf -s $cxx_lib_system | grep -Eo '@GLIBCXX_[0-9.]{1,}' | sort --version-sort -r | sed -n 's/@GLIBCXX_//g; 1p')
cxx_bundle=$(./optional/readelf -s $cxx_lib_bundle | grep -Eo '@GLIBCXX_[0-9.]{1,}' | sort --version-sort -r | sed -n 's/@GLIBCXX_//g; 1p')
cxx_newer=$(echo "$cxx_bundle\n$cxx_system" | sort -V | tail -n1)

gcc_lib_system=$(ldd ./optional/testrt | grep libgcc_s.so.1 | awk '{print $3}')
gcc_lib_bundle="./optional/libgcc_s/libgcc_s.so.1"
gcc_system=$(./optional/readelf -s $cxx_lib_system | grep -Eo '@GCC_[0-9.]{1,}' | sort --version-sort -r | sed -n 's/@GCC_//g; 1p')
gcc_bundle=$(./optional/readelf -s $cxx_lib_bundle | grep -Eo '@GCC_[0-9.]{1,}' | sort --version-sort -r | sed -n 's/@GCC_//g; 1p')
gcc_newer=$(echo "$gcc_bundle\n$gcc_system" | sort -V | tail -n1)

if [ $cxx_bundle = $cxx_newer ] && [ $cxx_system != $cxx_bundle ] ; then
  need_cxx_newer="yes"
fi
if [ $gcc_bundle = $gcc_newer ] && [ $gcc_system != $gcc_bundle ] ; then
  need_gcc_newer="yes"
fi

if [ "$need_cxx_newer" = "yes" ] && [ "$need_gcc_newer" = "no" ] ; then
  exit 1
elif [ "$need_cxx_newer" = "no" ] && [ "$need_gcc_newer" = "yes" ] ; then
  exit 2
elif [ "$need_cxx_newer" = "yes" ] && [ "$need_gcc_newer" = "yes" ] ; then
  exit 3
fi

exit 0

