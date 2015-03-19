#!/usr/bin/python

# probono 11-2010

import os, sys

sys.path.append(os.path.dirname(os.path.dirname(__file__))) # One directory up
import timesavers # Needs to be bundled


# To be implemented:
# 
# Check whether AppDir is valid
# - Check whether AppRun is present and executable
# - Check whether .desktop file is present and Exec and Icon can be found
# 
# Find Exec
# 
# Find Icon
# 
# Make new AppDir from template
# 
# Check whether /usr is patched
# Check whether /etc is patched
# 
# Run the app
# ldd the app
# 
# Package the AppDir as an AppImage
#

class AppDir(object):
    
    def __init__(self, path):
        self.path = path

    def patch_usr(self):
        command = 'find "%s"/*/ -type f -exec sed -i -e "s|/usr|././|g" {} \\;' % (self.path)
        timesavers.run_shell_command(command)
    
    def patch_etc(self):
        command = 'find "%s"/*/ -type f -exec sed -i -e "s|/etc|./et|g" {} \\;' % (self.path)
        timesavers.run_shell_command(command)
        command = 'cd "%s/usr" ; ln -s ../etc ./et ; cd -' % (self.path)
        timesavers.run_shell_command(command)

    def patch(self):
        self.patch_usr()
        self.patch_etc()

    def insert_apprun(self):
        print("FIXME: To be done properly")
        command = 'cp ../AppRun "%s/"' % (self.path)
        timesavers.run_shell_command(command)

    def insert_desktop(self, name="dummy"):
        f = open(self.path + "/" + name + ".desktop", "w")
        f.write("[Desktop Entry]\n")
        f.write("Type=Application\n")
        f.write("Icon=%s\n" % (name))
        f.write("Name=%s\n" % (name.capitalize()))
        f.write("Exec=%s\n" % (name))
        f.close

    def __repr__(self):
        return "<AppDir at %s>" % (self.path)
    
if __name__ == "__main__":
    A = AppDir("/some")
    print A
