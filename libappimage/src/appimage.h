#ifndef AppImage
#define AppImage

extern "C" {
    /* Return the md5 hash constructed according to
    * https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#THUMBSAVE
    * This can be used to identify files that are related to a given AppImage at a given location */
    char *get_md5(char const *path);

    /* Check if a file is an AppImage. Returns the image type if it is, or -1 if it isn't */
    int check_appimage_type( const char *path, int verbose);

    /* Register a type 1 AppImage in the system */
    bool appimage_type1_register_in_system(const char *path, int verbose);

    /* Register a type 2 AppImage in the system */
    bool appimage_type2_register_in_system(const char *path, int verbose);

    /* Register an AppImage in the system */
    int appimage_register_in_system(const char *path, int verbose);

    /* Unregister an AppImage in the system */
    int appimage_unregister_in_system(const char *path, int verbose);
}

#endif // !AppImage