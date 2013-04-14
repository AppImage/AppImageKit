"""
Base Class for DesktopEntry, IconTheme and IconData
"""

import re, os.path, codecs
from Exceptions import *
import xdg.Locale
import gettext

class IniFile:
    defaultGroup = ''
    fileExtension = ''

    filename = ''
    gettext_domain = None

    tainted = False

    def __init__(self, filename=None):
        self.content = dict()
        if filename:
            self.parse(filename)

    def __cmp__(self, other):
        return cmp(self.content, other.content)

    def parse(self, filename, headers):
        # for performance reasons
        content = self.content

        if not os.path.isfile(filename):
            raise ParsingError("File not found", filename)

        try:
            fd = file(filename, 'r')
        except IOError, e:
            if debug:
                raise e
            else:
                return

        # parse file
        for line in fd:
            line = line.strip()
            # empty line
            if not line:
                continue
            # comment
            elif line[0] == '#':
                continue
            # new group
            elif line[0] == '[':
                currentGroup = line.lstrip("[").rstrip("]")
                if debug and self.hasGroup(currentGroup):
                    raise DuplicateGroupError(currentGroup, filename)
                else:
                    content[currentGroup] = {}
            # key
            else:
                index = line.find("=")
                key = line[0:index].strip()
                value = line[index+1:].strip()
                try:
                    if debug and self.hasKey(key, currentGroup):
                        raise DuplicateKeyError(key, currentGroup, filename)
                    else:
                        content[currentGroup][key] = value
                except (IndexError, UnboundLocalError):
                    raise ParsingError("[%s]-Header missing" % headers[0], filename)

        fd.close()

        self.filename = filename
        self.tainted = False

        # check header
        for header in headers:
            if content.has_key(header):
                self.defaultGroup = header
                break
        else:
            raise ParsingError("[%s]-Header missing" % headers[0], filename)

        # check for gettext domain
        e = self.content.get('Desktop Entry', {})
        self.gettext_domain = e.get('X-GNOME-Gettext-Domain',
            e.get('X-Ubuntu-Gettext-Domain', None))

    # start stuff to access the keys
    def get(self, key, group=None, locale=False, type="string", list=False):
        # set default group
        if not group:
            group = self.defaultGroup

        # return key (with locale)
        if self.content.has_key(group) and self.content[group].has_key(key):
            if locale:
                key = self.__addLocale(key, group)
                if key.endswith(']') or not self.gettext_domain:
                    # inline translations
                    value = self.content[group][key]
                else:
                    value = gettext.dgettext(self.gettext_domain, self.content[group][key])
            else:
                value = self.content[group][key]
        else:
            if debug:
                if not self.content.has_key(group):
                    raise NoGroupError(group, self.filename)
                elif not self.content[group].has_key(key):
                    raise NoKeyError(key, group, self.filename)
            else:
                value = ""

        if list == True:
            values = self.getList(value)
            result = []
        else:
            values = [value]

        for value in values:
            if type == "string" and locale == True:
                value = value.decode("utf-8", "ignore")
            elif type == "boolean":
                value = self.__getBoolean(value)
            elif type == "integer":
                try:
                    value = int(value)
                except ValueError:
                    value = 0
            elif type == "numeric":
                try:
                    value = float(value)
                except ValueError:
                    value = 0.0
            elif type == "regex":
                value = re.compile(value)
            elif type == "point":
                value = value.split(",")

            if list == True:
                result.append(value)
            else:
                result = value

        return result
    # end stuff to access the keys

    # start subget
    def getList(self, string):
        if re.search(r"(?<!\\)\;", string):
            list = re.split(r"(?<!\\);", string)
        elif re.search(r"(?<!\\)\|", string):
            list = re.split(r"(?<!\\)\|", string)
        elif re.search(r"(?<!\\),", string):
            list = re.split(r"(?<!\\),", string)
        else:
            list = [string]
        if list[-1] == "":
            list.pop()
        return list

    def __getBoolean(self, boolean):
        if boolean == 1 or boolean == "true" or boolean == "True":
            return True
        elif boolean == 0 or boolean == "false" or boolean == "False":
            return False
        return False
    # end subget

    def __addLocale(self, key, group=None):
        "add locale to key according the current lc_messages"
        # set default group
        if not group:
            group = self.defaultGroup

        for lang in xdg.Locale.langs:
            if self.content[group].has_key(key+'['+lang+']'):
                return key+'['+lang+']'

        return key

    # start validation stuff
    def validate(self, report="All"):
        "validate ... report = All / Warnings / Errors"

        self.warnings = []
        self.errors = []

        # get file extension
        self.fileExtension = os.path.splitext(self.filename)[1]

        # overwrite this for own checkings
        self.checkExtras()

        # check all keys
        for group in self.content:
            self.checkGroup(group)
            for key in self.content[group]:
                self.checkKey(key, self.content[group][key], group)
                # check if value is empty
                if self.content[group][key] == "":
                    self.warnings.append("Value of Key '%s' is empty" % key)

        # raise Warnings / Errors
        msg = ""

        if report == "All" or report == "Warnings":
            for line in self.warnings:
                msg += "\n- " + line

        if report == "All" or report == "Errors":
            for line in self.errors:
                msg += "\n- " + line

        if msg:
            raise ValidationError(msg, self.filename)

    # check if group header is valid
    def checkGroup(self, group):
        pass

    # check if key is valid
    def checkKey(self, key, value, group):
        pass

    # check random stuff
    def checkValue(self, key, value, type="string", list=False):
        if list == True:
            values = self.getList(value)
        else:
            values = [value]

        for value in values:
            if type == "string":
                code = self.checkString(value)
            elif type == "boolean":
                code = self.checkBoolean(value)
            elif type == "number":
                code = self.checkNumber(value)
            elif type == "integer":
                code = self.checkInteger(value)
            elif type == "regex":
                code = self.checkRegex(value)
            elif type == "point":
                code = self.checkPoint(value)
            if code == 1:
                self.errors.append("'%s' is not a valid %s" % (value, type))
            elif code == 2:
                self.warnings.append("Value of key '%s' is deprecated" % key)

    def checkExtras(self):
        pass

    def checkBoolean(self, value):
        # 1 or 0 : deprecated
        if (value == "1" or value == "0"):
            return 2
        # true or false: ok
        elif not (value == "true" or value == "false"):
            return 1

    def checkNumber(self, value):
        # float() ValueError
        try:
            float(value)
        except:
            return 1

    def checkInteger(self, value):
        # int() ValueError
        try:
            int(value)
        except:
            return 1

    def checkPoint(self, value):
        if not re.match("^[0-9]+,[0-9]+$", value):
            return 1

    def checkString(self, value):
        # convert to ascii
        if not value.decode("utf-8", "ignore").encode("ascii", 'ignore') == value:
            return 1

    def checkRegex(self, value):
        try:
            re.compile(value)
        except:
            return 1

    # write support
    def write(self, filename=None):
        if not filename and not self.filename:
            raise ParsingError("File not found", "")

        if filename:
            self.filename = filename
        else:
            filename = self.filename

        if os.path.dirname(filename) and not os.path.isdir(os.path.dirname(filename)):
            os.makedirs(os.path.dirname(filename))

        fp = codecs.open(filename, 'w')
        if self.defaultGroup:
            fp.write("[%s]\n" % self.defaultGroup)
            for (key, value) in self.content[self.defaultGroup].items():
                fp.write("%s=%s\n" % (key, value))
            fp.write("\n")
        for (name, group) in self.content.items():
            if name != self.defaultGroup:
                fp.write("[%s]\n" % name)
                for (key, value) in group.items():
                    fp.write("%s=%s\n" % (key, value))
                fp.write("\n")
        self.tainted = False

    def set(self, key, value, group=None, locale=False):
        # set default group
        if not group:
            group = self.defaultGroup

        if locale == True and len(xdg.Locale.langs) > 0:
            key = key + "[" + xdg.Locale.langs[0] + "]"

        try:
            if isinstance(value, unicode):
                self.content[group][key] = value.encode("utf-8", "ignore")
            else:
                self.content[group][key] = value
        except KeyError:
            raise NoGroupError(group, self.filename)
            
        self.tainted = (value == self.get(key, group))

    def addGroup(self, group):
        if self.hasGroup(group):
            if debug:
                raise DuplicateGroupError(group, self.filename)
            else:
                pass
        else:
            self.content[group] = {}
            self.tainted = True

    def removeGroup(self, group):
        existed = group in self.content
        if existed:
            del self.content[group]
            self.tainted = True
        else:
            if debug:
                raise NoGroupError(group, self.filename)
        return existed

    def removeKey(self, key, group=None, locales=True):
        # set default group
        if not group:
            group = self.defaultGroup

        try:
            if locales:
                for (name, value) in self.content[group].items():
                    if re.match("^" + key + xdg.Locale.regex + "$", name) and name != key:
                        value = self.content[group][name]
                        del self.content[group][name]
            value = self.content[group][key]
            del self.content[group][key]
            self.tainted = True
            return value
        except KeyError, e:
            if debug:
                if e == group:
                    raise NoGroupError(group, self.filename)
                else:
                    raise NoKeyError(key, group, self.filename)
            else:
                return ""

    # misc
    def groups(self):
        return self.content.keys()

    def hasGroup(self, group):
        if self.content.has_key(group):
            return True
        else:
            return False

    def hasKey(self, key, group=None):
        # set default group
        if not group:
            group = self.defaultGroup

        if self.content[group].has_key(key):
            return True
        else:
            return False

    def getFileName(self):
        return self.filename
