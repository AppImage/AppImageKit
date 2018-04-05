#include "appimage/appimage.h"

#include <unistd.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <fstream>

#include <glib/gstdio.h>

#include <squashfuse.h>
#include <gtest/gtest.h>

#include "fixtures.h"


namespace AppImageTests {

    class LibAppImageTest : public AppImageKitTest {
    protected:
        void rm_file(const std::string &path) {
            g_remove(path.c_str());
        }

        bool areIntegrationFilesDeployed(const std::string &path) {
            gchar *sum = appimage_get_md5(path.c_str());

            GDir *dir;
            GError *error = NULL;
            const gchar *filename = NULL;

            char *data_home = xdg_data_home();
            char *apps_path = g_strconcat(data_home, "/applications/", NULL);
            free(data_home);

            bool found = false;
            dir = g_dir_open(apps_path, 0, &error);
            if (dir != NULL) {
                while ((filename = g_dir_read_name(dir))) {
                    gchar* m = g_strrstr(filename, sum);

                    if (m != NULL)
                        found = true;
                }
                g_dir_close(dir);
            }
            g_free(apps_path);
            g_free(sum);
            return found;
        }
    };

    TEST_F(LibAppImageTest, appimage_get_type_invalid) {
        ASSERT_EQ(appimage_get_type("/tmp", false), -1);
    }

    TEST_F(LibAppImageTest, appimage_get_type_1) {
        ASSERT_EQ(appimage_get_type(appImage_type_1_file_path.c_str(), false), 1);
    }

    TEST_F(LibAppImageTest, appimage_get_type_2) {
        ASSERT_EQ(appimage_get_type(appImage_type_2_file_path.c_str(), false), 2);
    }

    TEST_F(LibAppImageTest, appimage_register_in_system_with_type1) {
        ASSERT_EQ(appimage_register_in_system(appImage_type_1_file_path.c_str(), false), 0);

        ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_1_file_path));

        appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
    }

    TEST_F(LibAppImageTest, appimage_register_in_system_with_type2) {
        ASSERT_EQ(appimage_register_in_system(appImage_type_2_file_path.c_str(), false), 0);

        ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_2_file_path));

        appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);
    }

    TEST_F(LibAppImageTest, appimage_type1_register_in_system) {
        ASSERT_TRUE(appimage_type1_register_in_system(appImage_type_1_file_path.c_str(), false));

        ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_1_file_path));

        appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
    }

    TEST_F(LibAppImageTest, appimage_type2_register_in_system) {
        EXPECT_TRUE(appimage_type2_register_in_system(appImage_type_2_file_path.c_str(), false));

        EXPECT_TRUE(areIntegrationFilesDeployed(appImage_type_2_file_path));
        appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);
    }

    TEST_F(LibAppImageTest, appimage_unregister_in_system) {
        ASSERT_FALSE(areIntegrationFilesDeployed(appImage_type_1_file_path));
        ASSERT_FALSE(areIntegrationFilesDeployed(appImage_type_2_file_path));
    }

    TEST_F(LibAppImageTest, appimage_get_md5) {
        std::string pathToTestFile = "/some/fixed/path";

        std::string expected = "972f4824b8e6ea26a55e9af60a285af7";

        gchar *sum = appimage_get_md5(pathToTestFile.c_str());
        EXPECT_EQ(sum, expected);
        g_free(sum);

        unlink(pathToTestFile.c_str());
    }

    TEST_F(LibAppImageTest, get_md5_invalid_file_path) {
        std::string expected = "";
        gchar *sum = appimage_get_md5("");

        int res = g_strcmp0(expected.c_str(), sum);
        ASSERT_EQ(res, 0);
    }

    TEST_F(LibAppImageTest, create_thumbnail_appimage_type_1) {
        appimage_create_thumbnail(appImage_type_1_file_path.c_str(), false);

        gchar *sum = appimage_get_md5(appImage_type_1_file_path.c_str());

        char *cache_home = xdg_cache_home();
        std::string path = std::string(cache_home)
                           + "/thumbnails/normal/"
                           + std::string(sum) + ".png";

        g_free(cache_home);
        g_free(sum);

        ASSERT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_EXISTS));

        // Clean
        rm_file(path);
    }

    TEST_F(LibAppImageTest, create_thumbnail_appimage_type_2) {
        appimage_create_thumbnail(appImage_type_2_file_path.c_str(), false);

        gchar *sum = appimage_get_md5(appImage_type_2_file_path.c_str());

        char* cache_home = xdg_cache_home();
        std::string path = std::string(cache_home)
                           + "/thumbnails/normal/"
                           + std::string(sum) + ".png";

        g_free(cache_home);
        g_free(sum);

        ASSERT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_EXISTS));

        // Clean
        rm_file(path);
    }

    TEST_F(LibAppImageTest, appimage_extract_file_following_symlinks) {
        std::string target_path = tempDir + "test_libappimage_tmp_file";
        appimage_extract_file_following_symlinks(appImage_type_2_file_path.c_str(), "echo.desktop",
                                                 target_path.c_str());

        const char expected[] = ("[Desktop Entry]\n"
                "Version=1.0\n"
                "Type=Application\n"
                "Name=Echo\n"
                "Comment=Just echo.\n"
                "Exec=echo %F\n"
                "Icon=utilities-terminal\n");

        ASSERT_TRUE(g_file_test(target_path.c_str(), G_FILE_TEST_EXISTS));

        std::ifstream file(target_path.c_str(), std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(static_cast<unsigned long>(size));
        if (file.read(buffer.data(), size))
            ASSERT_TRUE(strncmp(expected, buffer.data(), strlen(expected)) == 0);
        else
            FAIL();

        // Clean
        remove(target_path.c_str());
    }

    bool test_appimage_is_registered_in_system(const std::string &pathToAppImage, bool integrateAppImage) {
        if (integrateAppImage) {
            EXPECT_EQ(appimage_register_in_system(pathToAppImage.c_str(), false), 0);
        }

        return appimage_is_registered_in_system(pathToAppImage.c_str());
    }

    TEST_F(LibAppImageTest, appimage_is_registered_in_system) {
        // make sure tested AppImages are not registered
        appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
        appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);

        // if the test order is false -> true, cleanup isn't necessary

        // type 1 tests
        EXPECT_FALSE(test_appimage_is_registered_in_system(appImage_type_1_file_path, false));
        EXPECT_TRUE(test_appimage_is_registered_in_system(appImage_type_1_file_path, true));

        // type 2 tests
        EXPECT_FALSE(test_appimage_is_registered_in_system(appImage_type_2_file_path, false));
        EXPECT_TRUE(test_appimage_is_registered_in_system(appImage_type_2_file_path, true));

        // cleanup
        appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
        appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);
    }

    TEST_F(LibAppImageTest, appimage_list_files_false_appimage) {

        char **files = appimage_list_files("/bin/ls");

        char *expected[] = {NULL};

        int i = 0;
        for (; files[i] != NULL && expected[i] != NULL; i++)
            EXPECT_STREQ(files[i], expected[i]);

        appimage_string_list_free(files);

        if (i != 0)
            FAIL();
    }

    TEST_F(LibAppImageTest, appimage_list_files_type_1) {

        char **files = appimage_list_files(appImage_type_1_file_path.c_str());

        char *expected[] = {
                (char *) "AppImageExtract.desktop",
                (char *) ".DirIcon",
                (char *) "AppImageExtract.png",
                (char *) "usr/bin/appimageextract",
                (char *) "usr/bin/xorriso",
                (char *) "usr/lib/libisofs.so.6",
                (char *) "usr/lib/libisoburn.so.1",
                (char *) "usr/lib/libburn.so.4",
                (char *) "AppRun",
                NULL};

        int i = 0;
        for (; files[i] != NULL && expected[i] != NULL; i++)
            EXPECT_STREQ(files[i], expected[i]);

        appimage_string_list_free(files);
        if (i != 9)
            FAIL();
    }

    TEST_F(LibAppImageTest, appimage_list_files_type_2) {

        char **files = appimage_list_files(appImage_type_2_file_path.c_str());

        char *expected[] = {
                (char *) ".DirIcon",
                (char *) "AppRun",
                (char *) "echo.desktop",
                (char *) "usr",
                (char *) "usr/bin",
                (char *) "usr/bin/echo",
                (char *) "utilities-terminal.svg",
                NULL};

        int i = 0;
        for (; files[i] != NULL && expected[i] != NULL; i++)
            EXPECT_STREQ(files[i], expected[i]);

        appimage_string_list_free(files);
        if (i != 7)
            FAIL();
    }

    TEST_F(LibAppImageTest, test_appimage_registered_desktop_file_path_not_registered) {
        EXPECT_TRUE(appimage_registered_desktop_file_path(appImage_type_1_file_path.c_str(), NULL, false) == NULL);
        EXPECT_TRUE(appimage_registered_desktop_file_path(appImage_type_2_file_path.c_str(), NULL, false) == NULL);
    }

    TEST_F(LibAppImageTest, test_appimage_registered_desktop_file_path_type1) {
        EXPECT_TRUE(appimage_type1_register_in_system(appImage_type_1_file_path.c_str(), false));

        char* desktop_file_path = appimage_registered_desktop_file_path(appImage_type_1_file_path.c_str(), NULL, false);

        EXPECT_TRUE(desktop_file_path != NULL);

        free(desktop_file_path);
    }

    TEST_F(LibAppImageTest, test_appimage_registered_desktop_file_path_type2) {
        EXPECT_TRUE(appimage_type2_register_in_system(appImage_type_2_file_path.c_str(), false));

        char* desktop_file_path = appimage_registered_desktop_file_path(appImage_type_2_file_path.c_str(), NULL, false);

        EXPECT_TRUE(desktop_file_path != NULL);

        free(desktop_file_path);
    }

    TEST_F(LibAppImageTest, test_appimage_registered_desktop_file_path_type1_precalculated_md5) {
        EXPECT_TRUE(appimage_type1_register_in_system(appImage_type_1_file_path.c_str(), false));

        char* md5 = appimage_get_md5(appImage_type_1_file_path.c_str());
        char* desktop_file_path = appimage_registered_desktop_file_path(appImage_type_1_file_path.c_str(), md5, false);
        free(md5);

        EXPECT_TRUE(desktop_file_path != NULL);

        free(desktop_file_path);
    }

    TEST_F(LibAppImageTest, test_appimage_registered_desktop_file_path_type2_precalculated_md5) {
        EXPECT_TRUE(appimage_type2_register_in_system(appImage_type_2_file_path.c_str(), false));

        char* md5 = appimage_get_md5(appImage_type_2_file_path.c_str());
        char* desktop_file_path = appimage_registered_desktop_file_path(appImage_type_2_file_path.c_str(), md5, false);
        free(md5);

        EXPECT_TRUE(desktop_file_path != NULL);

        free(desktop_file_path);
    }

    TEST_F(LibAppImageTest, test_appimage_registered_desktop_file_path_type1_wrong_md5) {
        EXPECT_TRUE(appimage_type1_register_in_system(appImage_type_1_file_path.c_str(), false));

        char* md5 = strdup("abcdefg");
        char* desktop_file_path = appimage_registered_desktop_file_path(appImage_type_1_file_path.c_str(), md5, false);
        free(md5);

        EXPECT_TRUE(desktop_file_path == NULL);

        free(desktop_file_path);
    }

    TEST_F(LibAppImageTest, test_appimage_registered_desktop_file_path_type2_wrong_md5) {
        EXPECT_TRUE(appimage_type2_register_in_system(appImage_type_2_file_path.c_str(), false));

        char* md5 = strdup("abcdefg");
        char* desktop_file_path = appimage_registered_desktop_file_path(appImage_type_2_file_path.c_str(), md5, false);
        free(md5);

        EXPECT_TRUE(desktop_file_path == NULL);

        free(desktop_file_path);
    }

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
