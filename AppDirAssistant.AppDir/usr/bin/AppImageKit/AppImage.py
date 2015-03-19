#!/usr/bin/env python


# probono 11-2010


import os, sys, subprocess, hashlib, urllib, tempfile, shutil
from locale import normalize

sys.path.append(os.path.dirname(os.path.dirname(__file__))) # One directory up

import timesavers, xxdg.DesktopEntry, xxdg.BaseDirectory # Needs to be bundled


#
# Helper functions
#


def get_appimages_in_dir(directory):
    "Returns a list of all AppImages in a given directory and its subdirectories"
    list = []
    files = timesavers.run_shell_command("find '" + directory + "'")
    for file in files:
        AI = AppImage(file)
        #AIL = AppImage(os.path.realpath(file)) # follow symlinks
        if AI.check_whether_is_appimage() == True:
            list.append(AI)
    return list


#
# Classes
#


class AppImage:

    def __init__(self, path):
        self.path = os.path.normpath(os.path.abspath(path))
        self.path_md5 = hashlib.md5('file://' + urllib.quote(self.path)).hexdigest()
        self.is_appimage = False
        self.iconfile_path = os.path.expanduser('~/.thumbnails/normal/') + self.path_md5 + '.png'
        desktopfiles_location = xxdg.BaseDirectory.xdg_data_home + "/applications/appimage/"
        self.desktopfile_path = desktopfiles_location + self.path_md5 + ".desktop" # TODO: factor out
        
    def check_whether_is_appimage(self):
        "Checks whether the file is an AppImage. TODO: Speed up by not using file"
        type = timesavers.run_shell_command("file -k '" + os.path.realpath(self.path) + "' | grep 'executable' | grep 'ISO 9660'") # follow symlinks
        if type != None:
            self.is_appimage = True
            return True
        else:
            return False

    def get_file(self, filepath, destination=None): 
        "Returns the contents of a given file as a variable"
        command = "'cmginfo' -f '/" + self.path + "' -e '" + filepath + "'"
        result = timesavers.run_shell_command(command, False)
        if result != "":
            return result
        else:
            return None

    def get_desktop_filename(self, destination=None):
        command = "'cmginfo' -l / -f '" + self.path + "'"
        toplevel_files = timesavers.run_shell_command(command)
        for toplevel_file in toplevel_files:
            if ".desktop" in toplevel_file:
                return toplevel_file

    def get_icon_filename(self, destination=None):
        return "/.DirIcon" # Could become more elaborate


    def get_file_list(self):
        command = "'cmginfo' '/" + self.path + "'"
        return timesavers.run_shell_command(command)
    
    def install_desktop_integration(self, parentifthreaded=None):
        if not os.path.exists(self.iconfile_path): # Don't overwrite if it already exists
            try: os.makedirs(os.path.dirname(self.iconfile_path))
            except: pass
            print "* Installing %s" % (self.iconfile_path)
            f = open(self.iconfile_path, "w")
            f.write(self.get_file("/.DirIcon"))
            f.close()        
        if not os.path.exists(self.desktopfile_path): # Don't overwrite if it already exists, as this triggers cache rebuild
            try: os.makedirs(os.path.dirname(self.desktopfile_path))
            except: pass
            print "* Installing %s" % (self.desktopfile_path)
            #f = open(desktopfile_path, "w")
            f = tempfile.NamedTemporaryFile(delete=False)
            f.write(self.get_file(self.get_desktop_filename()))
            f.close()
            desktop = xxdg.DesktopEntry.DesktopEntry()
            desktop.parse(f.name)
            desktop.set("X-AppImage-Original-Exec", desktop.get("Exec")) 
            desktop.set("X-AppImage-Original-Icon", desktop.get("Icon"))
            try: 
                if desktop.get("TryExec"):
                    desktop.set("X-AppImage-Original-TryExec", desktop.get("TryExec"))
                    desktop.set("TryExec", self.path) # Definitely quotes are not accepted here
            except: 
                pass
            desktop.set("Icon", self.iconfile_path)
            desktop.set("X-AppImage-Location", self.path)
            desktop.set("Type", "Application") # Fix for invalid .desktop files that contain no Type field
            desktop.set("Exec", '"' + self.path + '"') # Quotes seem accepted here but only one % argument????
            # desktop.validate()
            desktop.write(f.name) 
            os.chmod(f.name, 0755)
            print self.desktopfile_path
            shutil.move(f.name, self.desktopfile_path) # os.rename fails when tmpfs is mounted at /tmp
            if os.env("KDE_SESSION_VERSION") == "4":
	        timesavers.run_shell_command("kbuildsycoca4") # Otherwise KDE4 ignores the menu

    def uninstall_desktop_integration(self):
        if os.path.isfile(self.desktopfile_path):
            try:
                os.unlink(self.desktopfile_path)
                print "* Removed %s" % (self.desktopfile_path)
            except:
                print "* Failed to remove %s" % (self.desktopfile_path)
        if os.path.isfile(self.iconfile_path):
            try:
                os.unlink(self.iconfile_path)
                print "* Removed %s" % (self.iconfile_path)
            except:
                print "* Failed to remove %s" % (self.iconfile_path)
            
    def extract(self, destination_dir="/tmp"):
        dest_dir = os.path.join(destination_dir, os.path.basename(self.path).replace(".AppImage","") + ".AppDir")
        # subprocess.call("xorriso", "-indev", self.path, "-osirrox", "on", "-extract", "/", destination_dir)
        # xorriso -indev "${1}" -osirrox on -extract / $HOME/Desktop/"${NAME}.AppDir"
        # sudo chown -R "${USER}" $HOME/Desktop/"${NAME}.AppDir"
        # sudo chmod -R u+w $HOME/Desktop/"${NAME}.AppDir"
        # print dirname
        print "To be implemented - looks like cmginfo can't extract whole images?"

    def set_executable_bit(self, boolean):
        # Ideally, we would try whether we can ask the user via a gtk.MESSAGE_WARNING
        # but that seems to be complicated to do without freezing the daemon (threading is tough to get right)
        if boolean == True:
            timesavers.run_shell_command("chmod a+x '" + self.path + "'")
        else:
            timesavers.run_shell_command("chmod a-x '" + self.path + "'")

    def __repr__(self):
        if self.is_appimage:
            existing = "Existing "
        else:
            existing = ""
        return "<%sAppImage %s>" % (existing, self.path)


def main():
    print "\nChecking /Applications for AppImages..."
    appimages = get_appimages_in_dir("/Applications/")
    for ai in appimages:
        print ai
        #thread.start_new_thread(ai.install_desktop_integration,(1,))
        ai.install_desktop_integration()
    # print "\nListing files in %s..." % (str(appimages[0]))
    # print appimages[0].get_file_list()
    # print "\nExtracting AppRun from %s..." % (str(appimages[0]))
    # print appimages[0].get_file("/AppRun")
    # print "\nExtracting desktop file from %s..." % (str(appimages[0]))
    # print appimages[0].get_file(appimages[0].get_desktop_filename())
    # appimages[0].install_desktop_integration()
    #appimages[0].extract()

if __name__ == "__main__":
    main()
