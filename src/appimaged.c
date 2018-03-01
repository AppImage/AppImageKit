/**************************************************************************
 *
 * Copyright (c) 2004-18 Simon Peter
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#ident "AppImage by Simon Peter, http://appimage.org/"

/*
 * Optional daempon to watch directories for AppImages
 * and register/unregister them with the system
 *
 * TODO (feel free to send pull requests):
 * - Switch to https://developer.gnome.org/gio/stable/GFileMonitor.html (but with subdirectories)
 *   which would drop the dependency on libinotifytools.so.0
 * - Add and remove subdirectories on the fly at runtime -
 *   see https://github.com/paragone/configure-via-inotify/blob/master/inotify/src/inotifywatch.c
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <inotifytools/inotifytools.h>
#include <inotifytools/inotify.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "shared.c"

#include <pthread.h>

#ifndef RELEASE_NAME
    #define RELEASE_NAME "continuous build"
#endif

extern int notify(char *title, char *body, int timeout);

static gboolean verbose = FALSE;
static gboolean showVersionOnly = FALSE;
static gboolean install = FALSE;
static gboolean uninstall = FALSE;
static gboolean no_install = FALSE;
gchar **remaining_args = NULL;

static GOptionEntry entries[] =
{
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
    { "install", 'i', 0, G_OPTION_ARG_NONE, &install, "Install this appimaged instance to $HOME", NULL },
    { "uninstall", 'u', 0, G_OPTION_ARG_NONE, &uninstall, "Uninstall an appimaged instance from $HOME", NULL },
    { "no-install", 'n', 0, G_OPTION_ARG_NONE, &no_install, "Force run without installation", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &showVersionOnly, "Show version number", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &remaining_args, NULL },
    { NULL }
};

#define EXCLUDE_CHUNK 1024
#define WR_EVENTS (IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF)

/* Run the actual work in treads;
 * pthread allows to pass only one argument to the thread function,
 * hence we use a struct as the argument in which the real arguments are */
struct arg_struct {
    char* path;
    gboolean verbose;
};

void *thread_appimage_register_in_system(void *arguments)
{
    struct arg_struct *args = arguments;
    appimage_register_in_system(args->path, args->verbose);
    pthread_exit(NULL);
}

void *thread_appimage_unregister_in_system(void *arguments)
{
    struct arg_struct *args = arguments;
    appimage_unregister_in_system(args->path, args->verbose);
    pthread_exit(NULL);
}

/* Recursively process the files in this directory and its subdirectories,
 * http://stackoverflow.com/questions/8436841/how-to-recursively-list-directories-in-c-on-linux
 */
void initially_register(const char *name, int level)
{
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name))) {
        if (verbose) {
            if (errno == EACCES) {
                g_print("_________________________\n");
                g_print("Permission denied on dir '%s'\n", name);
            }
            else {
                g_print("_________________________\n");
                g_print("Failed to open dir '%s'\n", name);
            }
        }
        closedir(dir);
        return;
    }

    if (!(entry = readdir(dir))) {
        if (verbose) {
            g_print("_________________________\n");
            g_print("Invalid directory stream descriptor '%s'\n", name);
        }
        closedir(dir);
        return;
    }

    do {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            int len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
            path[len] = 0;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            initially_register(path, level + 1);
        }
        else {
            int ret;
            gchar *absolute_path = g_build_path(G_DIR_SEPARATOR_S, name, entry->d_name, NULL);
            if(g_file_test(absolute_path, G_FILE_TEST_IS_REGULAR)){
                pthread_t some_thread;
                struct arg_struct args;
                args.path = absolute_path;
                args.verbose = verbose;
                ret = pthread_create(&some_thread, NULL, thread_appimage_register_in_system, &args);
                if (!ret) {
                    pthread_join(some_thread, NULL);
                }
            }
            g_free(absolute_path);
        }
    } while ((entry = readdir(dir)) != NULL);
    closedir(dir);
}

void add_dir_to_watch(const char *directory)
{
    if (g_file_test (directory, G_FILE_TEST_IS_DIR)){
        if(!inotifytools_watch_recursively(directory, WR_EVENTS) ) {
            fprintf(stderr, "%s\n", strerror(inotifytools_error()));
            exit(1);

        }
        initially_register(directory, 0);
    }
}

void handle_event(struct inotify_event *event)
{
    int ret;
    gchar *absolute_path = g_build_path(G_DIR_SEPARATOR_S, inotifytools_filename_from_wd(event->wd), event->name, NULL);

    if((event->mask & IN_CLOSE_WRITE) | (event->mask & IN_MOVED_TO)){
        if(g_file_test(absolute_path, G_FILE_TEST_IS_REGULAR)){
            pthread_t some_thread;
            struct arg_struct args;
            args.path = absolute_path;
            args.verbose = verbose;
            g_print("_________________________\n");
            ret = pthread_create(&some_thread, NULL, thread_appimage_register_in_system, &args);
            if (!ret) {
                pthread_join(some_thread, NULL);
            }
        }
    }

    if((event->mask & IN_MOVED_FROM) | (event->mask & IN_DELETE)){
        pthread_t some_thread;
        struct arg_struct args;
        args.path = absolute_path;
        args.verbose = verbose;
        g_print("_________________________\n");
        ret = pthread_create(&some_thread, NULL, thread_appimage_unregister_in_system, &args);
        if (!ret) {
            pthread_join(some_thread, NULL);
        }
    }

    g_free(absolute_path);

    /* Too many FS events were received, some event notifications were potentially lost */
    if (event->mask & IN_Q_OVERFLOW){
        printf ("Warning: AN OVERFLOW EVENT OCCURRED\n");
    }

    if(event->mask & IN_IGNORED){
        printf ("Warning: AN IN_IGNORED EVENT OCCURRED\n");
    }

}

int main(int argc, char ** argv) {

    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new ("");
    g_option_context_add_main_entries (context, entries, NULL);
    // g_option_context_add_group (context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print("option parsing failed: %s\n", error->message);
        exit (1);
    }

    // always show version, but exit immediately if only the version number was requested
    fprintf(
        stderr,
        "appimaged, %s (commit %s), build %s built on %s\n",
        RELEASE_NAME, GIT_COMMIT, BUILD_NUMBER, BUILD_DATE
    );

    if(showVersionOnly)
        exit(0);

    if (!inotifytools_initialize()) {
        fprintf(stderr, "inotifytools_initialize error\n");
        exit(1);
    }

    gchar *user_bin_dir = g_build_filename(g_get_home_dir(), "/.local/bin", NULL);
    gchar *installed_appimaged_location = g_build_filename(user_bin_dir, "appimaged", NULL);
    const gchar *appimage_location = g_getenv("APPIMAGE");
    gchar *own_desktop_file_location = g_build_filename(g_getenv("APPDIR"), "/appimaged.desktop", NULL);
    gchar *global_autostart_file = "/etc/xdg/autostart/appimaged.desktop";
    gchar *global_systemd_file = "/usr/lib/systemd/user/appimaged.service";
    gchar *partial_path = g_strdup_printf("autostart/appimagekit-appimaged.desktop");
    gchar *destination = g_build_filename(g_get_user_config_dir(), partial_path, NULL);

    if(uninstall){
            if(g_file_test (installed_appimaged_location, G_FILE_TEST_EXISTS))
                fprintf(stderr, "* Please delete %s\n", installed_appimaged_location);
            if(g_file_test (destination, G_FILE_TEST_EXISTS))
                fprintf(stderr, "* Please delete %s\n", destination);
            fprintf(stderr, "* To remove all AppImage desktop integration, run\n");
            fprintf(stderr, "  find ~/.local/share -name 'appimagekit_*' -exec rm {} \\;\n\n");
        exit(0);
    }

    if(install){
        if(((appimage_location != NULL)) && ((own_desktop_file_location != NULL))){
            printf("Running from within %s\n", appimage_location);
            if ( (! g_file_test ("/usr/bin/appimaged", G_FILE_TEST_EXISTS)) && (! g_file_test (global_autostart_file, G_FILE_TEST_EXISTS)) && (! g_file_test (global_systemd_file, G_FILE_TEST_EXISTS))){
                printf ("%s is not installed, moving it to %s\n", argv[0], installed_appimaged_location);
                g_mkdir_with_parents(user_bin_dir, 0755);
                gchar *command = g_strdup_printf("mv \"%s\" \"%s\"", appimage_location, installed_appimaged_location);
                system(command);
                /* When appimaged installs itself, then to the $XDG_CONFIG_HOME/autostart/ directory, falling back to ~/.config/autostart/ */
                fprintf(stderr, "Installing to autostart: %s\n", own_desktop_file_location);
                g_mkdir_with_parents(g_path_get_dirname(destination), 0755);
                gchar *command2 = g_strdup_printf("cp \"%s\" \"%s\"", own_desktop_file_location, destination);
                system(command2);
                if(g_file_test (installed_appimaged_location, G_FILE_TEST_EXISTS))
                    fprintf(stderr, "* Installed %s\n", installed_appimaged_location);
                if(g_file_test (destination, G_FILE_TEST_EXISTS)){
                    gchar *command3 = g_strdup_printf("sed -i -e 's|^Exec=.*|Exec=%s|g' '%s'", installed_appimaged_location, destination);
                    if(verbose)
                        fprintf(stderr, "%s\n", command3);
                    system(command3);
                    fprintf(stderr, "* Installed %s\n", destination);
                }
                if(g_file_test (installed_appimaged_location, G_FILE_TEST_EXISTS))
                    fprintf(stderr, "\nTo uninstall, run %s --uninstall and follow the instructions\n\n", installed_appimaged_location);
                char *title;
                char *body;
                title = g_strdup_printf("Please log out");
                body = g_strdup_printf("and log in again to complete the installation");
                notify(title, body, 15);
                exit(0);
            }
        } else {
            printf("Not running from within an AppImage. This binary cannot be installed in this way.\n");
            exit(1);
        }
    }

    /* When we run from inside an AppImage, then we check if we are installed
     * in a per-user location and if not, we install ourselves there */
    if(!no_install && (appimage_location != NULL && own_desktop_file_location != NULL)) {
        if ( (! g_file_test ("/usr/bin/appimaged", G_FILE_TEST_EXISTS)) && ((! g_file_test (global_autostart_file, G_FILE_TEST_EXISTS)) || (! g_file_test (destination, G_FILE_TEST_EXISTS))) && (! g_file_test (global_systemd_file, G_FILE_TEST_EXISTS)) && (! g_file_test (installed_appimaged_location, G_FILE_TEST_EXISTS)) && (g_file_test (own_desktop_file_location, G_FILE_TEST_IS_REGULAR))){
            char *title;
            char *body;
            title = g_strdup_printf("Not installed\n");
            body = g_strdup_printf("Please run %s --install", argv[0]);
            notify(title, body, 15);
            exit(1);
        }
    }

    add_dir_to_watch(user_bin_dir);
    add_dir_to_watch(g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD));
    add_dir_to_watch(g_build_filename(g_get_home_dir(), "/bin", NULL));
    add_dir_to_watch(g_build_filename(g_get_home_dir(), "/.bin", NULL));
    add_dir_to_watch(g_build_filename(g_get_home_dir(), "/Applications", NULL));
    add_dir_to_watch(g_build_filename("/Applications", NULL));
    // Perhaps we should determine the following dynamically using something like
    // mount | grep -i iso | head -n 1 | cut -d ' ' -f 3
    add_dir_to_watch(g_build_filename("/isodevice/Applications", NULL)); // Ubuntu Live media
    add_dir_to_watch(g_build_filename("/isofrom/Applications", NULL)); // openSUSE Live media
    add_dir_to_watch(g_build_filename("/run/archiso/img_dev/Applications", NULL)); // Antergos Live media
    add_dir_to_watch(g_build_filename("/lib/live/mount/findiso/Applications", NULL)); // Manjaro Live media
    add_dir_to_watch(g_build_filename("/opt", NULL));
    add_dir_to_watch(g_build_filename("/usr/local/bin", NULL));

    struct inotify_event * event = inotifytools_next_event(-1);
    while (event) {
        if(verbose){
            inotifytools_printf(event, "%w%f %e\n");
        }
        fflush(stdout);
        handle_event(event);
        fflush(stdout);
        event = inotifytools_next_event(-1);
    }
}
