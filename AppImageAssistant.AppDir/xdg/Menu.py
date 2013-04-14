"""
Implementation of the XDG Menu Specification Version 1.0.draft-1
http://standards.freedesktop.org/menu-spec/
"""

from __future__ import generators
import locale, os, xml.dom.minidom

from xdg.BaseDirectory import *
from xdg.DesktopEntry import *
from xdg.Exceptions import *

import xdg.Locale
import xdg.Config

ELEMENT_NODE = xml.dom.Node.ELEMENT_NODE

# for python <= 2.3
try:
    reversed = reversed
except NameError:
    def reversed(x):
        return x[::-1]

class Menu:
    def __init__(self):
        # Public stuff
        self.Name = ""
        self.Directory = None
        self.Entries = []
        self.Doc = ""
        self.Filename = ""
        self.Depth = 0
        self.Parent = None
        self.NotInXml = False

        # Can be one of Deleted/NoDisplay/Hidden/Empty/NotShowIn or True
        self.Show = True
        self.Visible = 0

        # Private stuff, only needed for parsing
        self.AppDirs = []
        self.DefaultLayout = None
        self.Deleted = "notset"
        self.Directories = []
        self.DirectoryDirs = []
        self.Layout = None
        self.MenuEntries = []
        self.Moves = []
        self.OnlyUnallocated = "notset"
        self.Rules = []
        self.Submenus = []

    def __str__(self):
        return self.Name

    def __add__(self, other):
        for dir in other.AppDirs:
            self.AppDirs.append(dir)

        for dir in other.DirectoryDirs:
            self.DirectoryDirs.append(dir)

        for directory in other.Directories:
            self.Directories.append(directory)

        if other.Deleted != "notset":
            self.Deleted = other.Deleted

        if other.OnlyUnallocated != "notset":
            self.OnlyUnallocated = other.OnlyUnallocated

        if other.Layout:
            self.Layout = other.Layout

        if other.DefaultLayout:
            self.DefaultLayout = other.DefaultLayout

        for rule in other.Rules:
            self.Rules.append(rule)

        for move in other.Moves:
            self.Moves.append(move)

        for submenu in other.Submenus:
            self.addSubmenu(submenu)

        return self

    # FIXME: Performance: cache getName()
    def __cmp__(self, other):
        return locale.strcoll(self.getName(), other.getName())

    def __eq__(self, other):
        if self.Name == str(other):
            return True
        else:
            return False

    """ PUBLIC STUFF """
    def getEntries(self, hidden=False):
        for entry in self.Entries:
            if hidden == True:
                yield entry
            elif entry.Show == True:
                yield entry

    # FIXME: Add searchEntry/seaqrchMenu function
    # search for name/comment/genericname/desktopfileide
    # return multiple items

    def getMenuEntry(self, desktopfileid, deep = False):
        for menuentry in self.MenuEntries:
            if menuentry.DesktopFileID == desktopfileid:
                return menuentry
        if deep == True:
            for submenu in self.Submenus:
                submenu.getMenuEntry(desktopfileid, deep)

    def getMenu(self, path):
        array = path.split("/", 1)
        for submenu in self.Submenus:
            if submenu.Name == array[0]:
                if len(array) > 1:
                    return submenu.getMenu(array[1])
                else:
                    return submenu

    def getPath(self, org=False, toplevel=False):
        parent = self
        names=[]
        while 1:
            if org:
                names.append(parent.Name)
            else:
                names.append(parent.getName())
            if parent.Depth > 0:
                parent = parent.Parent
            else:
                break
        names.reverse()
        path = ""
        if toplevel == False:
            names.pop(0)
        for name in names:
            path = os.path.join(path, name)
        return path

    def getName(self):
        try:
            return self.Directory.DesktopEntry.getName()
        except AttributeError:
            return self.Name

    def getGenericName(self):
        try:
            return self.Directory.DesktopEntry.getGenericName()
        except AttributeError:
            return ""

    def getComment(self):
        try:
            return self.Directory.DesktopEntry.getComment()
        except AttributeError:
            return ""

    def getIcon(self):
        try:
            return self.Directory.DesktopEntry.getIcon()
        except AttributeError:
            return ""

    """ PRIVATE STUFF """
    def addSubmenu(self, newmenu):
        for submenu in self.Submenus:
            if submenu == newmenu:
                submenu += newmenu
                break
        else:
            self.Submenus.append(newmenu)
            newmenu.Parent = self
            newmenu.Depth = self.Depth + 1

class Move:
    "A move operation"
    def __init__(self, node=None):
        if node:
            self.parseNode(node)
        else:
            self.Old = ""
            self.New = ""

    def __cmp__(self, other):
        return cmp(self.Old, other.Old)

    def parseNode(self, node):
        for child in node.childNodes:
            if child.nodeType == ELEMENT_NODE:
                if child.tagName == "Old":
                    try:
                        self.parseOld(child.childNodes[0].nodeValue)
                    except IndexError:
                        raise ValidationError('Old cannot be empty', '??')                                            
                elif child.tagName == "New":
                    try:
                        self.parseNew(child.childNodes[0].nodeValue)
                    except IndexError:
                        raise ValidationError('New cannot be empty', '??')                                            

    def parseOld(self, value):
        self.Old = value
    def parseNew(self, value):
        self.New = value


class Layout:
    "Menu Layout class"
    def __init__(self, node=None):
        self.order = []
        if node:
            self.show_empty = node.getAttribute("show_empty") or "false"
            self.inline = node.getAttribute("inline") or "false"
            self.inline_limit = node.getAttribute("inline_limit") or 4
            self.inline_header = node.getAttribute("inline_header") or "true"
            self.inline_alias = node.getAttribute("inline_alias") or "false"
            self.inline_limit = int(self.inline_limit)
            self.parseNode(node)
        else:
            self.show_empty = "false"
            self.inline = "false"
            self.inline_limit = 4
            self.inline_header = "true"
            self.inline_alias = "false"
            self.order.append(["Merge", "menus"])
            self.order.append(["Merge", "files"])

    def parseNode(self, node):
        for child in node.childNodes:
            if child.nodeType == ELEMENT_NODE:
                if child.tagName == "Menuname":
                    try:
                        self.parseMenuname(
                            child.childNodes[0].nodeValue,
                            child.getAttribute("show_empty") or "false",
                            child.getAttribute("inline") or "false",
                            child.getAttribute("inline_limit") or 4,
                            child.getAttribute("inline_header") or "true",
                            child.getAttribute("inline_alias") or "false" )
                    except IndexError:
                        raise ValidationError('Menuname cannot be empty', "")
                elif child.tagName == "Separator":
                    self.parseSeparator()
                elif child.tagName == "Filename":
                    try:
                        self.parseFilename(child.childNodes[0].nodeValue)
                    except IndexError:
                        raise ValidationError('Filename cannot be empty', "")
                elif child.tagName == "Merge":
                    self.parseMerge(child.getAttribute("type") or "all")

    def parseMenuname(self, value, empty="false", inline="false", inline_limit=4, inline_header="true", inline_alias="false"):
        self.order.append(["Menuname", value, empty, inline, inline_limit, inline_header, inline_alias])
        self.order[-1][4] = int(self.order[-1][4])

    def parseSeparator(self):
        self.order.append(["Separator"])

    def parseFilename(self, value):
        self.order.append(["Filename", value])

    def parseMerge(self, type="all"):
        self.order.append(["Merge", type])


class Rule:
    "Inlcude / Exclude Rules Class"
    def __init__(self, type, node=None):
        # Type is Include or Exclude
        self.Type = type
        # Rule is a python expression
        self.Rule = ""

        # Private attributes, only needed for parsing
        self.Depth = 0
        self.Expr = [ "or" ]
        self.New = True

        # Begin parsing
        if node:
            self.parseNode(node)
            self.compile()

    def __str__(self):
        return self.Rule

    def compile(self):
        exec("""
def do(menuentries, type, run):
    for menuentry in menuentries:
        if run == 2 and ( menuentry.MatchedInclude == True \
        or menuentry.Allocated == True ):
            continue
        elif %s:
            if type == "Include":
                menuentry.Add = True
                menuentry.MatchedInclude = True
            else:
                menuentry.Add = False
    return menuentries
""" % self.Rule) in self.__dict__

    def parseNode(self, node):
        for child in node.childNodes:
            if child.nodeType == ELEMENT_NODE:
                if child.tagName == 'Filename':
                    try:
                        self.parseFilename(child.childNodes[0].nodeValue)
                    except IndexError:
                        raise ValidationError('Filename cannot be empty', "???")
                elif child.tagName == 'Category':
                    try:
                        self.parseCategory(child.childNodes[0].nodeValue)
                    except IndexError:
                        raise ValidationError('Category cannot be empty', "???")
                elif child.tagName == 'All':
                    self.parseAll()
                elif child.tagName == 'And':
                    self.parseAnd(child)
                elif child.tagName == 'Or':
                    self.parseOr(child)
                elif child.tagName == 'Not':
                    self.parseNot(child)

    def parseNew(self, set=True):
        if not self.New:
            self.Rule += " " + self.Expr[self.Depth] + " "
        if not set:
            self.New = True
        elif set:
            self.New = False

    def parseFilename(self, value):
        self.parseNew()
        self.Rule += "menuentry.DesktopFileID == '%s'" % value.strip().replace("\\", r"\\").replace("'", r"\'")

    def parseCategory(self, value):
        self.parseNew()
        self.Rule += "'%s' in menuentry.Categories" % value.strip()

    def parseAll(self):
        self.parseNew()
        self.Rule += "True"

    def parseAnd(self, node):
        self.parseNew(False)
        self.Rule += "("
        self.Depth += 1
        self.Expr.append("and")
        self.parseNode(node)
        self.Depth -= 1
        self.Expr.pop()
        self.Rule += ")"

    def parseOr(self, node):
        self.parseNew(False)
        self.Rule += "("
        self.Depth += 1
        self.Expr.append("or")
        self.parseNode(node)
        self.Depth -= 1
        self.Expr.pop()
        self.Rule += ")"

    def parseNot(self, node):
        self.parseNew(False)
        self.Rule += "not ("
        self.Depth += 1
        self.Expr.append("or")
        self.parseNode(node)
        self.Depth -= 1
        self.Expr.pop()
        self.Rule += ")"


class MenuEntry:
    "Wrapper for 'Menu Style' Desktop Entries"
    def __init__(self, filename, dir="", prefix=""):
        # Create entry
        self.DesktopEntry = DesktopEntry(os.path.join(dir,filename))
        self.setAttributes(filename, dir, prefix)

        # Can be one of Deleted/Hidden/Empty/NotShowIn/NoExec or True
        self.Show = True

        # Semi-Private
        self.Original = None
        self.Parents = []

        # Private Stuff
        self.Allocated = False
        self.Add = False
        self.MatchedInclude = False

        # Caching
        self.Categories = self.DesktopEntry.getCategories()

    def save(self):
        if self.DesktopEntry.tainted == True:
            self.DesktopEntry.write()

    def getDir(self):
        return self.DesktopEntry.filename.replace(self.Filename, '')

    def getType(self):
        # Can be one of System/User/Both
        if xdg.Config.root_mode == False:
            if self.Original:
                return "Both"
            elif xdg_data_dirs[0] in self.DesktopEntry.filename:
                return "User"
            else:
                return "System"
        else:
            return "User"

    def setAttributes(self, filename, dir="", prefix=""):
        self.Filename = filename
        self.Prefix = prefix
        self.DesktopFileID = os.path.join(prefix,filename).replace("/", "-")

        if not os.path.isabs(self.DesktopEntry.filename):
            self.__setFilename()

    def updateAttributes(self):
        if self.getType() == "System":
            self.Original = MenuEntry(self.Filename, self.getDir(), self.Prefix)
            self.__setFilename()

    def __setFilename(self):
        if xdg.Config.root_mode == False:
            path = xdg_data_dirs[0]
        else:
            path= xdg_data_dirs[1]

        if self.DesktopEntry.getType() == "Application":
            dir = os.path.join(path, "applications")
        else:
            dir = os.path.join(path, "desktop-directories")

        self.DesktopEntry.filename = os.path.join(dir, self.Filename)

    def __cmp__(self, other):
        return locale.strcoll(self.DesktopEntry.getName(), other.DesktopEntry.getName())

    def __eq__(self, other):
        if self.DesktopFileID == str(other):
            return True
        else:
            return False

    def __repr__(self):
        return self.DesktopFileID


class Separator:
    "Just a dummy class for Separators"
    def __init__(self, parent):
        self.Parent = parent
        self.Show = True


class Header:
    "Class for Inline Headers"
    def __init__(self, name, generic_name, comment):
        self.Name = name
        self.GenericName = generic_name
        self.Comment = comment

    def __str__(self):
        return self.Name


tmp = {}

def __getFileName(filename):
    dirs = xdg_config_dirs[:]
    if xdg.Config.root_mode == True:
        dirs.pop(0)

    for dir in dirs:
        menuname = os.path.join (dir, "menus" , filename)
        if os.path.isdir(dir) and os.path.isfile(menuname):
            return menuname

def parse(filename=None):
    # conver to absolute path
    if filename and not os.path.isabs(filename):
        filename = __getFileName(filename)

    # use default if no filename given
    if not filename: 
        candidate = os.environ.get('XDG_MENU_PREFIX', '') + "applications.menu"
        filename = __getFileName(candidate)
        
    if not filename:
        raise ParsingError('File not found', "/etc/xdg/menus/%s" % candidate)

    # check if it is a .menu file
    if not os.path.splitext(filename)[1] == ".menu":
        raise ParsingError('Not a .menu file', filename)

    # create xml parser
    try:
        doc = xml.dom.minidom.parse(filename)
    except xml.parsers.expat.ExpatError:
        raise ParsingError('Not a valid .menu file', filename)

    # parse menufile
    tmp["Root"] = ""
    tmp["mergeFiles"] = []
    tmp["DirectoryDirs"] = []
    tmp["cache"] = MenuEntryCache()

    __parse(doc, filename, tmp["Root"])
    __parsemove(tmp["Root"])
    __postparse(tmp["Root"])

    tmp["Root"].Doc = doc
    tmp["Root"].Filename = filename

    # generate the menu
    __genmenuNotOnlyAllocated(tmp["Root"])
    __genmenuOnlyAllocated(tmp["Root"])

    # and finally sort
    sort(tmp["Root"])

    return tmp["Root"]


def __parse(node, filename, parent=None):
    for child in node.childNodes:
        if child.nodeType == ELEMENT_NODE:
            if child.tagName == 'Menu':
                __parseMenu(child, filename, parent)
            elif child.tagName == 'AppDir':
                try:
                    __parseAppDir(child.childNodes[0].nodeValue, filename, parent)
                except IndexError:
                    raise ValidationError('AppDir cannot be empty', filename)
            elif child.tagName == 'DefaultAppDirs':
                __parseDefaultAppDir(filename, parent)
            elif child.tagName == 'DirectoryDir':
                try:
                    __parseDirectoryDir(child.childNodes[0].nodeValue, filename, parent)
                except IndexError:
                    raise ValidationError('DirectoryDir cannot be empty', filename)
            elif child.tagName == 'DefaultDirectoryDirs':
                __parseDefaultDirectoryDir(filename, parent)
            elif child.tagName == 'Name' :
                try:
                    parent.Name = child.childNodes[0].nodeValue
                except IndexError:
                    raise ValidationError('Name cannot be empty', filename)
            elif child.tagName == 'Directory' :
                try:
                    parent.Directories.append(child.childNodes[0].nodeValue)
                except IndexError:
                    raise ValidationError('Directory cannot be empty', filename)
            elif child.tagName == 'OnlyUnallocated':
                parent.OnlyUnallocated = True
            elif child.tagName == 'NotOnlyUnallocated':
                parent.OnlyUnallocated = False
            elif child.tagName == 'Deleted':
                parent.Deleted = True
            elif child.tagName == 'NotDeleted':
                parent.Deleted = False
            elif child.tagName == 'Include' or child.tagName == 'Exclude':
                parent.Rules.append(Rule(child.tagName, child))
            elif child.tagName == 'MergeFile':
                try:
                    if child.getAttribute("type") == "parent":
                        __parseMergeFile("applications.menu", child, filename, parent)
                    else:
                        __parseMergeFile(child.childNodes[0].nodeValue, child, filename, parent)
                except IndexError:
                    raise ValidationError('MergeFile cannot be empty', filename)
            elif child.tagName == 'MergeDir':
                try:
                    __parseMergeDir(child.childNodes[0].nodeValue, child, filename, parent)
                except IndexError:
                    raise ValidationError('MergeDir cannot be empty', filename)
            elif child.tagName == 'DefaultMergeDirs':
                __parseDefaultMergeDirs(child, filename, parent)
            elif child.tagName == 'Move':
                parent.Moves.append(Move(child))
            elif child.tagName == 'Layout':
                if len(child.childNodes) > 1:
                    parent.Layout = Layout(child)
            elif child.tagName == 'DefaultLayout':
                if len(child.childNodes) > 1:
                    parent.DefaultLayout = Layout(child)
            elif child.tagName == 'LegacyDir':
                try:
                    __parseLegacyDir(child.childNodes[0].nodeValue, child.getAttribute("prefix"), filename, parent)
                except IndexError:
                    raise ValidationError('LegacyDir cannot be empty', filename)
            elif child.tagName == 'KDELegacyDirs':
                __parseKDELegacyDirs(filename, parent)

def __parsemove(menu):
    for submenu in menu.Submenus:
        __parsemove(submenu)

    # parse move operations
    for move in menu.Moves:
        move_from_menu = menu.getMenu(move.Old)
        if move_from_menu:
            move_to_menu = menu.getMenu(move.New)

            menus = move.New.split("/")
            oldparent = None
            while len(menus) > 0:
                if not oldparent:
                    oldparent = menu
                newmenu = oldparent.getMenu(menus[0])
                if not newmenu:
                    newmenu = Menu()
                    newmenu.Name = menus[0]
                    if len(menus) > 1:
                        newmenu.NotInXml = True
                    oldparent.addSubmenu(newmenu)
                oldparent = newmenu
                menus.pop(0)

            newmenu += move_from_menu
            move_from_menu.Parent.Submenus.remove(move_from_menu)

def __postparse(menu):
    # unallocated / deleted
    if menu.Deleted == "notset":
        menu.Deleted = False
    if menu.OnlyUnallocated == "notset":
        menu.OnlyUnallocated = False

    # Layout Tags
    if not menu.Layout or not menu.DefaultLayout:
        if menu.DefaultLayout:
            menu.Layout = menu.DefaultLayout
        elif menu.Layout:
            if menu.Depth > 0:
                menu.DefaultLayout = menu.Parent.DefaultLayout
            else:
                menu.DefaultLayout = Layout()
        else:
            if menu.Depth > 0:
                menu.Layout = menu.Parent.DefaultLayout
                menu.DefaultLayout = menu.Parent.DefaultLayout
            else:
                menu.Layout = Layout()
                menu.DefaultLayout = Layout()

    # add parent's app/directory dirs
    if menu.Depth > 0:
        menu.AppDirs = menu.Parent.AppDirs + menu.AppDirs
        menu.DirectoryDirs = menu.Parent.DirectoryDirs + menu.DirectoryDirs

    # remove duplicates
    menu.Directories = __removeDuplicates(menu.Directories)
    menu.DirectoryDirs = __removeDuplicates(menu.DirectoryDirs)
    menu.AppDirs = __removeDuplicates(menu.AppDirs)

    # go recursive through all menus
    for submenu in menu.Submenus:
        __postparse(submenu)

    # reverse so handling is easier
    menu.Directories.reverse()
    menu.DirectoryDirs.reverse()
    menu.AppDirs.reverse()

    # get the valid .directory file out of the list
    for directory in menu.Directories:
        for dir in menu.DirectoryDirs:
            if os.path.isfile(os.path.join(dir, directory)):
                menuentry = MenuEntry(directory, dir)
                if not menu.Directory:
                    menu.Directory = menuentry
                elif menuentry.getType() == "System":
                    if menu.Directory.getType() == "User":
                        menu.Directory.Original = menuentry
        if menu.Directory:
            break


# Menu parsing stuff
def __parseMenu(child, filename, parent):
    m = Menu()
    __parse(child, filename, m)
    if parent:
        parent.addSubmenu(m)
    else:
        tmp["Root"] = m

# helper function
def __check(value, filename, type):
    path = os.path.dirname(filename)

    if not os.path.isabs(value):
        value = os.path.join(path, value)

    value = os.path.abspath(value)

    if type == "dir" and os.path.exists(value) and os.path.isdir(value):
        return value
    elif type == "file" and os.path.exists(value) and os.path.isfile(value):
        return value
    else:
        return False

# App/Directory Dir Stuff
def __parseAppDir(value, filename, parent):
    value = __check(value, filename, "dir")
    if value:
        parent.AppDirs.append(value)

def __parseDefaultAppDir(filename, parent):
    for dir in reversed(xdg_data_dirs):
        __parseAppDir(os.path.join(dir, "applications"), filename, parent)

def __parseDirectoryDir(value, filename, parent):
    value = __check(value, filename, "dir")
    if value:
        parent.DirectoryDirs.append(value)

def __parseDefaultDirectoryDir(filename, parent):
    for dir in reversed(xdg_data_dirs):
        __parseDirectoryDir(os.path.join(dir, "desktop-directories"), filename, parent)

# Merge Stuff
def __parseMergeFile(value, child, filename, parent):
    if child.getAttribute("type") == "parent":
        for dir in xdg_config_dirs:
            rel_file = filename.replace(dir, "").strip("/")
            if rel_file != filename:
                for p in xdg_config_dirs:
                    if dir == p:
                        continue
                    if os.path.isfile(os.path.join(p,rel_file)):
                        __mergeFile(os.path.join(p,rel_file),child,parent)
                        break
    else:
        value = __check(value, filename, "file")
        if value:
            __mergeFile(value, child, parent)

def __parseMergeDir(value, child, filename, parent):
    value = __check(value, filename, "dir")
    if value:
        for item in os.listdir(value):
            try:
                if os.path.splitext(item)[1] == ".menu":
                    __mergeFile(os.path.join(value, item), child, parent)
            except UnicodeDecodeError:
                continue

def __parseDefaultMergeDirs(child, filename, parent):
    basename = os.path.splitext(os.path.basename(filename))[0]
    for dir in reversed(xdg_config_dirs):
        __parseMergeDir(os.path.join(dir, "menus", basename + "-merged"), child, filename, parent)

def __mergeFile(filename, child, parent):
    # check for infinite loops
    if filename in tmp["mergeFiles"]:
        if debug:
            raise ParsingError('Infinite MergeFile loop detected', filename)
        else:
            return

    tmp["mergeFiles"].append(filename)

    # load file
    try:
        doc = xml.dom.minidom.parse(filename)
    except IOError:
        if debug:
            raise ParsingError('File not found', filename)
        else:
            return
    except xml.parsers.expat.ExpatError:
        if debug:
            raise ParsingError('Not a valid .menu file', filename)
        else:
            return

    # append file
    for child in doc.childNodes:
        if child.nodeType == ELEMENT_NODE:
            __parse(child,filename,parent)
            break

# Legacy Dir Stuff
def __parseLegacyDir(dir, prefix, filename, parent):
    m = __mergeLegacyDir(dir,prefix,filename,parent)
    if m:
        parent += m

def __mergeLegacyDir(dir, prefix, filename, parent):
    dir = __check(dir,filename,"dir")
    if dir and dir not in tmp["DirectoryDirs"]:
        tmp["DirectoryDirs"].append(dir)

        m = Menu()
        m.AppDirs.append(dir)
        m.DirectoryDirs.append(dir)
        m.Name = os.path.basename(dir)
        m.NotInXml = True

        for item in os.listdir(dir):
            try:
                if item == ".directory":
                    m.Directories.append(item)
                elif os.path.isdir(os.path.join(dir,item)):
                    m.addSubmenu(__mergeLegacyDir(os.path.join(dir,item), prefix, filename, parent))
            except UnicodeDecodeError:
                continue

        tmp["cache"].addMenuEntries([dir],prefix, True)
        menuentries = tmp["cache"].getMenuEntries([dir], False)

        for menuentry in menuentries:
            categories = menuentry.Categories
            if len(categories) == 0:
                r = Rule("Include")
                r.parseFilename(menuentry.DesktopFileID)
                r.compile()
                m.Rules.append(r)
            if not dir in parent.AppDirs:
                categories.append("Legacy")
                menuentry.Categories = categories

        return m

def __parseKDELegacyDirs(filename, parent):
    f=os.popen3("kde-config --path apps")
    output = f[1].readlines()
    try:
        for dir in output[0].split(":"):
            __parseLegacyDir(dir,"kde", filename, parent)
    except IndexError:
        pass

# remove duplicate entries from a list
def __removeDuplicates(list):
    set = {}
    list.reverse()
    list = [set.setdefault(e,e) for e in list if e not in set]
    list.reverse()
    return list

# Finally generate the menu
def __genmenuNotOnlyAllocated(menu):
    for submenu in menu.Submenus:
        __genmenuNotOnlyAllocated(submenu)

    if menu.OnlyUnallocated == False:
        tmp["cache"].addMenuEntries(menu.AppDirs)
        menuentries = []
        for rule in menu.Rules:
            menuentries = rule.do(tmp["cache"].getMenuEntries(menu.AppDirs), rule.Type, 1)
        for menuentry in menuentries:
            if menuentry.Add == True:
                menuentry.Parents.append(menu)
                menuentry.Add = False
                menuentry.Allocated = True
                menu.MenuEntries.append(menuentry)

def __genmenuOnlyAllocated(menu):
    for submenu in menu.Submenus:
        __genmenuOnlyAllocated(submenu)

    if menu.OnlyUnallocated == True:
        tmp["cache"].addMenuEntries(menu.AppDirs)
        menuentries = []
        for rule in menu.Rules:
            menuentries = rule.do(tmp["cache"].getMenuEntries(menu.AppDirs), rule.Type, 2)
        for menuentry in menuentries:
            if menuentry.Add == True:
                menuentry.Parents.append(menu)
            #   menuentry.Add = False
            #   menuentry.Allocated = True
                menu.MenuEntries.append(menuentry)

# And sorting ...
def sort(menu):
    menu.Entries = []
    menu.Visible = 0

    for submenu in menu.Submenus:
        sort(submenu)

    tmp_s = []
    tmp_e = []

    for order in menu.Layout.order:
        if order[0] == "Filename":
            tmp_e.append(order[1])
        elif order[0] == "Menuname":
            tmp_s.append(order[1])
    
    for order in menu.Layout.order:
        if order[0] == "Separator":
            separator = Separator(menu)
            if len(menu.Entries) > 0 and isinstance(menu.Entries[-1], Separator):
                separator.Show = False
            menu.Entries.append(separator)
        elif order[0] == "Filename":
            menuentry = menu.getMenuEntry(order[1])
            if menuentry:
                menu.Entries.append(menuentry)
        elif order[0] == "Menuname":
            submenu = menu.getMenu(order[1])
            if submenu:
                __parse_inline(submenu, menu)
        elif order[0] == "Merge":
            if order[1] == "files" or order[1] == "all":
                menu.MenuEntries.sort()
                for menuentry in menu.MenuEntries:
                    if menuentry not in tmp_e:
                        menu.Entries.append(menuentry)
            elif order[1] == "menus" or order[1] == "all":
                menu.Submenus.sort()
                for submenu in menu.Submenus:
                    if submenu.Name not in tmp_s:
                        __parse_inline(submenu, menu)

    # getHidden / NoDisplay / OnlyShowIn / NotOnlyShowIn / Deleted / NoExec
    for entry in menu.Entries:
        entry.Show = True
        menu.Visible += 1
        if isinstance(entry, Menu):
            if entry.Deleted == True:
                entry.Show = "Deleted"
                menu.Visible -= 1
            elif isinstance(entry.Directory, MenuEntry):
                if entry.Directory.DesktopEntry.getNoDisplay() == True:
                    entry.Show = "NoDisplay"
                    menu.Visible -= 1
                elif entry.Directory.DesktopEntry.getHidden() == True:
                    entry.Show = "Hidden"
                    menu.Visible -= 1
        elif isinstance(entry, MenuEntry):
            if entry.DesktopEntry.getNoDisplay() == True:
                entry.Show = "NoDisplay"
                menu.Visible -= 1
            elif entry.DesktopEntry.getHidden() == True:
                entry.Show = "Hidden"
                menu.Visible -= 1
            elif entry.DesktopEntry.getTryExec() and not __try_exec(entry.DesktopEntry.getTryExec()):
                entry.Show = "NoExec"
                menu.Visible -= 1
            elif xdg.Config.windowmanager:
                if ( entry.DesktopEntry.getOnlyShowIn() != [] and xdg.Config.windowmanager not in entry.DesktopEntry.getOnlyShowIn() ) \
                or xdg.Config.windowmanager in entry.DesktopEntry.getNotShowIn():
                    entry.Show = "NotShowIn"
                    menu.Visible -= 1
        elif isinstance(entry,Separator):
            menu.Visible -= 1

    # remove separators at the beginning and at the end
    if len(menu.Entries) > 0:
        if isinstance(menu.Entries[0], Separator):
            menu.Entries[0].Show = False
    if len(menu.Entries) > 1:
        if isinstance(menu.Entries[-1], Separator):
            menu.Entries[-1].Show = False

    # show_empty tag
    for entry in menu.Entries:
        if isinstance(entry,Menu) and entry.Layout.show_empty == "false" and entry.Visible == 0:
            entry.Show = "Empty"
            menu.Visible -= 1
            if entry.NotInXml == True:
                menu.Entries.remove(entry)

def __try_exec(executable):
    paths = os.environ['PATH'].split(os.pathsep)
    if not os.path.isfile(executable):
        for p in paths:
            f = os.path.join(p, executable)
            if os.path.isfile(f):
                if os.access(f, os.X_OK):
                    return True
    else:
        if os.access(executable, os.X_OK):
            return True
    return False

# inline tags
def __parse_inline(submenu, menu):
    if submenu.Layout.inline == "true":
        if len(submenu.Entries) == 1 and submenu.Layout.inline_alias == "true":
            menuentry = submenu.Entries[0]
            menuentry.DesktopEntry.set("Name", submenu.getName(), locale = True)
            menuentry.DesktopEntry.set("GenericName", submenu.getGenericName(), locale = True)
            menuentry.DesktopEntry.set("Comment", submenu.getComment(), locale = True)
            menu.Entries.append(menuentry)
        elif len(submenu.Entries) <= submenu.Layout.inline_limit or submenu.Layout.inline_limit == 0:
            if submenu.Layout.inline_header == "true":
                header = Header(submenu.getName(), submenu.getGenericName(), submenu.getComment())
                menu.Entries.append(header)
            for entry in submenu.Entries:
                menu.Entries.append(entry)
        else:
            menu.Entries.append(submenu)
    else:
        menu.Entries.append(submenu)

class MenuEntryCache:
    "Class to cache Desktop Entries"
    def __init__(self):
        self.cacheEntries = {}
        self.cacheEntries['legacy'] = []
        self.cache = {}

    def addMenuEntries(self, dirs, prefix="", legacy=False):
        for dir in dirs:
            if not self.cacheEntries.has_key(dir):
                self.cacheEntries[dir] = []
                self.__addFiles(dir, "", prefix, legacy)

    def __addFiles(self, dir, subdir, prefix, legacy):
        for item in os.listdir(os.path.join(dir,subdir)):
            if os.path.splitext(item)[1] == ".desktop":
                try:
                    menuentry = MenuEntry(os.path.join(subdir,item), dir, prefix)
                except ParsingError:
                    continue

                self.cacheEntries[dir].append(menuentry)
                if legacy == True:
                    self.cacheEntries['legacy'].append(menuentry)
            elif os.path.isdir(os.path.join(dir,subdir,item)) and legacy == False:
                self.__addFiles(dir, os.path.join(subdir,item), prefix, legacy)

    def getMenuEntries(self, dirs, legacy=True):
        list = []
        ids = []
        # handle legacy items
        appdirs = dirs[:]
        if legacy == True:
            appdirs.append("legacy")
        # cache the results again
        key = "".join(appdirs)
        try:
            return self.cache[key]
        except KeyError:
            pass
        for dir in appdirs:
            for menuentry in self.cacheEntries[dir]:
                try:
                    if menuentry.DesktopFileID not in ids:
                        ids.append(menuentry.DesktopFileID)
                        list.append(menuentry)
                    elif menuentry.getType() == "System":
                    # FIXME: This is only 99% correct, but still...
                        i = list.index(menuentry)
                        e = list[i]
                        if e.getType() == "User":
                            e.Original = menuentry
                except UnicodeDecodeError:
                    continue
        self.cache[key] = list
        return list
