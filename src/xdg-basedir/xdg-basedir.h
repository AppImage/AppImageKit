#ifndef XDG_BASEDIR_H
#define XDG_BASEDIR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Get user's home directory. Convenience wrapper for getenv("HOME").
 * Returns a freshly allocated char array that must be free'd after usage.
 */
char* user_home();

/*
 * Get XDG config home directory using $XDG_CONFIG_HOME environment variable.
 * Falls back to default value ~/.config if environment variable is not set.
 * Returns a freshly allocated char array that must be free'd after usage.
 */
char* xdg_config_home();

/*
 * Get XDG data home directory using $XDG_DATA_HOME environment variable.
 * Falls back to default value ~/.local/share if environment variable is not set.
 * Returns a freshly allocated char array that must be free'd after usage.
 */
char* xdg_data_home();

/*
 * Get XDG cache home directory using $XDG_CACHE_HOME environment variable.
 * Falls back to default value ~/.cache if environment variable is not set.
 * Returns a freshly allocated char array that must be free'd after usage.
 */
char* xdg_cache_home();


#ifdef __cplusplus
}
#endif

#endif /* XDG_BASEDIR_H */
