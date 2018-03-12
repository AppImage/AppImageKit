#pragma once

#include <glib.h>
#include <squashfuse.h>

/* AppImage generic handler calback to be used in algorithms */
typedef void (*traverse_cb)(void *handler, void *entry_data, void *user_data);

/* AppImage generic handler to be used in algorithms */
struct appimage_handler
{
    const gchar *path;
    char* (*get_file_name) (struct appimage_handler *handler, void *entry);
    void (*extract_file) (struct appimage_handler *handler, void *entry, const char *target);

    void (*traverse)(struct appimage_handler *handler, traverse_cb command, void *user_data);

    void *cache;
    bool is_open;
} typedef appimage_handler;

void extract_file_following_symlinks(const gchar* appimage_file_path, const char* file_path,
                                     const char* target_dir);

void extract_appimage_icon(appimage_handler* h, gchar* target);

void extract_appimage_icon_command(void* handler_data, void* entry_data, void* user_data);

void extract_appimage_file(appimage_handler* h, const char* path, const char* destination);

void extract_appimage_file_command(void* handler_data, void* entry_data, void* user_data);

void move_file(const char* source, const char* target);

appimage_handler create_appimage_handler(const char* const path);

appimage_handler appimage_type_2_create_handler();

void appimage_type2_extract_file(appimage_handler* handler, void* data, const char* target);

void appimage_type2_extract_file_following_symlinks(sqfs* fs, sqfs_inode* inode, const char* target);

void appimage_type2_extract_regular_file(sqfs* fs, sqfs_inode* inode, const char* target);

void appimage_type2_extract_symlink(sqfs* fs, sqfs_inode* inode, const char* target);

void appimage_type2_extract_symlink(sqfs* fs, sqfs_inode* inode, const char* target);

char* appimage_type2_get_file_name(appimage_handler* handler, void* data);

void appimage_type2_traverse(appimage_handler* handler, traverse_cb command, void* command_data);

void appimage_type2_close(appimage_handler* handler);

void appimage_type2_open(appimage_handler* handler);

appimage_handler appimage_type_1_create_handler();

void appimage_type1_extract_file(appimage_handler* handler, void* data, const char* target);

char* appimage_type1_get_file_name(appimage_handler* handler, void* data);

void appimage_type1_traverse(appimage_handler* handler, traverse_cb command, void* command_data);

void appimage_type1_close(appimage_handler* handler);

void appimage_type1_open(appimage_handler* handler);

void dummy_extract_file(struct appimage_handler* handler, void* data, char* target);

char* dummy_get_file_name(appimage_handler* handler, void* data);

void dummy_traverse_func(appimage_handler* handler, traverse_cb command, void* data);

void mk_base_dir(const char* target);

bool is_handler_valid(const appimage_handler* handler);

int appimage_unregister_in_system(char* path, gboolean verbose);

void unregister_using_md5_id(const char* name, int level, char* md5, gboolean verbose);

void delete_thumbnail(char* path, char* size, gboolean verbose);

int appimage_register_in_system(char* path, gboolean verbose);

void create_thumbnail(const gchar* appimage_file_path, gboolean verbose);

void create_thumbnail(const gchar* appimage_file_path, gboolean verbose);

bool appimage_type2_register_in_system(char* path, gboolean verbose);

bool appimage_type1_register_in_system(const char* path, gboolean verbose);

bool write_edited_desktop_file(GKeyFile* key_file_structure, const char* appimage_path, gchar* desktop_filename,
                               int appimage_type, char* md5, gboolean verbose);

gboolean g_key_file_load_from_squash(sqfs* fs, char* path, GKeyFile* key_file_structure, gboolean verbose);

gchar** squash_get_matching_files(sqfs* fs, char* pattern, gchar* desktop_icon_value_original, char* md5, gboolean verbose);

void squash_extract_inode_to_file(sqfs* fs, sqfs_inode* inode, const gchar* dest);

int check_appimage_type(const char* path, gboolean verbose);

void move_icon_to_destination(gchar* icon_path, gboolean verbose);

char* get_thumbnail_path(const char* path, char* thumbnail_size, gboolean verbose);

char* get_md5(const char* path);

gchar* replace_str(const gchar* src, const gchar* find, const gchar* replace);

void set_executable(const char* path, gboolean verbose);

extern char* vendorprefix;
