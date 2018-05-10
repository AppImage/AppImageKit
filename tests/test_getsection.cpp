// system headers
#include <cmath>

// library headers
#include <gtest/gtest.h>

// local headers
#include <appimage/appimage.h>
#include "fixtures.h"

extern "C" {
    #include "getsection.h"
}


using namespace std;


// most simple derivative class for better naming of the tests in this file
class GetSectionCTest : public AppImageKitTest {};


bool isPowerOfTwo(int number) {
    return (number & (number - 1)) == 0;
}


TEST_F(GetSectionCTest, test_appimage_get_elf_section_offset_and_length) {
    std::string appImagePath = std::string(TEST_DATA_DIR) + "/appimaged-i686.AppImage";

    unsigned long offset, length;

    ASSERT_EQ(appimage_get_elf_section_offset_and_length(appImagePath.c_str(), ".upd_info", &offset, &length), 0);

    EXPECT_GT(offset, 0);
    EXPECT_GT(length, 0);

    EXPECT_PRED1(isPowerOfTwo, length);
}


TEST_F(GetSectionCTest, test_print_binary) {
    std::string appImagePath = std::string(TEST_DATA_DIR) + "/appimaged-i686.AppImage";

    unsigned long offset, length;

    ASSERT_EQ(appimage_get_elf_section_offset_and_length(appImagePath.c_str(), ".upd_info", &offset, &length), 0);

    EXPECT_EQ(print_binary(appImagePath.c_str(), offset, length), 0);
}


TEST_F(GetSectionCTest, test_print_hex) {
    std::string appImagePath = std::string(TEST_DATA_DIR) + "/appimaged-i686.AppImage";

    unsigned long offset, length;

    ASSERT_EQ(appimage_get_elf_section_offset_and_length(appImagePath.c_str(), ".sha256_sig", &offset, &length), 0);

    EXPECT_EQ(print_hex(appImagePath.c_str(), offset, length), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
