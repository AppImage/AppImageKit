#include <stdio.h>
#include <argp.h>

#include <stdlib.h>
#include <fcntl.h>
#include "squashfuse.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "binreloc.h"
#ifndef NULL
    #define NULL ((void *) 0)
#endif

#include <libgen.h>

#include <unistd.h>

#include <stdio.h>

extern char runtime[];
extern unsigned int runtime_len;


const char *argp_program_version =
  "appimagetool 0.1";
  
const char *argp_program_bug_address =
  "<probono@puredarwin.org>";
  
static char doc[] =
  "appimagetool -- Generate, extract, and inspect AppImages";

/* This structure is used by main to communicate with parse_opt. */
struct arguments
{
  char *args[2];            /* SOURCE and DESTINATION */
  int verbose;              /* The -v flag */
  int list;                 /* The -l flag */
  char *dumpfile;            /* Argument for -d */
};


static struct argp_option options[] =
{
  {"verbose", 'v', 0, 0, "Produce verbose output"},
  {"list",   'l', 0, 0,
   "List files in SOURCE AppImage"},
  {"dump", 'd', "FILE", 0,
   "Dump FILE from SOURCE AppImage to stdout"},
  {0}
};


static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'v':
      arguments->verbose = 1;
      break;
    case 'l':
      arguments->list = 1;
      break;
    case 'd':
      arguments->dumpfile = arg;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= 3)
	{
	  argp_usage(state);
	}
      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 1)
	{
	  argp_usage (state);
	}
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static char args_doc[] = "SOURCE {DESTINATION}";


static struct argp argp = {options, parse_opt, args_doc, doc};


// #####################################################################


static void die(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

int is_directory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/* Function that prints the contents of a squashfs file
 * using libsquashfuse (#include "squashfuse.h") */
int sfs_ls(char* image) {
	sqfs_err err = SQFS_OK;
	sqfs_traverse trv;
	sqfs fs;

	if ((err = sqfs_open_image(&fs, image, 0)))
		die("sqfs_open_image error");
	
	if ((err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs))))
		die("sqfs_traverse_open error");
	while (sqfs_traverse_next(&trv, &err)) {
		if (!trv.dir_end) {
			printf("%s\n", trv.path);
		}
	}
	if (err)
		die("sqfs_traverse_next error");
	sqfs_traverse_close(&trv);
	
	sqfs_fd_close(fs.fd);
	return 0;
}

/* Generate a squashfs filesystem using mksquashfs on the $PATH  */
int sfs_mksquashfs(char *source, char *destination) {
    pid_t parent = getpid();
    pid_t pid = fork();

    if (pid == -1) {
        // error, failed to fork()
        return(-1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        // we are the child
        execlp("mksquashfs", "mksquashfs", source, destination, "-root-owned", "-noappend", (char *)0);
        perror("execlp");   // execvp() returns only on error
        return(-1); // exec never returns
    }
    return(0);
}

/* Generate a squashfs filesystem
 * The following would work if we link to mksquashfs.o after we renamed 
 * main() to mksquashfs_main() in mksquashfs.c but we don't want to actually do
 * this because squashfs-tools is not under a permissive license
int sfs_mksquashfs(char *source, char *destination) {
    char *child_argv[5];
    child_argv[0] = NULL;
    child_argv[1] = source;
    child_argv[2] = destination;
    child_argv[3] = "-root-owned";
    child_argv[4] = "-noappend";
    mksquashfs_main(5, child_argv);
}
 */

// #####################################################################

int main (int argc, char **argv)
{

  /* Initialize binreloc so that we always know where we live */
  BrInitError error;
  if (br_init (&error) == 0) {
    printf ("Warning: binreloc failed to initialize (error code %d)\n", error);
  }
  printf ("This tool is located at %s\n", br_find_exe_dir(NULL));    
    
  struct arguments arguments;

  /* Set argument defaults */
  arguments.list = 0;
  arguments.verbose = 0;
  arguments.dumpfile = NULL;
  
  /* Where the magic happens */
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  /* If in list mode */
  if (arguments.list){
    sfs_ls(arguments.args[0]);
    exit(0);
  }
    
  /* If in dumpfile mode */
  if (arguments.dumpfile){
    fprintf (stdout, "%s from the AppImage %s should be dumped to stdout\n", arguments.dumpfile, arguments.args[0]);
    die("To be implemented");
  }
  
  /* Print argument values */
  if (arguments.verbose)
      fprintf (stdout, "Original SOURCE = %s\nOriginal DESTINATION = %s\n",
        arguments.args[0],
        arguments.args[1]);

  /* If the first argument is a directory, then we assume that we should package it */
  if(is_directory(arguments.args[0])){
      char *destination;
      char source[PATH_MAX];
      realpath(arguments.args[0], source);
      if (arguments.args[1]) {
          destination = arguments.args[1];
      } else {
          /* No destination has been specified, to let's construct one
           * TODO: Find out the architecture and use a $VERSION that might be around in the env */
          destination = basename(br_strcat(source, ".AppImage"));
          fprintf (stdout, "DESTINATION not specified, so assuming %s\n", destination);
      }
      fprintf (stdout, "%s should be packaged as %s\n", arguments.args[0], destination);

      /* mksquashfs can currently not start writing at an offset,
       * so we need a tempfile. https://github.com/plougher/squashfs-tools/pull/13
       * should hopefully change that. */
      char *tempfile;
      fprintf (stderr, "Generating squashfs...\n");
      tempfile = br_strcat(destination, ".temp");
      int result = sfs_mksquashfs(source, tempfile);
      if(result != 0)
          die("sfs_mksquashfs error");

      fprintf (stderr, "Generating AppImage...\n");
      FILE *fpsrc = fopen(tempfile, "rb");
      if (fpsrc == NULL) {
         die("Not able to open the tempfile for reading, aborting");
      }
      FILE *fpdst = fopen(destination, "w");
      if (fpdst == NULL) {
         die("Not able to open the destination file for writing, aborting");
      }

      /* runtime is embedded into this executable
       * http://stupefydeveloper.blogspot.de/2008/08/cc-embed-binary-data-into-elf.html */
      int size = runtime_len;
      char *data = runtime;
      if (arguments.verbose)
          printf("Size of the embedded runtime: %d bytes\n", size);
      /* Where to store updateinformation. Fixed offset preferred for easy manipulation 
       * after the fact. Proposal: 4 KB at the end of the 128 KB padding. 
       * Hence, offset 126976, max. 4096 bytes long. 
       * Possibly we might want to store additional information in the future.
       * Assuming 4 blocks of 4096 bytes each.
       */
      if(size > 128*1024-4*4096-2){
          die("Size of the embedded runtime is too large, aborting");
      }
      // printf("%s", data);
      fwrite(data, size, 1, fpdst);

      if(ftruncate(fileno(fpdst), 128*1024) != 0) {
          die("Not able to write padding to destination file, aborting");
      }
      
      fseek (fpdst, 0, SEEK_END);
      char byte;

      while (!feof(fpsrc))
      {
          fread(&byte, sizeof(char), 1, fpsrc);
          fwrite(&byte, sizeof(char), 1, fpdst);
      }
      
      fclose(fpsrc);
      fclose(fpdst);

      fprintf (stderr, "Marking the AppImage as executable...\n");
      if (chmod (destination, 0755) < 0) {
          printf("Could not set executable bit, aborting\n");
          exit(1);
      }
      if(unlink(tempfile) != 0) {
          die("Could not delete the tempfile, aborting");
      }
      fprintf (stderr, "Success\n");
}

  /* If the first argument is a regular file, then we assume that we should unpack it */
  if(is_regular_file(arguments.args[0])){
      fprintf (stdout, "%s is a file, assuming it is an AppImage and should be unpacked\n", arguments.args[0]);
      die("To be implemented");
  }

  return 0;
}
