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


namespace AppImageTests
{

class LibAppImageTest : public AppImageKitTest
{
protected:
    std::string appImage_type_1_file_path;
    std::string appImage_type_2_file_path;

    LibAppImageTest()
    {
        appImage_type_1_file_path = std::string(TEST_DATA_DIR) + "/AppImageExtract_6-x86_64.AppImage";
        appImage_type_2_file_path = std::string(TEST_DATA_DIR) + "/Echo-x86_64.AppImage";
    }

    void rm_file(const std::string& path)
    {
        g_remove(path.c_str());
    }

    bool areIntegrationFilesDeployed(const std::string& path)
    {
        gchar *sum = appimage_get_md5(path.c_str());

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

TEST_F(LibAppImageTest, appimage_get_type_invalid)
{
    ASSERT_EQ(appimage_get_type("/tmp", false), -1);
}

TEST_F(LibAppImageTest, appimage_get_type_1)
{
    ASSERT_EQ(appimage_get_type(appImage_type_1_file_path.c_str(), false), 1);
}

TEST_F(LibAppImageTest, appimage_get_type_2)
{
    ASSERT_EQ(appimage_get_type(appImage_type_2_file_path.c_str(), false), 2);
}

TEST_F(LibAppImageTest, appimage_register_in_system_with_type1)
{
    ASSERT_EQ(appimage_register_in_system(appImage_type_1_file_path.c_str(), true), 0);

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_1_file_path));

    appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
}

TEST_F(LibAppImageTest, appimage_register_in_system_with_type2)
{
    ASSERT_EQ(appimage_register_in_system(appImage_type_2_file_path.c_str(), true), 0);

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_2_file_path));

    appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);
}

TEST_F(LibAppImageTest, appimage_type1_register_in_system)
{
    ASSERT_TRUE(appimage_type1_register_in_system(appImage_type_1_file_path.c_str(), false));

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_1_file_path));

    appimage_unregister_in_system(appImage_type_1_file_path.c_str(), false);
}

TEST_F(LibAppImageTest, appimage_type2_register_in_system)
{
    ASSERT_TRUE(appimage_type2_register_in_system(appImage_type_2_file_path.c_str(), false));

    ASSERT_TRUE(areIntegrationFilesDeployed(appImage_type_2_file_path));
    appimage_unregister_in_system(appImage_type_2_file_path.c_str(), false);
}

TEST_F(LibAppImageTest, appimage_unregister_in_system) {
    ASSERT_FALSE(areIntegrationFilesDeployed(appImage_type_1_file_path));
    ASSERT_FALSE(areIntegrationFilesDeployed(appImage_type_2_file_path));
}

TEST_F(LibAppImageTest, appimage_get_md5)
{
    std::string expected = "128e476a7794288cad0eb2542f7c995b";
    gchar * sum = appimage_get_md5("/tmp/testfile");

    int res = g_strcmp0(expected.c_str(), sum);
    ASSERT_EQ(res, 0);
    g_free(sum);
}

TEST_F(LibAppImageTest, get_md5_invalid_file_path)
{
    std::string expected = "";
    gchar * sum = appimage_get_md5("");

    int res = g_strcmp0(expected.c_str(), sum);
    ASSERT_EQ(res, 0);
}

TEST_F(LibAppImageTest, create_thumbnail_appimage_type_1) {
        appimage_create_thumbnail(appImage_type_1_file_path.c_str());

    gchar *sum = appimage_get_md5(appImage_type_1_file_path.c_str());
    std::string path = std::string(g_get_user_cache_dir())
                       + "/thumbnails/normal/"
                       + std::string(sum) + ".png";

    g_free(sum);

    ASSERT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_EXISTS));

    // Clean
    rm_file(path);
}

TEST_F(LibAppImageTest, create_thumbnail_appimage_type_2) {
        appimage_create_thumbnail(appImage_type_2_file_path.c_str());

    gchar* sum = appimage_get_md5(appImage_type_2_file_path.c_str());
    std::string path = std::string(g_get_user_cache_dir())
                       + "/thumbnails/normal/"
                       + std::string(sum) + ".png";

    g_free(sum);

    ASSERT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_EXISTS));

    // Clean
    rm_file(path);
}

TEST_F(LibAppImageTest, appimage_extract_file_following_symlinks) {
    const char target_path[] = "/tmp/test_libappimage_tmp_file";
    appimage_extract_file_following_symlinks(appImage_type_2_file_path.c_str(), "echo.desktop", target_path);

    const char expected[] = ("[Desktop Entry]\n"
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

    std::vector<char> buffer(static_cast<unsigned long>(size));
    if (file.read(buffer.data(), size))
        ASSERT_TRUE( strncmp(expected, buffer.data(), strlen(expected)) == 0 );
    else
        FAIL();

    // Clean
    remove(target_path);
}

bool test_appimage_is_registered_in_system(const std::string& pathToAppImage, bool integrateAppImage) {
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

} // namespace

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
