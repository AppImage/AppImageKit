#! /bin/bash

cd $(dirname $(readlink -f $0))

cd squashfuse
if [ ! -e ./ll.c.orig ]; then
  patch -p1 --backup < ../squashfuse.patch
  patch -p1 --backup < ../squashfuse_dlopen.patch
fi
if [ ! -e ./squashfuse_dlopen.c ]; then
  cp ../squashfuse_dlopen.c .
fi
if [ ! -e ./squashfuse_dlopen.h ]; then
  cp ../squashfuse_dlopen.h .
fi
