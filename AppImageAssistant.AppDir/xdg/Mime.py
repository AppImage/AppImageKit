"""
This module is based on a rox module (LGPL):

http://cvs.sourceforge.net/viewcvs.py/rox/ROX-Lib2/python/rox/mime.py?rev=1.21&view=log

This module provides access to the shared MIME database.

types is a dictionary of all known MIME types, indexed by the type name, e.g.
types['application/x-python']

Applications can install information about MIME types by storing an
XML file as <MIME>/packages/<application>.xml and running the
update-mime-database command, which is provided by the freedesktop.org
shared mime database package.

See http://www.freedesktop.org/standards/shared-mime-info-spec/ for
information about the format of these files.

(based on version 0.13)
"""

import os
import stat
import fnmatch

import xdg.BaseDirectory
import xdg.Locale

from xml.dom import Node, minidom, XML_NAMESPACE

FREE_NS = 'http://www.freedesktop.org/standards/shared-mime-info'

types = {}      # Maps MIME names to type objects

exts = None     # Maps extensions to types
globs = None    # List of (glob, type) pairs
literals = None # Maps liternal names to types
magic = None

def _get_node_data(node):
    """Get text of XML node"""
    return ''.join([n.nodeValue for n in node.childNodes]).strip()

def lookup(media, subtype = None):
    "Get the MIMEtype object for this type, creating a new one if needed."
    if subtype is None and '/' in media:
        media, subtype = media.split('/', 1)
    if (media, subtype) not in types:
        types[(media, subtype)] = MIMEtype(media, subtype)
    return types[(media, subtype)]

class MIMEtype:
    """Type holding data about a MIME type"""
    def __init__(self, media, subtype):
        "Don't use this constructor directly; use mime.lookup() instead."
        assert media and '/' not in media
        assert subtype and '/' not in subtype
        assert (media, subtype) not in types

        self.media = media
        self.subtype = subtype
        self._comment = None

    def _load(self):
        "Loads comment for current language. Use get_comment() instead."
        resource = os.path.join('mime', self.media, self.subtype + '.xml')
        for path in xdg.BaseDirectory.load_data_paths(resource):
            doc = minidom.parse(path)
            if doc is None:
                continue
            for comment in doc.documentElement.getElementsByTagNameNS(FREE_NS, 'comment'):
                lang = comment.getAttributeNS(XML_NAMESPACE, 'lang') or 'en'
                goodness = 1 + (lang in xdg.Locale.langs)
                if goodness > self._comment[0]:
                    self._comment = (goodness, _get_node_data(comment))
                if goodness == 2: return

    # FIXME: add get_icon method
    def get_comment(self):
        """Returns comment for current language, loading it if needed."""
        # Should we ever reload?
        if self._comment is None:
            self._comment = (0, str(self))
            self._load()
        return self._comment[1]

    def __str__(self):
        return self.media + '/' + self.subtype

    def __repr__(self):
        return '[%s: %s]' % (self, self._comment or '(comment not loaded)')

class MagicRule:
    def __init__(self, f):
        self.next=None
        self.prev=None

        #print line
        ind=''
        while True:
            c=f.read(1)
            if c=='>':
                break
            ind+=c
        if not ind:
            self.nest=0
        else:
            self.nest=int(ind)

        start=''
        while True:
            c=f.read(1)
            if c=='=':
                break
            start+=c
        self.start=int(start)
        
        hb=f.read(1)
        lb=f.read(1)
        self.lenvalue=ord(lb)+(ord(hb)<<8)

        self.value=f.read(self.lenvalue)

        c=f.read(1)
        if c=='&':
            self.mask=f.read(self.lenvalue)
            c=f.read(1)
        else:
            self.mask=None

        if c=='~':
            w=''
            while c!='+' and c!='\n':
                c=f.read(1)
                if c=='+' or c=='\n':
                    break
                w+=c
            
            self.word=int(w)
        else:
            self.word=1

        if c=='+':
            r=''
            while c!='\n':
                c=f.read(1)
                if c=='\n':
                    break
                r+=c
            #print r
            self.range=int(r)
        else:
            self.range=1

        if c!='\n':
            raise 'Malformed MIME magic line'

    def getLength(self):
        return self.start+self.lenvalue+self.range

    def appendRule(self, rule):
        if self.nest<rule.nest:
            self.next=rule
            rule.prev=self

        elif self.prev:
            self.prev.appendRule(rule)
        
    def match(self, buffer):
        if self.match0(buffer):
            if self.next:
                return self.next.match(buffer)
            return True

    def match0(self, buffer):
        l=len(buffer)
        for o in range(self.range):
            s=self.start+o
            e=s+self.lenvalue
            if l<e:
                return False
            if self.mask:
                test=''
                for i in range(self.lenvalue):
                    c=ord(buffer[s+i]) & ord(self.mask[i])
                    test+=chr(c)
            else:
                test=buffer[s:e]

            if test==self.value:
                return True

    def __repr__(self):
        return '<MagicRule %d>%d=[%d]%s&%s~%d+%d>' % (self.nest,
                                  self.start,
                                  self.lenvalue,
                                  `self.value`,
                                  `self.mask`,
                                  self.word,
                                  self.range)

class MagicType:
    def __init__(self, mtype):
        self.mtype=mtype
        self.top_rules=[]
        self.last_rule=None

    def getLine(self, f):
        nrule=MagicRule(f)

        if nrule.nest and self.last_rule:
            self.last_rule.appendRule(nrule)
        else:
            self.top_rules.append(nrule)

        self.last_rule=nrule

        return nrule

    def match(self, buffer):
        for rule in self.top_rules:
            if rule.match(buffer):
                return self.mtype

    def __repr__(self):
        return '<MagicType %s>' % self.mtype
    
class MagicDB:
    def __init__(self):
        self.types={}   # Indexed by priority, each entry is a list of type rules
        self.maxlen=0

    def mergeFile(self, fname):
        f=file(fname, 'r')
        line=f.readline()
        if line!='MIME-Magic\0\n':
            raise 'Not a MIME magic file'

        while True:
            shead=f.readline()
            #print shead
            if not shead:
                break
            if shead[0]!='[' or shead[-2:]!=']\n':
                raise 'Malformed section heading'
            pri, tname=shead[1:-2].split(':')
            #print shead[1:-2]
            pri=int(pri)
            mtype=lookup(tname)

            try:
                ents=self.types[pri]
            except:
                ents=[]
                self.types[pri]=ents

            magictype=MagicType(mtype)
            #print tname

            #rline=f.readline()
            c=f.read(1)
            f.seek(-1, 1)
            while c and c!='[':
                rule=magictype.getLine(f)
                #print rule
                if rule and rule.getLength()>self.maxlen:
                    self.maxlen=rule.getLength()

                c=f.read(1)
                f.seek(-1, 1)

            ents.append(magictype)
            #self.types[pri]=ents
            if not c:
                break

    def match_data(self, data, max_pri=100, min_pri=0):
        pris=self.types.keys()
        pris.sort(lambda a, b: -cmp(a, b))
        for pri in pris:
            #print pri, max_pri, min_pri
            if pri>max_pri:
                continue
            if pri<min_pri:
                break
            for type in self.types[pri]:
                m=type.match(buf)
                if m:
                    return m
        

    def match(self, path, max_pri=100, min_pri=0):
        try:
            buf=file(path, 'r').read(self.maxlen)
            return self.match_data(buf, max_pri, min_pri)
        except:
            pass

        return None
    
    def __repr__(self):
        return '<MagicDB %s>' % self.types
            

# Some well-known types
text = lookup('text', 'plain')
inode_block = lookup('inode', 'blockdevice')
inode_char = lookup('inode', 'chardevice')
inode_dir = lookup('inode', 'directory')
inode_fifo = lookup('inode', 'fifo')
inode_socket = lookup('inode', 'socket')
inode_symlink = lookup('inode', 'symlink')
inode_door = lookup('inode', 'door')
app_exe = lookup('application', 'executable')

_cache_uptodate = False

def _cache_database():
    global exts, globs, literals, magic, _cache_uptodate

    _cache_uptodate = True

    exts = {}       # Maps extensions to types
    globs = []      # List of (glob, type) pairs
    literals = {}   # Maps liternal names to types
    magic = MagicDB()

    def _import_glob_file(path):
        """Loads name matching information from a MIME directory."""
        for line in file(path):
            if line.startswith('#'): continue
            line = line[:-1]

            type_name, pattern = line.split(':', 1)
            mtype = lookup(type_name)

            if pattern.startswith('*.'):
                rest = pattern[2:]
                if not ('*' in rest or '[' in rest or '?' in rest):
                    exts[rest] = mtype
                    continue
            if '*' in pattern or '[' in pattern or '?' in pattern:
                globs.append((pattern, mtype))
            else:
                literals[pattern] = mtype

    for path in xdg.BaseDirectory.load_data_paths(os.path.join('mime', 'globs')):
        _import_glob_file(path)
    for path in xdg.BaseDirectory.load_data_paths(os.path.join('mime', 'magic')):
        magic.mergeFile(path)

    # Sort globs by length
    globs.sort(lambda a, b: cmp(len(b[0]), len(a[0])))

def get_type_by_name(path):
    """Returns type of file by its name, or None if not known"""
    if not _cache_uptodate:
        _cache_database()

    leaf = os.path.basename(path)
    if leaf in literals:
        return literals[leaf]

    lleaf = leaf.lower()
    if lleaf in literals:
        return literals[lleaf]

    ext = leaf
    while 1:
        p = ext.find('.')
        if p < 0: break
        ext = ext[p + 1:]
        if ext in exts:
            return exts[ext]
    ext = lleaf
    while 1:
        p = ext.find('.')
        if p < 0: break
        ext = ext[p+1:]
        if ext in exts:
            return exts[ext]
    for (glob, mime_type) in globs:
        if fnmatch.fnmatch(leaf, glob):
            return mime_type
        if fnmatch.fnmatch(lleaf, glob):
            return mime_type
    return None

def get_type_by_contents(path, max_pri=100, min_pri=0):
    """Returns type of file by its contents, or None if not known"""
    if not _cache_uptodate:
        _cache_database()

    return magic.match(path, max_pri, min_pri)

def get_type_by_data(data, max_pri=100, min_pri=0):
    """Returns type of the data"""
    if not _cache_uptodate:
        _cache_database()

    return magic.match_data(data, max_pri, min_pri)

def get_type(path, follow=1, name_pri=100):
    """Returns type of file indicated by path.
    path     - pathname to check (need not exist)
    follow   - when reading file, follow symbolic links
    name_pri - Priority to do name matches.  100=override magic"""
    if not _cache_uptodate:
        _cache_database()
    
    try:
        if follow:
            st = os.stat(path)
        else:
            st = os.lstat(path)
    except:
        t = get_type_by_name(path)
        return t or text

    if stat.S_ISREG(st.st_mode):
        t = get_type_by_contents(path, min_pri=name_pri)
        if not t: t = get_type_by_name(path)
        if not t: t = get_type_by_contents(path, max_pri=name_pri)
        if t is None:
            if stat.S_IMODE(st.st_mode) & 0111:
                return app_exe
            else:
                return text
        return t
    elif stat.S_ISDIR(st.st_mode): return inode_dir
    elif stat.S_ISCHR(st.st_mode): return inode_char
    elif stat.S_ISBLK(st.st_mode): return inode_block
    elif stat.S_ISFIFO(st.st_mode): return inode_fifo
    elif stat.S_ISLNK(st.st_mode): return inode_symlink
    elif stat.S_ISSOCK(st.st_mode): return inode_socket
    return inode_door

def install_mime_info(application, package_file):
    """Copy 'package_file' as ~/.local/share/mime/packages/<application>.xml.
    If package_file is None, install <app_dir>/<application>.xml.
    If already installed, does nothing. May overwrite an existing
    file with the same name (if the contents are different)"""
    application += '.xml'

    new_data = file(package_file).read()

    # See if the file is already installed
    package_dir = os.path.join('mime', 'packages')
    resource = os.path.join(package_dir, application)
    for x in xdg.BaseDirectory.load_data_paths(resource):
        try:
            old_data = file(x).read()
        except:
            continue
        if old_data == new_data:
            return  # Already installed

    global _cache_uptodate
    _cache_uptodate = False

    # Not already installed; add a new copy
    # Create the directory structure...
    new_file = os.path.join(xdg.BaseDirectory.save_data_path(package_dir), application)

    # Write the file...
    file(new_file, 'w').write(new_data)

    # Update the database...
    command = 'update-mime-database'
    if os.spawnlp(os.P_WAIT, command, command, xdg.BaseDirectory.save_data_path('mime')):
        os.unlink(new_file)
        raise Exception("The '%s' command returned an error code!\n" \
                  "Make sure you have the freedesktop.org shared MIME package:\n" \
                  "http://standards.freedesktop.org/shared-mime-info/") % command
