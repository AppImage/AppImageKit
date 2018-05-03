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

    // should be large enough
    vector<char> buffer(100 * 1024 * 1024);

    // read in desktop file
    ifs.read(buffer.data(), buffer.size());

    GKeyFile *keyFile = g_key_file_new();
    gboolean success = g_key_file_load_from_data(keyFile, buffer.data(), buffer.size(), G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    if (success) {
        write_edited_desktop_file(keyFile, "testpath", strdup("abc"), 1, strdup("def"), false);
    }

    g_key_file_free(keyFile);

    stringstream pathToInstalledDesktopFile;
    pathToInstalledDesktopFile << tempHome << "/.local/share/applications/appimagekit_def-abc";

    // now, read original and installed desktop file, and compare both
    ifstream originalStrm(pathToOriginalDesktopFile.str().c_str());
    ifstream installedStrm(pathToInstalledDesktopFile.str().c_str());

    ASSERT_TRUE(originalStrm) << "Failed to open desktop file " << pathToOriginalDesktopFile.str();
    ASSERT_TRUE(installedStrm) << "Failed to open desktop file " << pathToInstalledDesktopFile.str();

    // split both files by lines, then convert to key-value list, and check whether all lines from original file
    // are also available in the installed file
    // some values modified by write_edited_desktop_file need some extra checks, which can be performed then.
    vector<char> originalData(100 * 1024 * 1024);
    vector<char> installedData(100 * 1024 * 1024);

    originalStrm.read(originalData.data(), originalData.size());
    installedStrm.read(installedData.data(), originalData.size());

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
        ASSERT_EQ(lineSplit.size(), 2);
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

        if (key == "Exec" || key == "TryExec") {
            vector<string> execSplit = splitString(value);
            EXPECT_GT(execSplit.size(), 0) << "key: " << key;
            EXPECT_EQ(execSplit[0], "testpath") << "key: " << key;

            vector<string> originalExecSplit = splitString((*entry).second);
            EXPECT_EQ(execSplit.size(), originalExecSplit.size())
                << key << ": " << value << " and " << (*entry).second << " contain different number of parameters";

            // the rest of the split parts should be equal
            for (int i = 1; i < execSplit.size(); i++) {
                EXPECT_EQ(execSplit[i], originalExecSplit[i]);
            }
        } else if (key == "Icon") {
            EXPECT_EQ(value, "appimagekit_def_cura-icon");
        } else {
            EXPECT_EQ(value, (*entry).second);
        }

        installedLines.erase(line);
    }

    // finally, handle X-AppImage- entries
    for (vector<string>::iterator line = installedLines.begin(); line != installedLines.end();) {
        if (stringStartsWith(*line, "X-AppImage-Comment")) {
            EXPECT_EQ(*line, "X-AppImage-Comment=Generated by appimaged AppImageKit unit tests");
        } else if (stringStartsWith(*line, "X-AppImage-Identifier")) {
            EXPECT_EQ(*line, "X-AppImage-Identifier=def");
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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
