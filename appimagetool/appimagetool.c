/*-
 * Based on
 * https://github.com/libarchive/libarchive/blob/master/examples/minitar/minitar.c
 * sudo apt-get install libz-dev libarchive-dev
 * gcc main.c -larchive -o appimagetool
 * Can be statically linked so that libarchive is not needed on the target system
 * gcc main.c -Wl,-Bstatic -larchive -Wl,-Bdynamic -lz -o appimagetool
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


static void	create(const char *filename, int compress, const char **argv);
static void	errmsg(const char *);
static void	extract(const char *filename, int do_extract, int flags);
static int	copy_data(struct archive *, struct archive *);
static void	msg(const char *);
static void	usage(void);

static int verbose = 0;

int
main(int argc, const char **argv)
{
    const char *filename = NULL;
    int compress, flags, mode, opt;

    (void)argc;
    mode = 'x';
    verbose = 0;
    compress = '\0';
    flags = ARCHIVE_EXTRACT_TIME;

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
        create(filename, compress, argv);
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
create(const char *filename, int compress, const char **argv)
{
    struct archive *a;
    struct archive *disk;
    struct archive_entry *entry;
    ssize_t len;
    int fd;

    a = archive_write_new();
    switch (compress) {
    default:
        archive_write_add_filter_none(a);
        break;
    }
    archive_write_set_format_iso9660(a);
    archive_write_set_options(a, "rockridge");
    archive_write_set_options(a, "iso9660:zisofs");
    archive_write_set_options(a, "compression-level:9");
    archive_write_set_options(a, "joliet");

    if (filename != NULL && strcmp(filename, "-") == 0)
        filename = NULL;
    archive_write_open_filename(a, filename);

    disk = archive_read_disk_new();
#ifndef NO_LOOKUP
    archive_read_disk_set_standard_lookup(disk);
#endif
    while (*argv != NULL) {
        struct archive *disk = archive_read_disk_new();
        int r;

        r = archive_read_disk_open(disk, *argv);
        if (r != ARCHIVE_OK) {
            errmsg(archive_error_string(disk));
            errmsg("\n");
            exit(1);
        }

        for (;;) {
            int needcr = 0;

            entry = archive_entry_new();
            r = archive_read_next_header2(disk, entry);
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
#if 0
                /* Ideally, we would be able to use
                 * the same code to copy a body from
                 * an archive_read_disk to an
                 * archive_write that we use for
                 * copying data from an archive_read
                 * to an archive_write_disk.
                 * Unfortunately, this doesn't quite
                 * work yet. */
                copy_data(disk, a);
#else
                /* For now, we use a simpler loop to copy data
                 * into the target archive. */
                fd = open(archive_entry_sourcepath(entry), O_RDONLY);
                len = read(fd, buff, sizeof(buff));
                while (len > 0) {
                    archive_write_data(a, buff, len);
                    len = read(fd, buff, sizeof(buff));
                }
                close(fd);
#endif
            }
            archive_entry_free(entry);
            if (needcr)
                msg("\n");
        }
        archive_read_close(disk);
        archive_read_free(disk);
        argv++;
    }
    archive_write_close(a);
    archive_write_free(a);

    // Write AppImage magic bytes
    FILE *fp = fopen(filename, "r+");
    fseek(fp, 8, SEEK_SET );
    putc(0x41, fp);
    putc(0x49, fp);
    putc(0x01, fp);
    fclose(fp);

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

#ifndef NO_LOOKUP
    archive_write_disk_set_standard_lookup(ext);
#endif
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
/* Many program options depend on compile options. */
    const char *m = "Usage: minitar [-"
        "c"
        "tvx"
        "] [-f file] [file]\n";
    errmsg(m);
    exit(1);
}
