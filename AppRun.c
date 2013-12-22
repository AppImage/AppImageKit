/**************************************************************************

Version 20110903

Copyright (c) 2010 RazZziel
Copyright (c) 2005-11 Simon Peter

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

/* Debugging */
#define d(x) x

/*
 * We export the APP_IMAGE_ROOT environment and
 * let the author of the AppRunScript.sh deal with
 * setting up the environment based on this prefix.
 */
#define APP_IMAGE_ROOT_ENV  "APP_IMAGE_ROOT"

#define die(...)				    \
  do {						    \
    fprintf (stderr, "Error: " __VA_ARGS__ );	    \
    exit (1);					    \
  } while (0);


static int
find_script (const struct dirent *dir)
{
  if (strcmp (dir->d_name, "AppRunScript.sh") == 0)
    return 1;

  return 0;
}

static char *
setup_executable (const char *root_path, int argc, char **argv)
{
  struct dirent **namelist;
  int n_entries, i;
  char *executable;
  size_t arguments_size = 0;

  d (printf ("Scanning '%s' for startup script\n", root_path));

  n_entries = scandir (root_path, &namelist, find_script, NULL);

  if (n_entries < 0)
    {
      die ("Could not scan directory %s\n", root_path);
    }
  else if (n_entries == 0)
    {
      die ("No AppRunScript.sh\n");
    }

  /* Find out how much space we need for the arguments */
  for (i = 1; i < argc; i++)
    {
      /* reserve space for a leading ' ' for each argument */
      arguments_size += 1;
      arguments_size += strlen (argv[i]);
    }

  /* 2 extry bytes, one for '\0' and one for '/' */
  executable = malloc (strlen (root_path) + strlen (namelist[0]->d_name) + 2 + arguments_size);

  strcpy (executable, root_path);
  strcat (executable, "/");
  strcat (executable, namelist[0]->d_name);

  for (i = 1; i < argc; i++)
    {
      strcat (executable, " ");
      strcat (executable, argv[i]);
    }

  d (printf ("Setup runner script as '%s'\n", executable));

  return executable;
}

static char *
setup_image_root (const char *root_path)
{
  char *root_env;

  /* 2 extry bytes, one for '\0' and one for '=' */
  root_env = malloc (strlen (root_path) + strlen (APP_IMAGE_ROOT_ENV) + 2);
  strcpy (root_env, APP_IMAGE_ROOT_ENV);
  strcat (root_env, "=");
  strcat (root_env, root_path);

  d (printf ("Setup root environment as '%s'\n", root_env));

  return root_env;
}

static void
run_script (char *executable)
{
  char *arguments[4];

  /* Get a vector large enough to hold the arguments plus a null pointer */
  arguments[0] = "/bin/sh";
  arguments[1] = "-c";
  arguments[2] = executable;
  arguments[3] = NULL;

  /* Run the AppRunScript.sh */
  if (execvp (arguments[0], arguments) < 0)
    {
      die ("Error executing '%s'\n", executable);
    }
}

int
main (int argc, char *argv[])
{
  char *root_path;
  char *self_path;
  char *executable;
  char *root_env;

  self_path = realpath ("/proc/self/exe", NULL);
  if (!self_path)
    {
      die ("Could not access /proc/self/exe\n");
    }

  root_path = strdup (dirname (self_path));
  d (printf ("Root path resolved as '%s'\n", root_path));

  /* Resolve full path to the runner script and get a string for putenv() */
  executable = setup_executable (root_path, argc, argv);
  root_env   = setup_image_root (root_path);

  /* Move to the AppDir root */
  if (chdir (root_path) < 0)
    {
      die ("Could not cd into: %s\n", root_path);
    }

  /* Setup environment root */
  if (putenv (root_env) != 0)
    {
      die ("Failed to set environment '%s'\n", root_env);
    }

  /* Run the startup script */
  run_script (executable);

  free (root_path);
  free (root_env);
  free (executable);

  return 0;
}
