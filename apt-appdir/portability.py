#!/usr/bin/env python

import os, sys, fnmatch

AppDir = os.path.realpath(os.path.dirname(__file__))

modules = []

for root, dirs, files in os.walk(AppDir):
    for fname in files:
        fpath = os.path.join(root,fname)
        if fnmatch.fnmatch(fpath, '*__init__.py'):
            modules.append(os.path.dirname(os.path.dirname(fpath)))
        if fnmatch.fnmatch(fpath, '*/_*.so'):
            modules.append(os.path.dirname(fpath))
        if fnmatch.fnmatch(fpath, '*python*.so'):
            modules.append(os.path.dirname(fpath))
        if fnmatch.fnmatch(fpath, '*.py'):
            modules.append(os.path.dirname(fpath))

modules = set(modules)

for module in modules:
    sys.path.append(module)

usage = """
E.g., if the main executable is in $APPDIR/usr/bin, then add to the head of it:

import os,sys
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__)))))
import portability
"""

if __name__=="__main__":
    print ""
    print("Put this file into the root of an AppDir, then import it to make all Python modules in the AppDir visible to the app")
    print usage
