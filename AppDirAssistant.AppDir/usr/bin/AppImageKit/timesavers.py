#!/usr/bin/env python

# probono 11-2010

import os, sys

# Make external binaries and their libs available if they are privately bundled
ldp = os.environ.get("LD_LIBRARY_PATH")
p = os.environ.get("PATH")
if ldp == None: ldp = ""
if p == None: p = ""
binpath = os.path.join(os.path.dirname(__file__), "bin")
ld_library_path = os.path.join(os.path.dirname(__file__), "lib")
os.environ.update({"PATH": binpath + ":" + p})
os.environ.update({"LD_LIBRARY_PATH": ld_library_path + ":" + ldp})

def run_shell_command(command, lines_as_list=True):
    "Runs a command like in a shell, including | and returns the resulting lines"
    # print "Running command: %s" % (command)
    if lines_as_list == True:
        list = os.popen(command).read().split("\n")
        del list[-1] # Remove last newline
    else:
        list = os.popen(command).read() # Not really a list in this case
        # Need to remove last newline?
    if list == []:
        return None
    else:
        return list

if __name__ == "__main__":
    print run_shell_command("ls -l / | grep root")
