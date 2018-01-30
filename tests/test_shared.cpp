#include <gtest/gtest.h>
#include <fstream>
#include <ftw.h>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
    #include "shared.h"
}


using namespace std;


// fixture providing a temporary directory, and a temporary home directory within that directory
// overwrites HOME environment variable to ensure desktop files etc. are not installed in the system
class SharedCTest : public ::testing::Test {
private:
    char* oldHome;

public:
    string tempDir;
    string tempHome;

public:
    SharedCTest() {
        char* tmpl = strdup("/tmp/AppImageKit-unit-tests-XXXXXX");
        tempDir = mkdtemp(tmpl);
        free(tmpl);

        tempHome = tempDir + "/HOME";

        oldHome = getenv("HOME");
        stringstream newHome;
        newHome << "HOME=" << tempHome;
        putenv(strdup(newHome.str().c_str()));

        mkdir(tempHome.c_str(), 0700);
    };

    ~SharedCTest() {
        if (isDir(tempDir)) {
            rmTree(tempDir);
        }
    }

private:
    static int rmTree(const string& path) {
        return nftw(path.c_str(), unlinkCb, 64, FTW_DEPTH | FTW_PHYS);
    }

    static int unlinkCb(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf) {
        int rv = remove(fpath);

        if (rv)
            perror(fpath);

        return rv;
    };

public:
    static const bool isFile(const string& path) {
        struct stat st;

        if (stat(path.c_str(), &st) != 0) {
            perror("Failed to call stat(): ");
            return false;
        }

        return S_ISREG(st.st_mode);
    }

    static const bool isDir(const string& path) {
        struct stat st;

        if (stat(path.c_str(), &st) != 0) {
            perror("Failed to call stat(): ");
            return false;
        }

        return S_ISDIR(st.st_mode);
    }

    static std::vector<std::string> splitString(const std::string& s, char delim = ' ') {
        std::vector<std::string> result;

        std::stringstream ss(s);
        std::string item;

        while (std::getline(ss, item, delim)) {
            result.push_back(item);
        }

        return result;
    }

    static bool isEmptyString(const string& str) {
        // check whether string is empty beforehand, as the string is interpreted as C string and contains a trailing \0
        if (str.empty())
            return true;

        for (int i = 0; i < str.length(); i++) {
            char chr = str[i];
            if (chr != ' ' && chr != '\t')
                return false;
        }

        return true;
    }

    static bool stringStartsWith(const string& str, const string& prefix) {
        for (int i = 0; i < prefix.length(); i++) {
            if (str[i] != prefix[i])
                return false;
        }

        return true;
    }
};


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
    // first line should be "[Desktop Entry]" header
    ASSERT_EQ(originalLines.front(), "[Desktop Entry]");
    ASSERT_EQ(installedLines.front(), "[Desktop Entry]");
    // drop "[Desktop Entry]" header
    originalLines.erase(originalLines.begin());
    installedLines.erase(installedLines.begin());
    // also remove all empty lines
    originalLines.erase(std::remove_if(originalLines.begin(), originalLines.end(), isEmptyString), originalLines.end());
    installedLines.erase(std::remove_if(installedLines.begin(), installedLines.end(), isEmptyString), installedLines.end());

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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
