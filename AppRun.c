/**************************************************************************

Copyright (c) 2004-16 Simon Peter
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

#define die(...)                                \
    do {                                        \
        fprintf(stderr, "Error: " __VA_ARGS__);   \
        exit(1);                              \
    } while (0);

#define LINE_SIZE 255

int filter (const struct dirent *dir)
{
    char *p = (char*) &dir->d_name;
    p = strrchr(p, '.');
    return p && !strcmp(p, ".desktop");
}

int main(int argc, char *argv[])
{
    char *appdir = dirname(realpath("/proc/self/exe", NULL));
    if (!appdir)
        die("Could not access /proc/self/exe\n");
    
    char path[LINE_SIZE];
    int ret;

    struct dirent **namelist;

    ret = scandir(appdir, &namelist, filter, NULL);

    if (ret == 0) {
        die("No .desktop files found\n");
    } else if(ret == -1) {
        die("Could not scan directory %s\n", appdir);
    }
    
    /* Extract executable from .desktop file */

    FILE *f;
    char *desktop_file = malloc(LINE_SIZE);
    snprintf(desktop_file, LINE_SIZE-1, "%s/%s", appdir, namelist[0]->d_name);
    f = fopen(desktop_file, "r");

    char *line = malloc(LINE_SIZE);
    unsigned int n = LINE_SIZE;
    int found = 0;

    while (getline(&line, &n, f) != -1)
    {
        if (!strncmp(line,"Exec=",5))
        {
            char *p = line+5;
            while (*++p && *p != ' ' &&  *p != '%'  &&  *p != '\n');
            *p = 0;
            found = 1;
            break;
        }
    }

    fclose(f);

    if (!found)
      die("Executable not found, make sure there is a line starting with 'Exec='\n");

    /* Execution */
    char *executable = basename(line+5);

    char usr_in_appdir[1024];
    sprintf(usr_in_appdir, "%s/usr", appdir);
    ret = chdir(usr_in_appdir);
    if (ret != 0)
        die("Could not cd into %s\n", usr_in_appdir);

    /* Build environment */
    char *old_env;
    int length = 2047;
    char new_env1[length+1];
    char new_env2[length+1];
    char new_env3[length+1];
    char new_env4[length+1];
    char new_env5[length+1];
    char new_env6[length+1];
    char new_env7[length+1];
    
    old_env = getenv("PATH") ?: "";
    snprintf(new_env1, length, "PATH=%s/usr/bin/:%s/usr/sbin/:%s/usr/games/:%s/bin/:%s/sbin/:%s", appdir, appdir, appdir, appdir, appdir, old_env);
    putenv(new_env1);
    
    old_env = getenv("LD_LIBRARY_PATH") ?: "";
    snprintf(new_env2, length, "LD_LIBRARY_PATH=%s/usr/lib/:%s/usr/lib/i386-linux-gnu/:%s/usr/lib/x86_64-linux-gnu/:%s/usr/lib32/:%s/usr/lib64/:%s/lib/:%s/lib/i386-linux-gnu/:%s/lib/x86_64-linux-gnu/:%s/lib32/:%s/lib64/:%s", appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, old_env);
    putenv(new_env2);

    old_env = getenv("PYTHONPATH") ?: "";
    snprintf(new_env3, length, "PYTHONPATH=%s/usr/share/pyshared/:%s", appdir, old_env);
    putenv(new_env3);

    old_env = getenv("XDG_DATA_DIRS") ?: "";
    snprintf(new_env4, length, "XDG_DATA_DIRS=%s/usr/share/:%s", appdir, old_env);
    putenv(new_env4);

    old_env = getenv("PERLLIB") ?: "";
    snprintf(new_env5, length, "PERLLIB=%s/usr/share/perl5/:%s/usr/lib/perl5/:%s", appdir, appdir, old_env);
    putenv(new_env5);

    /* http://askubuntu.com/questions/251712/how-can-i-install-a-gsettings-schema-without-root-privileges */
    old_env = getenv("GSETTINGS_SCHEMA_DIR") ?: "";
    snprintf(new_env6, length, "GSETTINGS_SCHEMA_DIR=%s/usr/share/glib-2.0/schemas/:%s", appdir, old_env);
    putenv(new_env6);
    
    old_env = getenv("QT_PLUGIN_PATH") ?: "";
    snprintf(new_env7, length, "QT_PLUGIN_PATH=%s/usr/lib/qt4/plugins/:%s/usr/lib/i386-linux-gnu/qt4/plugins/:%s/usr/lib/x86_64-linux-gnu/qt4/plugins/:%s/usr/lib32/qt4/plugins/:%s/usr/lib64/qt4/plugins/:%s/usr/lib/qt5/plugins/:%s/usr/lib/i386-linux-gnu/qt5/plugins/:%s/usr/lib/x86_64-linux-gnu/qt5/plugins/:%s/usr/lib32/qt5/plugins/:%s/usr/lib64/qt5/plugins/:%s", appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, appdir, old_env);
    putenv(new_env7);
    
    /* Run */
    ret = execvp(executable, argv); // FIXME: What about arguments in the Exec= line of the desktop file?

    if (ret == -1)
        die("Error executing '%s'; return code: %d\n", executable, ret);

    free(line);
    free(desktop_file);
    return 0;
}
