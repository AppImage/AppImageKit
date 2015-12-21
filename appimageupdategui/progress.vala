using Gtk;

// http://stackoverflow.com/questions/20846511/read-write-file-pipes-in-vala-glib
// http://www.valadoc.org/#!api=glib-2.0/GLib.IOChannel.IOChannel.unix_new
// http://www.valadoc.org/#!api=glib-2.0/GLib.IOChannel

public class ProgressWindow : Window {

    private string file_name;
    private string[] files_to_be_updated;   
    private int counter;    
    private int file_counts;         
    
    private ProgressBar progress;

    private Label action_label;
    private Label message_label;
    
    private Image icon_image;

    private Button btn_cancel;
    private Button btn_show_files;
    private Button btn_quit;
            
    private Pid child_pid;
    
    
    public ProgressWindow(string file_name, string[] files_to_be_updated) {
            var builder = new Builder();
            try {
                builder.add_from_file ("ui/progress.ui");
            }
            catch (Error e) {
                error ("Unable to load file: %s", e.message);
            }
            
            action_label = builder.get_object("action_label") as Label;
            action_label.set_vexpand(false);

            message_label = builder.get_object("message_label") as Label;
            progress = builder.get_object("progress_progressbar") as ProgressBar; 

            this.title = "Updating " + file_name;
            this.window_position = WindowPosition.CENTER;    
            
            /* Set app icon */
            try {
                this.icon = new Gdk.Pixbuf.from_file("appimageupdate");
            } catch (Error e) {
                stderr.printf ("Could not load application icon: %s\n", e.message);
            }
            
            var vbox = builder.get_object("main_box") as Box;
            add(vbox);

            icon_image = builder.get_object("icon_image") as Image;

            btn_cancel = builder.get_object("btn_cancel") as Button;       
            btn_cancel.clicked.connect(cancel);
                 
            btn_quit = builder.get_object("btn_quit") as Button;
            btn_quit.clicked.connect(Gtk.main_quit);     
            btn_quit.set_visible(false);

            btn_show_files = builder.get_object("btn_show_files") as Button;
            btn_show_files.clicked.connect(this.show_files); 
            btn_show_files.set_visible(false);

            this.files_to_be_updated = files_to_be_updated;
            this.file_name = file_name;
                       
            counter = 0;
            this.show_all();
             
            update();
    }
    
    private void update() {
        try {
            int extra_args = 0;
            
            string[] spawn_args = new string[8 + files_to_be_updated.length + extra_args];
	    spawn_args[0] = "appimageupdate";
	    spawn_args[1] = this.file_name;

	    string[] spawn_env = Environ.get ();	    	
	    int standard_error;
	    int standard_out;

            string a_string = string.joinv(" ", spawn_args);
	    stdout.printf(a_string);

	    Process.spawn_async_with_pipes ("/",
		    spawn_args,
		    spawn_env,
		    SpawnFlags.SEARCH_PATH | SpawnFlags.DO_NOT_REAP_CHILD,
		    null,
		    out child_pid,
		    null,
		    out standard_out,
		    out standard_error);
		
	    // stdout:
	    IOChannel output = new IOChannel.unix_new (standard_out);
	    output.add_watch (IOCondition.IN | IOCondition.HUP, (channel, condition) => {
		    return this.process_line (channel, condition, "stdout");
	    });		
		
	    // stderr:
	    IOChannel error = new IOChannel.unix_new (standard_error);
	    error.add_watch (IOCondition.IN | IOCondition.HUP, (channel, condition) => {
		    return this.process_error (channel, condition, "stderr");
	    });
		
            ChildWatch.add (child_pid, (pid, status) => {
			    // Triggered when the child indicated by child_pid exits			
			    Process.close_pid (pid);
			    btn_cancel.set_label("Close");
        	            btn_quit.set_visible(true);
        	            btn_show_files.set_visible(true);
	    });

        } catch (SpawnError e) {
	        action_label.label = "Error: " + e.message;
        }            
            
    }
    
    /* Handle output pipe */
    private bool process_line (IOChannel channel, IOCondition condition, string stream_name) {            
	    if (condition == IOCondition.HUP) {
		    // action_label.label = "Done";
		    return false;
	    }

	    try {
		    string line;
		    channel.read_line (out line, null, null);
		    stdout.printf(line); // Be verbose
                    file_counts = 30; // TODO: Get from command line output
                    action_label.label = line.substring(0, line.length - 1);
                    counter++;
                    progress.set_fraction( 1.0f * counter / file_counts);
                    if(line.contains("checksum matches OK")) {
		        icon_image.icon_name="emblem-ok-symbolic";
                    }
                    if(line.contains("Cannot")) {
		        icon_image.icon_name="dialog-error-symbolic";
                    }
	    } catch (IOChannelError e) {
		    stdout.printf ("%s: IOChannelError: %s\n", stream_name, e.message);
		    return false;
	    } catch (ConvertError e) {
		    stdout.printf ("%s: ConvertError: %s\n", stream_name, e.message);
		    return false;
	    }

	    return true;
    }

    private bool process_error(IOChannel channel, IOCondition condition, string stream_name) {            
            if (condition == IOCondition.HUP) {
		    return false;
	    }
	
	    try {
		    string line;
		    channel.read_line (out line, null, null);
		    stdout.printf(line); // Be verbose
                    message_label.label = line.substring(0, line.length - 1);
                    
	    } catch (IOChannelError e) {
		    stdout.printf ("%s: IOChannelError: %s\n", stream_name, e.message);
		    return false;
	    } catch (ConvertError e) {
		    stdout.printf ("%s: ConvertError: %s\n", stream_name, e.message);
		    return false;
	    }
	
            return true;
    }
    
    private void show_files() {
	    // To be implemented
            this.close();
    }
    
    private void cancel() {
        Posix.kill(child_pid, Posix.SIGTERM);
        this.close();    
    }

}
