#pragma once

#include <ftw.h>
#include <unistd.h>

using namespace std;


// fixture providing a temporary directory, and a temporary home directory within that directory
// overwrites HOME environment variable to ensure desktop files etc. are not installed in the system
class AppImageKitTest : public ::testing::Test {
private:
    char* oldHome;

public:
    string tempDir;
    string tempHome;

public:
    AppImageKitTest() {
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

    ~AppImageKitTest() {
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
