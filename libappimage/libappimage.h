#ifndef AppImage
#define AppImage


#include <glib.h>
#include <squashfuse.h>

extern "C" {
    void set_executable(char const *path, gboolean verbose);

    /* Search and replace on a string, this really should be in Glib
    * https://mail.gnome.org/archives/gtk-list/2012-February/msg00005.html */
    gchar *replace_str(const gchar *src, const gchar *find, const gchar *replace);

    /* Return the md5 hash constructed according to
    * https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#THUMBSAVE
    * This can be used to identify files that are related to a given AppImage at a given location */
    char *get_md5(char const *path);

    /* Return the path of the thumbnail regardless whether it already exists; may be useful because
    * G*_FILE_ATTRIBUTE_THUMBNAIL_PATH only exists if the thumbnail is already there.
    * Check libgnomeui/gnome-thumbnail.h for actually generating thumbnails in the correct
    * sizes at the correct locations automatically; which would draw in a dependency on gdk-pixbuf.
    */
    char *get_thumbnail_path(char *path, char *thumbnail_size, gboolean verbose);

    /* Move an icon file to the path where a given icon can be installed in $HOME.
    * This is needed because png and xpm icons cannot be installed in a generic
    * location but are only picked up in directories that have the size of 
    * the icon as part of their directory name, as specified in the theme.index
    * See https://github.com/AppImage/AppImageKit/issues/258
    */

    void move_icon_to_destination(gchar *icon_path, gboolean verbose);

    /* Check if a file is an AppImage. Returns the image type if it is, or -1 if it isn't */
    int check_appimage_type(char *path, gboolean verbose);

    /* Get filename extension */
    gchar *get_file_extension(const gchar *filename);

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
    gchar **squash_get_matching_files(sqfs *fs, char *pattern, gchar *desktop_icon_value_original, char *md5, gboolean verbose);

    /* Loads a desktop file from squashfs into an empty GKeyFile structure.
    * FIXME: Use sqfs_lookup_path() instead of g_key_file_load_from_squash()
    * should help for performance. Please submit a pull request if you can
    * get it to work.
    */
    gboolean g_key_file_load_from_squash(sqfs *fs, char *path, GKeyFile *key_file_structure, gboolean verbose);

    /* Write a modified desktop file to disk that points to the AppImage */
    void write_edited_desktop_file(GKeyFile *key_file_structure, char *appimage_path, gchar *desktop_filename, int appimage_type, char *md5, gboolean verbose);

    /* Register a type 1 AppImage in the system */
    bool appimage_type1_register_in_system(char *path, gboolean verbose);

    /* Register a type 2 AppImage in the system */
    bool appimage_type2_register_in_system(char *path, gboolean verbose);

    /* Register an AppImage in the system */
    int appimage_register_in_system(char *path, gboolean verbose);

    /* Delete the thumbnail for a given file and size if it exists */
    void delete_thumbnail(char *path, char *size, gboolean verbose);

    /* Recursively delete files in path and subdirectories that contain the given md5
    */
    void unregister_using_md5_id(const char *name, int level, char *md5, gboolean verbose);

    /* Unregister an AppImage in the system */
    int appimage_unregister_in_system(char *path, gboolean verbose);
}

#endif // !AppImage