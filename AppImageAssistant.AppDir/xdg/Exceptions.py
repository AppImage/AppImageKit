"""
Exception Classes for the xdg package
"""

debug = False

class Error(Exception):
    def __init__(self, msg):
        self.msg = msg
        Exception.__init__(self, msg)
    def __str__(self):
        return self.msg

class ValidationError(Error):
    def __init__(self, msg, file):
        self.msg = msg
        self.file = file
        Error.__init__(self, "ValidationError in file '%s': %s " % (file, msg))

class ParsingError(Error):
    def __init__(self, msg, file):
        self.msg = msg
        self.file = file
        Error.__init__(self, "ParsingError in file '%s', %s" % (file, msg))

class NoKeyError(Error):
    def __init__(self, key, group, file):
        Error.__init__(self, "No key '%s' in group %s of file %s" % (key, group, file))
        self.key = key
        self.group = group

class DuplicateKeyError(Error):
    def __init__(self, key, group, file):
        Error.__init__(self, "Duplicate key '%s' in group %s of file %s" % (key, group, file))
        self.key = key
        self.group = group

class NoGroupError(Error):
    def __init__(self, group, file):
        Error.__init__(self, "No group: %s in file %s" % (group, file))
        self.group = group

class DuplicateGroupError(Error):
    def __init__(self, group, file):
        Error.__init__(self, "Duplicate group: %s in file %s" % (group, file))
        self.group = group

class NoThemeError(Error):
    def __init__(self, theme):
        Error.__init__(self, "No such icon-theme: %s" % theme)
        self.theme = theme
