/**************************************************************************

Copyright (c) 2004-18 Simon Peter
Portions Copyright (c) 2010 RazZziel

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

/** Macro to throw error and exit. */
#define die(...)                                    \
    do {                                            \
        fprintf(stderr, "Error: " __VA_ARGS__);     \
        exit(1);                                    \
    } while(0);

/** Macro to set environmental variables. */
#define SET_NEW_ENV(str,len,fmt,...)                \
    format = fmt;                                   \
    length = strlen(format) + (len);                \
    char *str = calloc(length, sizeof(char));     \
    snprintf(str, length, format, __VA_ARGS__);   \
    putenv(str);

/** Macro to return the largest of a pair of numbers */
#define MAX(a,b)    (a > b ? a : b)

/* Define the boolean type and its constants to use integers, with n<0 == true.
 * FIXME: This is terrible, bad and wrong. Fix it in refactor.
 */
#define bool int
#define false 0
#define true -1

#define LINE_SIZE 255

/** Check for a *.desktop file at the given path.
  * \param the file path to check
  * \return True (1) if the path points to a .desktop file, else false (0).
  */
int filter(const struct dirent *dir) {
    // Get pointer to file path.
    char *p = (char*) &dir->d_name;
    // Read only the file extension (start reading from last dot in path)
    p = strrchr(p, '.');
    // Return true if the file extension .desktop is found
    return p && !strcmp(p, ".desktop");
}

int main(int argc, char *argv[]) {
    // Get path to the application directory.
    char *appdir = dirname(realpath("/proc/self/exe", NULL));
    /* If there's a problem accessing the application directory,
     * throw fatal error.
     */
    if (!appdir)
        die("Could not access /proc/self/exe\n");

    // Check for .desktop files
    // The return code for whether we found a .desktop file
    int ret;
    // An array of pointers to lists of file paths.
    struct dirent** namelist;
    // Scan for .desktop files.
    ret = scandir(appdir, &namelist, filter, NULL);

    // If we found no .desktop files, there's nothing to run. Fatal error.
    if (ret == 0) {
        die("No .desktop files found\n");

    // If there was an error with scandir(...), we can't go on. Fatal error.
    } else if(ret == -1) {
        die("Could not scan directory %s\n", appdir);
    }

    /* Extract executable from .desktop file */
    char *desktop_file = calloc(LINE_SIZE, sizeof(char));
    snprintf(desktop_file, LINE_SIZE, "%s/%s", appdir, namelist[0]->d_name);
    FILE *f     = fopen(desktop_file, "r");
    // Store the line we're reading.
    char *line  = malloc(LINE_SIZE);
    // Skip the `Exec=` prefix on `line`, only looking at the stored value.
    char *exe   = line+5;
    size_t n    = LINE_SIZE;

    /* Search each line of our file for the 'Exec=' entry.
     * To search, we store the line in `line`, and then check for the
     * correct prefix. If it doesn't match, we keep looking.
     */
    do {
        // If we reach the end of the file without finding 'Exec=', fatal error.
        if (getline(&line, &n, f) == -1)
            die("Executable not found, make sure there is a line starting with 'Exec='\n");
    } while(strncmp(line, "Exec=", 5));
    // We now have the line we need, close the file.
    fclose(f);

    /* FIXME: We aren't ensuring that 'exe' has a value!
     * If the entire line is `Exec=`, attempting to access `exe` will result
     * in undefined behavior (wild pointer).
     */

    /* Parse arguments */

    // Flag if we're within quotation marks.
    bool in_quotes = 0;

    // Loop through each character in a line.
    /* FIXME: This should use a while or do-while, instead of a for with an
     * arbitrary character range (LINE_SIZE = 255). This is both inefficient
     * and prone to undesired edge-case behavior.
     * Also, why in the hey aren't we using the `exe` string?
     */
    for (n = 0; n < LINE_SIZE; n++) {
        // If we've reached the end of the string/line, break out of loop.
        if (!line[n]) {
            break;

        /* If we encounter a LINE FEED (10) or CARRIAGE RETURN (13)... */
        } else if (line[n] == 10 || line[n] == 13) {
            /* Change the next three characters to NULL CHAR.
             * NOTE: Why?
             * FIXME: What if [n+1] and/or [n+2] are past the end of the string?
             */
            line[n] = '\0';
            line[n+1] = '\0';
            line[n+2] = '\0';
            // Break out of loop.
            break;

        // If the current character is a quote, invert our `in_quotes` flag.
        } else if (line[n] == '"') {
            // FIXME: This logic doesn't account for escaped quotes (\")
            in_quotes = !in_quotes;
        }
        /* If the current character is a space, change it to a NULL CHAR,
         * effectively splitting the string into multiple strings.
         * This will allow us to iterate through the space-delimited args
         * in the upcoming step.
         */
        else if (line[n] == ' ' && !in_quotes)
            line[n] = '\0';
    }

    // Create a pointer to each "substring" within `line`.
    char*   arg         = exe;
    // Counter for the number of arguments found.
    int     argcount    = 0;

    /* FIXME: The following seems to only abort the loop on a NULL pointer!
     * Possible undefined behavior if `line+1` is a valid (thus wild) pointer.
     */
    // Loop through each memory-adjacent string, counting them up.
    while ((arg += (strlen(arg)+1)) && *arg)
        argcount += 1;

    /* Merge the `Exec=` arguments with the `argv` arguments
     * from the call to `main()`.
     */
    // Store the final list of arguments as an array of C-strings.
    char*   outargptrs[argcount + argc + 1];
    // Store the name of the executable as the first argument.
    outargptrs[0] = exe;
    // Keep track of which argument (index) we're storing.
    int     outargindex = 1;
    /* Reset the `arg` pointer to the beginning of the sequence of
     * memory-adjacent strings.
     */
    arg                 = exe;
    // Skip the first argument, which is the filename, from the `main()` call.
    int     argc_       = argc - 1;
    char**  argv_       = argv + 1;
    // Iterate through each argument from `Exec()`.
    // FIXME: Same possible undefined behavior as earlier.
    while ((arg += (strlen(arg)+1)) && *arg) {
        // If the current argument is a desktop file field code...
        if (arg[0] == '%' || (arg[0] == '"' && arg[1] == '%')) {
            // Get the actual code after the `%` or `"%`
            char code = arg[arg[0] == '%' ? 1 : 2];
            switch(code) {
                // For single file or URL args, merge in one of the `argv` args.
                case 'f':
                case 'u':
                    if (argc_ > 0) {
                        outargptrs[outargindex++] = *argv_++;
                        argc_--;
                    }
                    break;
                /* For multiple file or URL args, merge in the rest of the
                 * `argv` args
                 */
                case 'F':
                case 'U':
                    while (argc_ > 0) {
                        outargptrs[outargindex++] = *argv_++;
                        argc_--;
                    }
                    break;
                /* The Icon (i), translated name (c), and desktop file location
                 * (k) keys are not supported. Throw a non-fatal error.
                 */
                case 'i':
                case 'c':
                case 'k':
                    fprintf(stderr, "WARNING: Desktop file field code %%%c is not currently supported\n", code);
                    break;
                // Everything else is invalid. Throw a non-fatal error.
                // TODO: Properly support deprecated codes (accept but remove) according to spec (???)
                default:
                    fprintf(stderr, "WARNING: Invalid desktop file field code %%%c\n", code);
                    break;
            }

        // If the current arg is anything else, just store it as-is.
        } else {
            outargptrs[outargindex++] = arg;
        }
    }

    // Iterate through and store all of the remaining `argv` arguments.
    while (argc_ > 0) {
        outargptrs[outargindex++] = *argv_++;
        argc_--;
    }
    // A trailing null argument is required by `execvp()`
    outargptrs[outargindex] = '\0';

    // Change directory to the application directory we found earlier.

    // Calculate the length of the application directory path string.
    size_t appdir_s = strlen(appdir);
    // Define the path to the ./usr subfolder in the application directory.
    char *usr_in_appdir = malloc(appdir_s + 5);
    snprintf(usr_in_appdir, appdir_s + 5, "%s/usr", appdir);
    // Attempt to change to the `<appdir>/usr` directory.
    ret = chdir(usr_in_appdir);
    // If we encounter a problem changing to that directory, throw a fatal error.
    if (ret != 0)
        die("Could not cd into %s\n", usr_in_appdir);

    // Append to the environment variables, so the AppImage can run correctly.
    char *old_env;
    size_t length;
    const char *format;

    /* Append the Python home path.
     * https://docs.python.org/2/using/cmdline.html#envvar-PYTHONHOME
     */
    SET_NEW_ENV(new_pythonhome, appdir_s, "PYTHONHOME=%s/usr/", appdir);

    // Append the system path.
    old_env = getenv("PATH") ?: "";
    SET_NEW_ENV(new_path, appdir_s*5 + strlen(old_env), "PATH=%s/usr/bin/:%s/usr/sbin/:%s/usr/games/:%s/bin/:%s/sbin/:%s", appdir, appdir, appdir, appdir, appdir, old_env);

    // Append the system library path.
    old_env = getenv("LD_LIBRARY_PATH") ?: "";
    SET_NEW_ENV(new_ld_library_path, appdir_s*10 + strlen(old_env), "LD_LIBRARY_PATH=%s/usr/lib/:%s/usr/lib/i386-linux-gnu/:%s/usr/lib/x86_64-linux-gnu/:%s/usr/lib32/:%s/usr/lib64/:%s/lib/:%s/lib/i386-linux-gnu/:%s/lib/x86_64-linux-gnu/:%s/lib32/:%s/lib64/:%s", appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, old_env);

    // Append the Python path.
    old_env = getenv("PYTHONPATH") ?: "";
    SET_NEW_ENV(new_pythonpath, appdir_s + strlen(old_env), "PYTHONPATH=%s/usr/share/pyshared/:%s", appdir, old_env);

    // Append the XDG path
    old_env = getenv("XDG_DATA_DIRS") ?: "/usr/local/share/:/usr/share/";
    SET_NEW_ENV(new_xdg_data_dirs, appdir_s + strlen(old_env), "XDG_DATA_DIRS=%s/usr/share/:%s", appdir, old_env);

    // Append the Perl path
    old_env = getenv("PERLLIB") ?: "";
    SET_NEW_ENV(new_perllib, appdir_s*2 + strlen(old_env), "PERLLIB=%s/usr/share/perl5/:%s/usr/lib/perl5/:%s", appdir, appdir, old_env);

    // Append the GSettings Schema path
    /* http://askubuntu.com/questions/251712/how-can-i-install-a-gsettings-schema-without-root-privileges */
    old_env = getenv("GSETTINGS_SCHEMA_DIR") ?: "";
    SET_NEW_ENV(new_gsettings_schema_dir, appdir_s + strlen(old_env), "GSETTINGS_SCHEMA_DIR=%s/usr/share/glib-2.0/schemas/:%s", appdir, old_env);

    //Append the Qt Path
    old_env = getenv("QT_PLUGIN_PATH") ?: "";
    SET_NEW_ENV(new_qt_plugin_path, appdir_s*10 + strlen(old_env), "QT_PLUGIN_PATH=%s/usr/lib/qt4/plugins/:%s/usr/lib/i386-linux-gnu/qt4/plugins/:%s/usr/lib/x86_64-linux-gnu/qt4/plugins/:%s/usr/lib32/qt4/plugins/:%s/usr/lib64/qt4/plugins/:%s/usr/lib/qt5/plugins/:%s/usr/lib/i386-linux-gnu/qt5/plugins/:%s/usr/lib/x86_64-linux-gnu/qt5/plugins/:%s/usr/lib32/qt5/plugins/:%s/usr/lib64/qt5/plugins/:%s", appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, old_env);

    /* We need this, otherwise we may get errors because Python cannot
     * write __pycache__ bytecode cache.
     */
    putenv("PYTHONDONTWRITEBYTECODE=1");

    /* Execute the program, using the executable name from the .desktop
     * file (`exe`) and the final merged array of arguments.
     */
    ret = execvp(exe, outargptrs);

    // Store the last error number; in this case, the one from `execvp(...)`.
    int error = errno;

    /* If `execvp(...)` returned a negative value, throw a fatal error using
     * the retrieved error code.
     */
    if (ret == -1)
        die("Error executing '%s': %s\n", exe, strerror(error));

    // Deallocate everything we've malloc'd.
    free(line);
    free(desktop_file);
    free(usr_in_appdir);
    free(new_pythonhome);
    free(new_path);
    free(new_ld_library_path);
    free(new_pythonpath);
    free(new_xdg_data_dirs);
    free(new_perllib);
    free(new_gsettings_schema_dir);
    free(new_qt_plugin_path);

    return 0;
}
