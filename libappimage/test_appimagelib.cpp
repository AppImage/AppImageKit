#include "libappimage.h"

#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace AppImageTests
{

class AppImageTest : public testing::Test
{
  protected:
    char tests_dir[250];
    bool tests_dir_created;
    std::string test_file_content;

    virtual void SetUp()
    {
        tests_dir_created = false;
        test_file_content = "Hello World\n";
        createTestsDir();
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

    std::string build_test_file_path(std::string name)
    {
        return tests_dir + name;
    }

    void mk_file(std::string path)
    {
        g_file_set_contents(path.c_str(),
                            test_file_content.c_str(),
                            test_file_content.size(),
                            0);
    }

    void rm_file(std::string path)
    {
        g_remove(path.c_str());
    }
};

TEST_F(AppImageTest, get_md5)
{
    char text[] = "Hello World\n";
    // generated using md5sum
    std::string expected = "f0ef7081e1539ac00ef5b761b4fb01b3";

    gchar *sum = get_md5(text);

    int res = g_strcmp0(expected.c_str(), sum);
    ASSERT_TRUE(res == 0);
}

} // namespace

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}