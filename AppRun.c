/**************************************************************************

Copyright (c) 2004-17 Simon Peter
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

#define die(...)                                    \
    do {                                            \
        fprintf(stderr, "Error: " __VA_ARGS__);     \
        exit(1);                                    \
    } while(0);
#define MAX(a,b)    (a > b ? a : b)
#define bool int
#define false 0
#define true -1

#define LINE_SIZE 255

int filter(const struct dirent *dir) {
    char *p = (char*) &dir->d_name;
    p = strrchr(p, '.');
    return p && !strcmp(p, ".desktop");
}

char** assemble_args(int argc, char* argv[], const char* appdir, struct dirent** namelist, char *execline) {
    // parse arguments
    bool in_quotes = 0;
    int n;
    for (n = 0; n < LINE_SIZE; n++) {
        if (!execline[n])         // end of string
            break;
        else if (execline[n] == 10 || execline[n] == 13) {
            execline[n] = '\0';
            execline[n+1] = '\0';
            execline[n+2] = '\0';
            break;
        } else if (execline[n] == '"') {
            in_quotes = !in_quotes;
        } else if (execline[n] == ' ' && !in_quotes)
            execline[n] = '\0';
    }

    // count arguments
    char*   arg         = execline;
    int     argcount    = 0;
    while ((arg += (strlen(arg)+1)) && *arg)
        argcount += 1;

    // merge args
    char**  outargptrs = malloc(argcount + argc + 1);
    outargptrs[0] = execline;
    int     outargindex = 1;
    arg           = execline;
    int     argc_       = argc - 1;     // argv[0] is the filename
    char**  argv_       = argv + 1;
    while ((arg += (strlen(arg)+1)) && *arg) {
        if (arg[0] == '%' || (arg[0] == '"' && arg[1] == '%')) {         // handle desktop file field codes
            char code = arg[arg[0] == '%' ? 1 : 2];
            switch(code) {
                case 'f':
                case 'u':
                    if (argc_ > 0) {
                        outargptrs[outargindex++] = *argv_++;
                        argc_--;
                    }
                    break;
                case 'F':
                case 'U':
                    while (argc_ > 0) {
                        outargptrs[outargindex++] = *argv_++;
                        argc_--;
                    }
                    break;
                case 'i':
                case 'c':
                case 'k':
                    fprintf(stderr, "WARNING: Desktop file field code %%%c is not currently supported\n", code);
                    break;
                default:
                    fprintf(stderr, "WARNING: Invalid desktop file field code %%%c\n", code);
                    break;
            }
        } else {
            outargptrs[outargindex++] = arg;
        }
    }
    while (argc_ > 0) {
        outargptrs[outargindex++] = *argv_++;
        argc_--;
    }
    outargptrs[outargindex] = '\0';     // trailing null argument required by execvp()
    return outargptrs;
}

void set_env_vars(const char* appdir) {
    char*   old_env;
    const   LENGTH = 2047;
    char    new_env[8][LENGTH+1];

    /* https://docs.python.org/2/using/cmdline.html#envvar-PYTHONHOME */
    snprintf(new_env[0], LENGTH, "PYTHONHOME=%s/usr/", appdir);

    int a,b;
    old_env = getenv("PATH") ?: "";
    snprintf(new_env[1], LENGTH, "PATH=%s/usr/bin/:%s/usr/sbin/:%s/usr/games/:%s/bin/:%s/sbin/:%s", appdir, appdir, appdir, appdir, appdir, old_env);

    old_env = getenv("LD_LIBRARY_PATH") ?: "";
    snprintf(new_env[2], LENGTH, "LD_LIBRARY_PATH=%s/usr/lib/:%s/usr/lib/i386-linux-gnu/:%s/usr/lib/x86_64-linux-gnu/:%s/usr/lib32/:%s/usr/lib64/:%s/lib/:%s/lib/i386-linux-gnu/:%s/lib/x86_64-linux-gnu/:%s/lib32/:%s/lib64/:%s", appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, old_env);

    old_env = getenv("PYTHONPATH") ?: "";
    snprintf(new_env[3], LENGTH, "PYTHONPATH=%s/usr/share/pyshared/:%s", appdir, old_env);

    old_env = getenv("XDG_DATA_DIRS") ?: "";
    snprintf(new_env[4], LENGTH, "XDG_DATA_DIRS=%s/usr/share/:%s", appdir, old_env);

    old_env = getenv("PERLLIB") ?: "";
    snprintf(new_env[5], LENGTH, "PERLLIB=%s/usr/share/perl5/:%s/usr/lib/perl5/:%s", appdir, appdir, old_env);

    /* http://askubuntu.com/questions/251712/how-can-i-install-a-gsettings-schema-without-root-privileges */
    old_env = getenv("GSETTINGS_SCHEMA_DIR") ?: "";
    snprintf(new_env[6], LENGTH, "GSETTINGS_SCHEMA_DIR=%s/usr/share/glib-2.0/schemas/:%s", appdir, old_env);

    old_env = getenv("QT_PLUGIN_PATH") ?: "";
    snprintf(new_env[7], LENGTH, "QT_PLUGIN_PATH=%s/usr/lib/qt4/plugins/:%s/usr/lib/i386-linux-gnu/qt4/plugins/:%s/usr/lib/x86_64-linux-gnu/qt4/plugins/:%s/usr/lib32/qt4/plugins/:%s/usr/lib64/qt4/plugins/:%s/usr/lib/qt5/plugins/:%s/usr/lib/i386-linux-gnu/qt5/plugins/:%s/usr/lib/x86_64-linux-gnu/qt5/plugins/:%s/usr/lib32/qt5/plugins/:%s/usr/lib64/qt5/plugins/:%s", appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, old_env);

    int n;
    for (n = 0; n < 8; n++)
        putenv(new_env[n]);
}

void trim_lf(char *s) {
    int n;
    int len = strlen(s);
    for (n = 0; n < len; n++)
        switch(s[n]) {
            case '\n':
            case '\r':
                s[n] = '\0';
        }
}

int main(int argc, char* argv[]) {
    char *appdir = dirname(realpath("/proc/self/exe", NULL));
    if (!appdir)
        die("Could not access /proc/self/exe\n");

    struct dirent** namelist;
    int ret = scandir(appdir, &namelist, filter, NULL);

    if (ret == 0) {
        die("No .desktop files found\n");
    } else if (ret == -1) {
        die("Could not scan directory %s\n", appdir);
    }

    /* Extract properties from .desktop file */
    char* desktop_file = malloc(LINE_SIZE);
    snprintf(desktop_file, LINE_SIZE-1, "%s/%s", appdir, namelist[0]->d_name);
    FILE* f         = fopen(desktop_file, "r");
    char* execline  = malloc(LINE_SIZE);
    char* pathline  = malloc(LINE_SIZE);
    size_t n        = LINE_SIZE;

    strcpy(pathline, "usr");        // default path

    // do {
    //     if (getline(&line, &n, f) == -1)
    //         die("Executable not found, make sure there is a line starting with 'Exec='\n");
    // } while(strncmp(line, "Exec=", 5));
    char* line      = malloc(LINE_SIZE);
    while (getline(&line, &n, f) != -1) {
        if (!strncmp(line, "Exec=", 5))
            strcpy(execline, line+5);
        if (!strncmp(line, "Path=", 5))
            strcpy(pathline, line+5);
    }
    fclose(f);
    free(line);
    trim_lf(execline);
    trim_lf(pathline);
    if (strlen(execline) == 0)
        die("Executable not found, make sure there is a line starting with 'Exec='\n");

    char** outargptrs = assemble_args(argc, argv, appdir, namelist, execline);

    // change directory
    // char usr_in_appdir[1024];
    // sprintf(usr_in_appdir, "%s/usr", appdir);
    char path[1024];
    sprintf(path, "%s/%s", appdir, pathline);        // default path
    ret = chdir(path);
    if (ret != 0)
        die("%d: Could not cd into %s\n", errno, path);

    set_env_vars(appdir);

    /* Run */
    ret = execvp(execline, outargptrs);

    if (ret == -1)
        die("Error executing '%s'; return code: %d\n", execline, ret);

    free(execline);
    free(pathline);
    free(desktop_file);
    free(outargptrs);
    return 0;
}
