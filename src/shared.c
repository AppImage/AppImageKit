/**************************************************************************
 *
 * Copyright (c) 2004-18 Simon Peter
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "squashfuse.h"
#include <squashfs_fs.h>
#include "getsection.h"
#include "elf.h"

#include "xdg-basedir.h"

// own header
#include "shared.h"

#if HAVE_LIBARCHIVE3 == 1 // CentOS
# include <archive3.h>
# include <archive_entry3.h>
#else // other systems
# include <archive.h>
# include <archive_entry.h>
#endif

#include <regex.h>
#include <glob.h>

#include <cairo.h> // To get the size of icons, move_icon_to_destination()

#define FNM_FILE_NAME 2

#define URI_MAX (FILE_MAX * 3 + 8)

char *vendorprefix = "appimagekit";

void set_executable(const char *path, gboolean verbose)
{
    if(!g_find_program_in_path ("firejail")){
        int result = chmod(path, 0755); // TODO: Only do this if signature verification passed
        if(result != 0){
#ifdef STANDALONE
            fprintf(stderr, "Could not set %s executable: %s\n", path, strerror(errno));
#endif
        } else {
            if(verbose)
                fprintf(stderr, "Set %s executable\n", path);
        }
    }
}

/* Search and replace on a string, this really should be in Glib */
gchar* replace_str(const gchar const *src, const gchar const *find, const gchar const *replace){
    gchar **split = g_strsplit(src, find, -1);
    gchar *retval = g_strjoinv(replace, split);

    g_strfreev(split);
    return retval;
}

/* Return the md5 hash constructed according to
 * https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#THUMBSAVE
 * This can be used to identify files that are related to a given AppImage at a given location */
char *appimage_get_md5(const char* path)
{
    char* absolute_path;

    if ((absolute_path = realpath(path, NULL)) == NULL)
        absolute_path = strdup(path);

    gchar *uri = g_filename_to_uri(absolute_path, NULL, NULL);

    free(absolute_path);

    if (uri != NULL)
    {
        GChecksum *checksum;
        checksum = g_checksum_new(G_CHECKSUM_MD5);
        guint8 digest[16];
        gsize digest_len = sizeof (digest);
        g_checksum_update(checksum, (const guchar *) uri, strlen (uri));
        g_checksum_get_digest(checksum, digest, &digest_len);
        g_assert(digest_len == 16);
        gchar *result = g_strdup(g_checksum_get_string(checksum));
        g_checksum_free(checksum);
        g_free(uri);
        return result;
    } else {
        return "";
    }
}

/* Return the path of the thumbnail regardless whether it already exists; may be useful because
 * G*_FILE_ATTRIBUTE_THUMBNAIL_PATH only exists if the thumbnail is already there.
 * Check libgnomeui/gnome-thumbnail.h for actually generating thumbnails in the correct
 * sizes at the correct locations automatically; which would draw in a dependency on gdk-pixbuf.
 */
char *get_thumbnail_path(const char *path, char *thumbnail_size, gboolean verbose)
{
    char *md5 = appimage_get_md5(path);
    char *file  = g_strconcat(md5, ".png", NULL);
    char* cache_home = xdg_cache_home();
    gchar *thumbnail_path = g_build_filename(cache_home, "thumbnails", thumbnail_size, file, NULL);
    g_free(cache_home);
    g_free(file);
    g_free(md5);
    return thumbnail_path;
}

/* Move an icon file to the path where a given icon can be installed in $HOME.
 * This is needed because png and xpm icons cannot be installed in a generic
 * location but are only picked up in directories that have the size of
 * the icon as part of their directory name, as specified in the theme.index
 * See https://github.com/AppImage/AppImageKit/issues/258
 */

void move_icon_to_destination(gchar *icon_path, gboolean verbose)
{
    // FIXME: This default location is most likely wrong, but at least the icons with unknown size can go somewhere
    char* data_home = xdg_data_home();
    gchar *dest_dir = g_build_path("/", data_home, "/icons/hicolor/48x48/apps/", NULL);

    if((g_str_has_suffix (icon_path, ".svg")) || (g_str_has_suffix (icon_path, ".svgz"))) {
        g_free(dest_dir);
        dest_dir = g_build_path("/", data_home, "/icons/hicolor/scalable/apps/", NULL);
    }
    g_free(data_home);

    if((g_str_has_suffix (icon_path, ".png")) || (g_str_has_suffix (icon_path, ".xpm"))) {

        cairo_surface_t *image;

        int w = 0;
        int h = 0;

        if(g_str_has_suffix (icon_path, ".xpm")) {
            // TODO: GdkPixbuf has a convenient way to load XPM data. Then you can call
            // gdk_cairo_set_source_pixbuf() to transfer the data to a Cairo surface.
#ifdef STANDALONE
            fprintf(stderr, "XPM size parsing not yet implemented\n");
#endif
        }

        if(g_str_has_suffix (icon_path, ".png")) {
            image = cairo_image_surface_create_from_png(icon_path);
            w = cairo_image_surface_get_width (image);
            h = cairo_image_surface_get_height (image);
            cairo_surface_destroy (image);
        }

        // FIXME: The following sizes are taken from the hicolor icon theme.
        // Probably the right thing to do would be to figure out at runtime which icon sizes are allowable.
        // Or could we put our own index.theme into .local/share/icons/ and have it observed?
        if((w != h) || ((w != 16) && (w != 24) && (w != 32) && (w != 36) && (w != 48) && (w != 64) && (w != 72) && (w != 96) && (w != 128) && (w != 192) && (w != 256) && (w != 512))){
#ifdef STANDALONE
            fprintf(stderr, "%s has nonstandard size w = %i, h = %i; please fix it\n", icon_path, w, h);
#endif
        } else {
            g_free(dest_dir);
            char* data_home = xdg_data_home();
            dest_dir = g_build_path("/", data_home, "/icons/hicolor/", g_strdup_printf("%ix%i", w, h), "/apps/", NULL);
            free(data_home);
        }
    }
    if(verbose)
        fprintf(stderr, "dest_dir %s\n", dest_dir);

    gchar *basename = g_path_get_basename(icon_path);

    gchar* icon_dest_path = g_build_path("/", dest_dir, basename, NULL);

    g_free(basename);
    if(verbose)
        fprintf(stderr, "Move from %s to %s\n", icon_path, icon_dest_path);
    gchar *dirname = g_path_get_dirname(dest_dir);
    if(g_mkdir_with_parents(dirname, 0755)) {
#ifdef STANDALONE
        fprintf(stderr, "Could not create directory: %s\n", dest_dir);
#endif
    }

    g_free(dirname);
    g_free(dest_dir);

    // This is required only for old versions of glib2 and is harmless for newer
    g_type_init();

    GError *error = NULL;
    GFile *icon_file = g_file_new_for_path(icon_path);
    GFile *target_file = g_file_new_for_path(icon_dest_path);
    if(!g_file_move(icon_file, target_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)){
#ifdef STANDALONE
        fprintf(stderr, "Error moving file: %s\n", error->message);
#endif
        g_error_free(error);
    }
    g_object_unref(icon_file);
    g_object_unref(target_file);
    g_free(icon_dest_path);

}

/* Check if a file is an AppImage. Returns the image type if it is, or -1 if it isn't */
int appimage_get_type(const char* path, bool verbose)
{
    FILE *f = fopen(path, "rt");
    if (f != NULL)
    {
        char buffer[3] = {0};

        /* Check magic bytes at offset 8 */
        fseek(f, 8, SEEK_SET);
        fread(buffer, 1, 3, f);
        fclose(f);
        if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x01)){
#ifdef STANDALONE
            fprintf(stderr, "_________________________\n");
#endif
            if(verbose){
                fprintf(stderr, "AppImage type 1\n");
            }
            return 1;
        } else if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x02)){
#ifdef STANDALONE
            fprintf(stderr, "_________________________\n");
#endif
            if(verbose){
                fprintf(stderr, "AppImage type 2\n");
            }
            return 2;
        } else {
            if((g_str_has_suffix(path,".AppImage")) || (g_str_has_suffix(path,".appimage"))) {
#ifdef STANDALONE
                fprintf(stderr, "_________________________\n");
#endif
                if (verbose) {
                    fprintf(stderr, "Blindly assuming AppImage type 1\n");
                    fprintf(stderr, "The author of this AppImage should embed the magic bytes, see https://github.com/AppImage/AppImageSpec\n");
                }
                return 1;
            } else {
#ifdef STANDALONE
                fprintf(stderr, "_________________________\n");
#endif
                if(verbose){
                    fprintf(stderr, "Unrecognized file '%s'\n", path);
                }
                return -1;
            }
        }
    }
    return -1;
}

/* Get filename extension */
static gchar *get_file_extension(const gchar *filename)
{
    gchar **tokens;
    gchar *extension;
    tokens = g_strsplit(filename, ".", 2);
    if (tokens[0] == NULL)
        extension = NULL;
    else
        extension = g_strdup(tokens[1]);
    g_strfreev(tokens);
    return extension;
}

// Read the file in chunks
void squash_extract_inode_to_file(sqfs *fs, sqfs_inode *inode, const gchar *dest) {
    off_t bytes_already_read = 0;
    sqfs_off_t bytes_at_a_time = 64*1024;
    FILE * f;
    f = fopen (dest, "w+");
    if (f == NULL){
#ifdef STANDALONE
        fprintf(stderr, "fopen error\n");
#endif
        return;
    }
    while (bytes_already_read < (*inode).xtra.reg.file_size)
    {
        char buf[bytes_at_a_time];
        if (sqfs_read_range(fs, inode, (sqfs_off_t) bytes_already_read, &bytes_at_a_time, buf)) {
#ifdef STANDALONE
            fprintf(stderr, "sqfs_read_range error\n");
#endif
        }
        fwrite(buf, 1, (size_t) bytes_at_a_time, f);
        bytes_already_read = bytes_already_read + bytes_at_a_time;
    }
    fclose(f);
}

/* Find files in the squashfs matching to the regex pattern.
 * Returns a newly-allocated NULL-terminated array of strings.
 * Use g_strfreev() to free it.
 *
 * The following is done within the sqfs_traverse run for performance reaons:
 * 1.) For found files that are in usr/share/icons, install those icons into the system
 * with a custom name that involves the md5 identifier to tie them to a particular
 * AppImage.
 * 2.) For found files that are in usr/share/mime/packages, install those icons into the system
 * with a custom name that involves the md5 identifier to tie them to a particular
 * AppImage.
 */
gchar **squash_get_matching_files_install_icons_and_mime_data(sqfs* fs, char* pattern,
                                                              gchar* desktop_icon_value_original, char* md5,
                                                              gboolean verbose) {
    GPtrArray *array = g_ptr_array_new();
    sqfs_traverse trv;
    sqfs_err err = sqfs_traverse_open(&trv, fs, sqfs_inode_root(fs));
    if (err!= SQFS_OK) {
#ifdef STANDALONE
        fprintf(stderr, "sqfs_traverse_open error\n");
#endif
    }
    while (sqfs_traverse_next(&trv, &err)) {
        if (!trv.dir_end) {
            int r;
            regex_t regex;
            regmatch_t match[2];
            regcomp(&regex, pattern, REG_ICASE | REG_EXTENDED);
            r = regexec(&regex, trv.path, 2, match, 0);
            regfree(&regex);
            sqfs_inode inode;
            if(sqfs_inode_get(fs, &inode, trv.entry.inode)) {
#ifdef STANDALONE
                fprintf(stderr, "sqfs_inode_get error\n");
#endif
            }
            if(r == 0){
                // We have a match
                if(verbose)
                    fprintf(stderr, "squash_get_matching_files found: %s\n", trv.path);
                g_ptr_array_add(array, g_strdup(trv.path));
                gchar *dest = NULL;
                if(inode.base.inode_type == SQUASHFS_REG_TYPE) {
                    if(g_str_has_prefix(trv.path, "usr/share/icons/") || g_str_has_prefix(trv.path, "usr/share/pixmaps/") || (g_str_has_prefix(trv.path, "usr/share/mime/") && g_str_has_suffix(trv.path, ".xml"))){
                        char* data_home = xdg_data_home();
                        gchar *path = replace_str(trv.path, "usr/share", data_home);
                        free(data_home);
                        char *dest_dirname = g_path_get_dirname(path);
                        g_free(path);
                        gchar *base_name = g_path_get_basename(trv.path);
                        gchar *dest_basename = g_strdup_printf("%s_%s_%s", vendorprefix, md5, base_name);

                        dest = g_build_path("/", dest_dirname, dest_basename, NULL);

                        g_free(base_name);
                        g_free(dest_basename);
                    }
                    /* According to https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html#install_icons
                     * share/pixmaps is ONLY searched in /usr but not in $XDG_DATA_DIRS and hence $HOME and this seems to be true at least in XFCE */
                    if(g_str_has_prefix (trv.path, "usr/share/pixmaps/")){
                        gchar *dest_basename = g_strdup_printf("%s_%s_%s", vendorprefix, md5, g_path_get_basename(trv.path));

                        dest = g_build_path("/", "/tmp", dest_basename, NULL);

                        g_free(dest_basename);
                    }
                    /* Some AppImages only have the icon in their root directory, so we have to get it from there */
                    if((g_str_has_prefix(trv.path, desktop_icon_value_original)) && (! strstr(trv.path, "/")) && ( (g_str_has_suffix(trv.path, ".png")) || (g_str_has_suffix(trv.path, ".xpm")) || (g_str_has_suffix(trv.path, ".svg")) || (g_str_has_suffix(trv.path, ".svgz")))){
                        gchar *ext = get_file_extension(trv.path);
                        gchar *dest_basename = g_strdup_printf("%s_%s_%s.%s", vendorprefix, md5, desktop_icon_value_original, ext);

                        dest = g_build_path("/", "/tmp", dest_basename, NULL);

                        g_free(dest_basename);
                        g_free(ext);
                    }

                    if(dest){
                        if(verbose)
                            fprintf(stderr, "install: %s\n", dest);

                        gchar *dirname = g_path_get_dirname(dest);
                        if(g_mkdir_with_parents(dirname, 0755)) {
#ifdef STANDALONE
                            fprintf(stderr, "Could not create directory: %s\n", dirname);
#endif
                        }

                        g_free(dirname);

                        squash_extract_inode_to_file(fs, &inode, dest);

                        chmod (dest, 0644);

                        if(verbose)
                            fprintf(stderr, "Installed: %s\n", dest);

                        // If we were unsure about the size of an icon, we temporarily installed
                        // it to /tmp and now move it into the proper place
                        if(g_str_has_prefix (dest, "/tmp/")) {
                            move_icon_to_destination(dest, verbose);
                        }

                        g_free(dest);
                    }
                }
            }
        }
    }
    g_ptr_array_add(array, NULL);
    if (err) {
#ifdef STANDALONE
        fprintf(stderr, "sqfs_traverse_next error\n");
#endif
    }
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
    gboolean success = true;
    sqfs_err err = sqfs_traverse_open(&trv, fs, sqfs_inode_root(fs));
    if (err != SQFS_OK) {
        fprintf(stderr, "sqfs_traverse_open error\n");
        return false;
    }
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
#ifdef STANDALONE
                    fprintf(stderr, "TODO: Implement inode.base.inode_type %i\n", inode.base.inode_type);
#endif
                }
                break;
            }
        }
    }

#ifdef STANDALONE
    if (err)
        fprintf(stderr, "sqfs_traverse_next error\n");
#endif
    sqfs_traverse_close(&trv);

    return success;
}

gchar *build_installed_desktop_file_path(gchar* md5, gchar* desktop_filename) {
    gchar *partial_path;
    partial_path = g_strdup_printf("applications/appimagekit_%s-%s", md5, desktop_filename);

    char *data_home = xdg_data_home();
    gchar *destination = g_build_filename(data_home, partial_path, NULL);
    g_free(data_home);

    g_free(partial_path);

    return destination;
}

/* Write a modified desktop file to disk that points to the AppImage */
bool write_edited_desktop_file(GKeyFile *key_file_structure, const char* appimage_path, gchar* desktop_filename, int appimage_type, char *md5, gboolean verbose){
    if(!g_key_file_has_key(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL)){
#ifdef STANDALONE
        fprintf(stderr, "Desktop file has no Exec key\n");
#endif
        return false;
    }

    // parse [Try]Exec= value, replace executable by AppImage path, append parameters
    // TODO: should respect quoted strings within value
    const gchar const* fields[] = {G_KEY_FILE_DESKTOP_KEY_EXEC, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC};

    for (int i = 0; i < (sizeof(fields) / sizeof(gchar*)); i++) {
        char* field_value = g_key_file_get_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, fields[i], NULL);

        // TryExec is not a mandatory field
        if (field_value == NULL) {
            if (fields[i] != G_KEY_FILE_DESKTOP_KEY_EXEC)
                continue;

#ifdef STANDALONE
            fprintf(stderr, "%s entry missing in Desktop file\n", fields[i]);
#endif
            return false;
        };

        // saving a copy for later free() call
        char* original_field_value = field_value;

        char* executable = strsep(&field_value, " ");

        // error handling
        if (executable == NULL) {
#ifdef STANDALONE
            fprintf(stderr, "Invalid value for Exec= entry in Desktop file\n");
#endif
            return false;
        }

        unsigned long new_exec_value_size = strlen(appimage_path) + 1;

        if (field_value != NULL)
            new_exec_value_size += strlen(field_value) + 1;

        gchar* new_exec_value = calloc(new_exec_value_size, sizeof(gchar));

        // build new value
        strcpy(new_exec_value, appimage_path);

        if (field_value != NULL && strlen(field_value) > 0) {
            strcat(new_exec_value, " ");
            strcat(new_exec_value, field_value);
        }

        if (original_field_value != NULL)
            free(original_field_value);

        g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, fields[i], new_exec_value);

        g_free(new_exec_value);
    }

#ifdef APPIMAGED
    /* If firejail is on the $PATH, then use it to run AppImages */
    if(g_find_program_in_path ("firejail")){
        char *firejail_exec;
        firejail_exec = g_strdup_printf("firejail --env=DESKTOPINTEGRATION=appimaged --noprofile --appimage '%s'", appimage_path);
        g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, firejail_exec);

        gchar *firejail_profile_group = "Desktop Action FirejailProfile";
        gchar *firejail_profile_exec = g_strdup_printf("firejail --env=DESKTOPINTEGRATION=appimaged --private --appimage '%s'", appimage_path);
        gchar *firejail_tryexec = "firejail";
        g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_NAME, "Run without sandbox profile");
        g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_EXEC, firejail_profile_exec);
        g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, firejail_tryexec);
        g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, "Actions", "FirejailProfile;");
    }
#endif

#ifdef APPIMAGED
    /* Add AppImageUpdate desktop action
     * https://specifications.freedesktop.org/desktop-entry-spec/latest/ar01s10.html
     * This will only work if AppImageUpdate is on the user's $PATH.
     * TODO: we could have it call this appimaged instance instead of AppImageUpdate and let it
     * figure out how to update the AppImage */
    unsigned long upd_offset = 0;
    unsigned long upd_length = 0;
    if(g_find_program_in_path ("AppImageUpdate")){
        if(appimage_type == 2){
            get_elf_section_offset_and_length(appimage_path, ".upd_info", &upd_offset, &upd_length);
            if(upd_length != 1024) {
#ifdef STANDALONE
                fprintf(stderr,
                    "WARNING: .upd_info length is %lu rather than 1024, this might be a bug in the AppImage\n",
                    upd_length);
#endif
            }
        }
        if(appimage_type == 1){
            /* If we have a type 1 AppImage, then we hardcode the offset and length */
            upd_offset = 33651; // ISO 9660 Volume Descriptor field
            upd_length = 512; // Might be wrong
        }
#ifdef STANDALONE
        fprintf(stderr, ".upd_info offset: %lu\n", upd_offset);
        fprintf(stderr, ".upd_info length: %lu\n", upd_length);
#endif
        char buffer[3];
        FILE *binary = fopen(appimage_path, "rt");
        if (binary != NULL)
        {
            /* Check whether the first three bytes at the offset are not NULL */
            fseek(binary, upd_offset, SEEK_SET);
            fread(buffer, 1, 3, binary);
            fclose(binary);
            if((buffer[0] != 0x00) && (buffer[1] != 0x00) && (buffer[2] != 0x00)){
                gchar *appimageupdate_group = "Desktop Action AppImageUpdate";
                gchar *appimageupdate_exec = g_strdup_printf("%s %s", "AppImageUpdate", appimage_path);
                g_key_file_set_value(key_file_structure, appimageupdate_group, G_KEY_FILE_DESKTOP_KEY_NAME, "Update");
                g_key_file_set_value(key_file_structure, appimageupdate_group, G_KEY_FILE_DESKTOP_KEY_EXEC, appimageupdate_exec);
                g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, "Actions", "AppImageUpdate;");
            }
        }
    }
#endif

    gchar *icon_path = g_key_file_get_value(key_file_structure, "Desktop Entry", "Icon", NULL);
    gchar *basename = g_path_get_basename(icon_path);

    {
        gchar *icon_with_md5 = g_strdup_printf("%s_%s_%s", vendorprefix, md5, basename);

        g_free(basename);
        g_free(icon_path);

        g_key_file_set_value(key_file_structure, "Desktop Entry", "Icon", icon_with_md5);

        g_free(icon_with_md5);
    }

#ifdef APPIMAGED
    {
        gchar *generated_by = g_strdup_printf("Generated by appimaged %s", GIT_COMMIT);
        g_key_file_set_value(key_file_structure, "Desktop Entry", "X-AppImage-Comment", generated_by);
        g_free(generated_by);
    }
#endif
    g_key_file_set_value(key_file_structure, "Desktop Entry", "X-AppImage-Identifier", md5);
#ifdef STANDALONE
    fprintf(stderr, "Installing desktop file\n");
#endif
    if(verbose) {
        gchar *buf  = g_key_file_to_data(key_file_structure, NULL, NULL);
        fprintf(stderr, "%s", buf);
        g_free(buf);
    }


    /* https://specifications.freedesktop.org/menu-spec/menu-spec-latest.html#paths says:
     *
     * $XDG_DATA_DIRS/applications/
     * When two desktop entries have the same name, the one appearing earlier in the path is used.
     *
     * --
     *
     * https://developer.gnome.org/integration-guide/stable/desktop-files.html.en says:
     *
     * Place this file in the /usr/share/applications directory so that it is accessible
     * by everyone, or in ~/.local/share/applications if you only wish to make it accessible
     * to a single user. Which is used should depend on whether your application is
     * installed systemwide or into a user's home directory. GNOME monitors these directories
     * for changes, so simply copying the file to the right location is enough to register it
     * with the desktop.
     *
     * Note that the ~/.local/share/applications location is not monitored by versions of GNOME
     * prior to version 2.10 or on Fedora Core Linux, prior to version 2.8.
     * These versions of GNOME follow the now-deprecated vfolder standard,
     * and so desktop files must be installed to ~/.gnome2/vfolders/applications.
     * This location is not supported by GNOME 2.8 on Fedora Core nor on upstream GNOME 2.10
     * so for maximum compatibility with deployed desktops, put the file in both locations.
     *
     * Note that the KDE Desktop requires one to run kbuildsycoca to force a refresh of the menus.
     *
     * --
     *
     * https://specifications.freedesktop.org/menu-spec/menu-spec-latest.html says:
     *
     * To prevent that a desktop entry from one party inadvertently cancels out
     * the desktop entry from another party because both happen to get the same
     * desktop-file id it is recommended that providers of desktop-files ensure
     * that all desktop-file ids start with a vendor prefix.
     * A vendor prefix consists of [a-zA-Z] and is terminated with a dash ("-").
     * For example, to ensure that GNOME applications start with a vendor prefix of "gnome-",
     * it could either add "gnome-" to all the desktop files it installs
     * in datadir/applications/ or it could install desktop files in a
     * datadir/applications/gnome subdirectory.
     *
     * --
     *
     * https://specifications.freedesktop.org/desktop-entry-spec/latest/ape.html says:
     * The desktop file ID is the identifier of an installed desktop entry file.
     *
     * To determine the ID of a desktop file, make its full path relative
     * to the $XDG_DATA_DIRS component in which the desktop file is installed,
     * remove the "applications/" prefix, and turn '/' into '-'.
     * For example /usr/share/applications/foo/bar.desktop has the desktop file ID
     * foo-bar.desktop.
     * If multiple files have the same desktop file ID, the first one in the
     * $XDG_DATA_DIRS precedence order is used.
     * For example, if $XDG_DATA_DIRS contains the default paths
     * /usr/local/share:/usr/share, then /usr/local/share/applications/org.foo.bar.desktop
     * and /usr/share/applications/org.foo.bar.desktop both have the same desktop file ID
     * org.foo.bar.desktop, but only the first one will be used.
     *
     * --
     *
     * https://specifications.freedesktop.org/desktop-entry-spec/latest/ar01s07.html says:
     *
     * The application must name its desktop file in accordance with the naming
     * recommendations in the introduction section (e.g. the filename must be like
     * org.example.FooViewer.desktop). The application must have a D-Bus service
     * activatable at the well-known name that is equal to the desktop file name
     * with the .desktop portion removed (for our example, org.example.FooViewer).
     *
     * --
     *
     * Can it really be that no one thought about having multiple versions of the same
     * application installed? What are we supposed to do if we want
     * a) have desktop files installed by appimaged not interfere with desktop files
     *    provided by the system, i.e., if an application is installed in the system
     *    and the user also installs the AppImage, then both should be available to the user
     * b) both should be D-Bus activatable
     * c) the one installed by appimaged should have an AppImage vendor prefix to make
     *    it easy to distinguish it from system- or upstream-provided ones
     */

    /* FIXME: The following is most likely not correct; see the comments above.
     * Open a GitHub issue or send a pull request if you would like to propose asolution. */
    /* TODO: Check for consistency of the id with the AppStream file, if it exists in the AppImage */
    gchar *destination = build_installed_desktop_file_path(md5, desktop_filename);

    /* When appimaged sees itself, then do nothing here */
    if(strcmp ("appimaged.desktop", desktop_filename) == 0) {
        g_free(destination);
#ifdef STANDALONE
        fprintf(stderr, "appimaged's desktop file found -- not installing desktop file for myself\n");
#endif
        return true;
    }

    if(verbose)
        fprintf(stderr, "install: %s\n", destination);

    gchar *dirname = g_path_get_dirname(destination);
    if(g_mkdir_with_parents(dirname, 0755)) {
#ifdef STANDALONE
        fprintf(stderr, "Could not create directory: %s\n", dirname);
#endif
    }
    g_free(dirname);

    // g_key_file_save_to_file(key_file_structure, destination, NULL);
    // g_key_file_save_to_file is too new, only since 2.40
    /* Write config file on disk */
    gsize length;
    gchar *buf;
    GIOChannel *file;
    buf = g_key_file_to_data(key_file_structure, &length, NULL);
    file = g_io_channel_new_file(destination, "w", NULL);
    g_io_channel_write_chars(file, buf, length, NULL, NULL);
    g_io_channel_shutdown(file, TRUE, NULL);
    g_io_channel_unref(file);

    g_free(buf);

    /* GNOME shows the icon and name on the desktop file only if it is executable */
    chmod(destination, 0755);

    g_free(destination);

    return true;
}

bool appimage_type1_get_desktop_filename_and_key_file(struct archive** a, gchar** desktop_filename, GKeyFile** key_file) {
    // iterate over all files ("entries") in the archive
    // looking for a file with .desktop extension in the root directory

    // must not be freed
    struct archive_entry* entry;

    gchar* filename;

    for (;;) {
        int r = archive_read_next_header(*a, &entry);

        if (r == ARCHIVE_EOF) {
            return false;
        }

        if (r != ARCHIVE_OK) {
            fprintf(stderr, "%s\n", archive_error_string(*a));
            return false;
        }

        /* Skip all but regular files; FIXME: Also handle symlinks correctly */
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;

        filename = replace_str(archive_entry_pathname(entry), "./", "");

        /* Get desktop file(s) in the root directory of the AppImage and act on it in one go */
        if ((g_str_has_suffix(filename, ".desktop") && (NULL == strstr(filename, "/")))) {
#ifdef STANDALONE
            fprintf(stderr, "Got root desktop: %s\n", filename);
#endif

            const void* buff;

            size_t size = 1024 * 1024;
            int64_t offset = 0;

            r = archive_read_data_block(*a, &buff, &size, &offset);

            if (r == ARCHIVE_EOF) {
                // cleanup
                g_free(filename);

                return true;
            }

            if (r != ARCHIVE_OK) {
                fprintf(stderr, "%s", archive_error_string(*a));
                break;
            }

            *desktop_filename = g_path_get_basename(filename);

            // a structure that will hold the information from the desktop file
            *key_file = g_key_file_new();

            gboolean success = g_key_file_load_from_data(*key_file, buff, size,
                G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

            if (!success) {
                // cleanup
                g_free(key_file);
                key_file = NULL;

                break;
            }

            // cleanup
            g_free(filename);

            return true;
        }
    }

    g_free(filename);

    return false;
}

bool archive_copy_icons_recursively_to_destination(struct archive** a, const gchar* md5,
                                                   const gchar* desktop_icon_value_original, gboolean verbose) {
    // iterate over all files ("entries") in the archive
    // looking for a file with .desktop extension in the root directory
    struct archive_entry* entry;
    gchar* filename = NULL;

    bool errored = false;

    for (;;) {
        int r = archive_read_next_header(*a, &entry);

        if (r == ARCHIVE_EOF) {
            break;
        }

        if (r != ARCHIVE_OK) {
            fprintf(stderr, "%s\n", archive_error_string(*a));

            errored = true;
            break;
        }

        /* Skip all but regular files; FIXME: Also handle symlinks correctly */
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;

        filename = replace_str(archive_entry_pathname(entry), "./", "");

        gchar* dest = NULL;

        // Get icon file(s) and MIME types and act on them in one go

        // add vendor prefix (and MD5 hash as an identifier for future operations)
        if (g_str_has_prefix(filename, "usr/share/icons/")
            || g_str_has_prefix(filename, "usr/share/pixmaps/")
            || (g_str_has_prefix(filename, "usr/share/mime/") && g_str_has_suffix(filename, ".xml"))
            ) {

            gchar* dest_path = replace_str(filename, "usr/share", xdg_data_home());
            gchar* dest_dirname = g_path_get_dirname(dest_path);
            g_free(dest_path);

            gchar* file_basename = g_path_get_basename(filename);
            gchar* dest_basename = g_strdup_printf("%s_%s_%s", vendorprefix, md5, file_basename);
            g_free(file_basename);

            dest = g_build_path("/", dest_dirname, dest_basename, NULL);

            g_free(dest_basename);
            g_free(dest_dirname);
        }

        // According to https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html#install_icons
        // share/pixmaps is ONLY searched in /usr but not in $XDG_DATA_DIRS and hence $HOME and this seems to be true at least in XFCE
        if (g_str_has_prefix(filename, "usr/share/pixmaps/")) {
            // clean up dest in case it has been set before
            g_free(dest);

            // TODO: avoid cluttering /tmp too much
            dest = g_build_path("/", "/tmp", NULL);
        }

        if (desktop_icon_value_original != NULL) {
            if (g_str_has_prefix(filename, desktop_icon_value_original) && !strstr(filename, "/") && (g_str_has_suffix(filename, ".png") || g_str_has_suffix(filename, ".xpm") || g_str_has_suffix(filename, ".svg") || g_str_has_suffix(filename, ".svgz"))) {
                gchar* file_extension = get_file_extension(filename);
                gchar* dest_basename = g_strdup_printf("%s_%s_%s.%s", vendorprefix, md5, desktop_icon_value_original,
                    file_extension);
                g_free(file_extension);

                // clean up dest in case it has been set before
                g_free(dest);

                // TODO: avoid cluttering /tmp too much
                dest = g_build_path("/", "/tmp", dest_basename, NULL);

                g_free(dest_basename);
            }
        }

        // cleanup
        g_free(filename);

        if (dest != NULL) {
            if (verbose)
                fprintf(stderr, "install: %s\n", dest);

            gchar* dir_name = g_path_get_dirname(dest);

            if (g_mkdir_with_parents(dir_name, 0755)) {
                fprintf(stderr, "Could not create directory: %s\n", dir_name);

                // cleanup
                g_free(dir_name);

                errored = true;
                break;
            }

            g_free(dir_name);

            FILE *f = fopen(dest, "w+");

            if (f == NULL) {
                int error = errno;
#ifdef STANDALONE
                fprintf(stderr, "Could not open file %s for writing: %s\n", dest, strerror(error));
#endif

                // cleanup
                g_free(dest);

                errored = true;
                break;
            }

            const void* buff;

            size_t size;
            int64_t offset;

            for (;;) {
                r = archive_read_data_block(*a, &buff, &size, &offset);

                if (r == ARCHIVE_EOF)
                    break;

                if (r != ARCHIVE_OK) {
#ifdef STANDALONE
                    fprintf(stderr, "%s\n", archive_error_string(*a));
#endif
                    break;
                }

                if ((fwrite(buff, size, 1, f) != size) || ferror(f) || feof(f)) {
#ifdef STANDALONE
                    int error = errno;
                    fprintf(stderr, "Failed to copy icon: %s\n", strerror(error));
#endif
                    errored = true;
                    break;
                }
            }

            fclose(f);
            chmod(dest, 0644);

            if (verbose && !errored) {
                fprintf(stderr, "Installed: %s\n", dest);
            }

            if (g_str_has_prefix(dest, "/tmp")) {
                move_icon_to_destination(dest, verbose);
            }

            g_free(dest);

            if (errored)
                break;
        }
    }

    return !errored;
}

/* Register a type 1 AppImage in the system */
bool appimage_type1_register_in_system(const char *path, bool verbose)
{
#ifdef STANDALONE
    fprintf(stderr, "ISO9660 based type 1 AppImage\n");
#endif
    gchar *desktop_icon_value_original = NULL;

    char *md5 = appimage_get_md5(path);

    if(verbose)
        fprintf(stderr, "md5 of URI RFC 2396: %s\n", md5);

    // open ISO9660 image using libarchive
    struct archive *a = archive_read_new();
    archive_read_support_format_iso9660(a);

    // libarchive status int -- passed to called functions
    int r;

    // use global bool to not have duplicate cleanup code in the following calls
    // all if() checks need to be prefixed with "!errored &&" therefore, and on error need to set errored to true
    // this would be _by far_ less complex code in C++, where lambdas and strings and other nice things exist...
    bool errored = false;

    if ((r = archive_read_open_filename(a, path, 10240)) != ARCHIVE_OK) {
#ifdef STANDALONE
        fprintf(stderr, "%s\n", archive_error_string(a));
#endif
        errored = true;
    }
    // search image for root desktop file, and read it into key file structure so it can be edited eventually
    gchar *desktop_filename = NULL;
    GKeyFile *key_file = NULL;

    if (!errored && !appimage_type1_get_desktop_filename_and_key_file(&a, &desktop_filename, &key_file)) {
        errored = true;
    }

    // validate that both have been set to a non-NULL value
    if (desktop_filename == NULL || key_file == NULL) {
        errored = true;
    }

    if (!errored) {
        desktop_icon_value_original = g_key_file_get_string(key_file, "Desktop Entry", "Icon", NULL);

        if (verbose)
            fprintf(stderr, "desktop_icon_value_original: %s\n", desktop_icon_value_original);

        if (!write_edited_desktop_file(key_file, path, desktop_filename, 1, md5, verbose)) {
#ifdef STANDALONE
            fprintf(stderr, "Failed to install desktop file\n");
#endif
            errored = true;
        }
    }

    // next step: copy icons recursively to their target destination

    // reopen ISO9660 image
    // TODO: merge both steps (reading desktop file and copying icons) again to not have to read the ISO file twice
    if (!errored) {
        // close and reopen archive
        archive_read_free(a);

        a = archive_read_new();
        archive_read_support_format_iso9660(a);

        if ((r = archive_read_open_filename(a, path, 10240)) != ARCHIVE_OK) {
#ifdef STANDALONE
            fprintf(stderr, "archive error: %s\n", archive_error_string(a));
#endif
            errored = true;
        } else {
            if (!archive_copy_icons_recursively_to_destination(&a, md5, desktop_icon_value_original, verbose)) {
                errored = true;
            }
        }
    }

    // cleanup
    g_free(desktop_filename);
    g_free(desktop_icon_value_original);
    g_free(md5);
    g_key_file_free(key_file);
    archive_read_free(a);

    return !errored;
}

bool appimage_type2_get_desktop_filename_and_key_file(sqfs* fs, gchar** desktop_filename, gchar* md5, GKeyFile** key_file, gboolean verbose) {
    /* TOOO: Change so that only one run of squash_get_matching_files is needed in total,
     * this should hopefully improve performance */

    /* Get desktop file(s) in the root directory of the AppImage */
    // Only in root dir
    gchar** str_array = squash_get_matching_files_install_icons_and_mime_data(fs, "(^[^/]*?.desktop$)", "", md5, verbose);

    bool errored = false;

    // gchar **str_array = squash_get_matching_files(&fs, "(^.*?.desktop$)", md5, verbose); // Not only there
    /* Work trough the NULL-terminated array of strings */
    for (int i = 0; str_array[i]; ++i) {
#ifdef STANDALONE
        fprintf(stderr, "Got root desktop: %s\n", str_array[i]);
#endif

        if (!g_key_file_load_from_squash(fs, str_array[i], *key_file, verbose))
            errored = true;
        else
            *desktop_filename = g_path_get_basename(str_array[i]);
    }

    /* Free the NULL-terminated array of strings and its contents */
    g_strfreev(str_array);

    return !errored;
}

/* Register a type 2 AppImage in the system */
bool appimage_type2_register_in_system(const char *path, bool verbose) {
#ifdef STANDALONE
    fprintf(stderr, "squashfs based type 2 AppImage\n");
#endif

    // the offset at which a squashfs image is expected
    long unsigned int fs_offset;
    char* md5 = appimage_get_md5(path);

    // a structure that will hold the information from the desktop file
    GKeyFile* key_file = g_key_file_new();
    // FIXME: otherwise the regex does weird stuff in the first run
    gchar* desktop_icon_value_original = "iDoNotMatchARegex";

    if (verbose)
        fprintf(stderr, "md5 of URI RFC 2396: %s\n", md5);

    fs_offset = get_elf_size(path);

    if (verbose)
        fprintf(stderr, "fs_offset: %lu\n", fs_offset);

    sqfs fs;

    sqfs_err err = sqfs_open_image(&fs, path, fs_offset);

    if (err != SQFS_OK) {
        sqfs_destroy(&fs);
#ifdef STANDALONE
        fprintf(stderr, "sqfs_open_image error: %s\n", path);
#endif
        return FALSE;
    } else {
        if (verbose)
            fprintf(stderr, "sqfs_open_image: %s\n", path);
    }

    gchar* desktop_filename = NULL;

    bool errored = false;

    if (appimage_type2_get_desktop_filename_and_key_file(&fs, &desktop_filename, md5, &key_file, verbose)) {
        desktop_icon_value_original = g_key_file_get_value(key_file, "Desktop Entry", "Icon", NULL);

        if (desktop_icon_value_original == NULL) {
            errored = true;
        } else {
            if (verbose)
                fprintf(stderr, "desktop_icon_value_original: %s\n", desktop_icon_value_original);
            if (!write_edited_desktop_file(key_file, path, desktop_filename, 2, md5, verbose)) {
#ifdef STANDALONE
                fprintf(stderr, "Failed to install desktop file\n");
#endif
                return false;
            }
        }
    } else {
        errored = true;
    }

    if (!errored) {
        /* Get relevant file(s) */
        static char* const pattern = "(^usr/share/(icons|pixmaps)/.*.(png|svg|svgz|xpm)$|^.DirIcon$|^usr/share/mime/packages/.*.xml$|^usr/share/appdata/.*metainfo.xml$|^[^/]*?.(png|svg|svgz|xpm)$)";

        gchar** str_array2 = squash_get_matching_files_install_icons_and_mime_data(&fs, pattern, desktop_icon_value_original, md5, verbose);

        /* Free the NULL-terminated array of strings and its contents */
        g_strfreev(str_array2);
    }

    /* The above also gets AppStream metainfo file(s), TODO: Check if the id matches and do something with them*/
    set_executable(path, verbose);

    g_free(desktop_filename);
    sqfs_destroy(&fs);

    g_free(md5);
    g_free(desktop_icon_value_original);
    return TRUE;
}

int appimage_type1_is_no_display(const char* path) {
    // check if file exists
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR))
        return -1;

    // check if file is of correct type
    if (appimage_get_type(path, false) != 1)
        return -1;

    char* md5 = appimage_get_md5(path);

    if (md5 == NULL)
        return -1;

    unsigned long fs_offset = get_elf_size(path);

    // open ISO9660 image using libarchive
    struct archive *a = archive_read_new();
    archive_read_support_format_iso9660(a);

    // libarchive status int -- passed to called functions
    int r;

    if ((r = archive_read_open_filename(a, path, 10240)) != ARCHIVE_OK) {
        // cleanup
        free(md5);
        archive_read_free(a);

        return -1;
    }
    // search image for root desktop file, and read it into key file structure so it can be edited eventually
    gchar *desktop_filename = NULL;
    GKeyFile *key_file = NULL;

    if (!appimage_type1_get_desktop_filename_and_key_file(&a, &desktop_filename, &key_file)) {
        // cleanup
        free(md5);
        archive_read_free(a);
        g_free(desktop_filename);
        g_key_file_free(key_file);

        return -1;
    }

    // validate that both have been set to a non-NULL value
    if (desktop_filename == NULL || key_file == NULL) {
        // cleanup
        free(md5);
        archive_read_free(a);
        g_free(desktop_filename);
        g_key_file_free(key_file);

        return -1;
    }

    GError *error = NULL;
    gboolean rv = g_key_file_get_boolean(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, &error);

    // cleanup
    free(md5);
    archive_read_free(a);
    g_free(desktop_filename);
    g_key_file_free(key_file);

    int result;

    if (!rv) {
        // if the key file hasn't been found and the error is not set to NOT_FOUND, return an error
        if (error == G_KEY_FILE_ERROR_NOT_FOUND)
            result = -1;
        else
            result = 0;
    } else {
        result = 1;
    }

    if (error != NULL)
        g_error_free(error);

    return result;
};

int appimage_type2_is_no_display(const char* path) {
    // check if file exists
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR))
        return -1;

    // check if file is of correct type
    if (appimage_get_type(path, false) != 2)
        return -1;

    char* md5 = appimage_get_md5(path);

    if (md5 == NULL)
        return -1;

    unsigned long fs_offset = get_elf_size(path);

    sqfs fs;

    sqfs_err err = sqfs_open_image(&fs, path, fs_offset);

    if (err != SQFS_OK) {
        free(md5);
        sqfs_destroy(&fs);
        return -1;
    }

    gchar* desktop_filename = NULL;

    // a structure that will hold the information from the desktop file
    GKeyFile* key_file = g_key_file_new();

    if (!appimage_type2_get_desktop_filename_and_key_file(&fs, &desktop_filename, md5, &key_file, false)) {
        // cleanup
        free(md5);
        free(desktop_filename);
        sqfs_destroy(&fs);
        g_key_file_free(key_file);

        return -1;
    }

    // validate that both have been set to a non-NULL value
    if (desktop_filename == NULL || key_file == NULL) {
        // cleanup
        free(md5);
        sqfs_destroy(&fs);
        g_free(desktop_filename);
        g_key_file_free(key_file);

        return -1;
    }

    // no longer used
    free(md5);

    GError *error = NULL;
    gboolean rv = g_key_file_get_boolean(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, &error);

    // cleanup
    free(desktop_filename);
    sqfs_destroy(&fs);
    g_key_file_free(key_file);

    int result;

    if (!rv) {
        // if the key file hasn't been found and the error is not set to NOT_FOUND, return an error
        if (error == G_KEY_FILE_ERROR_NOT_FOUND)
            result = -1;
        else
            result = 0;
    } else {
            result = 1;
    }

    if (error != NULL)
        g_error_free(error);

    return result;
};

/*
 * Checks whether an AppImage's desktop file has set NoDisplay=true.
 * Useful to check whether the author of an AppImage doesn't want it to be integrated.
 *
 * Returns >0 if set, 0 if not set, <0 on errors.
 */
int appimage_is_no_display(const char *path) {
    // check if file exists
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR))
        return -1;

    int type = appimage_get_type(path, false);

    switch (type) {
        case 1:
            return appimage_type1_is_no_display(path);
        case 2:
            return appimage_type2_is_no_display(path);
        default:
            return -1;
    }
}

char* appimage_registered_desktop_file_path(const char *path, char *md5, bool verbose) {
    glob_t pglob = {};

    // if md5 has been calculated before, we can just use it to save these extra calculations
    // if not, we need to calculate it here
    if (md5 == NULL)
        md5 = appimage_get_md5(path);

    char *data_home = xdg_data_home();

    // TODO: calculate this value exactly
    char *glob_pattern = malloc(PATH_MAX);
    sprintf(glob_pattern, "%s/applications/appimagekit_%s-*.desktop", data_home, md5);

    glob(glob_pattern, 0, NULL, &pglob);

    char* rv = NULL;

    if (pglob.gl_pathc <= 0) {
        if (verbose) {
            fprintf(stderr, "No results found by glob()");
        }
    } else if (pglob.gl_pathc >= 1) {
        if (pglob.gl_pathc > 1 && verbose) {
            fprintf(stderr, "Too many results returned by glob(), returning first result found");
        }

        // need to copy value to be able to globfree() later on
        rv = strdup(pglob.gl_pathv[0]);
    }

    globfree(&pglob);

    return rv;
};

/* Check whether AppImage is registered in the system already */
bool appimage_is_registered_in_system(const char* path) {
    // there's two criteria whether an AppImage has been registered in the system:
    // 1) Has the thumbnail been created?
    // 2) Has the desktop file been registered?
    // if both questions can be answered yes, then an AppImage has been registered properly

    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR))
        return false;

    gchar* thumbnail_path = get_thumbnail_path(path, "normal", false);
    if (!g_file_test(thumbnail_path, G_FILE_TEST_IS_REGULAR)) {
        // cleanup
        g_free(thumbnail_path);

        return false;
    }

    gchar* md5 = appimage_get_md5(path);

    GKeyFile* key_file = g_key_file_new();
    gchar* desktop_file_path = appimage_registered_desktop_file_path(path, md5, false);

    bool rv = true;

    if (!g_file_test(desktop_file_path, G_FILE_TEST_IS_REGULAR))
        rv = false;

    g_free(thumbnail_path);
    g_free(md5);
    g_free(desktop_file_path);
    g_key_file_free(key_file);

    return rv;
}

/*
 * Register an AppImage in the system
 * Returns 0 on success, non-0 otherwise.
 */
int appimage_register_in_system(const char *path, bool verbose)
{
    if((g_str_has_suffix(path, ".part")) ||
        g_str_has_suffix(path, ".tmp") ||
        g_str_has_suffix(path, ".download") ||
        g_str_has_suffix(path, ".zs-old") ||
        g_str_has_suffix(path, ".~")
    ) {
        return 0;
    }

    int type = appimage_get_type(path, verbose);

    if(type != -1) {
#ifdef STANDALONE
        fprintf(stderr, "\n-> Registering type %d AppImage: %s\n", type, path);
#endif
        appimage_create_thumbnail(path, false);
    } else {
        if (verbose)
            fprintf(stderr, "-> Skipping file %s\n", path);
        return 0;
    }

    switch (type) {
        case 1:
            appimage_type1_register_in_system(path, verbose);
            break;
        case 2:
            appimage_type2_register_in_system(path, verbose);
            break;
        default:
#ifdef STANDALONE
            fprintf(stderr, "Error: unknown AppImage type %d\n", type);
#endif
            return 1;
    }

    if (type == 1) {
        if (!appimage_type1_register_in_system(path, verbose))
            return 1;
    }

    if (type == 2) {
        if (!appimage_type2_register_in_system(path, verbose))
            return 1;
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
    g_free(thumbnail_path);
}

/* Recursively delete files in path and subdirectories that contain the given md5
 */
void unregister_using_md5_id(const char *name, int level, char* md5, gboolean verbose)
{
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;
    if (!(entry = readdir(dir)))
        return;

    do {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            int len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
            path[len] = 0;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            unregister_using_md5_id(path, level + 1, md5, verbose);
        }

        else {
            gchar *needle = g_strdup_printf("%s_%s", vendorprefix, md5);
            if(strstr(entry->d_name, needle)) {
                    gchar *path_to_be_deleted = g_strdup_printf("%s/%s", name, entry->d_name);
                    if(g_file_test(path_to_be_deleted, G_FILE_TEST_IS_REGULAR)){
                        g_unlink(path_to_be_deleted);
                        if(verbose)
                            fprintf(stderr, "deleted: %s\n", path_to_be_deleted);
                    }
                    g_free(path_to_be_deleted);
                }
            g_free(needle);
        }
    } while ((entry = readdir(dir)) != NULL);
    closedir(dir);
}


/* Unregister an AppImage in the system */
int appimage_unregister_in_system(const char *path, bool verbose)
{
    char *md5 = appimage_get_md5(path);

    /* The file is already gone by now, so we can't determine its type anymore */
#ifdef STANDALONE
    fprintf(stderr, "_________________________\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "-> UNREGISTER %s\n", path);
#endif
    /* Could use gnome_desktop_thumbnail_factory_lookup instead of the next line */

    /* Delete the thumbnails if they exist */
    delete_thumbnail(path, "normal", verbose); // 128x128
    delete_thumbnail(path, "large", verbose); // 256x256

    char* data_home = xdg_data_home();
    unregister_using_md5_id(data_home, 0, md5, verbose);
    g_free(data_home);

    g_free(md5);

    return 0;
}



bool is_handler_valid(const appimage_handler *handler) {
    if (!handler) {
#ifdef STANDALONE
        fprintf(stderr, "WARNING: Invalid handler found, you should take a look at this now!");
#endif
        return false;
    }

    return true;
}

void mk_base_dir(const char *target)
{
    gchar *dirname = g_path_get_dirname(target);
    if(g_mkdir_with_parents(dirname, 0755)) {
#ifdef STANDALONE
        fprintf(stderr, "Could not create directory: %s\n", dirname);
#endif
    }

    g_free(dirname);
}

/*
 * Dummy fallback functions
 */
void dummy_traverse_func(appimage_handler *handler, traverse_cb command, void *data) {
    (void) handler;
    (void) command;
    (void) data;

    fprintf(stderr, "Called %s\n", __FUNCTION__);
}

char* dummy_get_file_name (appimage_handler *handler, void *data) {
    fprintf(stderr, "Called %s\n", __FUNCTION__);
}

void dummy_extract_file(struct appimage_handler *handler, void *data, char *target) {
    fprintf(stderr, "Called %s\n", __FUNCTION__);
}

/*
 * AppImage Type 1 Functions
 */

void appimage_type1_open(appimage_handler *handler) {
    if ( is_handler_valid(handler) && !handler->is_open ) {
#ifdef STANDALONE
        fprintf(stderr, "Opening %s as Type 1 AppImage\n", handler->path);
#endif
        struct archive *a;
        a = archive_read_new();
        archive_read_support_format_iso9660(a);
        if (archive_read_open_filename(a, handler->path, 10240) != ARCHIVE_OK) {
            fprintf(stderr, "%s", archive_error_string(a));
            handler->cache = NULL;
            handler->is_open = false;
        } else {
            handler->cache = a;
            handler->is_open = true;
        }
    }
}

void appimage_type1_close(appimage_handler *handler) {
    if ( is_handler_valid(handler) && handler->is_open ) {
#ifdef STANDALONE
        fprintf(stderr, "Closing %s\n", handler->path);
#endif
        struct archive *a = handler->cache;
        archive_read_close(a);
        archive_read_free(a);

        handler->cache = NULL;
        handler->is_open = false;
    }
}

void appimage_type1_traverse(appimage_handler *handler, traverse_cb command, void *command_data) {
    appimage_type1_open(handler);

    if (!command) {
#ifdef STANDALONE
        fprintf(stderr, "No traverse command set.\n");
#endif
        return;
    }

    if (handler->is_open) {
        struct archive *a = handler->cache;
        struct archive_entry *entry;
        int r;

        for (;;) {
            r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF) {
                break;
            }
            if (r != ARCHIVE_OK) {
                fprintf(stderr, "%s\n", archive_error_string(a));
                break;
            }

            /* Skip all but regular files; FIXME: Also handle symlinks correctly */
            if(archive_entry_filetype(entry) != AE_IFREG) {
                continue;
            }

            command(handler, entry, command_data);
        }
    }

    appimage_type1_close(handler);
}

char* appimage_type1_get_file_name (appimage_handler *handler, void *data) {
    (void) handler;

    struct archive_entry *entry = (struct archive_entry *) data;

    char *filename = replace_str(archive_entry_pathname(entry), "./", "");
    return filename;
}

void appimage_type1_extract_file (appimage_handler *handler, void *data, const char *target) {
    (void) data;

    struct archive *a = handler->cache;
    mk_base_dir(target);

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int f = open(target, O_WRONLY | O_CREAT | O_TRUNC, mode);

    if (f == -1){
#ifdef STANDALONE
        fprintf(stderr, "open error: %s\n", target);
#endif
        return;
    }

    archive_read_data_into_fd(a, f);
    close(f);
}

appimage_handler appimage_type_1_create_handler() {
    appimage_handler h;
    h.traverse = appimage_type1_traverse;
    h.get_file_name = appimage_type1_get_file_name;
    h.extract_file = appimage_type1_extract_file;

    return h;
}

/*
 * AppImage Type 2 Functions
 */

void appimage_type2_open(appimage_handler *handler) {
    if (is_handler_valid(handler) && !handler->is_open) {
#ifdef STANDALONE
        fprintf(stderr, "Opening %s as Type 2 AppImage\n", handler->path);
#endif
        long unsigned int fs_offset; // The offset at which a squashfs image is expected
        fs_offset = get_elf_size(handler->path);

        sqfs *fs = malloc(sizeof(sqfs));
        sqfs_err err = sqfs_open_image(fs, handler->path, fs_offset);
        if (err != SQFS_OK){
#ifdef STANDALONE
            fprintf(stderr, "sqfs_open_image error: %s\n", handler->path);
#endif
            free(fs);
            handler->is_open = false;
            handler->cache = NULL;
        } else {
            handler->is_open = true;
            handler->cache = fs;
        }
    }
}

void appimage_type2_close(appimage_handler *handler) {
    if ( is_handler_valid(handler) && handler->is_open ) {
#ifdef STANDALONE
        fprintf(stderr, "Closing %s\n", handler->path);
#endif

        sqfs_destroy(handler->cache);
        free(handler->cache);

        handler->is_open = false;
        handler->cache = NULL;
    }
}

void appimage_type2_traverse(appimage_handler *handler, traverse_cb command, void *command_data) {
    appimage_type2_open(handler);

    if (handler->is_open && handler->cache != NULL) {
        sqfs *fs = handler->cache;
        sqfs_traverse trv;
        sqfs_inode_id root_inode = sqfs_inode_root(fs);
        sqfs_err err = sqfs_traverse_open(&trv, fs, root_inode);
        if (err!= SQFS_OK) {
#ifdef STANDALONE
            fprintf(stderr, "sqfs_traverse_open error\n");
#endif
        }
        while (sqfs_traverse_next(&trv, &err))
            command(handler, &trv, command_data);

        if (err) {
#ifdef STANDALONE
            fprintf(stderr, "sqfs_traverse_next error\n");
#endif
        }
        sqfs_traverse_close(&trv);
    }

    appimage_type2_close(handler);
}

char* appimage_type2_get_file_name (appimage_handler *handler, void *data) {
    (void) handler;
    sqfs_traverse *trv = data;
    return strdup(trv->path);
}

void appimage_type2_extract_symlink(sqfs *fs, sqfs_inode *inode, const char *target);

void appimage_type2_extract_regular_file(sqfs *fs, sqfs_inode *inode, const char* target) {
    mk_base_dir(target);

    // Read the file in chunks
    squash_extract_inode_to_file(fs, inode, target);
}
void appimage_type2_extract_file_following_symlinks(sqfs *fs, sqfs_inode *inode, const char* target) {
    if(inode->base.inode_type == SQUASHFS_REG_TYPE)
        appimage_type2_extract_regular_file(fs, inode, target);
    else if(inode->base.inode_type == SQUASHFS_SYMLINK_TYPE) {
        appimage_type2_extract_symlink(fs, inode, target);
    } else {
#ifdef STANDALONE
        fprintf(stderr, "WARNING: Unable to extract file of type %d", inode->base.inode_type);
#endif
    }
}

void appimage_type2_extract_symlink(sqfs *fs, sqfs_inode *inode, const char *target) {
    size_t size;
    sqfs_readlink(fs, inode, NULL, &size);
    char buf[size];
    int ret = sqfs_readlink(fs, inode, buf, &size);
    if (ret != 0) {
#ifdef STANDALONE
        fprintf(stderr, "WARNING: Symlink error.");
#endif
    }
    else {

        sqfs_err err = sqfs_inode_get(fs, inode, fs->sb.root_inode);
        if (err != SQFS_OK) {
#ifdef STANDALONE
            fprintf(stderr, "WARNING: Unable to get the root inode. Error: %d", err);
#endif
        }

        bool found = false;
        err = sqfs_lookup_path(fs, inode, buf, &found);
        if (err != SQFS_OK) {
#ifdef STANDALONE
            fprintf(stderr, "WARNING: There was an error while trying to lookup a symblink. Error: %d", err);
#endif
        }

        if (found)
            appimage_type2_extract_file_following_symlinks(fs, inode, target);
    }
}

void appimage_type2_extract_file (appimage_handler *handler, void *data, const char *target) {
    sqfs *fs = handler->cache;
    sqfs_traverse *trv = data;

    sqfs_inode inode;
    if(sqfs_inode_get(fs, &inode, trv->entry.inode)) {
#ifdef STANDALONE
        fprintf(stderr, "sqfs_inode_get error\n");
#endif
    }

    appimage_type2_extract_file_following_symlinks(fs, &inode, target);
}

appimage_handler appimage_type_2_create_handler() {
    appimage_handler h;
    h.traverse = appimage_type2_traverse;
    h.get_file_name = appimage_type2_get_file_name;
    h.extract_file = appimage_type2_extract_file;

    return h;
}

/* Factory function for creating the right appimage handler for
 * a given file. */
appimage_handler create_appimage_handler(const char *const path) {
    int appimage_type = appimage_get_type(path, 0);

    appimage_handler handler;
#ifdef STANDALONE
    fprintf(stderr,"AppImage type: %d\n", appimage_type);
#endif
    switch (appimage_type) {
        case 1:
            handler = appimage_type_1_create_handler();
            break;
        case 2:
            handler = appimage_type_2_create_handler();
            break;
        default:
#ifdef STANDALONE
            fprintf(stderr,"Appimage type %d not supported yet\n", appimage_type);
#endif
            handler.traverse = dummy_traverse_func;
            break;
    }
    handler.path = path;
    handler.is_open = false;
    return handler;
}

void move_file(const char *source, const char *target) {
    g_type_init();
    GError *error = NULL;
    GFile *icon_file = g_file_new_for_path(source);
    GFile *target_file = g_file_new_for_path(target);
    if (!g_file_move (icon_file, target_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
#ifdef STANDALONE
        fprintf(stderr, "Error moving file: %s\n", error->message);
#endif
        g_clear_error (&error);
    }

    g_object_unref(icon_file);
    g_object_unref(target_file);
}

struct extract_appimage_file_command_data {
    const char *path;
    const char *destination;
};

void extract_appimage_file_command(void *handler_data, void *entry_data, void *user_data) {
    appimage_handler *h = handler_data;
    struct archive_entry *entry = entry_data;
    const struct extract_appimage_file_command_data const *params = user_data;

    char *filename = h->get_file_name(h, entry);
    if (strcmp(params->path, filename) == 0)
        h->extract_file(h, entry, params->destination);

    free(filename);
}

void extract_appimage_file(appimage_handler *h, const char *path, const char *destination) {
    struct extract_appimage_file_command_data data;
    data.path = path;
    data.destination = destination;
    h->traverse(h, extract_appimage_file_command, &data);
}

void extract_appimage_icon_command(void *handler_data, void *entry_data, void *user_data) {
    appimage_handler *h = handler_data;
    struct archive_entry *entry = entry_data;
    gchar *path = user_data;

    char *filename = h->get_file_name(h, entry);
    if (strcmp(".DirIcon", filename) == 0)
        h->extract_file(h, entry, path);

    free(filename);
}

void extract_appimage_icon(appimage_handler *h, gchar *target) {
    h->traverse(h, extract_appimage_icon_command, target);
}

/* Create AppImage thumbanil according to
 * https://specifications.freedesktop.org/thumbnail-spec/0.8.0/index.html
 */
void appimage_create_thumbnail(const char *appimage_file_path, bool verbose) {
    // extract AppImage icon to /tmp
    appimage_handler handler = create_appimage_handler(appimage_file_path);

    char *tmp_path = "/tmp/appimage_thumbnail_tmp";
    extract_appimage_icon(&handler, tmp_path);

    if (g_file_test(tmp_path, G_FILE_TEST_EXISTS) ) {
        // TODO: transform it to png with sizes 128x128 and 254x254
        gchar *target_path = get_thumbnail_path(appimage_file_path, "normal", verbose);

        mk_base_dir(target_path);

        // deploy icon as thumbnail
        move_file (tmp_path, target_path);

        // clean up
        g_free(target_path);
    } else {
#ifdef STANDALONE
        fprintf(stderr, "ERROR: Icon file not extracted: %s", tmp_path);
#endif
    }

}

void appimage_extract_file_following_symlinks(const gchar* appimage_file_path, const char* file_path,
                                              const char* target_dir) {
    appimage_handler handler = create_appimage_handler(appimage_file_path);

    extract_appimage_file(&handler, file_path, target_dir);
}

void extract_appimage_file_name(void *handler_data, void *entry_data, void *user_data) {
    appimage_handler *h = handler_data;
    struct archive_entry *entry = entry_data;
    GList **list = user_data;

    char *filename = h->get_file_name(h, entry);

    GList* ptr = g_list_find_custom (*list, filename, g_strcmp0);

    if (ptr == NULL)
        *list = g_list_append(*list, filename);
    else
        free(filename);
}


char** appimage_list_files(const char *path) {
    GList *list = NULL;
    appimage_handler handler = create_appimage_handler(path);

    handler.traverse(&handler, extract_appimage_file_name, &list);

    int n = g_list_length(list);
    char **result = malloc(sizeof(char*) * (n+1) );
    result[n] = NULL;

    GList *itr = list;
    for (int i = 0; i < n; i ++) {
        result[i] = (char *) itr->data;
        itr = itr->next;
    }


    g_list_free(list);

    return result;
}

void appimage_string_list_free(char** list) {
    for (char **ptr = list; ptr != NULL && *ptr != NULL; ptr ++)
        free(*ptr);

    free(list);
}
