#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "squashfuse.h"
#include <squashfs_fs.h>

#include "elf.h"
#include "getsection.h"

#include <regex.h>

#define FNM_FILE_NAME 2

#define URI_MAX (FILE_MAX * 3 + 8)

/* Return the path of the thumbnail regardless whether it already exists; may be useful because
* G*_FILE_ATTRIBUTE_THUMBNAIL_PATH only exists if the thumbnail is already there.
* Since the md5 actually incorporates the ".png" ending so we might not want to use
* this as a general identifier for the AppImage.
* Check libgnomeui/gnome-thumbnail.h for actually generating thumbnails in the correct
* sizes at the correct locations automatically; which would draw in a dependency on gdk-pixbuf.
*/
char * get_thumbnail_path(char *path, char *thumbnail_size, gboolean verbose)
{
    gchar *uri = g_filename_to_uri (path, NULL, NULL);
    char *file;
    GChecksum *checksum;
    checksum = g_checksum_new (G_CHECKSUM_MD5);
    guint8 digest[16];
    gsize digest_len = sizeof (digest);
    g_checksum_update (checksum, (const guchar *) uri, strlen (uri));
    g_checksum_get_digest (checksum, digest, &digest_len);
    g_assert (digest_len == 16);
    file = g_strconcat (g_checksum_get_string(checksum), ".png", NULL);
    gchar *thumbnail_path = g_build_filename (g_get_user_cache_dir(), "thumbnails", thumbnail_size, file, NULL);
    g_free (file);
    return thumbnail_path;
}

/* Check if a file is an AppImage. Returns the image type if it is, or -1 if it isn't */
int check_appimage_type(char *path, gboolean verbose)
{
    FILE *f;
    char buffer[4];
    if (f = fopen(path, "rt"))
    {
        fseek(f, 8, SEEK_SET);
        fread(buffer, 1, 3, f);
        buffer[4] = 0;
        fclose(f);
        if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x01)){
            if(verbose)
                fprintf(stderr, "AppImage type 1\n");
            return 1;
        } else if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x02)){
            if(verbose)
                fprintf(stderr, "AppImage type 2\n");
            return 2;
        } else {
            return -1;
        }
    }
}

/* Register a type 1 AppImage in the system */
bool appimage_type1_register_in_system(char *path, gboolean verbose)
{
    fprintf(stderr, "ISO9660 based type 1 AppImage, not yet implemented: %s\n", path);
}

/* Find files in the squashfs matching to the regex pattern. 
 * Returns a newly-allocated NULL-terminated array of strings.
 * Use g_strfreev() to free it. */
gchar **squash_get_matching_files(sqfs *fs, char *pattern, gboolean verbose) {
    GPtrArray *array = g_ptr_array_new();
    sqfs_err err = SQFS_OK;
    sqfs_traverse trv;
    if (err = sqfs_traverse_open(&trv, fs, sqfs_inode_root(fs)))
        fprintf(stderr, "sqfs_traverse_open error\n");
    while (sqfs_traverse_next(&trv, &err)) {
        if (!trv.dir_end) {
            int r;
            regex_t regex;
            regmatch_t match[2];
            regcomp(&regex, pattern, REG_ICASE | REG_EXTENDED);
            r = regexec(&regex, trv.path, 2, match, 0);
            if (r == 0){
                // We have a match
                if(verbose)
                    fprintf(stderr, "squash_get_matching_files found: %s\n", trv.path);
                g_ptr_array_add(array, g_strdup(trv.path));
            }
        }
    }
    g_ptr_array_add(array, NULL);
    if (err)
        fprintf(stderr, "sqfs_traverse_next error\n");
    sqfs_traverse_close(&trv);
    return (gchar **) g_ptr_array_free(array, FALSE);
}

/* Loads a desktop file from squashfs into an empty GKeyFile structure.
 * FIXME: Use sqfs_lookup_path() instead of g_key_file_load_from_squash()
 * should help for performance. Please submit a pull request if you can
 * get it to work.
 */
gboolean g_key_file_load_from_squash(sqfs *fs, char *path, GKeyFile *key_file_structure, gboolean verbose) {
    sqfs_traverse trv;
    sqfs_err err = SQFS_OK;
    gboolean success;
    if (err = sqfs_traverse_open(&trv, fs, sqfs_inode_root(fs)))
        fprintf(stderr, "sqfs_traverse_open error\n");
    while (sqfs_traverse_next(&trv, &err)) {
        if (!trv.dir_end) {
            if (strcmp(path, trv.path) == 0){
                sqfs_inode inode;
                if (sqfs_inode_get(fs, &inode, trv.entry.inode))
                    fprintf(stderr, "sqfs_inode_get error\n");
                if (inode.base.inode_type == SQUASHFS_REG_TYPE){
                    off_t bytes_already_read = 0;
                    sqfs_off_t max_bytes_to_read = 256*1024;
                    char buf[max_bytes_to_read];
                    if (sqfs_read_range(fs, &inode, (sqfs_off_t) bytes_already_read, &max_bytes_to_read, buf))
                        fprintf(stderr, "sqfs_read_range error\n");
                    // fwrite(buf, 1, max_bytes_to_read, stdout);
                    success = g_key_file_load_from_data (key_file_structure, buf, max_bytes_to_read, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
                } else {
                    fprintf(stderr, "TODO: Implement inode.base.inode_type %i\n", inode.base.inode_type);
                }
            break;
            }
        }
    }
    
    if (err)
        fprintf(stderr, "sqfs_traverse_next error\n");
    sqfs_traverse_close(&trv);

    return success;
}

/* Write a modified desktop file to disk that points to the AppImage for execution */
void write_edited_desktop_file(GKeyFile *key_file_structure, char* path, gboolean verbose){
    if(verbose)
        fprintf(stderr, "%s", g_key_file_to_data(key_file_structure, NULL, NULL));
    if(!g_key_file_has_key(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL)){
        fprintf(stderr, "Desktop file has no Exec key\n");
        return;
    }
    g_key_file_set_value (key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, path);
    fprintf(stderr, "Installing desktop file\n");
    fprintf(stderr, "%s", g_key_file_to_data(key_file_structure, NULL, NULL));
}

/* Register a type 2 AppImage in the system */
bool appimage_type2_register_in_system(char *path, gboolean verbose)
{
    long unsigned int fs_offset; // The offset at which a squashfs image is expected
    fs_offset = get_elf_size(path);
    if(verbose)
        fprintf(stderr, "fs_offset: %lu\n", fs_offset);
    sqfs_err err = SQFS_OK;
    sqfs_traverse trv;
    sqfs fs;
    if (err = sqfs_open_image(&fs, path, fs_offset)){
        fprintf(stderr, "sqfs_open_image error: %s\n", path);
    } else {
        if(verbose)
            fprintf(stderr, "sqfs_open_image: %s\n", path);
    }
    
    /* Get desktop file(s) in the root directory of the AppImage */
    gchar **str_array = squash_get_matching_files(&fs, "(^[^/]*?.desktop$)", verbose); // Only in root dir
    // gchar **str_array = squash_get_matching_files(&fs, "(^.*?.desktop$)", verbose); // Not only there
    /* Work trough the NULL-terminated array of strings */
    for (int i=0; str_array[i]; ++i) {
        const char *ch = str_array[i]; 
        fprintf(stderr, "Got root desktop: %s\n", str_array[i]);
        GKeyFile *key_file_structure = g_key_file_new();
        gboolean success = g_key_file_load_from_squash(&fs, str_array[i], key_file_structure, verbose);
        if(success)
            write_edited_desktop_file(key_file_structure, path, verbose);
        g_key_file_free(key_file_structure);
        
    }
    /* Free the NULL-terminated array of strings and its contents */
    g_strfreev (str_array);

    /* Get icon file(s) */
    str_array = squash_get_matching_files(&fs, "(^usr/share/(icons|pixmaps)/.*.(png|svg|svgz|xpm)$)", verbose); 
    /* Work trough the NULL-terminated array of strings */
    for (int i=0; str_array[i]; ++i) {
        const char *ch = str_array[i]; 
        fprintf(stderr, "Got icon: %s\n", str_array[i]);
    }
    /* Free the NULL-terminated array of strings and its contents */
    g_strfreev (str_array);

    /* Get MIME xml file(s) */
    str_array = squash_get_matching_files(&fs, "(^usr/share/mime/.*.xml$)", verbose); 
    /* Work trough the NULL-terminated array of strings */
    for (int i=0; str_array[i]; ++i) {
        const char *ch = str_array[i]; 
        fprintf(stderr, "Got MIME: %s\n", str_array[i]);
    }
    /* Free the NULL-terminated array of strings and its contents */
    g_strfreev (str_array);
    
    /* Get AppStream metainfo file(s) 
     * TODO: Check if the id matches
     */
    str_array = squash_get_matching_files(&fs, "(^usr/share/appdata/.*metainfo.xml$)", verbose); 
    /* Work trough the NULL-terminated array of strings */
    for (int i=0; str_array[i]; ++i) {
        const char *ch = str_array[i]; 
        fprintf(stderr, "Got AppStream metainfo: %s\n", str_array[i]);
    }
    /* Free the NULL-terminated array of strings and its contents */
    g_strfreev (str_array);
    
    /* Close the squashfs filesystem only after we need no more data from it */
    sqfs_fd_close(fs.fd); 
}

/* Register an AppImage in the system */
int appimage_register_in_system(char *path, gboolean verbose)
{
    int type = check_appimage_type(path, verbose);
    if(type == 1 || type == 2){
        fprintf(stderr, "\n");
        fprintf(stderr, "-> REGISTER %s\n", path);
        fprintf(stderr, "get_thumbnail_path: %s\n", get_thumbnail_path(path, "normal", verbose));
    }
    /* TODO: Generate thumbnails.
    * Generating proper thumbnails involves more than just copying images out of the AppImage,
    * including checking if the thumbnail already exists and if it's valid 
    * and writing attributes into the thumbnail, see
    * https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#CREATION */
    
    if(type == 1){
        appimage_type1_register_in_system(path, verbose);
    }
    
    if(type == 2){
        appimage_type2_register_in_system(path, verbose);
    }
    
    return 0;
}

/* Delete the thumbnail for a given file and size if it exists */
void delete_thumbnail(char *path, char *size, gboolean verbose)
{
    gchar *thumbnail_path = get_thumbnail_path(path, size, verbose);
    if(verbose)
        fprintf(stderr, "get_thumbnail_path: %s\n", thumbnail_path);
    if(g_file_test(thumbnail_path, G_FILE_TEST_IS_REGULAR)){
        g_unlink(thumbnail_path);
        if(verbose)
            fprintf(stderr, "deleted: %s\n", thumbnail_path);
    }
}

/* Unregister an AppImage in the system */
int appimage_unregister_in_system(char *path, gboolean verbose)
{
    /* The file is already gone by now, so we can't determine its type anymore */
    fprintf(stderr, "\n");
    fprintf(stderr, "-> UNREGISTER %s\n", path);
    /* Could use gnome_desktop_thumbnail_factory_lookup instead of the next line */
    
    /* Delete the thumbnails if they exist */
    delete_thumbnail(path, "normal", verbose); // 128x128
    delete_thumbnail(path, "large", verbose); // 256x256
    return 0;
}
