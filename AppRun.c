/**************************************************************************

Copyright (c) 2010 RazZziel
Copyright (c) 2004-16 Simon Peter

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
        fprintf( stderr, "Error: " __VA_ARGS__ );   \
        exit( 1 );                              \
    } while (0);

#define NEW_LD_LIBRARY_PATH "LD_LIBRARY_PATH=./lib/:./lib/i386-linux-gnu/:./lib/x86_64-linux-gnu/:./lib32/:./lib64/%s"
#define NEW_PATH "PATH=./bin/:./sbin/:./games/:%s"
#define NEW_PYTHONPATH "PYTHONPATH=./share/pyshared/:%s"
#define NEW_XDG_DATA_DIRS "XDG_DATA_DIRS=./share/:%s"
#define NEW_QT_PLUGIN_PATH "QT_PLUGIN_PATH=./lib/qt4/plugins/:./lib/qt5/plugins/:%s"
#define NEW_PERLLIB "PERLLIB=./share/perl5/:./lib/perl5/:%s"

// http://askubuntu.com/questions/251712/how-can-i-install-a-gsettings-schema-without-root-privileges
#define NEW_GSETTINGS_SCHEMA_DIR "GSETTINGS_SCHEMA_DIR=./share/glib-2.0/schemas/:%s" 

#define LINE_SIZE 255

int filter (const struct dirent *dir)
{
    char *p = (char*) &dir->d_name;
    while (*++p);
    while (*--p != '.');

    return !strcmp(p, ".desktop");
}

int main(int argc, char *argv[])
{
    char path[LINE_SIZE];
    int ret;
    FILE *f;

    char *dir = realpath( "/proc/self/exe", NULL );
    if (!dir)
    {
        die( "Could not access /proc/self/exe\n" );
    }

    sprintf( path, "%s", dirname(dir) );

    // printf( "Moving to %s ...\n", path );

    ret = chdir(path);

    if ( ret != 0 )
    {
        die( "Could not cd into %s\n", path );
    }

    struct dirent **namelist;

    ret = scandir( ".", &namelist, filter, NULL );

    if ( ret == 0 )
    {
        die( "No .desktop files found\n" );
    }
    else if ( ret == -1 )
    {
        die( "Could not scan directory %s\n", path );
    }


    /* Extract executable from .desktop file 
    printf( "Extracting executable name from '%s' ...\n",
            namelist[0]->d_name ); */

    f = fopen(namelist[0]->d_name, "r");

    char *line = malloc(LINE_SIZE);
    unsigned int n = LINE_SIZE;
    int found = 0;

    while (getline( &line, &n, f ) != -1)
    {
        if (!strncmp(line,"Exec=",5))
        {
            char *p = line+5;
            while (*++p && *p != ' ' &&  *p != '%'  &&  *p != '\n' );
            *p = 0;
            found = 1;
            break;
        }
    }

    fclose( f );

    if (!found)
    {
        die( "Executable not found, make sure there is a line starting with 'Exec='\n" );
    }

    /* Execution */
    char *executable = basename(line+5);
    // printf( "Executing '%s' ...\n", executable );

    ret = chdir("usr");
    if ( ret != 0 )
    {
        die( "Could not cd into %s\n", "usr" );
    }

    /* Build environment */
    char *env, *new_env[7];

    env = getenv("LD_LIBRARY_PATH") ?: "";
    new_env[0] = malloc( strlen(NEW_LD_LIBRARY_PATH) + strlen(env) );
    sprintf( new_env[0], NEW_LD_LIBRARY_PATH, env );
    // printf( "  using %s\n", new_env[0] );
    putenv( new_env[0] );

    env = getenv("PATH") ?: "";
    new_env[1] = malloc( strlen(NEW_PATH) + strlen(env) );
    sprintf( new_env[1], NEW_PATH, env );
    // printf( "  using %s\n", new_env[1] );
    putenv( new_env[1] );

    env = getenv("PYTHONPATH") ?: "";
    new_env[2] = malloc( strlen(NEW_PYTHONPATH) + strlen(env) );
    sprintf( new_env[2], NEW_PYTHONPATH, env );
    // printf( "  using %s\n", new_env[2] );
    putenv( new_env[2] );

    env = getenv("XDG_DATA_DIRS") ?: "";
    new_env[3] = malloc( strlen(NEW_XDG_DATA_DIRS) + strlen(env) );
    sprintf( new_env[3], NEW_XDG_DATA_DIRS, env );
    // printf( "  using %s\n", new_env[3] );
    putenv( new_env[3] );

    env = getenv("QT_PLUGIN_PATH") ?: "";
    new_env[4] = malloc( strlen(NEW_QT_PLUGIN_PATH) + strlen(env) );
    sprintf( new_env[4], NEW_QT_PLUGIN_PATH, env );
    // printf( "  using %s\n", new_env[4] );
    putenv( new_env[4] );

    env = getenv("PERLLIB") ?: "";
    new_env[5] = malloc( strlen(NEW_PERLLIB) + strlen(env) );
    sprintf( new_env[5], NEW_PERLLIB, env );
    // printf( "  using %s\n", new_env[5] );
    putenv( new_env[5] );
    
    env = getenv("GSETTINGS_SCHEMA_DIR") ?: "";
    new_env[6] = malloc( strlen(NEW_GSETTINGS_SCHEMA_DIR) + strlen(env) );
    sprintf( new_env[6], NEW_GSETTINGS_SCHEMA_DIR, env );
    // printf( "  using %s\n", new_env[6] );
    putenv( new_env[6] );    

    /* Run */
    ret = execvp(executable, argv);

    if (ret == -1)
    {
        die( "Error executing '%s'; return code: %d\n", executable, ret );
    }

    free(new_env[6]);
    free(new_env[5]);
    free(new_env[4]);
    free(new_env[3]);
    free(new_env[2]);
    free(new_env[1]);
    free(new_env[0]);
    free(line);
    return 0;
}
