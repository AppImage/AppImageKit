"""
Complete implementation of the XDG Icon Spec Version 0.8
http://standards.freedesktop.org/icon-theme-spec/
"""

import os, sys, time

from xdg.IniFile import *
from xdg.BaseDirectory import *
from xdg.Exceptions import *

import xdg.Config

class IconTheme(IniFile):
    "Class to parse and validate IconThemes"
    def __init__(self):
        IniFile.__init__(self)

    def __repr__(self):
        return self.name

    def parse(self, file):
        IniFile.parse(self, file, ["Icon Theme", "KDE Icon Theme"])
        self.dir = os.path.dirname(file)
        (nil, self.name) = os.path.split(self.dir)

    def getDir(self):
        return self.dir

    # Standard Keys
    def getName(self):
        return self.get('Name', locale=True)
    def getComment(self):
        return self.get('Comment', locale=True)
    def getInherits(self):
        return self.get('Inherits', list=True)
    def getDirectories(self):
        return self.get('Directories', list=True)
    def getHidden(self):
        return self.get('Hidden', type="boolean")
    def getExample(self):
        return self.get('Example')

    # Per Directory Keys
    def getSize(self, directory):
        return self.get('Size', type="integer", group=directory)
    def getContext(self, directory):
        return self.get('Context', group=directory)
    def getType(self, directory):
        value = self.get('Type', group=directory)
        if value:
            return value
        else:
            return "Threshold"
    def getMaxSize(self, directory):
        value = self.get('MaxSize', type="integer", group=directory)
        if value or value == 0:
            return value
        else:
            return self.getSize(directory)
    def getMinSize(self, directory):
        value = self.get('MinSize', type="integer", group=directory)
        if value or value == 0:
            return value
        else:
            return self.getSize(directory)
    def getThreshold(self, directory):
        value = self.get('Threshold', type="integer", group=directory)
        if value or value == 0:
            return value
        else:
            return 2

    # validation stuff
    def checkExtras(self):
        # header
        if self.defaultGroup == "KDE Icon Theme":
            self.warnings.append('[KDE Icon Theme]-Header is deprecated')

        # file extension
        if self.fileExtension == ".theme":
            pass
        elif self.fileExtension == ".desktop":
            self.warnings.append('.desktop fileExtension is deprecated')
        else:
            self.warnings.append('Unknown File extension')

        # Check required keys
        # Name
        try:
            self.name = self.content[self.defaultGroup]["Name"]
        except KeyError:
            self.errors.append("Key 'Name' is missing")

        # Comment
        try:
            self.comment = self.content[self.defaultGroup]["Comment"]
        except KeyError:
            self.errors.append("Key 'Comment' is missing")

        # Directories
        try:
            self.directories = self.content[self.defaultGroup]["Directories"]
        except KeyError:
            self.errors.append("Key 'Directories' is missing")

    def checkGroup(self, group):
        # check if group header is valid
        if group == self.defaultGroup:
            pass
        elif group in self.getDirectories():
            try:
                self.type = self.content[group]["Type"]
            except KeyError:
                self.type = "Threshold"
            try:
                self.name = self.content[group]["Name"]
            except KeyError:
                self.errors.append("Key 'Name' in Group '%s' is missing" % group)
        elif not (re.match("^\[X-", group) and group.decode("utf-8", "ignore").encode("ascii", 'ignore') == group):
            self.errors.append("Invalid Group name: %s" % group)

    def checkKey(self, key, value, group):
        # standard keys     
        if group == self.defaultGroup:
            if re.match("^Name"+xdg.Locale.regex+"$", key):
                pass
            elif re.match("^Comment"+xdg.Locale.regex+"$", key):
                pass
            elif key == "Inherits":
                self.checkValue(key, value, list=True)
            elif key == "Directories":
                self.checkValue(key, value, list=True)
            elif key == "Hidden":
                self.checkValue(key, value, type="boolean")
            elif key == "Example":
                self.checkValue(key, value)
            elif re.match("^X-[a-zA-Z0-9-]+", key):
                pass
            else:
                self.errors.append("Invalid key: %s" % key)
        elif group in self.getDirectories():
            if key == "Size":
                self.checkValue(key, value, type="integer")
            elif key == "Context":
                self.checkValue(key, value)
            elif key == "Type":
                self.checkValue(key, value)
                if value not in ["Fixed", "Scalable", "Threshold"]:
                    self.errors.append("Key 'Type' must be one out of 'Fixed','Scalable','Threshold', but is %s" % value)
            elif key == "MaxSize":
                self.checkValue(key, value, type="integer")
                if self.type != "Scalable":
                    self.errors.append("Key 'MaxSize' give, but Type is %s" % self.type)
            elif key == "MinSize":
                self.checkValue(key, value, type="integer")
                if self.type != "Scalable":
                    self.errors.append("Key 'MinSize' give, but Type is %s" % self.type)
            elif key == "Threshold":
                self.checkValue(key, value, type="integer")
                if self.type != "Threshold":
                    self.errors.append("Key 'Threshold' give, but Type is %s" % self.type)
            elif re.match("^X-[a-zA-Z0-9-]+", key):
                pass
            else:
                self.errors.append("Invalid key: %s" % key)


class IconData(IniFile):
    "Class to parse and validate IconData Files"
    def __init__(self):
        IniFile.__init__(self)

    def __repr__(self):
        return self.getDisplayName()

    def parse(self, file):
        IniFile.parse(self, file, ["Icon Data"])

    # Standard Keys
    def getDisplayName(self):
        return self.get('DisplayName', locale=True)
    def getEmbeddedTextRectangle(self):
        return self.get('EmbeddedTextRectangle', list=True)
    def getAttachPoints(self):
        return self.get('AttachPoints', type="point", list=True)

    # validation stuff
    def checkExtras(self):
        # file extension
        if self.fileExtension != ".icon":
            self.warnings.append('Unknown File extension')

    def checkGroup(self, group):
        # check if group header is valid
        if not (group == self.defaultGroup \
        or (re.match("^\[X-", group) and group.encode("ascii", 'ignore') == group)):
            self.errors.append("Invalid Group name: %s" % group.encode("ascii", "replace"))

    def checkKey(self, key, value, group):
        # standard keys     
        if re.match("^DisplayName"+xdg.Locale.regex+"$", key):
            pass
        elif key == "EmbeddedTextRectangle":
            self.checkValue(key, value, type="integer", list=True)
        elif key == "AttachPoints":
            self.checkValue(key, value, type="point", list=True)
        elif re.match("^X-[a-zA-Z0-9-]+", key):
            pass
        else:
            self.errors.append("Invalid key: %s" % key)



icondirs = []
for basedir in xdg_data_dirs:
    icondirs.append(os.path.join(basedir, "icons"))
    icondirs.append(os.path.join(basedir, "pixmaps"))
icondirs.append(os.path.expanduser("~/.icons"))

# just cache variables, they give a 10x speed improvement
themes = []
cache = dict()
dache = dict()
eache = dict()

def getIconPath(iconname, size = None, theme = None, extensions = ["png", "svg", "xpm"]):
    global themes

    if size == None:
        size = xdg.Config.icon_size
    if theme == None:
        theme = xdg.Config.icon_theme

    # if we have an absolute path, just return it
    if os.path.isabs(iconname):
        return iconname

    # check if it has an extension and strip it
    if os.path.splitext(iconname)[1][1:] in extensions:
        iconname = os.path.splitext(iconname)[0]

    # parse theme files
    try:
        if themes[0].name != theme:
            themes = []
            __addTheme(theme)
    except IndexError:
        __addTheme(theme)

    # more caching (icon looked up in the last 5 seconds?)
    tmp = "".join([iconname, str(size), theme, "".join(extensions)])
    if eache.has_key(tmp):
        if int(time.time() - eache[tmp][0]) >= xdg.Config.cache_time:
            del eache[tmp]
        else:
            return eache[tmp][1]

    for thme in themes:
        icon = LookupIcon(iconname, size, thme, extensions)
        if icon:
            eache[tmp] = [time.time(), icon]
            return icon

    # cache stuff again (directories lookuped up in the last 5 seconds?)
    for directory in icondirs:
        if (not dache.has_key(directory) \
            or (int(time.time() - dache[directory][1]) >= xdg.Config.cache_time \
            and dache[directory][2] < os.path.getmtime(directory))) \
            and os.path.isdir(directory):
            dache[directory] = [os.listdir(directory), time.time(), os.path.getmtime(directory)]

    for dir, values in dache.items():
        for extension in extensions:
            try:
                if iconname + "." + extension in values[0]:
                    icon = os.path.join(dir, iconname + "." + extension)
                    eache[tmp] = [time.time(), icon]
                    return icon
            except UnicodeDecodeError, e:
                if debug:
                    raise e
                else:
                    pass

    # we haven't found anything? "hicolor" is our fallback
    if theme != "hicolor":
        icon = getIconPath(iconname, size, "hicolor")
        eache[tmp] = [time.time(), icon]
        return icon

def getIconData(path):
    if os.path.isfile(path):
        dirname = os.path.dirname(path)
        basename = os.path.basename(path)
        if os.path.isfile(os.path.join(dirname, basename + ".icon")):
            data = IconData()
            data.parse(os.path.join(dirname, basename + ".icon"))
            return data

def __addTheme(theme):
    for dir in icondirs:
        if os.path.isfile(os.path.join(dir, theme, "index.theme")):
            __parseTheme(os.path.join(dir,theme, "index.theme"))
            break
        elif os.path.isfile(os.path.join(dir, theme, "index.desktop")):
            __parseTheme(os.path.join(dir,theme, "index.desktop"))
            break
    else:
        if debug:
            raise NoThemeError(theme)

def __parseTheme(file):
    theme = IconTheme()
    theme.parse(file)
    themes.append(theme)
    for subtheme in theme.getInherits():
        __addTheme(subtheme)

def LookupIcon(iconname, size, theme, extensions):
    # look for the cache
    if not cache.has_key(theme.name):
        cache[theme.name] = []
        cache[theme.name].append(time.time() - (xdg.Config.cache_time + 1)) # [0] last time of lookup
        cache[theme.name].append(0)               # [1] mtime
        cache[theme.name].append(dict())          # [2] dir: [subdir, [items]]

    # cache stuff (directory lookuped up the in the last 5 seconds?)
    if int(time.time() - cache[theme.name][0]) >= xdg.Config.cache_time:
        cache[theme.name][0] = time.time()
        for subdir in theme.getDirectories():
            for directory in icondirs:
                dir = os.path.join(directory,theme.name,subdir)
                if (not cache[theme.name][2].has_key(dir) \
                or cache[theme.name][1] < os.path.getmtime(os.path.join(directory,theme.name))) \
                and subdir != "" \
                and os.path.isdir(dir):
                    cache[theme.name][2][dir] = [subdir, os.listdir(dir)]
                    cache[theme.name][1] = os.path.getmtime(os.path.join(directory,theme.name))

    for dir, values in cache[theme.name][2].items():
        if DirectoryMatchesSize(values[0], size, theme):
            for extension in extensions:
                if iconname + "." + extension in values[1]:
                    return os.path.join(dir, iconname + "." + extension)

    minimal_size = sys.maxint
    closest_filename = ""
    for dir, values in cache[theme.name][2].items():
        distance = DirectorySizeDistance(values[0], size, theme)
        if distance < minimal_size:
            for extension in extensions:
                if iconname + "." + extension in values[1]:
                    closest_filename = os.path.join(dir, iconname + "." + extension)
                    minimal_size = distance

    return closest_filename

def DirectoryMatchesSize(subdir, iconsize, theme):
    Type = theme.getType(subdir)
    Size = theme.getSize(subdir)
    Threshold = theme.getThreshold(subdir)
    MinSize = theme.getMinSize(subdir)
    MaxSize = theme.getMaxSize(subdir)
    if Type == "Fixed":
        return Size == iconsize
    elif Type == "Scaleable":
        return MinSize <= iconsize <= MaxSize
    elif Type == "Threshold":
        return Size - Threshold <= iconsize <= Size + Threshold

def DirectorySizeDistance(subdir, iconsize, theme):
    Type = theme.getType(subdir)
    Size = theme.getSize(subdir)
    Threshold = theme.getThreshold(subdir)
    MinSize = theme.getMinSize(subdir)
    MaxSize = theme.getMaxSize(subdir)
    if Type == "Fixed":
        return abs(Size - iconsize)
    elif Type == "Scalable":
        if iconsize < MinSize:
            return MinSize - iconsize
        elif iconsize > MaxSize:
            return MaxSize - iconsize
        return 0
    elif Type == "Threshold":
        if iconsize < Size - Threshold:
            return MinSize - iconsize
        elif iconsize > Size + Threshold:
            return iconsize - MaxSize
        return 0
