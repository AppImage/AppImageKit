/**************************************************************************
 * 
 * Copyright (c) 2004-18 Simon Peter
 * Portions Copyright (c) 2007 Alexander Larsson
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

#define _GNU_SOURCE

#include "squashfuse.h"
#include <squashfs_fs.h>
#include <nonstd.h>

#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <wait.h>

#include "elf.h"
#include "getsection.h"

#ifndef ENABLE_DLOPEN
#define ENABLE_DLOPEN
#endif
#include "squashfuse_dlopen.h"

#include <fnmatch.h>

//#include "notify.c"
extern int notify(char *title, char *body, int timeout);

struct stat st;

static long unsigned int fs_offset; // The offset at which a filesystem image is expected = end of this ELF

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/* Check whether directory is writable */
bool is_writable_directory(char* str) {
    if(access(str, W_OK) == 0) {
        return true;
    } else {
        return false;   
    }
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
    lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

/* Fill in a stat structure. Does not set st_ino */
sqfs_err private_sqfs_stat(sqfs *fs, sqfs_inode *inode, struct stat *st) {
	sqfs_err err = SQFS_OK;
	uid_t id;
	
	memset(st, 0, sizeof(*st));
	st->st_mode = inode->base.mode;
	st->st_nlink = inode->nlink;
	st->st_mtime = st->st_ctime = st->st_atime = inode->base.mtime;
	
	if (S_ISREG(st->st_mode)) {
		/* FIXME: do symlinks, dirs, etc have a size? */
		st->st_size = inode->xtra.reg.file_size;
		st->st_blocks = st->st_size / 512;
	} else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
		st->st_rdev = sqfs_makedev(inode->xtra.dev.major,
			inode->xtra.dev.minor);
	} else if (S_ISLNK(st->st_mode)) {
		st->st_size = inode->xtra.symlink_size;
	}
	
	st->st_blksize = fs->sb.block_size; /* seriously? */
	
	err = sqfs_id_get(fs, inode->base.uid, &id);
	if (err)
		return err;
	st->st_uid = id;
	err = sqfs_id_get(fs, inode->base.guid, &id);
	st->st_gid = id;
	if (err)
		return err;
	
	return SQFS_OK;
}

/* ================= End ELF parsing */

extern int fusefs_main(int argc, char *argv[], void (*mounted) (void));
// extern void ext2_quit(void);

static pid_t fuse_pid;
static int keepalive_pipe[2];

static void *
write_pipe_thread (void *arg)
{
    char c[32];
    int res;
    //  sprintf(stderr, "Called write_pipe_thread");
    memset (c, 'x', sizeof (c));
    while (1) {
        /* Write until we block, on broken pipe, exit */
        res = write (keepalive_pipe[1], c, sizeof (c));
        if (res == -1) {
            kill (fuse_pid, SIGHUP);
            break;
        }
    }
    return NULL;
}

void
fuse_mounted (void)
{
    pthread_t thread;
    fuse_pid = getpid();
    pthread_create(&thread, NULL, write_pipe_thread, keepalive_pipe);
}

char* getArg(int argc, char *argv[],char chr)
{
    int i;
    for (i=1; i<argc; ++i)
        if ((argv[i][0]=='-') && (argv[i][1]==chr))
            return &(argv[i][2]);
        return NULL;
}

/* mkdir -p implemented in C, needed for https://github.com/AppImage/AppImageKit/issues/333
 * https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950 */
int
mkdir_p(const char *path)
{
    /* Adapted from http://stackoverflow.com/a/2336245/119527 */
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p; 

    errno = 0;

    /* Copy string so its mutable */
    if (len > sizeof(_path)-1) {
        errno = ENAMETOOLONG;
        return -1; 
    }   
    strcpy(_path, path);

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';

            if (mkdir(_path, S_IRWXU) != 0) {
                if (errno != EEXIST)
                    return -1; 
            }

            *p = '/';
        }
    }   

    if (mkdir(_path, S_IRWXU) != 0) {
        if (errno != EEXIST)
            return -1; 
    }   

    return 0;
}

void
print_help(const char *appimage_path)
{
    // TODO: "--appimage-list                 List content from embedded filesystem image\n"
    printf(
        "AppImage options:\n\n"
        "  --appimage-extract              Extract content from embedded filesystem image\n"
        "  --appimage-help                 Print this help\n"
        "  --appimage-mount                Mount embedded filesystem image and print\n"
        "                                  mount point and wait for kill with Ctrl-C\n"
        "  --appimage-offset               Print byte offset to start of embedded\n"
        "                                  filesystem image\n"
        "  --appimage-portable-home        Create a portable home folder to use as $HOME\n"
        "  --appimage-portable-config      Create a portable config folder to use as\n"
        "                                  $XDG_CONFIG_HOME\n"
        "  --appimage-signature            Print digital signature embedded in AppImage\n"
        "  --appimage-updateinfo[rmation]  Print update info embedded in AppImage\n"
        "  --appimage-version              Print version of AppImageKit\n"
        "\n"
        "Portable home:\n"
        "\n"
        "  If you would like the application contained inside this AppImage to store its\n"
        "  data alongside this AppImage rather than in your home directory, then you can\n"
        "  place a directory named\n"
        "\n"
        "  %s.home\n"
        "\n"
        "  Or you can invoke this AppImage with the --appimage-portable-home option,\n"
        "  which will create this directory for you. As long as the directory exists\n"
        "  and is neither moved nor renamed, the application contained inside this\n"
        "  AppImage to store its data in this directory rather than in your home\n"
        "  directory\n"
    , appimage_path);
}

void
portable_option(const char *arg, const char *appimage_path, const char *name)
{
    char option[32];
    sprintf(option, "appimage-portable-%s", name);

    if (arg && strcmp(arg, option)==0) {
        char portable_dir[PATH_MAX];
        char fullpath[PATH_MAX];

        ssize_t length = readlink(appimage_path, fullpath, sizeof(fullpath));
        if (length < 0) {
            printf("Error getting realpath for %s\n", appimage_path);
            exit(EXIT_FAILURE);
        }
        fullpath[length] = '\0';

        sprintf(portable_dir, "%s.%s", fullpath, name);
        if (!mkdir(portable_dir, S_IRWXU))
            printf("Portable %s directory created at %s\n", name, portable_dir);
        else
            printf("Error creating portable %s directory at %s: %s\n", name, portable_dir, strerror(errno));

        exit(0);
    }
}

int
main (int argc, char *argv[])
{
    char appimage_path[PATH_MAX];
    char argv0_path[PATH_MAX];
    char * arg;
    
    /* We might want to operate on a target appimage rather than this file itself,
     * e.g., for appimaged which must not run untrusted code from random AppImages.
     * This variable is intended for use by e.g., appimaged and is subject to 
     * change any time. Do not rely on it being present. We might even limit this
     * functionality specifically for builds used by appimaged.
     */
    if(getenv("TARGET_APPIMAGE") == NULL){
        sprintf(appimage_path, "/proc/self/exe");
    } else {
        sprintf(appimage_path, "%s", getenv("TARGET_APPIMAGE"));
        fprintf(stderr, "Using TARGET_APPIMAGE %s\n", appimage_path);
    }
	
    sprintf(argv0_path, argv[0]);

    fs_offset = get_elf_size(appimage_path);
    
    arg=getArg(argc,argv,'-');

    /* Print the help and then exit */
    if(arg && strcmp(arg,"appimage-help")==0) {
        char fullpath[PATH_MAX];

        ssize_t length = readlink(appimage_path, fullpath, sizeof(fullpath));
        if (length < 0) {
            printf("Error getting realpath for %s\n", appimage_path);
            exit(EXIT_FAILURE);
        }
        fullpath[length] = '\0';

        print_help(fullpath);
        exit(0);
    }

    /* Just print the offset and then exit */
    if(arg && strcmp(arg,"appimage-offset")==0) {
        printf("%lu\n", fs_offset);
        exit(0);
    }
    
    /* Exract the AppImage */
    arg=getArg(argc,argv,'-');
    if(arg && strcmp(arg,"appimage-extract")==0) {
        sqfs_err err = SQFS_OK;
        sqfs_traverse trv;
        sqfs fs;
        char *pattern;
        char *prefix;
        char prefixed_path_to_extract[1024];
        
        prefix = "squashfs-root/";
        
        if(access(prefix, F_OK ) == -1 ) {
            if (mkdir_p(prefix) == -1) {
                perror("mkdir_p error");
                exit(EXIT_FAILURE);
            }
        }
        
        if(argc == 3){
            pattern = argv[2];
	    if (pattern[0] == '/') pattern++; // Remove leading '/'
	}
        
        if ((err = sqfs_open_image(&fs, appimage_path, fs_offset)))
            exit(1);
        
        if ((err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs))))
            die("sqfs_traverse_open error");
        while (sqfs_traverse_next(&trv, &err)) {
            if (!trv.dir_end) {
                if((argc != 3) || (fnmatch (pattern, trv.path, FNM_FILE_NAME) == 0)){
                    // fprintf(stderr, "trv.path: %s\n", trv.path);
                    // fprintf(stderr, "sqfs_inode_id: %lu\n", trv.entry.inode);
                    sqfs_inode inode;
                    if (sqfs_inode_get(&fs, &inode, trv.entry.inode))
                        die("sqfs_inode_get error");
                    // fprintf(stderr, "inode.base.inode_type: %i\n", inode.base.inode_type);
                    // fprintf(stderr, "inode.xtra.reg.file_size: %lu\n", inode.xtra.reg.file_size);
                    strcpy(prefixed_path_to_extract, "");
                    strcat(strcat(prefixed_path_to_extract, prefix), trv.path);
		    fprintf(stderr, "%s\n", prefixed_path_to_extract);
                    if(inode.base.inode_type == SQUASHFS_DIR_TYPE || inode.base.inode_type == SQUASHFS_LDIR_TYPE){
                        // fprintf(stderr, "inode.xtra.dir.parent_inode: %ui\n", inode.xtra.dir.parent_inode);
                        // fprintf(stderr, "mkdir_p: %s/\n", prefixed_path_to_extract);
                        if(access(prefixed_path_to_extract, F_OK ) == -1 ) {
                            if (mkdir_p(prefixed_path_to_extract) == -1) {
                                perror("mkdir_p error");
                                exit(EXIT_FAILURE);
                            }
                        }
                    } else if(inode.base.inode_type == SQUASHFS_REG_TYPE || inode.base.inode_type == SQUASHFS_LREG_TYPE){
                        // fprintf(stderr, "Extract to: %s\n", prefixed_path_to_extract);
                        if(private_sqfs_stat(&fs, &inode, &st) != 0)
                            die("private_sqfs_stat error");
                        // Read the file in chunks
                        off_t bytes_already_read = 0;
                        sqfs_off_t bytes_at_a_time = 64*1024;
                        FILE * f;
                        f = fopen (prefixed_path_to_extract, "w+");
                        if (f == NULL)
                            die("fopen error");
                        while (bytes_already_read < inode.xtra.reg.file_size)
                        {
                            char buf[bytes_at_a_time];
                            if (sqfs_read_range(&fs, &inode, (sqfs_off_t) bytes_already_read, &bytes_at_a_time, buf))
                                die("sqfs_read_range error");
                            // fwrite(buf, 1, bytes_at_a_time, stdout);
                            fwrite(buf, 1, bytes_at_a_time, f);
                            bytes_already_read = bytes_already_read + bytes_at_a_time;
                        }
                        fclose(f);
                        chmod (prefixed_path_to_extract, st.st_mode);
                    } else if(inode.base.inode_type == SQUASHFS_SYMLINK_TYPE){
                        size_t size;
                        sqfs_readlink(&fs, &inode, NULL, &size);
                        char buf[size];
                        int ret = sqfs_readlink(&fs, &inode, buf, &size);
                        if (ret != 0)
                            die("symlink error");
                        // fprintf(stderr, "Symlink: %s to %s \n", prefixed_path_to_extract, buf);
                        unlink(prefixed_path_to_extract);
                        ret = symlink(buf, prefixed_path_to_extract);
                        if (ret != 0)
                            die("symlink error");
                    } else {
                        fprintf(stderr, "TODO: Implement inode.base.inode_type %i\n", inode.base.inode_type);
                    }
                    // fprintf(stderr, "\n");
                }
            }
        }
        if (err)
            die("sqfs_traverse_next error");
        sqfs_traverse_close(&trv);
        sqfs_fd_close(fs.fd);
        exit(0);
    }
    
    if(arg && strcmp(arg,"appimage-version")==0) {
        fprintf(stderr,"Version: %s\n", GIT_COMMIT);
        exit(0);
    }
    
    if(arg && (strcmp(arg,"appimage-updateinformation")==0 || strcmp(arg,"appimage-updateinfo")==0)) {
        unsigned long offset = 0;
        unsigned long length = 0;
        get_elf_section_offset_and_length(appimage_path, ".upd_info", &offset, &length);
        // printf("offset: %lu\n", offset);
        // printf("length: %lu\n", length);
        // print_hex(appimage_path, offset, length);
        print_binary(appimage_path, offset, length);
        exit(0);
    }
    
    if(arg && strcmp(arg,"appimage-signature")==0) {
        unsigned long offset = 0;
        unsigned long length = 0;
        get_elf_section_offset_and_length(appimage_path, ".sha256_sig", &offset, &length);
        // printf("offset: %lu\n", offset);
        // printf("length: %lu\n", length);
        // print_hex(appimage_path, offset, length);
        print_binary(appimage_path, offset, length);
        exit(0);
    }

    portable_option(arg, appimage_path, "home");
    portable_option(arg, appimage_path, "config");

    // If there is an argument starting with appimage- (but not appimage-mount which is handled further down)
    // then stop here and print an error message
    if((arg && strncmp(arg, "appimage-", 8) == 0) && (arg && strcmp(arg,"appimage-mount")!=0)) {
        fprintf(stderr,"--%s is not yet implemented in version %s\n", arg, GIT_COMMIT);
        exit(1);
    }

    LOAD_LIBRARY; /* exit if libfuse is missing */

    int dir_fd, res;

    char mount_dir[64];
    int namelen = strlen(basename(argv[0]));
    if(namelen>6){
        namelen=6;
    }
    strncpy(mount_dir, "/tmp/.mount_", 12);
    strncpy(mount_dir+12, basename(argv[0]), namelen);
    strncpy(mount_dir+12+namelen, "XXXXXX", 6);
    mount_dir[12+namelen+6] = 0; // null terminate destination
    
    char filename[100]; /* enough for mount_dir + "/AppRun" */
    pid_t pid;
    char **real_argv;
    int i;
    
    if (mkdtemp(mount_dir) == NULL) {
        exit (1);
    }
    
    if (pipe (keepalive_pipe) == -1) {
        perror ("pipe error");
        exit (1);
    }
    
    pid = fork ();
    if (pid == -1) {
        perror ("fork error");
        exit (1);
    }
    
    if (pid == 0) {
        /* in child */
        
        char *child_argv[5];
        
        /* close read pipe */
        close (keepalive_pipe[0]);
        
        char *dir = realpath(appimage_path, NULL );
        
        char options[100];
        sprintf(options, "ro,offset=%lu", fs_offset);
        
        child_argv[0] = dir;
        child_argv[1] = "-o";
        child_argv[2] = options;
        child_argv[3] = dir;
        child_argv[4] = mount_dir;
        
        if(0 != fusefs_main (5, child_argv, fuse_mounted)){
            char *title;
            char *body;
            title = "Cannot mount AppImage, please check your FUSE setup.";
            body = "You might still be able to extract the contents of this AppImage \n"
            "if you run it with the --appimage-extract option. \n"
            "See https://github.com/AppImage/AppImageKit/wiki/FUSE \n"
            "for more information";
	    notify(title, body, 0); // 3 seconds timeout
        };
    } else {
        /* in parent, child is $pid */
        int c;
        
        /* close write pipe */
        close (keepalive_pipe[1]);
        
        /* Pause until mounted */
        read (keepalive_pipe[0], &c, 1);
        
        
        dir_fd = open (mount_dir, O_RDONLY);
        if (dir_fd == -1) {
            perror ("open dir error");
            exit (1);
        }
        
        res = dup2 (dir_fd, 1023);
        if (res == -1) {
            perror ("dup2 error");
            exit (1);
        }
        close (dir_fd);
        
        strcpy (filename, mount_dir);
        strcat (filename, "/AppRun");
        
        real_argv = malloc (sizeof (char *) * (argc + 1));
        for (i = 0; i < argc; i++) {
            real_argv[i] = argv[i];
        }
        real_argv[i] = NULL;
        
        if(arg && strcmp(arg,"appimage-mount")==0) {
            printf("%s\n", mount_dir);
            for (;;) pause();
        }

        int length;
        char fullpath[PATH_MAX];
        
        if(getenv("TARGET_APPIMAGE") == NULL){
            // If we are operating on this file itself
            length = readlink(appimage_path, fullpath, sizeof(fullpath));
            fullpath[length] = '\0'; 
        } else {
            // If we are operating on a different AppImage than this file
            sprintf(fullpath, "%s", appimage_path); // TODO: Make absolute
        }
                
        /* Setting some environment variables that the app "inside" might use */
        setenv( "APPIMAGE", fullpath, 1 );
	setenv( "ARGV0", argv0_path, 1 );
        setenv( "APPDIR", mount_dir, 1 );

        char portable_home_dir[PATH_MAX];
        char portable_config_dir[PATH_MAX];
        
        /* If there is a directory with the same name as the AppImage plus ".home", then export $HOME */
        strcpy (portable_home_dir, fullpath);
        strcat (portable_home_dir, ".home");        
        if(is_writable_directory(portable_home_dir)){
            printf("Setting $HOME to %s\n", portable_home_dir);
            setenv("HOME",portable_home_dir,1); 
        }

        /* If there is a directory with the same name as the AppImage plus ".config", then export $XDG_CONFIG_HOME */
        strcpy (portable_config_dir, fullpath);
        strcat (portable_config_dir, ".config");        
        if(is_writable_directory(portable_config_dir)){
            printf("Setting $XDG_CONFIG_HOME to %s\n", portable_config_dir);
            setenv("XDG_CONFIG_HOME",portable_config_dir,1); 
        }
        
        /* Original working directory */
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            setenv( "OWD", cwd, 1 );
        }
        
        /* If we are operating on an AppImage different from this file,
         * then we do not execute the payload */
        if(getenv("TARGET_APPIMAGE") == NULL){
            /* TODO: Find a way to get the exit status and/or output of this */
            execv (filename, real_argv);
            /* Error if we continue here */
            perror ("execv error");
            exit (1);
        }
    }
    
    return 0;
}
