#!/usr/bin/env python
#coding:utf-8

# Don't forget to patch /etc to ./et, do the symlinking, patch /usr to ././

import sys, os, shutil
import pexpect

sys.path.append(os.path.dirname(os.path.dirname(__file__))) # Adjust this as you see fit so that AppImageKit is found
from AppImageKit import AppDir
from AppImageKit import timesavers

class Profiler(object):
    
    def __init__(self):
        f = open(os.path.dirname(__file__) + "/commonfiles.data")
        self.base_system_files = newlist = [x.replace("\n", "") for x in f.readlines()] 
        self.base_system_prefixes = ["/home", "/tmp", "/Applications", "/dev", "/var", "/proc", "/usr/share/icons", "/usr/share/themes", "/usr/share/locale", 
                                     "/usr/share/X11", "/etc/lib", "/etc/fonts", "/etc/passwd", "/etc/shadow", "/etc/localtime", 
                                     "/usr/lib/locale", "/usr/lib/gtk-2.0/", "/lib/i486-linux-gnu", "/lib/i686", "/lib/terminfo",
                                     "/lib/tls", "/usr/lib/i486-linux-gnu", "/usr/lib/i686", "/usr/lib/tls", "/usr/lib/gdk-pixbuf-2.0/", "/sys", "/usr/share/mime/mime.cache", "/usr/share/fonts"]
        self.opened_files = []
        self.target_dir = sys.argv[1] + "/" # "DIR/" # with trailing #
        #os.mkdir(self.target_dir)
        #command = "find %s -type f -exec sed -i -e 's|/usr|././|g' {} \;" % (self.target_dir)
        
    def write(self, text):
        text = text.split("\n")
        for line in text:
            # print line
            if line == "": continue
            # if not "open(\"" in line: continue # don't use this since execve and friends also exist
            if not "(\"" in line: continue
            if "ENOENT" in line: continue
            try: 
                file = line.split('"')[1]
            except:
                continue
            if file.startswith(", "): continue
            if file == "/": continue
            if file == "": continue
            if not os.path.exists(file): continue            
            self.handle_file(file)
            
    def handle_file(self, file):        
        if not file in self.opened_files:
            self.opened_files.append(file)
            if not file in self.base_system_files:
                exclude = False
                for p in self.base_system_prefixes:
                    if file.startswith(p) == True:
                        exclude = True
                if exclude == False:
                    print file
                    dir = os.path.join(self.target_dir + os.path.dirname(file))
                    # print dir
                    try: os.makedirs(dir)
                    except: pass
                    try: shutil.copy(file, dir)
                    except: pass
                else:
                    print "#QUESTIONABLE#" + file
            else:
                print "#BASESYSTEM# " + file
        
    def flush(self):
        pass


if __name__=='__main__':
    app = "'%s'" % ("' '".join(sys.argv[2:]))
    command = "strace_private -f " + app # don't -eopen since execve and friends also exist; use private copy since otherwise permission error when run from inside AppImage. Binary is identical to usual one.
    print command
    child = pexpect.spawn(command, logfile=Profiler() )
    child.expect(pexpect.EOF, timeout=999999999)
    child.close()
    p = Profiler()
    dir = p.target_dir
    ad = AppDir.AppDir(dir)
    print ad
    ad.patch()
    ad.insert_apprun()
    command = 'rm -r "%s/lib"' % (dir)
    timesavers.run_shell_command(command)
