#include <gtest/gtest.h>
#include <fstream>
#include <ftw.h>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#include "fixtures.h"

extern "C" {
    #include "shared.h"
}


using namespace std;


// most simple derivative class for better naming of the tests in this file
class SharedCTest : public AppImageKitTest {};


TEST_F(SharedCTest, test_write_desktop_file_exec) {
    // install Cura desktop file into temporary HOME with some hardcoded paths
    stringstream pathToOriginalDesktopFile;
    pathToOriginalDesktopFile << TEST_DATA_DIR << "/" << "Cura.desktop";
    ifstream ifs(pathToOriginalDesktopFile.str().c_str());

    ASSERT_TRUE(ifs) << "Failed to open file: " << pathToOriginalDesktopFile.str();

    ifs.seekg(0, ios::end);
    unsigned long bufferSize = static_cast<unsigned long>(ifs.tellg() + 1);
    ifs.seekg(0, ios::beg);

    // should be large enough
    vector<char> buffer(bufferSize, '\0');

    // read in desktop file
    ifs.read(buffer.data(), buffer.size());

    GError* error = NULL;

    GKeyFile *keyFile = g_key_file_new();
    gboolean success = g_key_file_load_from_data(keyFile, buffer.data(), buffer.size(), G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error);

    ASSERT_EQ(error, NULL) << "Error while creating key file from data: " << error->message;

    gchar desktop_filename[] = "desktop_filename";
    gchar md5testvalue[] = "md5testvalue";

    if (success) {
        write_edited_desktop_file(keyFile, "testpath", desktop_filename, 1, md5testvalue, false);
    }

    g_key_file_free(keyFile);

    stringstream pathToInstalledDesktopFile;
    pathToInstalledDesktopFile << tempHome << g_strdup_printf("/.local/share/applications/appimagekit_%s-%s", md5testvalue, desktop_filename);

    // now, read original and installed desktop file, and compare both
    ifstream originalStrm(pathToOriginalDesktopFile.str().c_str());
    ifstream installedStrm(pathToInstalledDesktopFile.str().c_str());

    ASSERT_TRUE(originalStrm) << "Failed to open desktop file " << pathToOriginalDesktopFile.str();
    ASSERT_TRUE(installedStrm) << "Failed to open desktop file " << pathToInstalledDesktopFile.str();

    originalStrm.seekg(0, ios::end);
    unsigned long originalStrmSize = static_cast<unsigned long>(originalStrm.tellg() + 1);
    originalStrm.seekg(0, ios::beg);

    installedStrm.seekg(0, ios::end);
    unsigned long installedStrmSize = static_cast<unsigned long>(installedStrm.tellg() + 1);
    installedStrm.seekg(0, ios::beg);

    // split both files by lines, then convert to key-value list, and check whether all lines from original file
    // are also available in the installed file
    // some values modified by write_edited_desktop_file need some extra checks, which can be performed then.
    vector<char> originalData(originalStrmSize, '\0');
    vector<char> installedData(installedStrmSize, '\0');

    originalStrm.read(originalData.data(), originalData.size());
    installedStrm.read(installedData.data(), installedData.size());

    vector<string> originalLines = splitString(originalData.data(), '\n');
    vector<string> installedLines = splitString(installedData.data(), '\n');
    // first of all, remove all empty lines
    // ancient glib versions like the ones CentOS 6 provides tend to introduce a blank line before the
    // [Desktop Entry] header, hence the blank lines need to be stripped out before the next step
    originalLines.erase(std::remove_if(originalLines.begin(), originalLines.end(), isEmptyString), originalLines.end());
    installedLines.erase(std::remove_if(installedLines.begin(), installedLines.end(), isEmptyString), installedLines.end());
    // first line should be "[Desktop Entry]" header
    ASSERT_EQ(originalLines.front(), "[Desktop Entry]");
    ASSERT_EQ(installedLines.front(), "[Desktop Entry]");
    // drop "[Desktop Entry]" header
    originalLines.erase(originalLines.begin());
    installedLines.erase(installedLines.begin());

    // now, create key-value maps
    map<string, string> entries;

    // sort original entries into map
    for (vector<string>::const_iterator line = originalLines.begin(); line != originalLines.end(); line++) {
        vector<string> lineSplit = splitString(*line, '=');
        ASSERT_EQ(lineSplit.size(), 2) << "line: " << *line;
        entries.insert(std::make_pair(lineSplit[0], lineSplit[1]));
    }

    // now, remove all entries found in installed desktop entry from entries
    for (vector<string>::iterator line = installedLines.begin(); line != installedLines.end();) {
        vector<string> lineSplit = splitString(*line, '=');
        ASSERT_EQ(lineSplit.size(), 2) << "Condition failed for line: " << *line;

        const string& key = lineSplit[0];
        const string& value = lineSplit[1];

        if (stringStartsWith(key, "X-AppImage-")) {
            // skip this entry
            line++;
            continue;
        }

        map<string, string>::const_iterator entry = entries.find(key);

        if (entry == entries.end())
            FAIL() << "No such entry in desktop file: " << key;

        if (key == "Exec" || key == "TryExec") {
            vector<string> execSplit = splitString(value);
            ASSERT_GT(execSplit.size(), 0) << "key: " << key;
            ASSERT_EQ(execSplit[0], "testpath") << "key: " << key;

            vector<string> originalExecSplit = splitString((*entry).second);
            ASSERT_EQ(execSplit.size(), originalExecSplit.size())
                << key << ": " << value << " and " << (*entry).second << " contain different number of parameters";

            // the rest of the split parts should be equal
            for (int i = 1; i < execSplit.size(); i++) {
                ASSERT_EQ(execSplit[i], originalExecSplit[i]);
            }
        } else if (key == "Icon") {
            ASSERT_EQ(value, g_strdup_printf("appimagekit_%s_cura-icon", md5testvalue));
        } else {
            ASSERT_EQ(value, (*entry).second);
        }

        installedLines.erase(line);
    }

    // finally, handle X-AppImage- entries
    for (vector<string>::iterator line = installedLines.begin(); line != installedLines.end();) {
        if (stringStartsWith(*line, "X-AppImage-Comment")) {
            ASSERT_EQ(*line, "X-AppImage-Comment=Generated by appimaged AppImageKit unit tests");
        } else if (stringStartsWith(*line, "X-AppImage-Identifier")) {
            ASSERT_EQ(*line, g_strdup_printf("X-AppImage-Identifier=%s", md5testvalue));
        } else {
            line++;
            continue;
        }

        installedLines.erase(line);
    }

    ASSERT_EQ(installedLines.size(), 0);
}

TEST_F(SharedCTest, test_appimage_type1_is_terminal_app) {
    // TODO: add type 1 AppImage with Terminal=false
    EXPECT_EQ(appimage_type1_is_terminal_app(appImage_type_1_file_path.c_str()), 1);
    EXPECT_EQ(appimage_type1_is_terminal_app(appImage_type_2_file_path.c_str()), -1);
    EXPECT_EQ(appimage_type1_is_terminal_app("/invalid/path"), -1);
}

TEST_F(SharedCTest, test_appimage_type2_is_terminal_app) {
    EXPECT_EQ(appimage_type2_is_terminal_app(appImage_type_1_file_path.c_str()), -1);
    EXPECT_EQ(appimage_type2_is_terminal_app(appImage_type_2_terminal_file_path.c_str()), 1);
    EXPECT_EQ(appimage_type2_is_terminal_app(appImage_type_2_file_path.c_str()), 0);
    EXPECT_EQ(appimage_type2_is_terminal_app("/invalid/path"), -1);
}

TEST_F(SharedCTest, test_appimage_is_terminal_app) {
    EXPECT_EQ(appimage_is_terminal_app(appImage_type_1_file_path.c_str()), 1);
    EXPECT_EQ(appimage_is_terminal_app(appImage_type_2_file_path.c_str()), 0);
    // TODO: add type 1 AppImage with Terminal=true
    //EXPECT_EQ(appimage_is_terminal_app(appImage_type_1_terminal_file_path.c_str()), 1);
    EXPECT_EQ(appimage_is_terminal_app(appImage_type_2_terminal_file_path.c_str()), 1);
    EXPECT_EQ(appimage_is_terminal_app("/invalid/path"), -1);
}

TEST_F(SharedCTest, test_appimage_type1_shall_not_integrate) {
    // TODO: add type 1 AppImage with X-AppImage-Integrate=false
    //EXPECT_EQ(appimage_is_terminal_app(appImage_type_1_shall_not_integrate_path.c_str()), 1);
    EXPECT_EQ(appimage_type1_shall_not_be_integrated(appImage_type_1_file_path.c_str()), 0);
    EXPECT_EQ(appimage_type1_shall_not_be_integrated(appImage_type_2_file_path.c_str()), -1);
    EXPECT_EQ(appimage_type1_shall_not_be_integrated("/invalid/path"), -1);
}

TEST_F(SharedCTest, test_appimage_type2_shall_not_integrate) {
    EXPECT_EQ(appimage_type2_shall_not_be_integrated(appImage_type_1_file_path.c_str()), -1);
    EXPECT_EQ(appimage_type2_shall_not_be_integrated(appImage_type_2_shall_not_integrate_path.c_str()), 1);
    EXPECT_EQ(appimage_type2_shall_not_be_integrated(appImage_type_2_file_path.c_str()), 0);
    EXPECT_EQ(appimage_type2_shall_not_be_integrated("/invalid/path"), -1);
}

TEST_F(SharedCTest, test_appimage_shall_not_integrate) {
    EXPECT_EQ(appimage_shall_not_be_integrated(appImage_type_1_file_path.c_str()), 0);
    EXPECT_EQ(appimage_shall_not_be_integrated(appImage_type_2_file_path.c_str()), 0);
    // TODO: add type 1 AppImage with X-AppImage-Integrate=false
    //EXPECT_EQ(appimage_shall_not_be_integrated(appImage_type_1_shall_not_integrate_path.c_str()), 1);
    EXPECT_EQ(appimage_shall_not_be_integrated(appImage_type_2_shall_not_integrate_path.c_str()), 1);
    EXPECT_EQ(appimage_is_terminal_app("/invalid/path"), -1);
}

static bool test_strcmp(char* a, char* b) {
    return strcmp(a, b) == 0;
}

TEST_F(SharedCTest, test_appimage_hexlify) {
    {
        char bytesIn[] = "\x00\x01\x02\x03\x04\x05\x06\x07";
        char expectedHex[] = "0001020304050607";

        char* hexlified = appimage_hexlify(bytesIn, 8);
        EXPECT_PRED2(test_strcmp, hexlified, expectedHex);

        // cleanup
        free(hexlified);
    }
    {
        char bytesIn[] = "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";
        char expectedHex[] = "f8f9fafbfcfdfeff";

        char* hexlified = appimage_hexlify(bytesIn, 8);
        EXPECT_PRED2(test_strcmp, hexlified, expectedHex);

        // cleanup
        free(hexlified);
    }
}

// compares whether the size first bytes of two given byte buffers are equal
bool test_compare_bytes(const char* buf1, const char* buf2, int size) {
    for (int i = 0; i < size; i++) {
        if (buf1[i] != buf2[i]) {
            return false;
        }
    }

    return true;
}

TEST_F(SharedCTest, appimage_type2_digest_md5) {
    char digest[16];
    char expectedDigest[] = {-20, 92, -89, 99, -47, -62, 14, 36, -5, -127, 65, -126, 116, -41, -33, -121};

    EXPECT_TRUE(appimage_type2_digest_md5(appImage_type_2_file_path.c_str(), digest));
    EXPECT_PRED3(test_compare_bytes, digest, expectedDigest, 16);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
