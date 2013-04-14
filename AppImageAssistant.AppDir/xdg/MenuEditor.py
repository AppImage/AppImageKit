""" CLass to edit XDG Menus """

from xdg.Menu import *
from xdg.BaseDirectory import *
from xdg.Exceptions import *
from xdg.DesktopEntry import *
from xdg.Config import *

import xml.dom.minidom
import os
import re

# XML-Cleanups: Move / Exclude
# FIXME: proper reverte/delete
# FIXME: pass AppDirs/DirectoryDirs around in the edit/move functions
# FIXME: catch Exceptions
# FIXME: copy functions
# FIXME: More Layout stuff
# FIXME: unod/redo function / remove menu...
# FIXME: Advanced MenuEditing Stuff: LegacyDir/MergeFile
#        Complex Rules/Deleted/OnlyAllocated/AppDirs/DirectoryDirs

class MenuEditor:
    def __init__(self, menu=None, filename=None, root=False):
        self.menu = None
        self.filename = None
        self.doc = None
        self.parse(menu, filename, root)

        # fix for creating two menus with the same name on the fly
        self.filenames = []

    def parse(self, menu=None, filename=None, root=False):
        if root == True:
            setRootMode(True)

        if isinstance(menu, Menu):
            self.menu = menu
        elif menu:
            self.menu = parse(menu)
        else:
            self.menu = parse()

        if root == True:
            self.filename = self.menu.Filename
        elif filename:
            self.filename = filename
        else:
            self.filename = os.path.join(xdg_config_dirs[0], "menus", os.path.split(self.menu.Filename)[1])

        try:
            self.doc = xml.dom.minidom.parse(self.filename)
        except IOError:
            self.doc = xml.dom.minidom.parseString('<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" "http://standards.freedesktop.org/menu-spec/menu-1.0.dtd"><Menu><Name>Applications</Name><MergeFile type="parent">'+self.menu.Filename+'</MergeFile></Menu>')
        except xml.parsers.expat.ExpatError:
            raise ParsingError('Not a valid .menu file', self.filename)

        self.__remove_whilespace_nodes(self.doc)

    def save(self):
        self.__saveEntries(self.menu)
        self.__saveMenu()

    def createMenuEntry(self, parent, name, command=None, genericname=None, comment=None, icon=None, terminal=None, after=None, before=None):
        menuentry = MenuEntry(self.__getFileName(name, ".desktop"))
        menuentry = self.editMenuEntry(menuentry, name, genericname, comment, command, icon, terminal)

        self.__addEntry(parent, menuentry, after, before)

        sort(self.menu)

        return menuentry

    def createMenu(self, parent, name, genericname=None, comment=None, icon=None, after=None, before=None):
        menu = Menu()

        menu.Parent = parent
        menu.Depth = parent.Depth + 1
        menu.Layout = parent.DefaultLayout
        menu.DefaultLayout = parent.DefaultLayout

        menu = self.editMenu(menu, name, genericname, comment, icon)

        self.__addEntry(parent, menu, after, before)

        sort(self.menu)

        return menu

    def createSeparator(self, parent, after=None, before=None):
        separator = Separator(parent)

        self.__addEntry(parent, separator, after, before)

        sort(self.menu)

        return separator

    def moveMenuEntry(self, menuentry, oldparent, newparent, after=None, before=None):
        self.__deleteEntry(oldparent, menuentry, after, before)
        self.__addEntry(newparent, menuentry, after, before)

        sort(self.menu)

        return menuentry

    def moveMenu(self, menu, oldparent, newparent, after=None, before=None):
        self.__deleteEntry(oldparent, menu, after, before)
        self.__addEntry(newparent, menu, after, before)

        root_menu = self.__getXmlMenu(self.menu.Name)
        if oldparent.getPath(True) != newparent.getPath(True):
            self.__addXmlMove(root_menu, os.path.join(oldparent.getPath(True), menu.Name), os.path.join(newparent.getPath(True), menu.Name))

        sort(self.menu)

        return menu

    def moveSeparator(self, separator, parent, after=None, before=None):
        self.__deleteEntry(parent, separator, after, before)
        self.__addEntry(parent, separator, after, before)

        sort(self.menu)

        return separator

    def copyMenuEntry(self, menuentry, oldparent, newparent, after=None, before=None):
        self.__addEntry(newparent, menuentry, after, before)

        sort(self.menu)

        return menuentry

    def editMenuEntry(self, menuentry, name=None, genericname=None, comment=None, command=None, icon=None, terminal=None, nodisplay=None, hidden=None):
        deskentry = menuentry.DesktopEntry

        if name:
            if not deskentry.hasKey("Name"):
                deskentry.set("Name", name)
            deskentry.set("Name", name, locale = True)
        if comment:
            if not deskentry.hasKey("Comment"):
                deskentry.set("Comment", comment)
            deskentry.set("Comment", comment, locale = True)
        if genericname:
            if not deskentry.hasKey("GnericNe"):
                deskentry.set("GenericName", genericname)
            deskentry.set("GenericName", genericname, locale = True)
        if command:
            deskentry.set("Exec", command)
        if icon:
            deskentry.set("Icon", icon)

        if terminal == True:
            deskentry.set("Terminal", "true")
        elif terminal == False:
            deskentry.set("Terminal", "false")

        if nodisplay == True:
            deskentry.set("NoDisplay", "true")
        elif nodisplay == False:
            deskentry.set("NoDisplay", "false")

        if hidden == True:
            deskentry.set("Hidden", "true")
        elif hidden == False:
            deskentry.set("Hidden", "false")

        menuentry.updateAttributes()

        if len(menuentry.Parents) > 0:
            sort(self.menu)

        return menuentry

    def editMenu(self, menu, name=None, genericname=None, comment=None, icon=None, nodisplay=None, hidden=None):
        # Hack for legacy dirs
        if isinstance(menu.Directory, MenuEntry) and menu.Directory.Filename == ".directory":
            xml_menu = self.__getXmlMenu(menu.getPath(True, True))
            self.__addXmlTextElement(xml_menu, 'Directory', menu.Name + ".directory")
            menu.Directory.setAttributes(menu.Name + ".directory")
        # Hack for New Entries
        elif not isinstance(menu.Directory, MenuEntry):
            if not name:
                name = menu.Name
            filename = self.__getFileName(name, ".directory").replace("/", "")
            if not menu.Name:
                menu.Name = filename.replace(".directory", "")
            xml_menu = self.__getXmlMenu(menu.getPath(True, True))
            self.__addXmlTextElement(xml_menu, 'Directory', filename)
            menu.Directory = MenuEntry(filename)

        deskentry = menu.Directory.DesktopEntry

        if name:
            if not deskentry.hasKey("Name"):
                deskentry.set("Name", name)
            deskentry.set("Name", name, locale = True)
        if genericname:
            if not deskentry.hasKey("GenericName"):
                deskentry.set("GenericName", genericname)
            deskentry.set("GenericName", genericname, locale = True)
        if comment:
            if not deskentry.hasKey("Comment"):
                deskentry.set("Comment", comment)
            deskentry.set("Comment", comment, locale = True)
        if icon:
            deskentry.set("Icon", icon)

        if nodisplay == True:
            deskentry.set("NoDisplay", "true")
        elif nodisplay == False:
            deskentry.set("NoDisplay", "false")

        if hidden == True:
            deskentry.set("Hidden", "true")
        elif hidden == False:
            deskentry.set("Hidden", "false")

        menu.Directory.updateAttributes()

        if isinstance(menu.Parent, Menu):
            sort(self.menu)

        return menu

    def hideMenuEntry(self, menuentry):
        self.editMenuEntry(menuentry, nodisplay = True)

    def unhideMenuEntry(self, menuentry):
        self.editMenuEntry(menuentry, nodisplay = False, hidden = False)

    def hideMenu(self, menu):
        self.editMenu(menu, nodisplay = True)

    def unhideMenu(self, menu):
        self.editMenu(menu, nodisplay = False, hidden = False)
        xml_menu = self.__getXmlMenu(menu.getPath(True,True), False)
        for node in self.__getXmlNodesByName(["Deleted", "NotDeleted"], xml_menu):
            node.parentNode.removeChild(node)

    def deleteMenuEntry(self, menuentry):
        if self.getAction(menuentry) == "delete":
            self.__deleteFile(menuentry.DesktopEntry.filename)
            for parent in menuentry.Parents:
                self.__deleteEntry(parent, menuentry)
            sort(self.menu)
        return menuentry

    def revertMenuEntry(self, menuentry):
        if self.getAction(menuentry) == "revert":
            self.__deleteFile(menuentry.DesktopEntry.filename)
            menuentry.Original.Parents = []
            for parent in menuentry.Parents:
                index = parent.Entries.index(menuentry)
                parent.Entries[index] = menuentry.Original
                index = parent.MenuEntries.index(menuentry)
                parent.MenuEntries[index] = menuentry.Original
                menuentry.Original.Parents.append(parent)
            sort(self.menu)
        return menuentry

    def deleteMenu(self, menu):
        if self.getAction(menu) == "delete":
            self.__deleteFile(menu.Directory.DesktopEntry.filename)
            self.__deleteEntry(menu.Parent, menu)
            xml_menu = self.__getXmlMenu(menu.getPath(True, True))
            xml_menu.parentNode.removeChild(xml_menu)
            sort(self.menu)
        return menu

    def revertMenu(self, menu):
        if self.getAction(menu) == "revert":
            self.__deleteFile(menu.Directory.DesktopEntry.filename)
            menu.Directory = menu.Directory.Original
            sort(self.menu)
        return menu

    def deleteSeparator(self, separator):
        self.__deleteEntry(separator.Parent, separator, after=True)

        sort(self.menu)

        return separator

    """ Private Stuff """
    def getAction(self, entry):
        if isinstance(entry, Menu):
            if not isinstance(entry.Directory, MenuEntry):
                return "none"
            elif entry.Directory.getType() == "Both":
                return "revert"
            elif entry.Directory.getType() == "User" \
            and (len(entry.Submenus) + len(entry.MenuEntries)) == 0:
                return "delete"

        elif isinstance(entry, MenuEntry):
            if entry.getType() == "Both":
                return "revert"
            elif entry.getType() == "User":
                return "delete"
            else:
                return "none"

        return "none"

    def __saveEntries(self, menu):
        if not menu:
            menu = self.menu
        if isinstance(menu.Directory, MenuEntry):
            menu.Directory.save()
        for entry in menu.getEntries(hidden=True):
            if isinstance(entry, MenuEntry):
                entry.save()
            elif isinstance(entry, Menu):
                self.__saveEntries(entry)

    def __saveMenu(self):
        if not os.path.isdir(os.path.dirname(self.filename)):
            os.makedirs(os.path.dirname(self.filename))
        fd = open(self.filename, 'w')
        fd.write(re.sub("\n[\s]*([^\n<]*)\n[\s]*</", "\\1</", self.doc.toprettyxml().replace('<?xml version="1.0" ?>\n', '')))
        fd.close()

    def __getFileName(self, name, extension):
        postfix = 0
        while 1:
            if postfix == 0:
                filename = name + extension
            else:
                filename = name + "-" + str(postfix) + extension
            if extension == ".desktop":
                dir = "applications"
            elif extension == ".directory":
                dir = "desktop-directories"
            if not filename in self.filenames and not \
                os.path.isfile(os.path.join(xdg_data_dirs[0], dir, filename)):
                self.filenames.append(filename)
                break
            else:
                postfix += 1

        return filename

    def __getXmlMenu(self, path, create=True, element=None):
        if not element:
            element = self.doc

        if "/" in path:
            (name, path) = path.split("/", 1)
        else:
            name = path
            path = ""

        found = None
        for node in self.__getXmlNodesByName("Menu", element):
            for child in self.__getXmlNodesByName("Name", node):
                if child.childNodes[0].nodeValue == name:
                    if path:
                        found = self.__getXmlMenu(path, create, node)
                    else:
                        found = node
                    break
            if found:
                break
        if not found and create == True:
            node = self.__addXmlMenuElement(element, name)
            if path:
                found = self.__getXmlMenu(path, create, node)
            else:
                found = node

        return found

    def __addXmlMenuElement(self, element, name):
        node = self.doc.createElement('Menu')
        self.__addXmlTextElement(node, 'Name', name)
        return element.appendChild(node)

    def __addXmlTextElement(self, element, name, text):
        node = self.doc.createElement(name)
        text = self.doc.createTextNode(text)
        node.appendChild(text)
        return element.appendChild(node)

    def __addXmlFilename(self, element, filename, type = "Include"):
        # remove old filenames
        for node in self.__getXmlNodesByName(["Include", "Exclude"], element):
            if node.childNodes[0].nodeName == "Filename" and node.childNodes[0].childNodes[0].nodeValue == filename:
                element.removeChild(node)

        # add new filename
        node = self.doc.createElement(type)
        node.appendChild(self.__addXmlTextElement(node, 'Filename', filename))
        return element.appendChild(node)

    def __addXmlMove(self, element, old, new):
        node = self.doc.createElement("Move")
        node.appendChild(self.__addXmlTextElement(node, 'Old', old))
        node.appendChild(self.__addXmlTextElement(node, 'New', new))
        return element.appendChild(node)

    def __addXmlLayout(self, element, layout):
        # remove old layout
        for node in self.__getXmlNodesByName("Layout", element):
            element.removeChild(node)

        # add new layout
        node = self.doc.createElement("Layout")
        for order in layout.order:
            if order[0] == "Separator":
                child = self.doc.createElement("Separator")
                node.appendChild(child)
            elif order[0] == "Filename":
                child = self.__addXmlTextElement(node, "Filename", order[1])
            elif order[0] == "Menuname":
                child = self.__addXmlTextElement(node, "Menuname", order[1])
            elif order[0] == "Merge":
                child = self.doc.createElement("Merge")
                child.setAttribute("type", order[1])
                node.appendChild(child)
        return element.appendChild(node)

    def __getXmlNodesByName(self, name, element):
        for child in element.childNodes:
            if child.nodeType == xml.dom.Node.ELEMENT_NODE and child.nodeName in name:
                yield child

    def __addLayout(self, parent):
        layout = Layout()
        layout.order = []
        layout.show_empty = parent.Layout.show_empty
        layout.inline = parent.Layout.inline
        layout.inline_header = parent.Layout.inline_header
        layout.inline_alias = parent.Layout.inline_alias
        layout.inline_limit = parent.Layout.inline_limit

        layout.order.append(["Merge", "menus"])
        for entry in parent.Entries:
            if isinstance(entry, Menu):
                layout.parseMenuname(entry.Name)
            elif isinstance(entry, MenuEntry):
                layout.parseFilename(entry.DesktopFileID)
            elif isinstance(entry, Separator):
                layout.parseSeparator()
        layout.order.append(["Merge", "files"])

        parent.Layout = layout

        return layout

    def __addEntry(self, parent, entry, after=None, before=None):
        if after or before:
            if after:
                index = parent.Entries.index(after) + 1
            elif before:
                index = parent.Entries.index(before)
            parent.Entries.insert(index, entry)
        else:
            parent.Entries.append(entry)

        xml_parent = self.__getXmlMenu(parent.getPath(True, True))

        if isinstance(entry, MenuEntry):
            parent.MenuEntries.append(entry)
            entry.Parents.append(parent)
            self.__addXmlFilename(xml_parent, entry.DesktopFileID, "Include")
        elif isinstance(entry, Menu):
            parent.addSubmenu(entry)

        if after or before:
            self.__addLayout(parent)
            self.__addXmlLayout(xml_parent, parent.Layout)

    def __deleteEntry(self, parent, entry, after=None, before=None):
        parent.Entries.remove(entry)

        xml_parent = self.__getXmlMenu(parent.getPath(True, True))

        if isinstance(entry, MenuEntry):
            entry.Parents.remove(parent)
            parent.MenuEntries.remove(entry)
            self.__addXmlFilename(xml_parent, entry.DesktopFileID, "Exclude")
        elif isinstance(entry, Menu):
            parent.Submenus.remove(entry)

        if after or before:
            self.__addLayout(parent)
            self.__addXmlLayout(xml_parent, parent.Layout)

    def __deleteFile(self, filename):
        try:
            os.remove(filename)
        except OSError:
            pass
        try:
            self.filenames.remove(filename)
        except ValueError:
            pass

    def __remove_whilespace_nodes(self, node):
        remove_list = []
        for child in node.childNodes:
            if child.nodeType == xml.dom.minidom.Node.TEXT_NODE:
                child.data = child.data.strip()
                if not child.data.strip():
                    remove_list.append(child)
            elif child.hasChildNodes():
                self.__remove_whilespace_nodes(child)
        for node in remove_list:
            node.parentNode.removeChild(node)
