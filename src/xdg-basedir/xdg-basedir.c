#include "xdg-basedir.h"
#include <string.h>
#include <stdlib.h>

char* user_home() {
    return strdup(getenv("HOME"));
}

char* xdg_config_home()  {
    char* config_home = getenv("XDG_CONFIG_HOME");

    if (config_home == NULL) {
        char* home = user_home();
        static const char const* suffix = "/.config";

        config_home = calloc(strlen(home) + strlen(suffix) + 1, sizeof(char));

        strcpy(config_home, home);
        strcat(config_home, suffix);

        free(home);

        return config_home;
    } else {
        return strdup(config_home);
    }
}

char* xdg_data_home() {
    char* data_home = getenv("XDG_DATA_HOME");

    if (data_home == NULL) {
        char* home = user_home();
        static const char const* suffix = "/.local/share";

        data_home = calloc(strlen(home) + strlen(suffix) + 1, sizeof(char));

        strcpy(data_home, home);
        strcat(data_home, suffix);

        free(home);

        return data_home;
    } else {
        return strdup(data_home);
    }
}

char* xdg_cache_home() {
    char* cache_home = getenv("XDG_CACHE_HOME");

    if (cache_home == NULL) {
        char* home = user_home();
        static const char const* suffix = "/.cache";

        cache_home = calloc(strlen(home) + strlen(suffix) + 1, sizeof(char));

        strcpy(cache_home, home);
        strcat(cache_home, suffix);

        free(home);

        return cache_home;
    } else {
        return strdup(cache_home);
    }
}
