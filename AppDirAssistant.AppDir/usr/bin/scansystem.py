#!/usr/bin/env python
#coding:utf-8

# Don't forget to patch /etc to ./et, do the symlinking, patch /usr to ././

import pexpect, sys, os, shutil

sys.path.append(os.path.dirname(os.path.dirname(__file__))) # Adjust this as you see fit so that AppImageKit is found
from AppImageKit import AppDir
from AppImageKit import timesavers
import profileapp
p = profileapp.Profiler()

if __name__=='__main__':
    command = 'find / > /tmp/_state_1 2>/dev/null' 
    timesavers.run_shell_command(command)
    command = "sh -e %s" % ("' '".join(sys.argv[1:]))
    var = raw_input('Install now, then press any key to take 2nd scan...')
    command = 'find / > /tmp/_state_2 2>/dev/null' 
    timesavers.run_shell_command(command)
    filea = "/tmp/_state_1"
    a = open(filea).readlines()
    fileb = "/tmp/_state_2"
    b = open(fileb).readlines()
    for x in a:
        try: b.remove(x)
        except: pass
    b = [x.replace("\n", "") for x in b]
    # print b
    [ p.handle_file(file) for file in b ]
    dir = p.target_dir
    ad = AppDir.AppDir(dir)
    print ad
    ad.patch()
    ad.insert_apprun()    
    command = 'mv "%s/lib"/* "%s/usr/lib/"' % (dir, dir)
    timesavers.run_shell_command(command)
    command = 'rm -r "%s/lib"' % (dir)
    timesavers.run_shell_command(command)
    command = 'find "%s" -name "*.desktop" -exec cp {} "%s" \\;' % (dir, dir)
    timesavers.run_shell_command(command)
