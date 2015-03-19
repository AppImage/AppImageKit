#!/usr/bin/env python
#coding:utf-8

# DNeed to run java -verbose:class -jar ...

import pexpect, sys, os, shutil

sys.path.append(os.path.dirname(os.path.dirname(__file__))) # Adjust this as you see fit so that AppImageKit is found
from AppImageKit import AppDir
from AppImageKit import timesavers

class Profiler(object):
    
    def __init__(self):
        self.opened_files = []
        self.target_dir = "DIR/" # with trailing #
        #os.mkdir(self.target_dir)
        #command = "find %s -type f -exec sed -i -e 's|/usr|././|g' {} \;" % (self.target_dir)
        
    def write(self, text):
        text = text.split("\n")
        for line in text:
            if not "[Loaded" in line: continue
            if not " from " in line: continue
            self.handle_file(line)
            
    def handle_file(self, line):
        (classfile, jarfile) = line.split(" from ")
        classfile = classfile.replace("[Loaded ", "") + ".class".strip()
        jarfile = jarfile.replace("]", "").strip().replace("jar:file:", "").replace("file:", "").replace(".jar!/", ".jar")

        if not (classfile, jarfile) in self.opened_files:
            if not os.path.exists(jarfile): pass
            classparts = classfile.split(".")
            classgroup = ".".join(classparts[0:3])
            print "%s : %s" % (jarfile, classgroup)
            self.opened_files.append((classgroup, jarfile))
        
    def flush(self):
        pass


if __name__=='__main__':
    app = "'%s'" % ("' '".join(sys.argv[1:]))
    command = app
    child = pexpect.spawn(command, logfile=Profiler() )
    child.expect(pexpect.EOF, timeout=999999999)
    child.close()
    p = Profiler()

