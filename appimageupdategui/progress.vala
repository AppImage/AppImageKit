using Gtk;

// http://stackoverflow.com/questions/20846511/read-write-file-pipes-in-vala-glib
// http://www.valadoc.org/#!api=glib-2.0/GLib.IOChannel.IOChannel.unix_new
// http://www.valadoc.org/#!api=glib-2.0/GLib.IOChannel

public class ProgressWindow : Window {

    private string file_name;
    private string[] files_to_be_updated;
    private int counter;
    private int total_steps;

    private ProgressBar progress;

    private Label action_label;
    private Label message_label;

    private Image icon_image;
    private Spinner spinner;

    private Revealer revealer1;
    private Revealer revealer2;

    private Button btn_cancel;
    private Button btn_show_files;
    private Button btn_quit;

    private Pid child_pid;

    private string result_file_name;

    public ProgressWindow(string file_name, string[] files_to_be_updated) {

	    var proc_file = File.new_for_path("/proc/self/exe");
	    FileInfo proc_info;
	    proc_info = proc_file.query_info("standard::symlink-target", FileQueryInfoFlags.NONE);
	    var self_path = proc_info.get_symlink_target();
	    var self_file = File.new_for_path(self_path);
	    var parent_dir = self_file.get_parent().get_path();

            var builder = new Builder();
            try {
                builder.add_from_file (parent_dir + "/ui/progress.ui");
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

            spinner = builder.get_object("spinner") as Spinner;

            revealer1 = builder.get_object("revealer1") as Revealer;
            revealer1.set_reveal_child (true);
            revealer2 = builder.get_object("revealer2") as Revealer;
            revealer2.set_reveal_child (false);

            spinner.set_vexpand(true);
            spinner.set_hexpand(true);

            btn_cancel = builder.get_object("btn_cancel") as Button;
            btn_cancel.clicked.connect(main_quit);

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

	    Process.spawn_async_with_pipes (null,
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

        // Regular expression to get the percentage
        try {
            var regex = new Regex ("(\\d+(\\.\\d+)?%)");
            if(regex.match(line)){
              int percent_read = int.parse(regex.split(line)[1].split(".")[0]);
              stdout.printf("%i percent\n", percent_read);
              progress.set_fraction( 1.0f * percent_read / 100);
            }

        } catch (RegexError e) {
            warning ("%s", e.message);
        }

      action_label.label = line.substring(0, line.length - 1);
      counter++;

      if(progress.get_fraction() == 1) spinner.stop();
      if(line.index_of ("Target ") == 0) {
			result_file_name=line.replace("Target ","");
			result_file_name=result_file_name.strip();
			result_file_name=Path.build_path (Path.DIR_SEPARATOR_S, Path.get_dirname(this.file_name), result_file_name);
                    }
                    if(line.contains("checksum matches OK")) {
			revealer1.set_reveal_child (false);
			revealer2.set_reveal_child (true);
		        icon_image.icon_name="emblem-ok-symbolic";
                        Posix.system("chmod 0755 " + result_file_name); // Because the CLI does not reliably do it (FIXME)
			btn_show_files.set_visible(true);
                    }

                    if(line.contains("Cannot") || line.contains("Error")) {
			display_error();
                    }
	    } catch (IOChannelError e) {
		    stdout.printf ("%s: IOChannelError: %s\n", stream_name, e.message);
		    display_error();
		    return false;
	    } catch (ConvertError e) {
		    stdout.printf ("%s: ConvertError: %s\n", stream_name, e.message);
		    display_error();
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
                    if(line.contains("Cannot") || line.contains("Error")) {
			display_error();
                    }

	    } catch (IOChannelError e) {
		    stdout.printf ("%s: IOChannelError: %s\n", stream_name, e.message);
		    display_error();
		    return false;
	    } catch (ConvertError e) {
		    stdout.printf ("%s: ConvertError: %s\n", stream_name, e.message);
		    display_error();
		    return false;
	    }

            return true;
    }

    private void display_error() {
        revealer1.set_reveal_child (false);
	revealer2.set_reveal_child (true);
	progress.set_fraction(0.0);
	icon_image.icon_name="dialog-error-symbolic";
    }

    private void show_files() {
	    print(result_file_name);
            try {
	        Posix.system(result_file_name + " &");

            } catch (SpawnError e) {
	            stdout.printf("Error launching file: " + e.message);
            }
            main_quit();
    }

    private void cancel() {
        Posix.kill(child_pid, Posix.SIGTERM);
        main_quit();
    }

}
