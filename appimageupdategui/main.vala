using GLib;
using Gtk;

static Window window_main;
static Gtk.Menu main_menu;
static Gtk.Menu popup_menu;  // Too new to assume it is there?
static HeaderBar header_bar; // Too new to assume it is there?

static string selected_file;

/* Open file */
static void open_file(string filename) {
    selected_file = filename;
    new ProgressWindow(selected_file, {});
}

/* Handle open click event */
static void on_open_clicked() {

    var file_chooser = new FileChooserDialog ("Open File", window_main,
                                      FileChooserAction.OPEN,
                                      "gtk-cancel", ResponseType.CANCEL,
                                      "gtk-open", ResponseType.ACCEPT);
        FileFilter filter = new FileFilter ();
	file_chooser.set_filter (filter);
  file_chooser.set_current_folder(GLib.Environment.get_variable("HOME"));
	filter.add_pattern("*.AppImage");
	filter.add_pattern("*.iso");
        if (file_chooser.run () == ResponseType.ACCEPT) {
            open_file(file_chooser.get_filename ());
        }
	else {
            Posix.exit(0);
	}
	window_main.destroy();
	file_chooser.destroy();
}

/* Handle about click event */
static void on_about_clicked() {
    show_about_dialog (window_main,
		"program-name", ("AppImageUpdate"),
		"copyright", ("Copyright © 2015 Simon Peter"),
		"version", "@@@VERSION@@@" // To be replaced e.g., with the git tag during the build process
		);
}

static void main (string[] args) {
	init (ref args);
  if(args.length > 1){
    open_file(args[1]);
    Gtk.main();
    Posix.exit(0);
  }

	/* Set up the main window */
	window_main = new Window();
	window_main.title = "AppImageUpdate";
	window_main.set_default_size (800, 600);
	window_main.destroy.connect (main_quit);

	/* Set app icon */
	try {
		window_main.icon = new Gdk.Pixbuf.from_file("appimageupdate.svg");
	} catch (Error e) {
		stderr.printf ("Could not load application icon: %s\n", e.message);
	}

	/* Vertical box containing widgets */
	var vbox_main = new Box (Orientation.VERTICAL, 0);

	/* Init headerbar for app title and buttons */
	header_bar = new HeaderBar();
	header_bar.set_title(window_main.title);
	header_bar.show_close_button = true;
	window_main.set_titlebar(header_bar);

	var  btn_menu = new MenuButton();
	var  btn_open = new Button();

	var menu_icon = new Image.from_icon_name("gtk-menu" , IconSize.LARGE_TOOLBAR);
	var open_icon = new Image.from_icon_name("gtk-open" , IconSize.LARGE_TOOLBAR);

	var proc_file = File.new_for_path("/proc/self/exe");
	FileInfo proc_info;
	proc_info = proc_file.query_info("standard::symlink-target", FileQueryInfoFlags.NONE);
	var self_path = proc_info.get_symlink_target();
	var self_file = File.new_for_path(self_path);
	var parent_dir = self_file.get_parent().get_path();

	var builder = new Builder();
	try {
		builder.add_from_file (parent_dir + "/ui/menu.ui");
	}
	catch (Error e) {
		error ("Unable to load file: %s", e.message);
	}
	builder.connect_signals (null);
	main_menu = builder.get_object("main_menu") as Gtk.Menu;
	var open_item = builder.get_object("open") as Gtk.MenuItem;
	var about_item = builder.get_object("about") as Gtk.MenuItem;
	open_item.activate.connect (on_open_clicked);
	about_item.activate.connect (on_about_clicked);

	popup_menu = builder.get_object("popup_menu") as Gtk.Menu;

	btn_menu.set_popup(main_menu);
	btn_menu.set_image(menu_icon);
	btn_open.set_image(open_icon);

	btn_open.set_tooltip_text("Open");

	header_bar.pack_start(btn_open);
	header_bar.pack_end(btn_menu);

	vbox_main.pack_start(header_bar, false, true);

	window_main.add (vbox_main);

	// window_main.show_all(); // Currently we are not showing the Main screen at all (TODO)

	on_open_clicked();

	Gtk.main();
}
