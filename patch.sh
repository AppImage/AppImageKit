#! /bin/bash

cd $(dirname $(readlink -f $0))

cd squashfuse
git checkout ll.c Makefile.am fuseprivate.c fuseprivate.h hl.c ll.h ll_inode.c nonstd-daemon.c

patch -p1 < ../squashfuse.patch
patch -p1 < ../squashfuse_dlopen.patch

cp -v ../squashfuse_dlopen.c ../squashfuse_dlopen.h .
