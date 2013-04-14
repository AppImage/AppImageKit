"""
Complete implementation of the XDG Desktop Entry Specification Version 0.9.4
http://standards.freedesktop.org/desktop-entry-spec/

Not supported:
- Encoding: Legacy Mixed
- Does not check exec parameters
- Does not check URL's
- Does not completly validate deprecated/kde items
- Does not completly check categories
"""

from xdg.IniFile import *
from xdg.BaseDirectory import *
import os.path

class DesktopEntry(IniFile):
    "Class to parse and validate DesktopEntries"

    defaultGroup = 'Desktop Entry'

    def __init__(self, filename=None):
        self.content = dict()
        if filename and os.path.exists(filename):
            self.parse(filename)
        elif filename:
            self.new(filename)

    def __str__(self):
        return self.getName()

    def parse(self, file):
        IniFile.parse(self, file, ["Desktop Entry", "KDE Desktop Entry"])

    # start standard keys
    def getType(self):
        return self.get('Type')
    def getVersion(self):
        return self.get('Version', type="numeric")
    def getEncoding(self):
        return self.get('Encoding')
    def getName(self):
        return self.get('Name', locale=True)
    def getGenericName(self):
        return self.get('GenericName', locale=True)
    def getComment(self):
        return self.get('Comment', locale=True)
    def getNoDisplay(self):
        return self.get('NoDisplay', type="boolean")
    def getIcon(self):
        return self.get('Icon', locale=True)
    def getHidden(self):
        return self.get('Hidden', type="boolean")
    def getFilePattern(self):
        return self.get('FilePattern', type="regex")
    def getTryExec(self):
        return self.get('TryExec')
    def getExec(self):
        return self.get('Exec')
    def getPath(self):
        return self.get('Path')
    def getTerminal(self):
        return self.get('Terminal', type="boolean")
    def getSwallowTitle(self):
        return self.get('SwallowTitle', locale=True)
    def getSwallowExec(self):
        return self.get('SwallowExec')
    def getActions(self):
        return self.get('Actions', list=True)
    """ @deprecated, use getMimeTypes instead """
    def getMimeType(self):
        return self.get('MimeType', list=True, type="regex")
    def getMimeTypes(self):
        return self.get('MimeType', list=True)
    def getSortOrder(self): 
        return self.get('SortOrder', list=True)
    def getDev(self):
        return self.get('Dev')
    def getFSType(self):
        return self.get('FSType')
    def getMountPoint(self):
        return self.get('MountPoint')
    def getReadonly(self):
        return self.get('ReadOnly', type="boolean")
    def getUnmountIcon(self):
        return self.get('UnmountIcon', locale=True)
    def getURL(self):
        return self.get('URL')
    def getCategories(self):
        return self.get('Categories', list=True)
    def getOnlyShowIn(self):
        return self.get('OnlyShowIn', list=True)
    def getNotShowIn(self):
        return self.get('NotShowIn', list=True)
    def getStartupNotify(self):
        return self.get('StartupNotify', type="boolean")
    def getStartupWMClass(self):
        return self.get('StartupWMClass')
    # end standard keys

    # start kde keys
    def getServiceTypes(self):
        return self.get('ServiceTypes', list=True)
    def getDocPath(self):
        return self.get('DocPath')
    def getKeywords(self):
        return self.get('Keywords', list=True, locale=True)
    def getInitialPreference(self):
        return self.get('InitialPreference')
    # end kde keys

    # start deprecated keys
    def getMiniIcon(self):
        return self.get('MiniIcon', locale=True)
    def getTerminalOptions(self):
        return self.get('TerminalOptions')
    def getDefaultApp(self):
        return self.get('DefaultApp')
    def getProtocols(self):
        return self.get('Protocols', list=True)
    def getExtensions(self):
        return self.get('Extensions', list=True)
    def getBinaryPattern(self):
        return self.get('BinaryPattern')
    def getMapNotify(self):
        return self.get('MapNotify')
    # end deprecated keys

    # desktop entry edit stuff
    def new(self, filename):
        if os.path.splitext(filename)[1] == ".desktop":
            type = "Application"
        elif os.path.splitext(filename)[1] == ".directory":
            type = "Directory"
        self.content = dict()
        self.addGroup(self.defaultGroup)
        self.set("Encoding", "UTF-8")
        self.set("Type", type)
        self.filename = filename
    # end desktop entry edit stuff

    # validation stuff
    def checkExtras(self):
        # header
        if self.defaultGroup == "KDE Desktop Entry":
            self.warnings.append('[KDE Desktop Entry]-Header is deprecated')

        # file extension
        if self.fileExtension == ".kdelnk":
            self.warnings.append("File extension .kdelnk is deprecated")
        elif self.fileExtension != ".desktop" and self.fileExtension != ".directory":
            self.warnings.append('Unknown File extension')

        # Type
        try:
            self.type = self.content[self.defaultGroup]["Type"]
        except KeyError:
            self.errors.append("Key 'Type' is missing")

        # Encoding
        try:
            self.encoding = self.content[self.defaultGroup]["Encoding"]
        except KeyError:
            self.errors.append("Key 'Encoding' is missing")

        # Version
        try:
            self.version = self.content[self.defaultGroup]["Version"]
        except KeyError:
            self.warnings.append("Key 'Version' is missing")

        # Name
        try:
            self.name = self.content[self.defaultGroup]["Name"]
        except KeyError:
            self.errors.append("Key 'Name' is missing")

    def checkGroup(self, group):
        # check if group header is valid
        if not (group == self.defaultGroup \
        or re.match("^\Desktop Action [a-zA-Z]+\$", group) \
        or (re.match("^\X-", group) and group.decode("utf-8", "ignore").encode("ascii", 'ignore') == group)):
            self.errors.append("Invalid Group name: %s" % group)
        else:
            #OnlyShowIn and NotShowIn
            if self.content[group].has_key("OnlyShowIn") and self.content[group].has_key("NotShowIn"):
                self.errors.append("Group may either have OnlyShowIn or NotShowIn, but not both")

    def checkKey(self, key, value, group):
        # standard keys     
        if key == "Type":
            if value == "ServiceType" or value == "Service":
                self.warnings.append("Type=%s is a KDE extension" % key)
            elif value == "MimeType":
                self.warnings.append("Type=MimeType is deprecated")
            elif not (value == "Application" or value == "Link" or value == "FSDevice" or value == "Directory"):
                self.errors.append("Value of key 'Type' must be Application, Link, FSDevice or Directory, but is '%s'" % value)

            if self.fileExtension == ".directory" and not value == "Directory":
                self.warnings.append("File extension is .directory, but Type is '%s'" % value)
            elif self.fileExtension == ".desktop" and value == "Directory":
                self.warnings.append("Files with Type=Directory should have the extension .directory")

        elif key == "Version":
            self.checkValue(key, value, type="number")

        elif key == "Encoding":
            if value == "Legacy-Mixed":
                self.errors.append("Encoding=Legacy-Mixed is deprecated and not supported by this parser")
            elif not value == "UTF-8":
                self.errors.append("Value of key 'Encoding' must be UTF-8")

        elif re.match("^Name"+xdg.Locale.regex+"$", key):
            pass # locale string

        elif re.match("^GenericName"+xdg.Locale.regex+"$", key):
            pass # locale string

        elif re.match("^Comment"+xdg.Locale.regex+"$", key):
            pass # locale string

        elif key == "NoDisplay":
            self.checkValue(key, value, type="boolean")

        elif key == "Hidden":
            self.checkValue(key, value, type="boolean")

        elif key == "Terminal":
            self.checkValue(key, value, type="boolean")
            self.checkType(key, "Application")

        elif key == "TryExec":
            self.checkValue(key, value)
            self.checkType(key, "Application")

        elif key == "Exec":
            self.checkValue(key, value)
            self.checkType(key, "Application")

        elif key == "Path":
            self.checkValue(key, value)
            self.checkType(key, "Application")

        elif re.match("^Icon"+xdg.Locale.regex+"$", key):
            self.checkValue(key, value)

        elif re.match("^SwallowTitle"+xdg.Locale.regex+"$", key):
            self.checkType(key, "Application")

        elif key == "SwallowExec":
            self.checkValue(key, value)
            self.checkType(key, "Application")

        elif key == "FilePatterns":
            self.checkValue(key, value, type="regex", list=True)
            self.checkType(key, "Application")

        elif key == "Actions":
            self.checkValue(key, value, list=True)
            self.checkType(key, "Application")

        elif key == "MimeType":
            self.checkValue(key, value, type="regex", list=True)
            self.checkType(key, "Application")

        elif key == "Categories":
            self.checkValue(key, value)
            self.checkType(key, "Application")
            self.checkCategorie(value)

        elif key == "OnlyShowIn":
            self.checkValue(key, value, list=True)
            self.checkOnlyShowIn(value)

        elif key == "NotShowIn":
            self.checkValue(key, value, list=True)
            self.checkOnlyShowIn(value)

        elif key == "StartupNotify":
            self.checkValue(key, value, type="boolean")
            self.checkType(key, "Application")

        elif key == "StartupWMClass":
            self.checkType(key, "Application")

        elif key == "SortOrder":
            self.checkValue(key, value, list=True)
            self.checkType(key, "Directory")

        elif key == "URL":
            self.checkValue(key, value)
            self.checkType(key, "URL")

        elif key == "Dev":
            self.checkValue(key, value)
            self.checkType(key, "FSDevice")

        elif key == "FSType":
            self.checkValue(key, value)
            self.checkType(key, "FSDevice")

        elif key == "MountPoint":
            self.checkValue(key, value)
            self.checkType(key, "FSDevice")

        elif re.match("^UnmountIcon"+xdg.Locale.regex+"$", key):
            self.checkValue(key, value)
            self.checkType(key, "FSDevice")

        elif key == "ReadOnly":
            self.checkValue(key, value, type="boolean")
            self.checkType(key, "FSDevice")

        # kde extensions
        elif key == "ServiceTypes":
            self.checkValue(key, value, list=True)
            self.warnings.append("Key '%s' is a KDE extension" % key)

        elif key == "DocPath":
            self.checkValue(key, value)
            self.warnings.append("Key '%s' is a KDE extension" % key)

        elif re.match("^Keywords"+xdg.Locale.regex+"$", key):
            self.checkValue(key, value, list=True)
            self.warnings.append("Key '%s' is a KDE extension" % key)

        elif key == "InitialPreference":
            self.checkValue(key, value, type="number")
            self.warnings.append("Key '%s' is a KDE extension" % key)

        # deprecated keys
        elif re.match("^MiniIcon"+xdg.Locale.regex+"$", key):
            self.checkValue(key, value)
            self.warnings.append("Key '%s' is deprecated" % key)

        elif key == "TerminalOptions":
            self.checkValue(key, value)
            self.warnings.append("Key '%s' is deprecated" % key)

        elif key == "DefaultApp":
            self.checkValue(key, value)
            self.warnings.append("Key '%s' is deprecated" % key)

        elif key == "Protocols":
            self.checkValue(key, value, list=True)
            self.warnings.append("Key '%s' is deprecated" % key)

        elif key == "Extensions":
            self.checkValue(key, value, list=True)
            self.warnings.append("Key '%s' is deprecated" % key)

        elif key == "BinaryPattern":
            self.checkValue(key, value)
            self.warnings.append("Key '%s' is deprecated" % key)

        elif key == "MapNotify":
            self.checkValue(key, value)
            self.warnings.append("Key '%s' is deprecated" % key)

        # "X-" extensions
        elif re.match("^X-[a-zA-Z0-9-]+", key):
            pass

        else:
            self.errors.append("Invalid key: %s" % key)

    def checkType(self, key, type):
        if not self.getType() == type:
            self.errors.append("Key '%s' only allowed in Type=%s" % (key, type))

    def checkOnlyShowIn(self, value):
        values = self.getList(value)
        valid = ["GNOME", "KDE", "ROX", "XFCE", "Old"]
        for item in values:
            if item not in valid:
                self.errors.append("'%s' is not a registered OnlyShowIn value" % item);

    def checkCategorie(self, value):
        values = self.getList(value)
        valid = ["Legacy","Core","Development","Building","Debugger","IDE","GUIDesigner","Profiling","RevisionControl","Translation","Office","Calendar","ContactManagement","Database","Dictionary","Chart","Email","Finance","FlowChart","PDA","ProjectManagement","Presentation","Spreadsheet","WordProcessor","Graphics","2DGraphics","VectorGraphics","RasterGraphics","3DGraphics","Scanning","OCR","Photograph","Viewer","Settings","DesktopSettings","HardwareSettings","PackageManager","Network","Dialup","InstantMessaging","IRCClient","FileTransfer","HamRadio","News","P2P","RemoteAccess","Telephony","WebBrowser","WebDevelopment","AudioVideo","Audio","Midi","Mixer","Sequencer","Tuner","Video","TV","AudioVideoEditing","Player","Recorder","DiscBurning","Game","ActionGame","AdventureGame","ArcadeGame","BoardGame","BlocksGame","CardGame","KidsGame","LogicGame","RolePlaying","Simulation","SportsGame","StrategyGame","Education","Art","Art","Contruction","Music","Languages","Science","Astronomy","Biology","Chemistry","Geology","Math","MedicalSoftware","Physics","Teaching","Amusement","Applet","Archiving","Electronics","Emulator","Engineering","FileManager","Shell","Screensaver","TerminalEmulator","TrayIcon","System","Filesystem","Monitor","Security","Utility","Accessibility","Calculator","Clock","TextEditor","KDE","GNOME","GTK","Qt","Motif","Java","ConsoleOnly"]
        for item in values:
            if item not in valid:
                self.errors.append("'%s' is not a registered Category" % item);
