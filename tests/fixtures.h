#pragma once

#include <cerrno>
#include <ftw.h>
#include <unistd.h>
#include <xdg-basedir.h>


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

        mkdir(tempHome.c_str(), 0700);

        oldHome = getenv("HOME");
        oldXdgDataHome = getenv("XDG_DATA_HOME");
        oldXdgConfigHome = getenv("XDG_CONFIG_HOME");

        std::string newXdgDataHome = tempHome + "/.local/share";
        std::string newXdgConfigHome = tempHome + "/.config";

        setenv("HOME", tempHome.c_str(), true);
        setenv("XDG_DATA_HOME", newXdgDataHome.c_str(), true);
        setenv("XDG_CONFIG_HOME", newXdgConfigHome.c_str(), true);

        char* xdgDataHome = xdg_data_home();
        char* xdgConfigHome = xdg_config_home();

        EXPECT_EQ(getenv("HOME"), tempHome);
        EXPECT_EQ(newXdgDataHome, xdgDataHome);
        EXPECT_EQ(newXdgConfigHome, xdgConfigHome);

        free(xdgDataHome);
        free(xdgConfigHome);
    };

    ~AppImageKitTest() {
        if (isDir(tempDir)) {
            rmTree(tempDir);
        }

        if (oldHome != NULL) {
            setenv("HOME", oldHome, true);
        } else {
            unsetenv("HOME");
        }

        if (oldXdgDataHome != NULL) {
            setenv("XDG_DATA_HOME", oldXdgDataHome, true);
        } else {
            unsetenv("XDG_DATA_HOME");
        }

        if (oldXdgConfigHome != NULL) {
            setenv("XDG_CONFIG_HOME", oldXdgConfigHome, true);
        } else {
            unsetenv("XDG_CONFIG_HOME");
        }
    }

private:
    static const int rmTree(const std::string& path) {
        int rv = nftw(path.c_str(), unlinkCb, 64, FTW_DEPTH|FTW_MOUNT|FTW_PHYS);

        if (rv != 0) {
            int error = errno;
            std::cerr << "nftw() in rmTree(" << path << ") failed: " << strerror(error) << std::endl;
            return rv;
        }

        return 0;
    }

    static int unlinkCb(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf) {
        int rv;

        switch (typeflag) {
            case FTW_D:
            case FTW_DNR:
            case FTW_DP:
                rv = rmdir(fpath);
                break;
            default:
                rv = unlink(fpath);
                break;
        }

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
