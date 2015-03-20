import os, sys, gtk, vte, threading


# probono 12/2010


__version__ = "0.1"


def threaded(f):
    def wrapper(*args):
        t = threading.Thread(target=f, args=args)
        t.setDaemon(True)
        t.start()
    wrapper.__name__ = f.__name__
    wrapper.__dict__ = f.__dict__
    wrapper.__doc__  = f.__doc__
    return wrapper


class Assistant(gtk.Assistant):
    
    def __init__(self, pages_list, width=600, height=400, title="Assistant"):
        gtk.Assistant.__init__(self)
        self.set_title(title)
        self.connect('close', gtk.main_quit)
        self.connect('cancel',gtk.main_quit)
        self.set_size_request(width, height)
        self.connect('prepare', self.prepare_cb)
        self.pages = []
        for page in pages_list:
            self.pages.append(page[0](self, page[1]))
        for page in self.pages:
            page.append()
        self.show()
        gtk.gdk.threads_init() # Must be called before gtk.main()
        self.errortext = False
    
    def prepare_cb(self, assistant, content):
        """Is called before a new page is rendered"""
        page = self.pages[assistant.get_current_page()]
        page.prepare_cb()
        
    def go_to_last_page(self):
        self.set_current_page(len(self.pages) -1)

    def go_to_next_page(self):
        self.set_current_page(self.get_current_page() + 1)    
        
        
class Page(object):

    def __init__(self, assistant, title, text=""):
        self.a = assistant
        self.t = title
        self.type = gtk.ASSISTANT_PAGE_CONTENT
        self.classname = str(self.__class__).split(".")[-1].replace("'>", "")
        self.text = text
        if self.text == "": self.text = self.classname
        self.compose_content()
        
    def compose_content(self):
        """Subclasses should implement this. Need to return self.content"""
        label = gtk.Label(self.text)
        label.show()
        label.set_line_wrap(True)
        self.content = label
        
    def append(self):
        self.a.append_page(self.content)
        self.a.set_page_complete(self.content, True)
        self.a.set_page_title(self.content, self.t)
        self.a.set_page_type(self.content, self.type)
        
    def prepare_cb(self):
        """Subclasses should implement this to do their action"""
        print "Preparing %s" % (self)
        
    def __repr__(self):
        return "<%s '%s'>" % (self.classname, self.t)

        
class TextPage(Page):

    pass
         
         
class DirChooserPage(Page):

    def __init__(self, assistant, title):
        self.chooser_type = gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER
        Page.__init__(self, assistant, title)
    
    def compose_content(self):
        self.chooser = gtk.FileChooserWidget(self.chooser_type)
        self.chooser.set_current_folder(os.environ.get('HOME'))
        self.chooser.show()
        self.content = self.chooser

    def prepare_cb(self):
        self.a.set_page_complete(self.content, False)
        Page.prepare_cb(self)
        self.chooser.connect("selection-changed", self.selection_changed)

    def selection_changed(self, widget):
        self.selection = widget.get_filename()
        print self.selection
        self.a.set_page_complete(self.content, True)
        
    
class ResultPage(Page):

    def __init__(self, assistant, title):
        Page.__init__(self, assistant, title)
        self.type = gtk.ASSISTANT_PAGE_SUMMARY

        vbox = gtk.VBox()
        self.icon = gtk.Image()
        self.icon.set_from_file(os.path.join(os.path.dirname(__file__), "Gnome-emblem-default.png"))
        self.label = gtk.Label(str(self))
        self.label.set_line_wrap(True)
        vbox.add(self.icon)
        vbox.add(self.label)
        vbox.show_all()
        self.content = vbox
        
    def prepare_cb(self):
        Page.prepare_cb(self)
        if self.a.errortext:
            self.label.set_text(self.a.errortext)
            self.icon.set_from_file(os.path.join(os.path.dirname(__file__), "Gnome-dialog-warning.png"))
            self.a.set_page_title(self.content, "Error")
        
class RunnerPage(Page):

    def __init__(self, assistant, title):
        Page.__init__(self, assistant, title)
        self.type = gtk.ASSISTANT_PAGE_PROGRESS
        self.command = ["sleep", "1"]
    
    def compose_content(self):
        self.rt = RunTerminal(self)
        self.rt.show()
        self.content = self.rt

    def prepare_cb(self):
        Page.prepare_cb(self)
        self.run_command()
        
    @threaded
    # For this to work, the gtk.gdk.threads_init() function must be called before the gtk.main() function
    def run_command(self):
        print self.command
        gtk.gdk.threads_enter() # this must be called in a function that is decorated with @threaded
        self.rt.run_command(self.command)
        gtk.gdk.threads_leave() # this must be called in a function that is decorated with @threaded
    
    def command_succeeded(self, output):
        print "The command ran successfully"
        self.a.set_page_complete(self.content, True) # otherwise we can't go anywhere
        self.a.go_to_next_page()
        
    def command_failed(self, output):
        print "The command did not run successfully"
        self.a.errortext = output
        self.a.set_page_complete(self.content, True) # otherwise we can't go anywhere
        self.a.go_to_last_page()


class ProgressPage(RunnerPage):

    def compose_content(self):
        """We also use the VTE here as in the superclass, but we don't show it"""
        box = gtk.VBox()
        self.rt = RunTerminal(self)
        box.add(self.rt)
        pbar = gtk.ProgressBar()
        pbar.show()
        box.add(pbar)
        box.show()
        self.content = box
        
        
class RunTerminal(vte.Terminal):
    def __init__(self, caller):
        vte.Terminal.__init__(self)
        self.connect('child-exited', self.run_command_done)
        self.caller = caller

    def run_command(self, command_list):
        self.caller.a.set_page_complete(self.caller.a.get_nth_page(self.caller.a.get_current_page()), False)
        self.thread_running = True
        command = command_list
        pid = self.fork_command(command=command[0], argv=command, directory=os.getcwd())
        if pid <= 0:
            self.caller.command_failed("Failed to run %s" % (command[0]))
        while self.thread_running:
            gtk.main_iteration()

    def run_command_done(self, terminal):
        self.thread_running = False
        result = terminal.get_child_exit_status()
        output = terminal.get_text(lambda *a: True).rstrip()
        print result
        print output
        if result == 0:
            self.caller.command_succeeded(output)
        else: 
            self.caller.command_failed(output)
     
    
if __name__=="__main__":

    pages = [ 
                [TextPage, "Welcome"],
                [DirChooserPage, "Select AppDir"],
                [ProgressPage, "Prescanning..."],
                [TextPage, "Install now"],
                [ProgressPage, "Postscanning..."],
                [TextPage, "Select desktop file"],
                [RunnerPage, "Profiling..."],
                [TextPage, "Fine-tune the AppDir"],
                [RunnerPage, "Creating AppImage..."],
                [ResultPage, "Done"] 
            ]
            
    a = Assistant(pages, title="Assistant Helper")

    gtk.main()
