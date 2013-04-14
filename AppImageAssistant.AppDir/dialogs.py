#
# Kiwi: a Framework and Enhanced Widgets for Python
#
# Copyright (C) 2005-2007 Async Open Source
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# Author(s): Johan Dahlin <jdahlin@async.com.br>
#

import os
import gettext

import atk
import gtk

__all__ = ['error', 'info', 'messagedialog', 'warning', 'yesno', 'save',
           'open', 'HIGAlertDialog', 'BaseDialog']

_ = lambda m: gettext.dgettext('kiwi', m)

_IMAGE_TYPES = {
    gtk.MESSAGE_INFO: gtk.STOCK_DIALOG_INFO,
    gtk.MESSAGE_WARNING : gtk.STOCK_DIALOG_WARNING,
    gtk.MESSAGE_QUESTION : gtk.STOCK_DIALOG_QUESTION,
    gtk.MESSAGE_ERROR : gtk.STOCK_DIALOG_ERROR,
}

_BUTTON_TYPES = {
    gtk.BUTTONS_NONE: (),
    gtk.BUTTONS_OK: (gtk.STOCK_OK, gtk.RESPONSE_OK,),
    gtk.BUTTONS_CLOSE: (gtk.STOCK_CLOSE, gtk.RESPONSE_CLOSE,),
    gtk.BUTTONS_CANCEL: (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,),
    gtk.BUTTONS_YES_NO: (gtk.STOCK_NO, gtk.RESPONSE_NO,
                         gtk.STOCK_YES, gtk.RESPONSE_YES),
    gtk.BUTTONS_OK_CANCEL: (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                            gtk.STOCK_OK, gtk.RESPONSE_OK)
    }

class HIGAlertDialog(gtk.Dialog):
    def __init__(self, parent, flags,
                 type=gtk.MESSAGE_INFO, buttons=gtk.BUTTONS_NONE):
        if not type in _IMAGE_TYPES:
            raise TypeError(
                "type must be one of: %s", ', '.join(_IMAGE_TYPES.keys()))
        if not buttons in _BUTTON_TYPES:
            raise TypeError(
                "buttons be one of: %s", ', '.join(_BUTTON_TYPES.keys()))

        gtk.Dialog.__init__(self, '', parent, flags)
        self.set_border_width(5)
        self.set_resizable(False)
        self.set_has_separator(False)
        # Some window managers (ION) displays a default title (???) if
        # the specified one is empty, workaround this by setting it
        # to a single space instead
        self.set_title(" ")
        self.set_skip_taskbar_hint(True)
        self.vbox.set_spacing(14)

        # It seems like get_accessible is not available on windows, go figure
        if hasattr(self, 'get_accessible'):
            self.get_accessible().set_role(atk.ROLE_ALERT)

        self._primary_label = gtk.Label()
        self._secondary_label = gtk.Label()
        self._details_label = gtk.Label()
        self._image = gtk.image_new_from_stock(_IMAGE_TYPES[type],
                                               gtk.ICON_SIZE_DIALOG)
        self._image.set_alignment(0.5, 0.0)

        self._primary_label.set_use_markup(True)
        for label in (self._primary_label, self._secondary_label,
                      self._details_label):
            label.set_line_wrap(True)
            label.set_selectable(True)
            label.set_alignment(0.0, 0.5)

        hbox = gtk.HBox(False, 12)
        hbox.set_border_width(5)
        hbox.pack_start(self._image, False, False)

        vbox = gtk.VBox(False, 0)
        hbox.pack_start(vbox, False, False)
        vbox.pack_start(self._primary_label, False, False)
        vbox.pack_start(self._secondary_label, False, False)

        self._expander = gtk.expander_new_with_mnemonic(
            _("Show more _details"))
        self._expander.set_spacing(6)
        self._expander.add(self._details_label)
        vbox.pack_start(self._expander, False, False)
        self.vbox.pack_start(hbox, False, False)
        hbox.show_all()
        self._expander.hide()
        self.add_buttons(*_BUTTON_TYPES[buttons])
        self.label_vbox = vbox

    def set_primary(self, text):
        self._primary_label.set_markup(
            "<span weight=\"bold\" size=\"larger\">%s</span>" % text)

    def set_secondary(self, text):
        self._secondary_label.set_markup(text)

    def set_details(self, text):
        self._details_label.set_text(text)
        self._expander.show()

    def set_details_widget(self, widget):
        self._expander.remove(self._details_label)
        self._expander.add(widget)
        widget.show()
        self._expander.show()

class BaseDialog(gtk.Dialog):
    def __init__(self, parent=None, title='', flags=0, buttons=()):
        if parent and not isinstance(parent, gtk.Window):
            raise TypeError("parent needs to be None or a gtk.Window subclass")

        if not flags and parent:
            flags &= (gtk.DIALOG_MODAL |
                      gtk.DIALOG_DESTROY_WITH_PARENT)

        gtk.Dialog.__init__(self, title=title, parent=parent,
                            flags=flags, buttons=buttons)
        self.set_border_width(6)
        self.set_has_separator(False)
        self.vbox.set_spacing(6)

def messagedialog(dialog_type, short, long=None, parent=None,
                  buttons=gtk.BUTTONS_OK, default=-1):
    """Create and show a MessageDialog.

    @param dialog_type: one of constants
      - gtk.MESSAGE_INFO
      - gtk.MESSAGE_WARNING
      - gtk.MESSAGE_QUESTION
      - gtk.MESSAGE_ERROR
    @param short:       A header text to be inserted in the dialog.
    @param long:        A long description of message.
    @param parent:      The parent widget of this dialog
    @type parent:       a gtk.Window subclass
    @param buttons:     The button type that the dialog will be display,
      one of the constants:
       - gtk.BUTTONS_NONE
       - gtk.BUTTONS_OK
       - gtk.BUTTONS_CLOSE
       - gtk.BUTTONS_CANCEL
       - gtk.BUTTONS_YES_NO
       - gtk.BUTTONS_OK_CANCEL
      or a tuple or 2-sized tuples representing label and response. If label
      is a stock-id a stock icon will be displayed.
    @param default: optional default response id
    """
    if buttons in (gtk.BUTTONS_NONE, gtk.BUTTONS_OK, gtk.BUTTONS_CLOSE,
                   gtk.BUTTONS_CANCEL, gtk.BUTTONS_YES_NO,
                   gtk.BUTTONS_OK_CANCEL):
        dialog_buttons = buttons
        buttons = []
    else:
        if buttons is not None and type(buttons) != tuple:
            raise TypeError(
                "buttons must be a GtkButtonsTypes constant or a tuple")
        dialog_buttons = gtk.BUTTONS_NONE

    if parent and not isinstance(parent, gtk.Window):
        raise TypeError("parent must be a gtk.Window subclass")

    d = HIGAlertDialog(parent=parent, flags=gtk.DIALOG_MODAL,
                       type=dialog_type, buttons=dialog_buttons)
    if buttons:
        for text, response in buttons:
            d.add_buttons(text, response)

    d.set_primary(short)

    if long:
        if isinstance(long, gtk.Widget):
            d.set_details_widget(long)
        elif isinstance(long, basestring):
            d.set_details(long)
        else:
            raise TypeError(
                "long must be a gtk.Widget or a string, not %r" % long)

    if default != -1:
        d.set_default_response(default)

    if parent:
        d.set_transient_for(parent)
        d.set_modal(True)

    response = d.run()
    d.destroy()
    return response

def _simple(type, short, long=None, parent=None, buttons=gtk.BUTTONS_OK,
          default=-1):
    if buttons == gtk.BUTTONS_OK:
        default = gtk.RESPONSE_OK
    return messagedialog(type, short, long,
                         parent=parent, buttons=buttons,
                         default=default)

def error(short, long=None, parent=None, buttons=gtk.BUTTONS_OK, default=-1):
    return _simple(gtk.MESSAGE_ERROR, short, long, parent=parent,
                   buttons=buttons, default=default)

def info(short, long=None, parent=None, buttons=gtk.BUTTONS_OK, default=-1):
    return _simple(gtk.MESSAGE_INFO, short, long, parent=parent,
                   buttons=buttons, default=default)

def warning(short, long=None, parent=None, buttons=gtk.BUTTONS_OK, default=-1):
    return _simple(gtk.MESSAGE_WARNING, short, long, parent=parent,
                   buttons=buttons, default=default)

def yesno(text, parent=None, default=gtk.RESPONSE_YES,
          buttons=gtk.BUTTONS_YES_NO):
    return messagedialog(gtk.MESSAGE_WARNING, text, None, parent,
                         buttons=buttons, default=default)

def open(title='', parent=None, patterns=None, folder=None, filter=None):
    """Displays an open dialog.
    @param title: the title of the folder, defaults to 'Select folder'
    @param parent: parent gtk.Window or None
    @param patterns: a list of pattern strings ['*.py', '*.pl'] or None
    @param folder: initial folder or None
    @param filter: a filter to use or None, is incompatible with patterns
    """

    ffilter = filter
    if patterns and ffilter:
        raise TypeError("Can't use patterns and filter at the same time")

    filechooser = gtk.FileChooserDialog(title or _('Open'),
                                        parent,
                                        gtk.FILE_CHOOSER_ACTION_OPEN,
                                        (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                                         gtk.STOCK_OPEN, gtk.RESPONSE_OK))

    if patterns or ffilter:
        if not ffilter:
            ffilter = gtk.FileFilter()
            for pattern in patterns:
                ffilter.add_pattern(pattern)
        filechooser.set_filter(ffilter)
    filechooser.set_default_response(gtk.RESPONSE_OK)

    if folder:
        filechooser.set_current_folder(folder)

    response = filechooser.run()
    if response != gtk.RESPONSE_OK:
        filechooser.destroy()
        return

    path = filechooser.get_filename()
    if path and os.access(path, os.R_OK):
        filechooser.destroy()
        return path

    abspath = os.path.abspath(path)

    error(_('Could not open file "%s"') % abspath,
          _('The file "%s" could not be opened. '
            'Permission denied.') %  abspath)

    filechooser.destroy()
    return

def selectfolder(title='', parent=None, folder=None):
    """Displays a select folder dialog.
    @param title: the title of the folder, defaults to 'Select folder'
    @param parent: parent gtk.Window or None
    @param folder: initial folder or None
    """

    filechooser = gtk.FileChooserDialog(
        title or _('Select folder'),
        parent,
        gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER,
        (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
         gtk.STOCK_OK, gtk.RESPONSE_OK))

    if folder:
        filechooser.set_current_folder(folder)

    filechooser.set_default_response(gtk.RESPONSE_OK)

    response = filechooser.run()
    if response != gtk.RESPONSE_OK:
        filechooser.destroy()
        return

    path = filechooser.get_filename()
    if path and os.access(path, os.R_OK | os.X_OK):
        filechooser.destroy()
        return path

    abspath = os.path.abspath(path)

    error(_('Could not select folder "%s"') % abspath,
          _('The folder "%s" could not be selected. '
            'Permission denied.') %  abspath)

    filechooser.destroy()
    return

def ask_overwrite(filename, parent=None):
    submsg1 = _('A file named "%s" already exists') % os.path.abspath(filename)
    submsg2 = _('Do you wish to replace it with the current one?')
    text = ('<span weight="bold" size="larger">%s</span>\n\n%s\n'
            % (submsg1, submsg2))
    result = messagedialog(gtk.MESSAGE_ERROR, text, parent=parent,
                           buttons=((gtk.STOCK_CANCEL,
                                     gtk.RESPONSE_CANCEL),
                                    (_("Replace"),
                                     gtk.RESPONSE_YES)))
    return result == gtk.RESPONSE_YES

def save(title='', parent=None, current_name='', folder=None):
    """Displays a save dialog."""
    filechooser = gtk.FileChooserDialog(title or _('Save'),
                                        parent,
                                        gtk.FILE_CHOOSER_ACTION_SAVE,
                                        (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                                         gtk.STOCK_SAVE, gtk.RESPONSE_OK))
    if current_name:
        filechooser.set_current_name(current_name)
    filechooser.set_default_response(gtk.RESPONSE_OK)

    if folder:
        filechooser.set_current_folder(folder)

    path = None
    while True:
        response = filechooser.run()
        if response != gtk.RESPONSE_OK:
            path = None
            break

        path = filechooser.get_filename()
        if not os.path.exists(path):
            break

        if ask_overwrite(path, parent):
            break
    filechooser.destroy()
    return path

def password(primary='', secondary='', parent=None):
    """
    Shows a password dialog and returns the password entered in the dialog
    @param primary: primary text
    @param secondary: secondary text
    @param parent: a gtk.Window subclass or None
    @returns: the password or None if none specified
    @rtype: string or None
    """
    if not primary:
        raise ValueError("primary cannot be empty")

    d = HIGAlertDialog(parent=parent, flags=gtk.DIALOG_MODAL,
                       type=gtk.MESSAGE_QUESTION,
                       buttons=gtk.BUTTONS_OK_CANCEL)
    d.set_default_response(gtk.RESPONSE_OK)

    d.set_primary(primary + '\n')
    if secondary:
        secondary += '\n'
        d.set_secondary(secondary)

    hbox = gtk.HBox()
    hbox.set_border_width(6)
    hbox.show()
    d.label_vbox.pack_start(hbox)

    label = gtk.Label(_('Password:'))
    label.show()
    hbox.pack_start(label, False, False)

    entry = gtk.Entry()
    entry.set_invisible_char(u'\u2022')
    entry.set_visibility(False)
    entry.show()

    d.add_action_widget(entry, gtk.RESPONSE_OK)
    # FIXME: Is there another way of connecting widget::activate to a response?
    d.action_area.remove(entry)
    hbox.pack_start(entry, True, True, 12)

    response = d.run()

    if response == gtk.RESPONSE_OK:
        password = entry.get_text()
    else:
        password = None
    d.destroy()
    return password

def _test():
    yesno('Kill?', default=gtk.RESPONSE_NO)

    info('Some information displayed not too long\nbut not too short',
         long=('foobar ba asdjaiosjd oiadjoisjaoi aksjdasdasd kajsdhakjsdh\n'
               'askdjhaskjdha skjdhasdasdjkasldj alksdjalksjda lksdjalksdj\n'
               'asdjaslkdj alksdj lkasjdlkjasldkj alksjdlkasjd jklsdjakls\n'
               'ask;ldjaklsjdlkasjd alksdj laksjdlkasjd lkajs kjaslk jkl\n'),
         default=gtk.RESPONSE_OK,
         )

    error('An error occurred', gtk.Button('Woho'))
    error('Unable to mount the selected volume.',
          'mount: can\'t find /media/cdrom0 in /etc/fstab or /etc/mtab')
    print open(title='Open a file', patterns=['*.py'])
    print save(title='Save a file', current_name='foobar.py')

    print password('Administrator password',
                   'To be able to continue the wizard you need to enter the '
                   'administrator password for the database on host anthem')
    print selectfolder()

if __name__ == '__main__':
    _test()
