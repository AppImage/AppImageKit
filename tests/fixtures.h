#pragma once

#include <ftw.h>
#include <unistd.h>


// fixture providing a temporary directory, and a temporary home directory within that directory
// overwrites HOME environment variable to ensure desktop files etc. are not installed in the system
class AppImageKitTest : public ::testing::Test {
private:
    char* oldHome;
    char* oldXdgDataHome;
    char* oldXdgConfigHome;

public:
    std::string tempDir;
    std::string tempHome;

public:
    AppImageKitTest() {
        char* tmpl = strdup("/tmp/AppImageKit-unit-tests-XXXXXX");
        tempDir = mkdtemp(tmpl);
        free(tmpl);

        tempHome = tempDir + "/HOME";

        oldHome = getenv("HOME");
        oldXdgDataHome = getenv("XDG_DATA_HOME");
        oldXdgConfigHome = getenv("XDG_CONFIG_HOME");

        std::stringstream newHome;
        newHome << "HOME=" << tempHome;
        putenv(strdup(newHome.str().c_str()));

        std::stringstream newXdgDataHome;
        newXdgDataHome << "XDG_DATA_HOME=" << tempHome << "/.local/share";
        putenv(strdup(newXdgDataHome.str().c_str()));

        std::stringstream newXdgConfigHome;
        newXdgDataHome << "XDG_CONFIG_HOME=" << tempHome << "/.config";
        putenv(strdup(newXdgConfigHome.str().c_str()));

        mkdir(tempHome.c_str(), 0700);
    };

    ~AppImageKitTest() {
        if (isDir(tempDir)) {
            rmTree(tempDir);
        }

        if (oldHome != NULL) {
            std::stringstream newHome;
            newHome << "HOME=" << oldHome;
            putenv(strdup(newHome.str().c_str()));
        } else {
            unsetenv("XDG_DATA_HOME");
        }

        if (oldXdgDataHome != NULL) {
            std::stringstream newXdgDataHome;
            newXdgDataHome << "XDG_DATA_HOME=" << oldXdgDataHome;
            putenv(strdup(newXdgDataHome.str().c_str()));
        } else {
            unsetenv("XDG_DATA_HOME");
        }

        if (oldXdgConfigHome != NULL) {
            std::stringstream newXdgConfigHome;
            newXdgConfigHome << "XDG_CONFIG_HOME=" << oldXdgConfigHome;
            putenv(strdup(newXdgConfigHome.str().c_str()));
        } else {
            unsetenv("XDG_CONFIG_HOME");
        }
    }

private:
    static const int rmTree(const std::string& path) {
        return nftw(path.c_str(), unlinkCb, 64, FTW_DEPTH | FTW_PHYS);
    }

    static int unlinkCb(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf) {
        int rv = remove(fpath);

        if (rv)
            perror(fpath);

        return rv;
    };

public:
    static const bool isFile(const std::string& path) {
        struct stat st;

        if (stat(path.c_str(), &st) != 0) {
            perror("Failed to call stat(): ");
            return false;
        }

        return S_ISREG(st.st_mode);
    }

    static const bool isDir(const std::string& path) {
        struct stat st;

        if (stat(path.c_str(), &st) != 0) {
            perror("Failed to call stat(): ");
            return false;
        }

        return S_ISDIR(st.st_mode);
    }

    static const std::vector<std::string> splitString(const std::string& s, char delim = ' ') {
        std::vector<std::string> result;

        std::stringstream ss(s);
        std::string item;

        while (std::getline(ss, item, delim)) {
            result.push_back(item);
        }

        return result;
    }

    static const bool isEmptyString(const std::string& str) {
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

    static const bool stringStartsWith(const std::string& str, const std::string& prefix) {
        for (int i = 0; i < prefix.length(); i++) {
            if (str[i] != prefix[i])
                return false;
        }

        return true;
    }
};
