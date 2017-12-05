/**************************************************************************
 * 
 * Copyright (c) 2004-17 Simon Peter
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
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "squashfuse.h"
#include <squashfs_fs.h>

#include "elf.h"
#include "getsection.h"

#if HAVE_LIBARCHIVE3 == 1 // CentOS
# include <archive3.h>
# include <archive_entry3.h>
#else // other systems
# include <archive.h>
# include <archive_entry.h>
#endif

#include <regex.h>

#include <cairo.h> // To get the size of icons, move_icon_to_destination()

#define FNM_FILE_NAME 2

#define URI_MAX (FILE_MAX * 3 + 8)

char *vendorprefix = "appimagekit";

void set_executable(char *path, gboolean verbose)
{
    if(!g_find_program_in_path ("firejail")){
        int result = chmod(path, 0755); // TODO: Only do this if signature verification passed
        if(result != 0){
            fprintf(stderr, "Could not set %s executable: %s\n", path, strerror(errno));
        } else {
            if(verbose)
                fprintf(stderr, "Set %s executable\n", path);
        }
    }
}

/* Search and replace on a string, this really should be in Glib
 * https://mail.gnome.org/archives/gtk-list/2012-February/msg00005.html */
gchar* replace_str(const gchar *src, const gchar *find, const gchar *replace){
    gchar* retval = g_strdup(src);
    gchar* ptr = NULL;
    ptr = g_strstr_len(retval,-1,find); 
    if (ptr != NULL){
        gchar* after_find = replace_str(ptr+strlen(find),find,replace);
        gchar* before_find = g_strndup(retval,ptr-retval);
        gchar* temp = g_strconcat(before_find,replace,after_find,NULL);
        g_free(retval);
        retval = g_strdup(temp);
        g_free(after_find);
        g_free(before_find);
        g_free(temp);
    }   
    return retval;
}

/* Return the md5 hash constructed according to
 * https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#THUMBSAVE
 * This can be used to identify files that are related to a given AppImage at a given location */
char * get_md5(char *path)
{
    gchar *uri = g_filename_to_uri (path, NULL, NULL);
    if (uri != NULL)
    {
        GChecksum *checksum;
        checksum = g_checksum_new(G_CHECKSUM_MD5);
        guint8 digest[16];
        gsize digest_len = sizeof (digest);
        g_checksum_update(checksum, (const guchar *) uri, strlen (uri));
        g_checksum_get_digest(checksum, digest, &digest_len);
        g_assert(digest_len == 16);

        gchar *out = g_strdup_printf("%s", g_checksum_get_string(checksum)); 

        g_checksum_free(checksum);
        g_free(uri);

        return out;
    } else {
        return NULL;
    }
}

/* Return the path of the thumbnail regardless whether it already exists; may be useful because
 * G*_FILE_ATTRIBUTE_THUMBNAIL_PATH only exists if the thumbnail is already there.
 * Check libgnomeui/gnome-thumbnail.h for actually generating thumbnails in the correct
 * sizes at the correct locations automatically; which would draw in a dependency on gdk-pixbuf.
 */
char * get_thumbnail_path(char *path, char *thumbnail_size, gboolean verbose)
{
    char *file, *md5;
    md5 = get_md5(path);
    file = g_strconcat (md5, ".png", NULL);

    gchar *thumbnail_path = g_build_filename (g_get_user_cache_dir(), "thumbnails", thumbnail_size, file, NULL);

    g_free(md5);
    g_free (file);
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
    gchar *dest_dir = g_build_path("/", g_get_user_data_dir(), "/icons/hicolor/48x48/apps", NULL);;

    if((g_str_has_suffix (icon_path, ".svg")) || (g_str_has_suffix (icon_path, ".svgz"))) {
        g_free(dest_dir);
        dest_dir = g_build_path("/", g_get_user_data_dir(), "/icons/hicolor/scalable/apps/", NULL);
    }
 
    if((g_str_has_suffix (icon_path, ".png")) || (g_str_has_suffix (icon_path, ".xpm"))) {
        
        cairo_surface_t *image;

        int w = 0;
        int h = 0;
        
        if(g_str_has_suffix (icon_path, ".xpm")) {
            // TODO: GdkPixbuf has a convenient way to load XPM data. Then you can call
            // gdk_cairo_set_source_pixbuf() to transfer the data to a Cairo surface.
            fprintf(stderr, "XPM size parsing not yet implemented\n");
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
            fprintf(stderr, "%s has nonstandard size w = %i, h = %i; please fix it\n", icon_path, w, h);
        } else {
            g_free(dest_dir);
            gchar *t = g_strdup_printf("%ix%i", w, h);
            dest_dir = g_build_path("/", g_get_user_data_dir(), "/icons/hicolor/", t, "/apps", NULL);
            g_free(t);
        }
    }
    if(verbose)
        fprintf(stderr, "dest_dir %s\n", dest_dir);
    
    gchar* icon_name = g_path_get_basename(icon_path);
    gchar* icon_dest_path = g_build_path("/", dest_dir, icon_name, NULL);
    g_free(icon_name);

    if(verbose)
        fprintf(stderr, "Move from %s to %s\n", icon_path, icon_dest_path);
    gchar *dirname = g_path_get_dirname(dest_dir);
    if(g_mkdir_with_parents(dirname, 0755))
        fprintf(stderr, "Could not create directory: %s\n", dest_dir);
    g_free(dirname);

    GError *error = NULL;
    GFile *icon_file = g_file_new_for_path(icon_path);
    GFile *target_file = g_file_new_for_path(icon_dest_path);
    if (!g_file_move (icon_file, target_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
        fprintf(stderr, "Error moving file: %s\n", error->message);
        g_clear_error (&error);
    }

    g_free(dest_dir);
    g_free(icon_dest_path);
    g_object_unref(icon_file); 
    g_object_unref(target_file);
}

/* Check if a file is an AppImage. Returns the image type if it is, or -1 if it isn't */
int check_appimage_type(char *path, gboolean verbose)
{
    char buffer[3];
    FILE *f = fopen(path, "rt");
    if (f != NULL)
    {
        /* Check magic bytes at offset 8 */
        fseek(f, 8, SEEK_SET);
        fread(buffer, 1, 3, f);
        fclose(f);
        if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x01)){
            fprintf(stderr, "_________________________\n");
            if(verbose){
                fprintf(stderr, "AppImage type 1\n");
            }
            return 1;
        } else if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x02)){
            fprintf(stderr, "_________________________\n");
            if(verbose){
                fprintf(stderr, "AppImage type 2\n");
            }
            return 2;
        } else {
            if((g_str_has_suffix(path,".AppImage")) || (g_str_has_suffix(path,".appimage"))) {
                fprintf(stderr, "_________________________\n");
                fprintf(stderr, "Blindly assuming AppImage type 1\n");
                fprintf(stderr, "The author of this AppImage should embed the magic bytes, see https://github.com/AppImage/AppImageSpec\n");
                return 1;
            } else {
                if(verbose){
                    fprintf(stderr, "_________________________\n");
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
gchar **squash_get_matching_files(sqfs *fs, char *pattern, gchar *desktop_icon_value_original, char *md5, gboolean verbose) {
    GPtrArray *array = g_ptr_array_new();
    sqfs_traverse trv;
    sqfs_err err = sqfs_traverse_open(&trv, fs, sqfs_inode_root(fs));
    if (err!= SQFS_OK)
        fprintf(stderr, "sqfs_traverse_open error\n");
    while (sqfs_traverse_next(&trv, &err)) {
        if (!trv.dir_end) {
            int r;
            regex_t regex;
            regmatch_t match[2];
            regcomp(&regex, pattern, REG_ICASE | REG_EXTENDED);
            r = regexec(&regex, trv.path, 2, match, 0);
            regfree(&regex);
            sqfs_inode inode;
            if(sqfs_inode_get(fs, &inode, trv.entry.inode))
                fprintf(stderr, "sqfs_inode_get error\n");
            if(r == 0){
                // We have a match
                if(verbose)
                    fprintf(stderr, "squash_get_matching_files found: %s\n", trv.path);
                g_ptr_array_add(array, g_strdup(trv.path));
                gchar *dest = NULL;
                gchar *dest_dirname = NULL;
                gchar *dest_basename = NULL;
                if(inode.base.inode_type == SQUASHFS_REG_TYPE) {
                    if(g_str_has_prefix(trv.path, "usr/share/icons/") || g_str_has_prefix(trv.path, "usr/share/pixmaps/") || (g_str_has_prefix(trv.path, "usr/share/mime/") && g_str_has_suffix(trv.path, ".xml"))){
                        dest_dirname = g_path_get_dirname(replace_str(trv.path, "usr/share", g_get_user_data_dir()));          
                        dest_basename = g_strdup_printf("%s_%s_%s", vendorprefix, md5, g_path_get_basename(trv.path));
                        dest = g_build_path("/", dest_dirname, dest_basename, NULL);
                    }
                    /* According to https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html#install_icons
                     * share/pixmaps is ONLY searched in /usr but not in $XDG_DATA_DIRS and hence $HOME and this seems to be true at least in XFCE */
                    if(g_str_has_prefix (trv.path, "usr/share/pixmaps/")){       
                        dest_basename = g_strdup_printf("%s_%s_%s", vendorprefix, md5, g_path_get_basename(trv.path));
                        dest = g_build_path("/", "/tmp", dest_basename, NULL);
                    }
                    /* Some AppImages only have the icon in their root directory, so we have to get it from there */
                    if((g_str_has_prefix(trv.path, desktop_icon_value_original)) && (! strstr(trv.path, "/")) && ( (g_str_has_suffix(trv.path, ".png")) || (g_str_has_suffix(trv.path, ".xpm")) || (g_str_has_suffix(trv.path, ".svg")) || (g_str_has_suffix(trv.path, ".svgz")))){
                        gchar* ext = get_file_extension(trv.path);
                        dest_basename = g_strdup_printf("%s_%s_%s.%s", vendorprefix, md5, desktop_icon_value_original, ext);
                        dest = g_build_path("/", "/tmp", dest_basename, NULL);
                        g_free(ext);
                    }
                    
                    if(dest){
                        if(verbose)
                            fprintf(stderr, "install: %s\n", dest);
                        
                        gchar *dirname = g_path_get_dirname(dest);
                        if(g_mkdir_with_parents(dirname, 0755))
                            fprintf(stderr, "Could not create directory: %s\n", dirname);

                        g_free(dirname);
                        
                        // Read the file in chunks
                        off_t bytes_already_read = 0;
                        sqfs_off_t bytes_at_a_time = 64*1024;
                        FILE * f;
                        f = fopen (dest, "w+");
                        if (f == NULL){
                            fprintf(stderr, "fopen error\n");
                            break;
                        }
                        while (bytes_already_read < inode.xtra.reg.file_size)
                        {
                            char buf[bytes_at_a_time];
                            if (sqfs_read_range(fs, &inode, (sqfs_off_t) bytes_already_read, &bytes_at_a_time, buf))
                                fprintf(stderr, "sqfs_read_range error\n");
                            fwrite(buf, 1, bytes_at_a_time, f);
                            bytes_already_read = bytes_already_read + bytes_at_a_time;
                        }
                        fclose(f);
                        chmod (dest, 0644);
                        
                        if(verbose)
                            fprintf(stderr, "Installed: %s\n", dest);
                        
                        // If we were unsure about the size of an icon, we temporarily installed
                        // it to /tmp and now move it into the proper place
                        if(g_str_has_prefix (dest, "/tmp/")) {
                            move_icon_to_destination(dest, verbose);
                        } 
                    }
                }
                
                g_free(dest);
                g_free(dest_dirname);
                g_free(dest_basename);
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
    gboolean success = true;
    sqfs_err err = sqfs_traverse_open(&trv, fs, sqfs_inode_root(fs));
    if (err != SQFS_OK)
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

/* Write a modified desktop file to disk that points to the AppImage */
void write_edited_desktop_file(GKeyFile *key_file_structure, char* appimage_path, gchar* desktop_filename, int appimage_type, char *md5, gboolean verbose){
    if(!g_key_file_has_key(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL)){
        fprintf(stderr, "Desktop file has no Exec key\n");
        return;
    }
    g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, appimage_path);
    //gchar *tryexec_path = replace_str(appimage_path," ", "\\ "); // TryExec does not support blanks
    g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, appimage_path);
    
    /* If firejail is on the $PATH, then use it to run AppImages */
    if(g_find_program_in_path ("firejail")){
        char *firejail_exec = NULL;
        firejail_exec = g_strdup_printf("firejail --env=DESKTOPINTEGRATION=appimaged --noprofile --appimage '%s'", appimage_path);
        g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, firejail_exec);

        gchar *firejail_profile_group = "Desktop Action FirejailProfile";
        gchar *firejail_profile_exec = g_strdup_printf("firejail --env=DESKTOPINTEGRATION=appimaged --private --appimage '%s'", appimage_path);
        gchar *firejail_tryexec = "firejail";
        g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_NAME, "Run without sandbox profile");
        g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_EXEC, firejail_profile_exec);
        g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, firejail_tryexec);
        g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, "Actions", "FirejailProfile;");

        g_free(firejail_exec);
        g_free(firejail_profile_group);
        g_free(firejail_profile_exec);
        g_free(firejail_tryexec);
    }
    
    /* Add AppImageUpdate desktop action
     * https://specifications.freedesktop.org/desktop-entry-spec/latest/ar01s10.html 
     * This will only work if AppImageUpdate is on the user's $PATH.
     * TODO: we could have it call this appimaged instance instead of AppImageUpdate and let it
     * figure out how to update the AppImage */
    unsigned long upd_offset = 0;
    unsigned long upd_length = 0;
    if(g_find_program_in_path ("AppImageUpdate")){
        if(appimage_type == 2){
            get_elf_section_offset_and_lenghth(appimage_path, ".upd_info", &upd_offset, &upd_length);
            if(upd_length != 1024)
                fprintf(stderr, "WARNING: .upd_info length is %lu rather than 1024, this might be a bug in the AppImage\n", upd_length);
        }
        if(appimage_type == 1){
            /* If we have a type 1 AppImage, then we hardcode the offset and length */
            upd_offset = 33651; // ISO 9660 Volume Descriptor field
            upd_length = 512; // Might be wrong
        }
        fprintf(stderr, ".upd_info offset: %lu\n", upd_offset);
        fprintf(stderr, ".upd_info length: %lu\n", upd_length);
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

                g_free(appimageupdate_group);
                g_free(appimageupdate_exec);
            }
        }
    }
    
    gchar *icon_path = g_key_file_get_value(key_file_structure, "Desktop Entry", "Icon", NULL);
    gchar *icon = g_path_get_basename(icon_path);

    gchar *icon_with_md5 = g_strdup_printf("%s_%s_%s", vendorprefix, md5, icon);

    g_free(icon_path);
    g_free(icon);

    g_key_file_set_value(key_file_structure, "Desktop Entry", "Icon", icon_with_md5);

    g_free(icon_with_md5);

    /* At compile time, inject VERSION_NUMBER like this:
     * cc ... -DVERSION_NUMBER=\"$(git describe --tags --always --abbrev=7)\" -..
     */
    gchar *generated_by = g_strdup_printf("Generated by appimaged %s", VERSION_NUMBER);
    g_key_file_set_value(key_file_structure, "Desktop Entry", "X-AppImage-Comment", generated_by);
    g_key_file_set_value(key_file_structure, "Desktop Entry", "X-AppImage-Identifier", md5);
    fprintf(stderr, "Installing desktop file\n");

    gchar * file_content = g_key_file_to_data(key_file_structure, NULL, NULL);
    if(verbose)
        fprintf(stderr, "%s", file_content);

    g_free(file_content);
    g_free(generated_by);
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
    gchar *partial_path = NULL;
    partial_path = g_strdup_printf("applications/appimagekit_%s-%s", md5, desktop_filename);
    gchar *destination = NULL;
    destination = g_build_filename(g_get_user_data_dir(), partial_path, NULL);

    /* When appimaged sees itself, then do nothing here */
    if(strcmp ("appimaged.desktop", desktop_filename) == 0) {
        return;
    }

    if(verbose)
        fprintf(stderr, "install: %s\n", destination);
    gchar *dirname = g_path_get_dirname(destination);
    if(g_mkdir_with_parents(dirname, 0755))
        fprintf(stderr, "Could not create directory: %s\n", dirname);
    g_free(dirname);

    // g_key_file_save_to_file(key_file_structure, destination, NULL);
    // g_key_file_save_to_file is too new, only since 2.40
    /* Write config file on disk */
    gsize length;
    gchar *buf = NULL;
    GIOChannel *file;
    buf = g_key_file_to_data(key_file_structure, &length, NULL);
    file = g_io_channel_new_file(destination, "w", NULL);
    g_io_channel_write_chars(file, buf, length, NULL, NULL);
    g_io_channel_shutdown(file, TRUE, NULL);
    g_io_channel_unref(file);
    
    
    /* GNOME shows the icon and name on the desktop file only if it is executable */
    chmod(destination, 0755);
    
    g_free(buf);
    g_free(destination);
    g_free(partial_path);
}

/* Register a type 1 AppImage in the system */
bool appimage_type1_register_in_system(char *path, gboolean verbose)
{
    fprintf(stderr, "ISO9660 based type 1 AppImage\n");
    gchar *desktop_icon_value_original = NULL;
    char *md5 = get_md5(path);
    
    if(verbose)
        fprintf(stderr, "md5 of URI RFC 2396: %s\n", md5);
    
    struct archive *a;
    struct archive_entry *entry;
    int r;
    
    a = archive_read_new();
    archive_read_support_format_iso9660(a);
    if ((r = archive_read_open_filename(a, path, 10240))) {
        fprintf(stderr, "%s", archive_error_string(a));
    }
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
        gchar *filename;
        filename = replace_str(archive_entry_pathname(entry), "./", "");

        /* Get desktop file(s) in the root directory of the AppImage and act on it in one go */
        if((g_str_has_suffix(filename,".desktop") && (NULL == strstr (filename,"/"))))
        {
            fprintf(stderr, "Got root desktop: %s\n", filename);
            int r;
            const void *buff;
            size_t size = 1024*1024;
            int64_t offset = 0;
            r = archive_read_data_block(a, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                return (ARCHIVE_OK);
            if (r != ARCHIVE_OK) {
                fprintf(stderr, "%s", archive_error_string(a));
                break;
            }
            GKeyFile *key_file_structure = g_key_file_new(); // A structure that will hold the information from the desktop file
            gboolean success = g_key_file_load_from_data (key_file_structure, buff, size, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
            if(success){
                gchar *desktop_filename = g_path_get_basename(filename);
                desktop_icon_value_original = g_strdup_printf("%s", g_key_file_get_value(key_file_structure, "Desktop Entry", "Icon", NULL));
                if(verbose)
                    fprintf(stderr, "desktop_icon_value_original: %s\n", desktop_icon_value_original);
                write_edited_desktop_file(key_file_structure, path, desktop_filename, 1, md5, verbose);
            }
            g_key_file_free(key_file_structure);
        }

        gchar *dest = NULL;
        gchar *dest_dirname = NULL;
        gchar *dest_basename = NULL;
        /* Get icon file(s) and act on them in one go */
        
        if(g_str_has_prefix(filename, "usr/share/icons/") || g_str_has_prefix(filename, "usr/share/pixmaps/") || (g_str_has_prefix(filename, "usr/share/mime/") && g_str_has_suffix(filename, ".xml"))){
            dest_dirname = g_path_get_dirname(replace_str(filename, "usr/share", g_get_user_data_dir()));          
            dest_basename = g_strdup_printf("%s_%s_%s", vendorprefix, md5, g_path_get_basename(filename));
            dest = g_build_path("/", dest_dirname, dest_basename, NULL);
        }
        /* According to https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html#install_icons
         * share/pixmaps is ONLY searched in /usr but not in $XDG_DATA_DIRS and hence $HOME and this seems to be true at least in XFCE */
        if(g_str_has_prefix (filename, "usr/share/pixmaps/")){
            dest = g_build_path("/", "/tmp", dest_basename, NULL);
        }
        /* Some AppImages only have the icon in their root directory, so we have to get it from there */
        if(desktop_icon_value_original){
            if((g_str_has_prefix(filename, desktop_icon_value_original)) && (! strstr(filename, "/")) && ( (g_str_has_suffix(filename, ".png")) || (g_str_has_suffix(filename, ".xpm")) || (g_str_has_suffix(filename, ".svg")) || (g_str_has_suffix(filename, ".svgz")))){
                gchar *ext = get_file_extension(filename);
                dest_basename = g_strdup_printf("%s_%s_%s.%s", vendorprefix, md5, desktop_icon_value_original, ext);
                dest = g_build_path("/", "/tmp", dest_basename, NULL);
                g_free(ext);
            }
        }
                    
        if(dest){
        
            if(verbose)
                fprintf(stderr, "install: %s\n", dest);
            gchar *dirname = g_path_get_dirname(dest);
            if(g_mkdir_with_parents(dirname, 0755))
                fprintf(stderr, "Could not create directory: %s\n", dirname);
            g_free(dirname);

            FILE *f;
            f = fopen(dest, "w+");
            
            if (f == NULL){
                fprintf(stderr, "fopen error\n");
                break;
            }
            
            int r;
            const void *buff;
            size_t size;
            int64_t offset;


            for (;;) {
                r = archive_read_data_block(a, &buff, &size, &offset);
                if (r == ARCHIVE_EOF)
                    break;
                if (r != ARCHIVE_OK) {
                    fprintf(stderr, "%s\n", archive_error_string(a));
                    break;
                }
                fwrite(buff, 1, size, f);
            }
            
            fclose(f);
            chmod (dest, 0644);

            
            if(verbose)
                fprintf(stderr, "Installed: %s\n", dest);
            
            // If we were unsure about the size of an icon, we temporarily installed
            // it to /tmp and now move it into the proper place
            if(g_str_has_prefix (dest, "/tmp/")) {
                move_icon_to_destination(dest, verbose);
            } 
        }

        g_free(dest);
        g_free(dest_dirname);
        g_free(dest_basename);
    }
    archive_read_close(a);
    archive_read_free(a);
    set_executable(path, verbose);
    return TRUE;
}

/* Register a type 2 AppImage in the system */
bool appimage_type2_register_in_system(char *path, gboolean verbose)
{
    fprintf(stderr, "squashfs based type 2 AppImage\n");
    long unsigned int fs_offset; // The offset at which a squashfs image is expected
    char *md5 = get_md5(path);
    GKeyFile *key_file_structure = g_key_file_new(); // A structure that will hold the information from the desktop file
    gchar *desktop_icon_value_original = "iDoNotMatchARegex"; // FIXME: otherwise the regex does weird stuff in the first run
    if(verbose)
        fprintf(stderr, "md5 of URI RFC 2396: %s\n", md5);
    fs_offset = get_elf_size(path);
    if(verbose)
        fprintf(stderr, "fs_offset: %lu\n", fs_offset);
    sqfs fs;
    sqfs_err err = sqfs_open_image(&fs, path, fs_offset);
    if (err != SQFS_OK){
        fprintf(stderr, "sqfs_open_image error: %s\n", path);
        return FALSE;
    } else {
        if(verbose)
            fprintf(stderr, "sqfs_open_image: %s\n", path);
    }
    
    /* TOOO: Change so that only one run of squash_get_matching_files is needed in total,
     * this should hopefully improve performance */
    
    /* Get desktop file(s) in the root directory of the AppImage */
    gchar **str_array = squash_get_matching_files(&fs, "(^[^/]*?.desktop$)", desktop_icon_value_original, md5, verbose); // Only in root dir
    // gchar **str_array = squash_get_matching_files(&fs, "(^.*?.desktop$)", md5, verbose); // Not only there
    /* Work trough the NULL-terminated array of strings */
    for (int i=0; str_array[i]; ++i) {
        fprintf(stderr, "Got root desktop: %s\n", str_array[i]);
        gboolean success = g_key_file_load_from_squash(&fs, str_array[i], key_file_structure, verbose);
        if(success){
            gchar *desktop_filename = g_path_get_basename(str_array[i]);
            
            desktop_icon_value_original = g_strdup_printf("%s", g_key_file_get_value(key_file_structure, "Desktop Entry", "Icon", NULL));
            if(verbose)
                fprintf(stderr, "desktop_icon_value_original: %s\n", desktop_icon_value_original);
            write_edited_desktop_file(key_file_structure, path, desktop_filename, 2, md5, verbose);
        }
        g_key_file_free(key_file_structure);
        
    }
    /* Free the NULL-terminated array of strings and its contents */
    g_strfreev(str_array);
    
    /* Get relevant  file(s) */
    gchar **str_array2 = squash_get_matching_files(&fs, "(^usr/share/(icons|pixmaps)/.*.(png|svg|svgz|xpm)$|^.DirIcon$|^usr/share/mime/packages/.*.xml$|^usr/share/appdata/.*metainfo.xml$|^[^/]*?.(png|svg|svgz|xpm)$)", desktop_icon_value_original, md5, verbose);
    
    /* Free the NULL-terminated array of strings and its contents */
    g_strfreev(str_array2);
    
    /* The above also gets AppStream metainfo file(s), TODO: Check if the id matches and do something with them*/
    
    set_executable(path, verbose);
    
    return TRUE;
}

/* Register an AppImage in the system */
int appimage_register_in_system(char *path, gboolean verbose)
{
    if((g_str_has_suffix(path, ".part")) || (g_str_has_suffix(path, ".tmp")) || (g_str_has_suffix(path, ".download")) || (g_str_has_suffix(path, ".zs-old")) || (g_str_has_suffix(path, ".~")))
        return 0;
    int type = check_appimage_type(path, verbose);
    if(type == 1 || type == 2){
        fprintf(stderr, "\n");
        fprintf(stderr, "-> REGISTER %s\n", path);
    }
    /* TODO: Generate thumbnails.
     * Generating proper thumbnails involves more than just copying images out of the AppImage,
     * including checking if the thumbnail already exists and if it's valid 
     * and writing attributes into the thumbnail, see
     * https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#CREATION */
    if(verbose)
        fprintf(stderr, "get_thumbnail_path: %s\n", get_thumbnail_path(path, "normal", verbose));
    
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
        gchar *path_to_be_deleted = g_strdup_printf("%s/%s", name, entry->d_name);
        if (entry->d_type == DT_DIR) {
            if ( strcmp(".", entry->d_name) != 0 && strcmp("..", entry->d_name) != 0 )
                unregister_using_md5_id(path_to_be_deleted, level + 1, md5, verbose);
        } else
        if (strstr(entry->d_name, vendorprefix) != NULL &&
            strstr(entry->d_name, md5)  != NULL ) {
            if(g_file_test(path_to_be_deleted, G_FILE_TEST_IS_REGULAR)){
                g_unlink(path_to_be_deleted);
                if(verbose)
                    fprintf(stderr, "deleted: %s\n", path_to_be_deleted);
            }
        }
        g_free(path_to_be_deleted);
    } while ((entry = readdir(dir)) != NULL);
    closedir(dir);
}


/* Unregister an AppImage in the system */
int appimage_unregister_in_system(char *path, gboolean verbose)
{
    char *md5 = get_md5(path);
    
    /* The file is already gone by now, so we can't determine its type anymore */
    fprintf(stderr, "_________________________\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "-> UNREGISTER %s\n", path);
    /* Could use gnome_desktop_thumbnail_factory_lookup instead of the next line */
    
    /* Delete the thumbnails if they exist */
    delete_thumbnail(path, "normal", verbose); // 128x128
    delete_thumbnail(path, "large", verbose); // 256x256
    
    unregister_using_md5_id(g_get_user_data_dir(), 0, md5, verbose);
    
    return 0;
}
