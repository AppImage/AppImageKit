#include <gtest/gtest.h>

#include "fixtures.h"

extern "C" {
    #include "getsection.h"
}


using namespace std;


// most simple derivative class for better naming of the tests in this file
class GetSectionCTest : public AppImageKitTest {};


TEST_F(GetSectionCTest, test_print_binary) {
    std::string appImagePath = std::string(TEST_DATA_DIR) + "/Echo-x86_64.AppImage";

    unsigned long offset, length;

    get_elf_section_offset_and_length(appImagePath.c_str(), ".upd_info", &offset, &length);

    print_binary(appImagePath.c_str(), offset, length);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
