#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "libappimage.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <gtest/gtest.h>

namespace AppImageTests
{

class AppImageTest : public testing::Test
{
  protected:
    char tests_dir[250];
    bool tests_dir_created = false;

    virtual void SetUp()
    {
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
        std::string content = "Hello World\n";
        g_file_set_contents(path.c_str(),
                            content.c_str(),
                            content.size(),
                            0);
    }

    void rm_file(std::string path)
    {
        g_remove(path.c_str());
    }
};


} // namespace

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}