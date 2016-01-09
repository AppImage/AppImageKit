#!/usr/bin/env python2

# /**************************************************************************
# 
# Copyright (c) 2004-16 Simon Peter
# 
# All Rights Reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# 
# **************************************************************************/

import os, sys, subprocess, glob
import xdg.IconTheme, xdg.DesktopEntry # apt-get install python-xdg, present on Ubuntu
from locale import gettext as _

def get_status_output(*args, **kwargs):
    p = subprocess.Popen(*args, **kwargs)
    stdout, stderr = p.communicate()
    return p.returncode, stdout, stderr

class AppDirXdgHandler(object):
    
    def __init__(self, appdir):
        self.appdir = appdir
        # print _("AppDir: %s" % [self.appdir])
        newicondirs = [appdir]
        for icondir in xdg.IconTheme.icondirs:
            if icondir.startswith(xdg.IconTheme.basedir):
                icondir = self.appdir + icondir
                newicondirs.append(icondir)
        xdg.IconTheme.icondirs = newicondirs + xdg.IconTheme.icondirs # search AppDir, then system
        self.desktopfile = self._find_desktop_file_in_appdir()
        try: self.name = self.get_nice_name_from_desktop_file(self.desktopfile) 
        except: self.name = "Unknown"
        try: self.executable = self.get_exec_path(self.get_exec_name_from_desktop_file(self.desktopfile)) 
        except: self.executable = None
        try: self.icon = self.get_icon_path_by_icon_name(self.get_icon_name_from_desktop_file(self.desktopfile)) 
        except: self.icon = None

    def __repr__(self):
        try: return("%s AppDir %s with desktop file %s, executable %s and icon %s" % (self.name, self.appdir, self.desktopfile, self.executable, self.icon))
        except: return("AppDir")
        
    def get_icon_path_by_icon_name(self, icon_name):
        """Return the path of the icon for a given icon name"""
        for format in ["png", "xpm"] :
            icon = xdg.IconTheme.getIconPath(icon_name, 48, None, format)
            if icon != None :
                return icon
        return None # If we have not found an icon

    def _find_all_executables_in_appdir(self):
        """Return all executable files in the AppDir, or None"""
        results = []
        result, executables = get_status_output("find " + self.appdir + " -type f -perm -u+x")
        executables = executables.split("\n")
        if result != 0:
            return None
        for executable in executables:
            if os.path.basename(executable) != "AppRun":
                results.append(executable)
        return results

    def _find_desktop_file_in_appdir(self):
        try:
            result = glob.glob(self.appdir + '/*.desktop')[0]
            return result
        except:
            return None

    def get_icon_name_from_desktop_file(self, desktop_file):
        "Returns the Icon= entry of a given desktop file"
        icon_name = os.path.basename(xdg.DesktopEntry.DesktopEntry(filename=desktop_file).getIcon())
        # print icon_name
        return icon_name

    def get_exec_name_from_desktop_file(self, desktop_file):
        "Returns the Exec= entry of a given desktop file"
        exec_name = os.path.basename(xdg.DesktopEntry.DesktopEntry(filename=desktop_file).getExec()).replace(" %u","").replace(" %U","").replace(" %f","").replace(" %F","")
        # print exec_name
        return exec_name    

    def get_nice_name_from_desktop_file(self, desktop_file):
        "Returns the Name= entry of a given desktop file"
        name = xdg.DesktopEntry.DesktopEntry(filename=desktop_file).getName()
        return name

    def get_exec_path(self, exec_name):
        results = []
        for excp in ["", "bin", "sbin", "usr/bin", "usr/sbin", "usr/games"]:
            trial = os.path.join(self.appdir, excp, exec_name)
            if os.path.exists(trial):
                return trial
        return None

    
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print (_("Usage: %s AppDir" % (sys.argv[0])))
        exit(1)
    appdir = sys.argv[1]
    H = AppDirXdgHandler(appdir)
    print(H)

