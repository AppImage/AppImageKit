// system headers
#include <climits>
#include <string>

// library headers
#include <gtest/gtest.h>

// local headers
#include "xdg-basedir.h"

bool compareStrings(const char* str1, const char* str2) {
    if (str1 == NULL || str2 == NULL)
        return false;

    return strcmp(str1, str2) == 0;
}

TEST(xdg_basedir_test, test_user_home_default_value) {
    char* home = user_home();
    EXPECT_PRED2(compareStrings, home, getenv("HOME"));
    free(home);
}

TEST(xdg_basedir_test, test_user_home_custom_value) {
    char* oldValue = strdup(getenv("HOME"));
    setenv("HOME", "ABCDEFG", true);

    char* currentValue = user_home();
    EXPECT_PRED2(compareStrings, currentValue, getenv("HOME"));
    EXPECT_PRED2(compareStrings, currentValue, "ABCDEFG");
    free(currentValue);

    setenv("HOME", oldValue, true);
    free(oldValue);
}

TEST(xdg_basedir_test, test_xdg_data_home_default_value) {
    // make sure env var is not set, to force function to use default value
    char* oldValue;

    if ((oldValue = getenv("XDG_DATA_HOME")) != NULL) {
        unsetenv("XDG_DATA_HOME");
    }

    char* currentValue = xdg_data_home();

    // too lazy to calculate size
    char* expectedValue = static_cast<char*>(malloc(PATH_MAX));
    strcpy(expectedValue, getenv("HOME"));
    strcat(expectedValue, "/.local/share");

    EXPECT_PRED2(compareStrings, currentValue, expectedValue);

    free(expectedValue);
    free(currentValue);

    if (oldValue != NULL) {
        setenv("XDG_DATA_HOME", oldValue, true);
        free(oldValue);
    }
}

TEST(xdg_basedir_test, test_xdg_data_home_custom_value) {
    char* oldValue = getenv("XDG_DATA_HOME");
    setenv("XDG_DATA_HOME", "HIJKLM", true);

    char* currentValue = xdg_data_home();
    EXPECT_PRED2(compareStrings, currentValue, "HIJKLM");
    free(currentValue);

    if (oldValue != NULL) {
        setenv("XDG_DATA_HOME", oldValue, true);
        free(oldValue);
    } else {
        unsetenv("XDG_DATA_HOME");
    }
}

TEST(xdg_basedir_test, test_xdg_config_home_default_value) {
    // make sure env var is not set, to force function to use default value
    char* oldValue;

    if ((oldValue = getenv("XDG_CONFIG_HOME")) != NULL) {
        unsetenv("XDG_CONFIG_HOME");
    }

    char* currentValue = xdg_config_home();

    // too lazy to calculate size
    char* expectedValue = static_cast<char*>(malloc(PATH_MAX));
    strcpy(expectedValue, getenv("HOME"));
    strcat(expectedValue, "/.config");

    EXPECT_PRED2(compareStrings, currentValue, expectedValue);

    free(expectedValue);
    free(currentValue);

    if (oldValue != NULL) {
        setenv("XDG_CONFIG_HOME", oldValue, true);
        free(oldValue);
    }
}

TEST(xdg_basedir_test, test_xdg_config_home_custom_value) {
    char* oldValue = getenv("XDG_CONFIG_HOME");
    setenv("XDG_CONFIG_HOME", "NOPQRS", true);

    char* currentValue = xdg_config_home();
    EXPECT_PRED2(compareStrings, currentValue, "NOPQRS");
    free(currentValue);

    if (oldValue != NULL) {
        setenv("XDG_CONFIG_HOME", oldValue, true);
        free(oldValue);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
}

TEST(xdg_basedir_test, test_xdg_cache_home_default_value) {
    // make sure env var is not set, to force function to use default value
    char* oldValue;

    if ((oldValue = getenv("XDG_CACHE_HOME")) != NULL) {
        unsetenv("XDG_CACHE_HOME");
    }

    char* currentValue = xdg_cache_home();

    // too lazy to calculate size
    char* expectedValue = static_cast<char*>(malloc(PATH_MAX));
    strcpy(expectedValue, getenv("HOME"));
    strcat(expectedValue, "/.cache");

    EXPECT_PRED2(compareStrings, currentValue, expectedValue);

    free(expectedValue);
    free(currentValue);

    if (oldValue != NULL) {
        setenv("XDG_CACHE_HOME", oldValue, true);
        free(oldValue);
    }
}

TEST(xdg_basedir_test, test_xdg_cache_home_custom_value) {
    char* oldValue = getenv("XDG_CACHE_HOME");
    setenv("XDG_CACHE_HOME", "TUVWXY", true);

    char* currentValue = xdg_cache_home();
    EXPECT_PRED2(compareStrings, currentValue, "TUVWXY");
    free(currentValue);

    if (oldValue != NULL) {
        setenv("XDG_CACHE_HOME", oldValue, true);
        free(oldValue);
    } else {
        unsetenv("XDG_CACHE_HOME");
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
