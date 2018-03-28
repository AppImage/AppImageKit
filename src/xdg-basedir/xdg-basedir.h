#ifndef XDG_BASEDIR_H
#define XDG_BASEDIR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Convenience wrapper. Result must be free'd.
 */
char* user_home();

/*
 *
 */
char* xdg_config_home();

/*
 *
 */
char* xdg_data_home();

/*
 *
 */
char* xdg_cache_home();


#ifdef __cplusplus
}
#endif

#endif /* XDG_BASEDIR_H */
