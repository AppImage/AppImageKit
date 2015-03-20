"""
This module is based on a rox module (LGPL):

http://cvs.sourceforge.net/viewcvs.py/rox/ROX-Lib2/python/rox/basedir.py?rev=1.9&view=log

The freedesktop.org Base Directory specification provides a way for
applications to locate shared data and configuration:

    http://standards.freedesktop.org/basedir-spec/

(based on version 0.6)

This module can be used to load and save from and to these directories.

Typical usage:

    from rox import basedir
    
    for dir in basedir.load_config_paths('mydomain.org', 'MyProg', 'Options'):
        print "Load settings from", dir

    dir = basedir.save_config_path('mydomain.org', 'MyProg')
    print >>file(os.path.join(dir, 'Options'), 'w'), "foo=2"

Note: see the rox.Options module for a higher-level API for managing options.
"""

from __future__ import generators
import os

_home = os.environ.get('HOME', '/')
xdg_data_home = os.environ.get('XDG_DATA_HOME',
            os.path.join(_home, '.local', 'share'))

xdg_data_dirs = [xdg_data_home] + \
    os.environ.get('XDG_DATA_DIRS', '/usr/local/share:/usr/share').split(':')

xdg_config_home = os.environ.get('XDG_CONFIG_HOME',
            os.path.join(_home, '.config'))

xdg_config_dirs = [xdg_config_home] + \
    os.environ.get('XDG_CONFIG_DIRS', '/etc/xdg').split(':')

xdg_cache_home = os.environ.get('XDG_CACHE_HOME',
            os.path.join(_home, '.cache'))

xdg_data_dirs = filter(lambda x: x, xdg_data_dirs)
xdg_config_dirs = filter(lambda x: x, xdg_config_dirs)

def save_config_path(*resource):
    """Ensure $XDG_CONFIG_HOME/<resource>/ exists, and return its path.
    'resource' should normally be the name of your application. Use this
    when SAVING configuration settings. Use the xdg_config_dirs variable
    for loading."""
    resource = os.path.join(*resource)
    assert not resource.startswith('/')
    path = os.path.join(xdg_config_home, resource)
    if not os.path.isdir(path):
        os.makedirs(path, 0700)
    return path

def save_data_path(*resource):
    """Ensure $XDG_DATA_HOME/<resource>/ exists, and return its path.
    'resource' is the name of some shared resource. Use this when updating
    a shared (between programs) database. Use the xdg_data_dirs variable
    for loading."""
    resource = os.path.join(*resource)
    assert not resource.startswith('/')
    path = os.path.join(xdg_data_home, resource)
    if not os.path.isdir(path):
        os.makedirs(path)
    return path

def load_config_paths(*resource):
    """Returns an iterator which gives each directory named 'resource' in the
    configuration search path. Information provided by earlier directories should
    take precedence over later ones (ie, the user's config dir comes first)."""
    resource = os.path.join(*resource)
    for config_dir in xdg_config_dirs:
        path = os.path.join(config_dir, resource)
        if os.path.exists(path): yield path

def load_first_config(*resource):
    """Returns the first result from load_config_paths, or None if there is nothing
    to load."""
    for x in load_config_paths(*resource):
        return x
    return None

def load_data_paths(*resource):
    """Returns an iterator which gives each directory named 'resource' in the
    shared data search path. Information provided by earlier directories should
    take precedence over later ones."""
    resource = os.path.join(*resource)
    for data_dir in xdg_data_dirs:
        path = os.path.join(data_dir, resource)
        if os.path.exists(path): yield path
