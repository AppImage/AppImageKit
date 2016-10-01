/*-
 * Based on
 * https://github.com/libarchive/libarchive/blob/master/examples/minitar/minitar.c
 * sudo apt-get install libz-dev libarchive-dev
 * gcc -DENABLE_BINRELOC binreloc.c appimagetool.c -larchive -o appimagetool -lpthread
 * Can be statically linked so that libarchive is not needed on the target system
 * gcc -DENABLE_BINRELOC binreloc.c appimagetool.c -Wl,-Bstatic -larchive -Wl,-Bdynamic -lz -lpthread -o appimagetool
 *
 * Issues:
 * Corrupts data when zisofs is turned on (see below)
 * Crashes when archiving large (QtCreator) archives - message once got message along the line of "can't add itself to the archive"
 * When extracting, we DO get hardlinks (TBD: check symlinks)
    Symlinks have a file type `AE_IFLNK` and require a target to be set with `archive_entry_set_symlink()`
    Hardlinks require a target to be set with `archive_entry_set_hardlink()`; if this is set, the regular filetype is ignored. If the entry describing a hardlink has a size, you must be prepared to write data to the linked files. If you don't want to overwrite the file, leave the size unset.
 * Look at tar/ (bsdtar) source example for this
 * When writing, permissions do not seem to be written correctly
 *
 * TODO:
 * Make it possible to dump the contents of one file to stdout
 *
 */


#include <sys/types.h>
#include <sys/stat.h>

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>

#include "binreloc.h"
#ifndef NULL
    #define NULL ((void *) 0)
#endif

#include <stdbool.h>

int PATH_MAX;

static void	create(const char *filename, const char **argv);
static void	errmsg(const char *);
static void	extract(const char *filename, int do_extract, int flags);
static int	copy_data(struct archive *, struct archive *);
static void	msg(const char *);
static void     usage(void);

static int verbose = 0;

size_t used;
size_t buffsize = 36 * 2048;

const char *argv0;

int main(int argc, const char **argv)
{
    argv0 = argv[0];
    const char *filename = NULL;
    int flags, mode, opt;

    (void)argc;
    mode = 'x';
    verbose = 0;
    flags = ARCHIVE_EXTRACT_TIME;

    BrInitError error;

    if (br_init (&error) == 0) {
        printf ("Warning: binreloc failed to initialize (error code %d)\n", error);
    }

    printf ("The location of this application is: %s\n", br_find_exe_dir(NULL));

    // TODO: Check whether the icon specified in Icon= exists
    // TODO: Check whether the executable specified in Exec= exists

    /* Among other sins, getopt(3) pulls in printf(3). */
    while (*++argv != NULL && **argv == '-') {
        const char *p = *argv + 1;

        while ((opt = *p++) != '\0') {
            switch (opt) {
            case 'c':
                mode = opt;
                break;
            case 'f':
                if (*p != '\0')
                    filename = p;
                else
                    filename = *++argv;
                p += strlen(p);
                break;
            case 'p':
                flags |= ARCHIVE_EXTRACT_PERM;
                flags |= ARCHIVE_EXTRACT_ACL;
                flags |= ARCHIVE_EXTRACT_FFLAGS;
                break;
            case 't':
                mode = opt;
                break;
            case 'v':
                verbose++;
                break;
            case 'x':
                mode = opt;
                break;
            default:
                usage();
            }
        }
    }

    switch (mode) {
    case 'c':
        create(filename, argv);
        break;
    case 't':
        extract(filename, 0, flags);
        break;
    case 'x':
        extract(filename, 1, flags);
        break;
    }

    return (0);
}


static char buff[16384];

static void
create(const char *filename, const char **argv)
{
    struct archive *a;
    struct archive *disk;
    struct archive_entry *entry;
    ssize_t len;
    int fd;

    int ret;

    char *runtimefile;
    runtimefile = br_strcat (br_find_exe_dir(NULL), "/runtime");
    if(!file_exists(runtimefile)) {
        printf("AppImage runtime file cannot be found, aborting\n");
        exit(1);
    } else {
        printf ("Using runtime: %s\n" ,runtimefile);
    }

    a = archive_write_new();

    archive_write_set_format_iso9660(a);
    // archive_write_set_options(a, "iso9660:zisofs"); // FIXME: Enabling this destroys data; XChat then segfaults
    archive_write_set_options(a, "rockridge");
    // archive_write_set_options(a, "joliet");

    archive_write_set_option(a, NULL, "pad", NULL); // Works
    archive_write_set_option(a, NULL, "volume-id", "APPIMAGE"); // Works
    // archive_write_set_option(a, NULL, "zisofs", true); // FIXME: Immediately segfaults
    // archive_write_set_option(a, NULL, "compression-level", 9); // FIXME: Immediately segfaults

    if (filename != NULL && strcmp(filename, "-") == 0)
        filename = NULL;
    archive_write_open_filename(a, filename);

    disk = archive_read_disk_new();
    archive_read_disk_set_standard_lookup(disk);

    while (*argv != NULL) {
        struct archive *disk = archive_read_disk_new();
        int r;

        r = archive_read_disk_open(disk, *argv);
        if (r != ARCHIVE_OK) {
            errmsg(archive_error_string(disk));
            errmsg("\n");
            exit(1);
        }

        entry = archive_entry_new();
        for (;;) {
            int needcr = 0;
            r = archive_read_next_header2(disk, entry);

            // Make everything owned by root
            archive_entry_set_uid(entry, 0);
            archive_entry_set_uname(entry, "root");
            archive_entry_set_gid(entry, 0);
            archive_entry_set_gname(entry, "root");

            if (r == ARCHIVE_EOF)
                break;
            if (r != ARCHIVE_OK) {
                errmsg(archive_error_string(disk));
                errmsg("\n");
                exit(1);
            }
            archive_read_disk_descend(disk);
            if (verbose) {
                msg("a ");
                msg(archive_entry_pathname(entry));
                needcr = 1;
            }
            r = archive_write_header(a, entry);

            if (r < ARCHIVE_OK) {
                errmsg(": ");
                errmsg(archive_error_string(a));
                needcr = 1;
            }
            if (r == ARCHIVE_FATAL)
                exit(1);
            if (r > ARCHIVE_FAILED) {
                /* For now, we use a simpler loop to copy data
                 * into the target archive. */
                fd = open(archive_entry_sourcepath(entry), O_RDONLY);
                len = read(fd, buff, sizeof(buff));
                while (len > 0) {
                    archive_write_data(a, buff, len);
                    len = read(fd, buff, sizeof(buff));
                }
                close(fd);
            }
            archive_entry_clear(entry);
            if (needcr)
                msg("\n");
        }
        archive_read_close(disk);
        archive_read_free(disk);
        argv++;
    }
    archive_write_close(a);
    archive_write_free(a);

    // Embed AppImage runtime file into the header of the ISO9660 file
    // and then inject the AppImage magic bytes

    fprintf (stderr, "Embedding AppImage runtime...\n");

    FILE *source = fopen(runtimefile, "rb");
    if (source == NULL) {
       puts("Not able to open the file 'runtime' for reading, aborting\n");
       exit(1);
    }
    FILE *dest = fopen(filename, "r+");
    if (dest == NULL) {
       puts("Not able to open the destination file for writing, aborting\n");
       exit(1);
    }

    char byte;

    while (!feof(source))
    {
        fread(&byte, sizeof(char), 1, source);
        fwrite(&byte, sizeof(char), 1, dest);
    }

     // Write AppImage magic bytes
     fprintf (stderr, "Writing AppImage magic bytes...\n");

     fseek(dest, 8, SEEK_SET );
     putc(0x41, dest);
     putc(0x49, dest);
     putc(0x01, dest);

     fclose(source);
     fclose(dest);


     fprintf (stderr, "Marking the AppImage as executable...\n");
     if (chmod (filename, 0755) < 0) {
         printf("Could not set executable bit, aborting\n");
         exit(1);
     }

     fprintf (stderr, "Success\n");
}


static void
extract(const char *filename, int do_extract, int flags)
{
    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_read_support_format_iso9660(a);
    archive_write_disk_set_standard_lookup(ext);
    if (filename != NULL && strcmp(filename, "-") == 0)
        filename = NULL;
    if ((r = archive_read_open_filename(a, filename, 10240))) {
        errmsg(archive_error_string(a));
        errmsg("\n");
        exit(r);
    }
    for (;;) {
        int needcr = 0;
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK) {
            errmsg(archive_error_string(a));
            errmsg("\n");
            exit(1);
        }
        if (verbose && do_extract)
            msg("x ");
        if (verbose || !do_extract) {
            msg(archive_entry_pathname(entry));
            msg(" ");
            needcr = 1;
        }
        if (do_extract) {
            r = archive_write_header(ext, entry);
            if (r != ARCHIVE_OK) {
                errmsg(archive_error_string(a));
                needcr = 1;
            }
            else {
                r = copy_data(a, ext);
                if (r != ARCHIVE_OK)
                    needcr = 1;
            }
        }
        if (needcr)
            msg("\n");
    }
    archive_read_close(a);
    archive_read_free(a);
    exit(0);
}

static int
copy_data(struct archive *ar, struct archive *aw)
{
    int r;
    const void *buff;
    size_t size;
    int64_t offset;

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF)
            return (ARCHIVE_OK);
        if (r != ARCHIVE_OK) {
            errmsg(archive_error_string(ar));
            return (r);
        }
        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) {
            errmsg(archive_error_string(ar));
            return (r);
        }
    }
}

static void
msg(const char *m)
{
    write(1, m, strlen(m));
}

static void
errmsg(const char *m)
{
    if (m == NULL) {
        m = "Error: No error description provided.\n";
    }
    write(2, m, strlen(m));
}

static void
usage(void)
{
    fprintf (stderr, "Usage: appimagetool [-ctvx] [-f file] [file]\n");
    exit(1);
}

int file_exists(char *filename)
{
  struct stat   buffer;
  return (stat (filename, &buffer) == 0);
}
