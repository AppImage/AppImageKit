public static int main (string[] args) {
    MainLoop loop = new MainLoop ();
    try {
        string[] spawn_args = {"pkexec", "appimageupdategui", "arg1", "arg2"};
        string[] spawn_env = Environ.get ();
        Pid child_pid;

        Process.spawn_async ("/",
            spawn_args,
            spawn_env,
            SpawnFlags.SEARCH_PATH | SpawnFlags.DO_NOT_REAP_CHILD,
            null,
            out child_pid);

        ChildWatch.add (child_pid, (pid, status) => {
            // Triggered when the child indicated by child_pid exits
            Process.close_pid (pid);
            loop.quit ();
        });

        loop.run ();
    } catch (SpawnError e) {
        stdout.printf ("Error: %s\n", e.message);
    }
    return 0;
}
