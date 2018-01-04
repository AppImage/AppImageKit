#include "appimage/appimage.h"

#include <unistd.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <fstream>

#include <glib/gstdio.h>

#include <squashfuse.h>
#include <gtest/gtest.h>


namespace AppImageTests
{

class AppImageTest : public testing::Test
{
  protected:
    char tests_dir[250];
    bool tests_dir_created = false;
    std::string test_file_content;
    std::string appImage_type_1_file_path;
    std::string appImage_type_2_file_path;

    virtual void SetUp()
    {
        test_file_content = "Hello World\n";
        createTestsDir();

        appImage_type_1_file_path = std::string(TEST_DATA_DIR) + "/AppImageExtract_6-x86_64.AppImage";
        appImage_type_2_file_path = std::string(TEST_DATA_DIR) + "/Echo-x86_64.AppImage";
    }

    virtual void TearDown()
    {
        removeTestsDir();
    }

    inline void createTestsDir()
    {
        sprintf(tests_dir, "/tmp/appimagelib_tests_dir_%d/", rand());

        int result = mkdir(tests_dir, 0700);
        tests_dir_created = !result;
    }

    inline void removeTestsDir()
    {
        rmdir(tests_dir);
    }

    std::string build_test_file_path(const std::string& name)
    {
        return tests_dir + name;
    }

    void mk_file(const std::string& path)
    {

        g_file_set_contents(path.c_str(),
                            test_file_content.c_str(),
                            test_file_content.size(),
                            0);
    }

    void rm_file(const std::string& path)
    {
        g_remove(path.c_str());
    }

    bool areIntegrationFilesDeployed(const std::string& path)
    {
        gchar *sum = get_md5(path.c_str());

        GDir *dir;
        GError *error;
        const gchar *filename;

        char *apps_path = g_strconcat(g_get_user_data_dir(), "/applications/", NULL);

        bool found = false;
        dir = g_dir_open(apps_path, 0, &error);
        while ((filename = g_dir_read_name(dir)))
        {
            gchar *m = g_strrstr(filename, sum);

            if (m != NULL)
                found = true;
        }
        g_dir_close(dir);
        g_free(apps_path);
        g_free(sum);
        return found;
    }
};

TEST_F(AppImageTest, check_appimage_type_invalid)
{
    int t = check_appimage_type("/tmp", 0);
    ASSERT_EQ(t, -1);
}

TEST_F(AppImageTest, check_appimage_type_1)
{
    int t = check_appimage_type(appImage_type_1_file_path.c_str(), 0);
    ASSERT_EQ(t, 1);
}

TEST_F(AppImageTest, check_appimage_type_2)
{
    int t = check_appimage_type(appImage_type_2_file_path.c_str(), 0);
    ASSERT_EQ(t, 2);
}

TEST_F(AppImageTest, appimage_register_in_system_with_type1)
{
    int r = appimage_register_in_system(appImage_type_1_file_path.c_str(), true);
    ASSERT_EQ(r, 0);

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_1_file_path));

    appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
}

TEST_F(AppImageTest, appimage_register_in_system_with_type2)
{
    int r = appimage_register_in_system(appImage_type_2_file_path.c_str(), true);
    ASSERT_EQ(r, 0);

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_2_file_path));

    appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);
}

TEST_F(AppImageTest, appimage_type1_register_in_system)
{
    bool r = appimage_type1_register_in_system(appImage_type_1_file_path.c_str(), false);
    ASSERT_TRUE(r);

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_1_file_path));

    appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
}

TEST_F(AppImageTest, appimage_type2_register_in_system)
{
    bool r = appimage_type2_register_in_system(appImage_type_2_file_path.c_str(), false);
    ASSERT_TRUE(r);

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_2_file_path));
    appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);
}

TEST_F(AppImageTest, appimage_unregister_in_system) {
    ASSERT_FALSE(areIntegrationFilesDeployed(appImage_type_1_file_path));
    ASSERT_FALSE(areIntegrationFilesDeployed(appImage_type_2_file_path));
}

TEST_F(AppImageTest, get_md5)
{
    std::string expected = "128e476a7794288cad0eb2542f7c995b";
    gchar * sum = get_md5("/tmp/testfile");

//    std::cout << sum;
    int res = g_strcmp0(expected.c_str(), sum);
    ASSERT_EQ(res, 0);
    g_free(sum);
}

TEST_F(AppImageTest, get_md5_invalid_file_path)
{
    std::string expected = "";
    gchar * sum = get_md5("");

    int res = g_strcmp0(expected.c_str(), sum);
    ASSERT_EQ(res, 0);
}

TEST_F(AppImageTest, create_thumbnail_appimage_type_1) {
    create_thumbnail(appImage_type_1_file_path.c_str());

    gchar *sum = get_md5(appImage_type_1_file_path.c_str());
    std::string path = std::string(g_get_user_cache_dir())
                       + "/thumbnails/normal/"
                       + std::string(sum) + ".png";

    g_free(sum);

    ASSERT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_EXISTS));

    // Clean
    rm_file(path);
}

TEST_F(AppImageTest, create_thumbnail_appimage_type_2) {
    create_thumbnail(appImage_type_2_file_path.c_str());

    gchar* sum = get_md5(appImage_type_2_file_path.c_str());
    std::string path = std::string(g_get_user_cache_dir())
                       + "/thumbnails/normal/"
                       + std::string(sum) + ".png";

    g_free(sum);

    ASSERT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_EXISTS));

    // Clean
    rm_file(path);
}

TEST_F(AppImageTest, extract_file_following_symlinks) {
        char * target_path = const_cast<char *>("/tmp/test_libappimage_tmp_file");
    extract_file_following_symlinks(appImage_type_2_file_path.c_str(), "echo.desktop", target_path);

    char *expected = const_cast<char *>("[Desktop Entry]\n"
                    "Version=1.0\n"
                    "Type=Application\n"
                    "Name=Echo\n"
                    "Comment=Just echo.\n"
                    "Exec=echo %F\n"
                    "Icon=utilities-terminal\n");

    ASSERT_TRUE(g_file_test(target_path, G_FILE_TEST_EXISTS));

    std::ifstream file(target_path, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size))
        ASSERT_TRUE( strncmp(expected, buffer.data(), strlen(expected)) == 0 );
    else
        FAIL();

    // Clean
    remove(target_path);
}

} // namespace

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
