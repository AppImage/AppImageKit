/**************************************************************************
 * 
 * Copyright (c) 2004-16 Simon Peter
 * Copyright (c) 2007 Alexander Larsson
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include <elf.h>
#include <byteswap.h>
#include <stdint.h>

long unsigned int fs_offset; // The offset at which a filesystem image is expected = end of this ELF

/* ================= Start ELF parsing */

/* Do not use fixed offset but determine it dynamically, based on the length of the ELF */

typedef Elf32_Nhdr Elf_Nhdr;

static char *fname;
static Elf64_Ehdr ehdr;
static Elf64_Phdr *phdr;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATANATIVE ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATANATIVE ELFDATA2MSB
#else
#error "Unknown machine endian"
#endif

static uint16_t file16_to_cpu(uint16_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_16(val);
	return val;
}

static uint32_t file32_to_cpu(uint32_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_32(val);
	return val;
}

static uint64_t file64_to_cpu(uint64_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_64(val);
	return val;
}

static long unsigned int read_elf32(int fd)
{
	Elf32_Ehdr ehdr32;
	ssize_t ret, i;

	ret = pread(fd, &ehdr32, sizeof(ehdr32), 0);
	if (ret < 0 || (size_t)ret != sizeof(ehdr)) {
		fprintf(stderr, "Read of ELF header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_shoff		= file32_to_cpu(ehdr32.e_shoff);
	ehdr.e_shentsize	= file16_to_cpu(ehdr32.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr32.e_shnum);

	return(ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum));
}

static long unsigned int read_elf64(int fd)
{
	Elf64_Ehdr ehdr64;
	ssize_t ret, i;

	ret = pread(fd, &ehdr64, sizeof(ehdr64), 0);
	if (ret < 0 || (size_t)ret != sizeof(ehdr)) {
		fprintf(stderr, "Read of ELF header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_shoff		= file64_to_cpu(ehdr64.e_shoff);
	ehdr.e_shentsize	= file16_to_cpu(ehdr64.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr64.e_shnum);

	return(ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum));
}

static long unsigned int get_elf_size(char *fname)
/* TODO, FIXME: This assumes that the section header table (SHT) is
the last part of the ELF. This is usually the case but
it could also be that the last section is the last part
of the ELF. This should be checked for.
*/
{
	ssize_t ret;
	int fd;
	static long unsigned int size = 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n",
			fname, strerror(errno));
		return(1);
	}
	ret = pread(fd, ehdr.e_ident, EI_NIDENT, 0);
	if (ret != EI_NIDENT) {
		fprintf(stderr, "Read of e_ident from %s failed: %s\n",
			fname, strerror(errno));
		return(1);
	}
	if ((ehdr.e_ident[EI_DATA] != ELFDATA2LSB) &&
	    (ehdr.e_ident[EI_DATA] != ELFDATA2MSB))
	{
		fprintf(stderr, "Unkown ELF data order %u\n",
			ehdr.e_ident[EI_DATA]);
		return(1);
	}
	if(ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
		size = read_elf32(fd);
	} else if(ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
		size = read_elf64(fd);
	} else {
		fprintf(stderr, "Unknown ELF class %u\n", ehdr.e_ident[EI_CLASS]);
		return(1);
	}

	close(fd);
	return size;
}
        
/* ================= End ELF parsing */

/* ======================================================== Start helper functions for icon extraction */  
/* 
 * Constructs the name of the thumbnail image for $HOME/.thumbnails for the executable that is itself
 * See http://people.freedesktop.org/~vuntz/thumbnail-spec-cache/
 * Partly borrowed from
 * http://www.google.com/codesearch#n76pnUnMG18/trunk/blender/imbuf/intern/thumbs.c&q=.thumbnails/normal%20lang:c%20md5&type=cs
 */

#include "md5.h"
#include "md5.c"
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#define FILE_MAX                                 240
#define URI_MAX FILE_MAX*3 + 8

/* --- begin of adapted code from glib ---
 * The following code is adapted from function g_escape_uri_string from the gnome glib
 * Source: http://svn.gnome.org/viewcvs/glib/trunk/glib/gconvert.c?view=markup
 * released under the Gnu General Public License.
 * NOTE THIS DOESN'T WORK PROPERLY FOR öäüß - FIXME
 */

typedef enum {
    UNSAFE_ALL        = 0x1,  /* Escape all unsafe characters   */
    UNSAFE_ALLOW_PLUS = 0x2,  /* Allows '+'  */
    UNSAFE_PATH       = 0x8,  /* Allows '/', '&', '=', ':', '@', '+', '$' and ',' */
    UNSAFE_HOST       = 0x10, /* Allows '/' and ':' and '@' */
    UNSAFE_SLASHES    = 0x20  /* Allows all characters except for '/' and '%' */
} UnsafeCharacterSet;

static const unsigned char acceptable[96] = {
    /* A table of the ASCII chars from space (32) to DEL (127) */
    0x00,0x3F,0x20,0x20,0x28,0x00,0x2C,0x3F,0x3F,0x3F,0x3F,0x2A,0x28,0x3F,0x3F,0x1C,
    0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x38,0x20,0x20,0x2C,0x20,0x20,
    0x38,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
    0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x20,0x3F,
    0x20,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
    0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x3F,0x20
};

static const char hex[17] = "0123456789abcdef";

void escape_uri_string (const char *string, char* escaped_string, int len,UnsafeCharacterSet mask)
{
    #define ACCEPTABLE(a) ((a)>=32 && (a)<128 && (acceptable[(a)-32] & use_mask))
    
    const char *p;
    char *q;
    int c;
    UnsafeCharacterSet use_mask;
    use_mask = mask;
    
    for (q = escaped_string, p = string; (*p != '\0') && len; p++) {
        c = (unsigned char) *p;
        len--;
        
        if (!ACCEPTABLE (c)) {
            *q++ = '%'; /* means hex coming */
            *q++ = hex[c >> 4];
            *q++ = hex[c & 15];
        } else {
            *q++ = *p;
        }
    }
    
    *q = '\0';
}

void to_hex_char(char* hexbytes, const unsigned char* bytes, int len)
{
    const unsigned char *p;
    char *q;
    
    for (q = hexbytes, p = bytes; len; p++) {
        const unsigned char c = (unsigned char) *p;
        len--;
        *q++ = hex[c >> 4];
        *q++ = hex[c & 15];
    }
}

/* --- end of adapted code from glib --- */

static int uri_from_filename( const char *dir, char *newuri )
{
    char uri[URI_MAX];
    sprintf (uri, "file://%s", dir);
    char newstring[URI_MAX];
    strncpy(newstring, uri, URI_MAX); 
    newstring[URI_MAX - 1] = 0; 
    unsigned int i = 0;
    escape_uri_string(newstring, newuri, FILE_MAX*3+8, UNSAFE_PATH);
    return 1;
}


static void thumbname_from_uri(const char* uri, char* thumb)
{
    char hexdigest[33];
    unsigned char digest[16];
    md5_buffer( uri, strlen(uri), digest);
    hexdigest[0] = '\0';
    to_hex_char(hexdigest, digest, 16);
    hexdigest[32] = '\0';
    sprintf(thumb, "%s.png", hexdigest);
    
}

/* ======================================================== End helper functions for icon extraction */  

extern int fusefs_main(int argc, char *argv[], void (*mounted) (void));
extern void ext2_quit(void);

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
    int res;
    
    fuse_pid = getpid();
    res = pthread_create(&thread, NULL, write_pipe_thread, keepalive_pipe);
}

char* getArg(int argc, char *argv[],char chr)
{
    int i;
    for (i=1; i<argc; ++i)
        if ((argv[i][0]=='-') && (argv[i][1]==chr))
            return &(argv[i][2]);
        return NULL;
}


int
main (int argc, char *argv[])
{
    fs_offset = get_elf_size("/proc/self/exe");
    
    char * arg;
    /* Just print the offset and then exit */
    arg=getArg(argc,argv,'-');
    if(arg && strcmp(arg,"appimage-offset")==0) {
        printf("%lu\n", fs_offset);
        exit(0);
    }

    int dir_fd, res;
    char mount_dir[] = "/tmp/.mount_XXXXXX";  /* create mountpoint */
    char filename[100]; /* enough for mount_dir + "/AppRun" */
    pid_t pid;
    char **real_argv;
    int i;
    
    // We are using glib anyway for fuseiso, so we can use it here too to make our life easier
    char *xdg_cache_home;
    char thumbnails_medium_dir[FILE_MAX];
    
    if(getenv("XDG_CACHE_HOME") == NULL){
        xdg_cache_home = "~/.cache/";
    } else {
        xdg_cache_home = getenv("XDG_CACHE_HOME");
    }
    
    sprintf(thumbnails_medium_dir, "%s/thumbnails/normal/", xdg_cache_home);
    /*  printf("%s\n", thumbnails_medium_dir); */
    
    if (mkdtemp(mount_dir) == NULL) {
        exit (1);
    }
    
    if (pipe (keepalive_pipe) == -1) {
        perror ("pipe error: ");
        exit (1);
    }
    
    pid = fork ();
    if (pid == -1) {
        perror ("fork error: ");
        exit (1);
    }
    
    if (pid == 0) {
        /* in child */
        
        char *child_argv[5];
        
        /* close read pipe */
        close (keepalive_pipe[0]);
        
        char *dir = realpath( "/proc/self/exe", NULL );
                
        char options[100];
        sprintf(options, "ro,offset=%lu", fs_offset);
        
        child_argv[0] = dir;
        child_argv[1] = "-o";
        child_argv[2] = options;
        child_argv[3] = dir;
        child_argv[4] = mount_dir;
        
        fusefs_main (5, child_argv, fuse_mounted);
    } else {
        /* in parent, child is $pid */
        int c;
        
        /* close write pipe */
        close (keepalive_pipe[1]);
        
        /* Pause until mounted */
        read (keepalive_pipe[0], &c, 1);
        
        
        dir_fd = open (mount_dir, O_RDONLY);
        if (dir_fd == -1) {
            // perror ("open dir error: ");
            printf("Could not mount AppImage\n");
            printf("Please see https://github.com/probonopd/AppImageKit/wiki/FUSE\n");
            exit (1);
        }
        
        res = dup2 (dir_fd, 1023);
        if (res == -1) {
            perror ("dup2 error: ");
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
 
        /* Extract all the data from inside the AppImage. 
         * TODO: Do this without FUSE. squashfuse should be able to
         * extract without the need for mounting it.
         * So that users without FUSE can still extract. */
        if(arg && strcmp(arg,"appimage-extract")==0) {
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
                execlp("cp", "cp", "-rfv", "--no-preserve=ownership,xattr",  mount_dir, "Extracted.AppDir", (char *)0);
                perror("execlp");   // execlp() returns only on error
                return(-1); // exec never returns
            }
            exit(0);
        }
       
        
        /* ======================================================== Start icon extraction */
        
        int length;
        char fullpath[FILE_MAX];
        length = readlink("/proc/self/exe", fullpath, sizeof(fullpath));
        fullpath[length] = '\0'; 
        /* printf("%s\n", fullpath); */
        char theuri[URI_MAX];
        uri_from_filename(fullpath, theuri);
        /* printf("%s\n", theuri);  */
        char path_to_thumbnail[URI_MAX];
        char thumbname[URI_MAX];
        thumbname_from_uri(theuri, thumbname);
        sprintf(path_to_thumbnail, "%s%s", thumbnails_medium_dir, thumbname);
        
        FILE *from, *to;
        char ch;
        
        char diricon[FILE_MAX];
        sprintf (diricon, "%s/.DirIcon", mount_dir);
        
        /* open source file */
        if((from = fopen(diricon, "rb"))==NULL) {
            printf("Cannot open %s\n", diricon);
            exit(1);
        }
        
        /* open destination file */
        char mkcmd[FILE_MAX];
        char iconsdir[FILE_MAX];
        
        sprintf(mkcmd, "mkdir -p '%s'", thumbnails_medium_dir);
        system(mkcmd);
        if((to = fopen(path_to_thumbnail, "wb"))==NULL) {
            printf("Cannot open %s for writing\n", path_to_thumbnail);
            
        } else {
            
            /* copy the file */
            while(!feof(from)) {
                ch = fgetc(from);
                if(ferror(from)) {
                    printf("Error reading source file\n");
                    exit(1);
                }
                if(!feof(from)) fputc(ch, to);
                if(ferror(to)) {
                    printf("Error writing destination file\n");
                    exit(1);
                }
            }
            
            if(fclose(from)==EOF) {
                printf("Error closing source file\n");
                exit(1);
            }
            
            if(fclose(to)==EOF) {
                printf("Error closing destination file\n");
                exit(1);
            }
        }
        
        /* If called with --icon, then do not run the main app, just print print a message and exit after extracting the icon */ 
        if (arg && strcmp(arg,"appimage-icon")==0) {
            printf("Written %s\n", path_to_thumbnail);
            exit(0);
        }
        
        /* ======================================================== End icon extraction */   
        

        /* Setting some environment variables that the app "inside" might use */
        setenv( "APPIMAGE", fullpath, 1 );
        setenv( "APPDIR", mount_dir, 1 );
        
        /* Original working directory */
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            setenv( "OWD", cwd, 1 );
        }
        
        execv (filename, real_argv);
        /* Error if we continue here */
        perror ("execv error: ");
        exit (1);
    }
    
    return 0;
}
