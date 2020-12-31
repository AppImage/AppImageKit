/**************************************************************************
 * 
 * Copyright (c) 2004-19 Simon Peter
 * 
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 **************************************************************************/

#ident "AppImage by Simon Peter, http://appimage.org/"

#ifndef RELEASE_NAME
    #define RELEASE_NAME "continuous build"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <argp.h>

#include <fcntl.h>
#include "squashfuse.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "binreloc.h"

#include <libgen.h>

#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include "appimage/appimage.h"

#ifdef __linux__
#define HAVE_BINARY_RUNTIME
extern char runtime[];
extern unsigned int runtime_len;
#endif

enum fARCH { 
    fARCH_i386,
    fARCH_x86_64,
    fARCH_arm,
    fARCH_aarch64
};

static gchar const APPIMAGEIGNORE[] = ".appimageignore";
static char _exclude_file_desc[256];

static gboolean list = FALSE;
static gboolean verbose = FALSE;
static gboolean showVersionOnly = FALSE;
static gboolean sign = FALSE;
static gboolean no_appstream = FALSE;
gchar **remaining_args = NULL;
gchar *updateinformation = NULL;
static gboolean guess_update_information = FALSE;
gchar *bintray_user = NULL;
gchar *bintray_repo = NULL;
gchar *sqfs_comp = "gzip";
gchar *exclude_file = NULL;
gchar *runtime_file = NULL;
gchar *sign_args = NULL;
gchar *sign_key = NULL;
gchar *pathToMksquashfs = NULL;

// #####################################################################

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/* Function that prints the contents of a squashfs file
* using libsquashfuse (#include "squashfuse.h") */
int sfs_ls(char* image) {
    sqfs_err err = SQFS_OK;
    sqfs_traverse trv;
    sqfs fs;
    
    ssize_t fs_offset = appimage_get_elf_size(image);

    // error check
    if (fs_offset < 0)
        die("failed to read elf size");

    if ((err = sqfs_open_image(&fs, image, fs_offset)))
        die("sqfs_open_image error");
    
    if ((err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs))))
        die("sqfs_traverse_open error");
    while (sqfs_traverse_next(&trv, &err)) {
        if (!trv.dir_end) {
            printf("%s\n", trv.path);
        }
    }
    if (err)
        die("sqfs_traverse_next error");
    sqfs_traverse_close(&trv);
    
    sqfs_fd_close(fs.fd);
    return 0;
}

/* Generate a squashfs filesystem using mksquashfs on the $PATH 
* execlp(), execvp(), and execvpe() search on the $PATH */
int sfs_mksquashfs(char *source, char *destination, int offset) {
    pid_t pid = fork();
    
    if (pid == -1) {
        // error, failed to fork()
        return(-1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        // we are the child
        gchar* offset_string;
        offset_string = g_strdup_printf("%i", offset);

        char* args[32];
        bool use_xz = strcmp(sqfs_comp, "xz") >= 0;

        int i = 0;
#ifndef AUXILIARY_FILES_DESTINATION
        args[i++] = "mksquashfs";
#else
        args[i++] = pathToMksquashfs;
#endif
        args[i++] = source;
        args[i++] = destination;
        args[i++] = "-offset";
        args[i++] = offset_string;
        args[i++] = "-comp";

        if (use_xz)
            args[i++] = "xz";
        else
            args[i++] = sqfs_comp;

        args[i++] = "-root-owned";
        args[i++] = "-noappend";

        if (use_xz) {
            // https://jonathancarter.org/2015/04/06/squashfs-performance-testing/ says:
            // improved performance by using a 16384 block size with a sacrifice of around 3% more squashfs image space
            args[i++] = "-Xdict-size";
            args[i++] = "100%";
            args[i++] = "-b";
            args[i++] = "16384";
        }

        // check if ignore file exists and use it if possible
        if (access(APPIMAGEIGNORE, F_OK) >= 0) {
            printf("Including %s", APPIMAGEIGNORE);
            args[i++] = "-wildcards";
            args[i++] = "-ef";

            // avoid warning: assignment discards ‘const’ qualifier
            char* buf = strdup(APPIMAGEIGNORE);
            args[i++] = buf;
        }

        // if an exclude file has been passed on the command line, should be used, too
        if (exclude_file != 0 && strlen(exclude_file) > 0) {
            if (access(exclude_file, F_OK) < 0) {
                printf("WARNING: exclude file %s not found!", exclude_file);
                return -1;
            }

            args[i++] = "-wildcards";
            args[i++] = "-ef";
            args[i++] = exclude_file;
        }

        args[i++] = "-mkfs-time";
        args[i++] = "0";

        args[i++] = 0;

        if (verbose) {
            printf("mksquashfs commandline: ");
            for (char** t = args; *t != 0; t++) {
                printf("%s ", *t);
            }
            printf("\n");
        }

#ifndef AUXILIARY_FILES_DESTINATION
        execvp("mksquashfs", args);
#else
        execvp(pathToMksquashfs, args);
#endif

        perror("execlp");   // exec*() returns only on error
        return -1; // exec never returns
    }
    return 0;
}

/* Validate desktop file using desktop-file-validate on the $PATH
* execlp(), execvp(), and execvpe() search on the $PATH */
int validate_desktop_file(char *file) {
    int statval;
    int child_pid;
    child_pid = fork();
    if(child_pid == -1)
    {
        printf("could not fork! \n");
        return 1;
    }
    else if(child_pid == 0)
    {
        execlp("desktop-file-validate", "desktop-file-validate", file, NULL);
    }
    else
    {
        waitpid(child_pid, &statval, WUNTRACED | WCONTINUED);
        if(WIFEXITED(statval)){
            return(WEXITSTATUS(statval));
        }
    }
    return -1;
}

/* Generate a squashfs filesystem
* The following would work if we link to mksquashfs.o after we renamed 
* main() to mksquashfs_main() in mksquashfs.c but we don't want to actually do
* this because squashfs-tools is not under a permissive license
* i *nt sfs_mksquashfs(char *source, char *destination) {
* char *child_argv[5];
* child_argv[0] = NULL;
* child_argv[1] = source;
* child_argv[2] = destination;
* child_argv[3] = "-root-owned";
* child_argv[4] = "-noappend";
* mksquashfs_main(5, child_argv);
* }
*/

/* in-place modification of the string, and assuming the buffer pointed to by
* line is large enough to hold the resulting string*/
static void replacestr(char *line, const char *search, const char *replace)
{
    char *sp = NULL;
    
    if ((sp = strstr(line, search)) == NULL) {
        return;
    }
    int search_len = strlen(search);
    int replace_len = strlen(replace);
    int tail_len = strlen(sp+search_len);
    
    memmove(sp+replace_len,sp+search_len,tail_len+1);
    memcpy(sp, replace, replace_len);
    
    /* Do it recursively again until no more work to do */
    
    if ((sp = strstr(line, search))) {
        replacestr(line, search, replace);
    }
}

int count_archs(bool* archs) {
    int countArchs = 0;
    int i;
    for (i = 0; i < 4; i++) {
        countArchs += archs[i];
    }
    return countArchs;
}

gchar* getArchName(bool* archs) {
    if (archs[fARCH_i386])
        return "i386";
    else if (archs[fARCH_x86_64])
        return "x86_64";
    else if (archs[fARCH_arm])
        return "armhf";
    else if (archs[fARCH_aarch64])
        return "aarch64";
    else
        return "all";
}

void extract_arch_from_e_machine_field(int16_t e_machine, const gchar* sourcename, bool* archs) {
    if (e_machine == 3) {
        archs[fARCH_i386] = 1;
        if(verbose)
            fprintf(stderr, "%s used for determining architecture i386\n", sourcename);
    }

    if (e_machine == 62) {
        archs[fARCH_x86_64] = 1;
        if(verbose)
            fprintf(stderr, "%s used for determining architecture x86_64\n", sourcename);
    }

    if (e_machine == 40) {
        archs[fARCH_arm] = 1;
        if(verbose)
            fprintf(stderr, "%s used for determining architecture armhf\n", sourcename);
    }

    if (e_machine == 183) {
        archs[fARCH_aarch64] = 1;
        if(verbose)
            fprintf(stderr, "%s used for determining architecture aarch64\n", sourcename);
    }
}

void extract_arch_from_text(gchar *archname, const gchar* sourcename, bool* archs) {
    if (archname) {
        archname = g_strstrip(archname);
        if (archname) {
            replacestr(archname, "-", "_");
            replacestr(archname, " ", "_");
            if (g_ascii_strncasecmp("i386", archname, 20) == 0
                    || g_ascii_strncasecmp("i486", archname, 20) == 0
                    || g_ascii_strncasecmp("i586", archname, 20) == 0
                    || g_ascii_strncasecmp("i686", archname, 20) == 0
                    || g_ascii_strncasecmp("intel_80386", archname, 20) == 0
                    || g_ascii_strncasecmp("intel_80486", archname, 20) == 0
                    || g_ascii_strncasecmp("intel_80586", archname, 20) == 0
                    || g_ascii_strncasecmp("intel_80686", archname, 20) == 0
                    ) {
                archs[fARCH_i386] = 1;
                if (verbose)
                    fprintf(stderr, "%s used for determining architecture i386\n", sourcename);
            } else if (g_ascii_strncasecmp("x86_64", archname, 20) == 0) {
                archs[fARCH_x86_64] = 1;
                if (verbose)
                    fprintf(stderr, "%s used for determining architecture x86_64\n", sourcename);
            } else if (g_ascii_strncasecmp("arm", archname, 20) == 0) {
                archs[fARCH_arm] = 1;
                if (verbose)
                    fprintf(stderr, "%s used for determining architecture ARM\n", sourcename);
            } else if (g_ascii_strncasecmp("arm_aarch64", archname, 20) == 0) {
                archs[fARCH_aarch64] = 1;
                if (verbose)
                    fprintf(stderr, "%s used for determining architecture ARM aarch64\n", sourcename);
            }
        }
    }
}

int16_t read_elf_e_machine_field(const gchar* file_path) {
    int16_t e_machine = 0x00;
    FILE* file = 0;
    file = fopen(file_path, "rb");
    if (file) {
        fseek(file, 0x12, SEEK_SET);
        fgets((char*) (&e_machine), 0x02, file);
        fclose(file);
    }

    return e_machine;
}

void guess_arch_of_file(const gchar *archfile, bool* archs) {
    int16_t e_machine_field = read_elf_e_machine_field(archfile);
    extract_arch_from_e_machine_field(e_machine_field, archfile, archs);
}

void find_arch(const gchar *real_path, const gchar *pattern, bool* archs) {
    GDir *dir;
    gchar *full_name;
    dir = g_dir_open(real_path, 0, NULL);
    if (dir != NULL) {
        const gchar *entry;
        while ((entry = g_dir_read_name(dir)) != NULL) {
            full_name = g_build_filename(real_path, entry, NULL);
            if (g_file_test(full_name, G_FILE_TEST_IS_SYMLINK)) {
            } else if (g_file_test(full_name, G_FILE_TEST_IS_DIR)) {
                find_arch(full_name, pattern, archs);
            } else if (g_file_test(full_name, G_FILE_TEST_IS_EXECUTABLE) || g_pattern_match_simple(pattern, entry) ) {
                guess_arch_of_file(full_name, archs);
            }
        }
        g_dir_close(dir);
    }
    else {
        g_warning("%s: %s", real_path, g_strerror(errno));
    }
}

gchar* find_first_matching_file_nonrecursive(const gchar *real_path, const gchar *pattern) {
    GDir *dir;
    gchar *full_name;
    dir = g_dir_open(real_path, 0, NULL);
    if (dir != NULL) {
        const gchar *entry;
        while ((entry = g_dir_read_name(dir)) != NULL) {
            full_name = g_build_filename(real_path, entry, NULL);
            if (g_file_test(full_name, G_FILE_TEST_IS_REGULAR)) {
                if(g_pattern_match_simple(pattern, entry))
                    return(full_name);
            }
        }
        g_dir_close(dir);
    }
    else { 
        g_warning("%s: %s", real_path, g_strerror(errno));
    }
    return NULL;
}

gchar* get_desktop_entry(GKeyFile *kf, char *key) {
    gchar *value = g_key_file_get_string (kf, "Desktop Entry", key, NULL);
    if (! value){
        fprintf(stderr, "%s entry not found in desktop file\n", key);
    }
    return value;
}

bool readFile(char* filename, int* size, char** buffer) {
    FILE* f = fopen(filename, "rb");
    if (f==NULL) {
        *buffer = 0;
        *size = 0;
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *indata = malloc(fsize);
    fread(indata, fsize, 1, f);
    fclose(f);
    *size = (int)fsize;
    *buffer = indata; 
    return TRUE;
}

/* run a command outside the current appimage, block environs like LD_LIBRARY_PATH */
int run_external(const char *filename, char *const argv []) {
    int pid = fork();
    if (pid < 0) {
        g_print("run_external: fork failed");
        // fork failed
        exit(1);
    } else if (pid == 0) {
        // blocks env defined in resources/AppRun
        unsetenv("LD_LIBRARY_PATH");
        unsetenv("PYTHONPATH");
        unsetenv("XDG_DATA_DIRS");
        unsetenv("PERLLIB");
        unsetenv("GSETTINGS_SCHEMA_DIR");
        unsetenv("QT_PLUGIN_PATH");
        // runs command
        execv(filename, argv);
        // execv(3) returns, indicating error
        g_print("run_external: subprocess execv(3) got error %s", g_strerror(errno));
        exit(1);
    } else {
        int wstatus;
        if (waitpid(pid, &wstatus, 0) == -1) {
            g_print("run_external: wait failed");
            return -1;
        }
        if (WIFEXITED(wstatus) && (WEXITSTATUS(wstatus) == 0)) {
            return 0;
        } else {
            g_print("run_external: subprocess exited with status %d", WEXITSTATUS(wstatus));
            return 1;
        }
    }
}

// #####################################################################

static GOptionEntry entries[] =
{
    { "list", 'l', 0, G_OPTION_ARG_NONE, &list, "List files in SOURCE AppImage", NULL },
    { "updateinformation", 'u', 0, G_OPTION_ARG_STRING, &updateinformation, "Embed update information STRING; if zsyncmake is installed, generate zsync file", NULL },
    { "guess", 'g', 0, G_OPTION_ARG_NONE, &guess_update_information, "Guess update information based on Travis CI or GitLab environment variables", NULL },
    { "bintray-user", 0, 0, G_OPTION_ARG_STRING, &bintray_user, "Bintray user name", NULL },
    { "bintray-repo", 0, 0, G_OPTION_ARG_STRING, &bintray_repo, "Bintray repository", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &showVersionOnly, "Show version number", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Produce verbose output", NULL },
    { "sign", 's', 0, G_OPTION_ARG_NONE, &sign, "Sign with gpg[2]", NULL },
    { "comp", 0, 0, G_OPTION_ARG_STRING, &sqfs_comp, "Squashfs compression", NULL },
    { "no-appstream", 'n', 0, G_OPTION_ARG_NONE, &no_appstream, "Do not check AppStream metadata", NULL },
    { "exclude-file", 0, 0, G_OPTION_ARG_STRING, &exclude_file, _exclude_file_desc, NULL },
    { "runtime-file", 0, 0, G_OPTION_ARG_STRING, &runtime_file, "Runtime file to use", NULL },
    { "sign-key", 0, 0, G_OPTION_ARG_STRING, &sign_key, "Key ID to use for gpg[2] signatures", NULL},
    { "sign-args", 0, 0, G_OPTION_ARG_STRING, &sign_args, "Extra arguments to use when signing with gpg[2]", NULL},
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &remaining_args, NULL, NULL },
    { 0,0,0,0,0,0,0 }
};

int
main (int argc, char *argv[])
{

    /* Parse Travis CI environment variables. 
     * https://docs.travis-ci.com/user/environment-variables/#Default-Environment-Variables
     * TRAVIS_COMMIT: The commit that the current build is testing.
     * TRAVIS_REPO_SLUG: The slug (in form: owner_name/repo_name) of the repository currently being built.
     * TRAVIS_TAG: If the current build is for a git tag, this variable is set to the tag’s name.
     * We cannot use g_environ_getenv (g_get_environ() since it is too new for CentOS 6 */
    // char* travis_commit;
    // travis_commit = getenv("TRAVIS_COMMIT");
    char* travis_repo_slug;
    travis_repo_slug = getenv("TRAVIS_REPO_SLUG");
    char* travis_tag;
    travis_tag = getenv("TRAVIS_TAG");
    char* travis_pull_request;
    travis_pull_request = getenv("TRAVIS_PULL_REQUEST");
    /* https://github.com/probonopd/uploadtool */
    char* github_token;
    github_token = getenv("GITHUB_TOKEN");

    /* Parse GitLab CI environment variables.
     * https://docs.gitlab.com/ee/ci/variables/#predefined-variables-environment-variables
     * echo "${CI_PROJECT_URL}/-/jobs/artifacts/${CI_COMMIT_REF_NAME}/raw/QtQuickApp-x86_64.AppImage?job=${CI_JOB_NAME}"
     */    
    char* CI_PROJECT_URL;
    CI_PROJECT_URL = getenv("CI_PROJECT_URL");
    char* CI_COMMIT_REF_NAME;
    CI_COMMIT_REF_NAME = getenv("CI_COMMIT_REF_NAME"); // The branch or tag name for which project is built
    char* CI_JOB_NAME;
    CI_JOB_NAME = getenv("CI_JOB_NAME"); // The name of the job as defined in .gitlab-ci.yml
    
    /* Parse OWD environment variable.
     * If it is available then cd there. It is the original CWD prior to running AppRun */
    char* owd_env = NULL;
    owd_env = getenv("OWD");    
    if(NULL!=owd_env){
        int ret;
        ret = chdir(owd_env);
        if (ret != 0){
            fprintf(stderr, "Could not cd into %s\n", owd_env);
            exit(1);
        }
    }
        
    GError *error = NULL;
    GOptionContext *context;
    char command[PATH_MAX];

    // initialize help text of argument
    sprintf(_exclude_file_desc, "Uses given file as exclude file for mksquashfs, in addition to %s.", APPIMAGEIGNORE);
    
    context = g_option_context_new ("SOURCE [DESTINATION] - Generate, extract, and inspect AppImages");
    g_option_context_add_main_entries (context, entries, NULL);
    // g_option_context_add_group (context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        fprintf(stderr, "Option parsing failed: %s\n", error->message);
        exit(1);
    }

    fprintf(
        stderr,
        "appimagetool, %s (commit %s), build %s built on %s\n",
        RELEASE_NAME, GIT_COMMIT, BUILD_NUMBER, BUILD_DATE
    );

    // always show version, but exit immediately if only the version number was requested
    if (showVersionOnly)
        exit(0);

    if(!((0 == strcmp(sqfs_comp, "gzip")) || (0 ==strcmp(sqfs_comp, "xz"))))
        die("Only gzip (faster execution, larger files) and xz (slower execution, smaller files) compression is supported at the moment. Let us know if there are reasons for more, should be easy to add. You could help the project by doing some systematic size/performance measurements. Watch for size, execution speed, and zsync delta size.");
    /* Check for dependencies here. Better fail early if they are not present. */
    if(! g_find_program_in_path ("file"))
        die("file command is missing but required, please install it");
#ifndef AUXILIARY_FILES_DESTINATION
    if(! g_find_program_in_path ("mksquashfs"))
        die("mksquashfs command is missing but required, please install it");
#else
    {
        // build path relative to appimagetool binary
        char *appimagetoolDirectory = dirname(realpath("/proc/self/exe", NULL));
        if (!appimagetoolDirectory) {
            g_print("Could not access /proc/self/exe\n");
            exit(1);
        }

        pathToMksquashfs = g_build_filename(appimagetoolDirectory, "..", AUXILIARY_FILES_DESTINATION, "mksquashfs", NULL);

        if (!g_file_test(pathToMksquashfs, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE)) {
            g_printf("No such file or directory: %s\n", pathToMksquashfs);
            g_free(pathToMksquashfs);
            exit(1);
        }
    }
#endif
    if(! g_find_program_in_path ("desktop-file-validate"))
        die("desktop-file-validate command is missing, please install it");
    if(! g_find_program_in_path ("zsyncmake"))
        g_print("WARNING: zsyncmake command is missing, please install it if you want to use binary delta updates\n");
    if(! no_appstream)
        if(! g_find_program_in_path ("appstreamcli"))
            g_print("WARNING: appstreamcli command is missing, please install it if you want to use AppStream metadata\n");
    if(! g_find_program_in_path ("gpg2") && ! g_find_program_in_path ("gpg"))
        g_print("WARNING: gpg2 or gpg command is missing, please install it if you want to create digital signatures\n");
    if(! g_find_program_in_path ("sha256sum") && ! g_find_program_in_path ("shasum"))
        g_print("WARNING: sha256sum or shasum command is missing, please install it if you want to create digital signatures\n");
    
    if(!&remaining_args[0])
        die("SOURCE is missing");
    
    /* If in list mode */
    if (list){
        sfs_ls(remaining_args[0]);
        exit(0);
    }
    
    /* If the first argument is a directory, then we assume that we should package it */
    if (g_file_test(remaining_args[0], G_FILE_TEST_IS_DIR)) {
        /* Parse VERSION environment variable.
 * We cannot use g_environ_getenv (g_get_environ() since it is too new for CentOS 6
 * Also, if VERSION is not set and -g is called and if git is on the path, use
 * git rev-parse --short HEAD
 * TODO: Might also want to somehow make use of
 * git rev-parse --abbrev-ref HEAD
 * git log -1 --format=%ci */
        gchar* version_env = getenv("VERSION");

        if (guess_update_information) {
            char* gitPath = g_find_program_in_path("git");

            if (gitPath != NULL) {
                if (version_env == NULL) {
                    GError* error = NULL;
                    gchar* out = NULL;

                    char command_line[] = "git rev-parse --short HEAD";

                    // *not* the exit code! must be interpreted via g_spawn_check_exit_status!
                    int exit_status = -1;

                    // g_spawn_command_line_sync returns whether the program succeeded
                    // its return value is buggy, hence we're using g_spawn_check_exit_status to check for errors
                    g_spawn_command_line_sync(command_line, &out, NULL, &exit_status, &error);

                    // g_spawn_command_line_sync might have set error already, in that case we don't want to overwrite
                    if (error != NULL || !g_spawn_check_exit_status(exit_status, &error)) {
                        if (error == NULL) {
                            g_printerr("Failed to run 'git rev-parse --short HEAD, but failed to interpret GLib error state: %d\n", exit_status);
                        } else {
                            g_printerr("Failed to run 'git rev-parse --short HEAD: %s (code %d)\n", error->message, error->code);
                        }
                    } else {
                        version_env = g_strstrip(out);

                        if (version_env != NULL) {
                            g_printerr("NOTE: Using the output of 'git rev-parse --short HEAD' as the version:\n");
                            g_printerr("      %s\n", version_env);
                            g_printerr("      Please set the $VERSION environment variable if this is not intended\n");
                        }
                    }
                }
            }

            free(gitPath);
        }

        char *destination;
        char source[PATH_MAX];
        realpath(remaining_args[0], source);
        
        /* Check if *.desktop file is present in source AppDir */
        gchar *desktop_file = find_first_matching_file_nonrecursive(source, "*.desktop");
        if(desktop_file == NULL){
            die("Desktop file not found, aborting");
        }
        if(verbose)
            fprintf (stdout, "Desktop file: %s\n", desktop_file);

        if(g_find_program_in_path ("desktop-file-validate")) {
            if(validate_desktop_file(desktop_file) != 0){
                fprintf(stderr, "ERROR: Desktop file contains errors. Please fix them. Please see\n");
                fprintf(stderr, "       https://standards.freedesktop.org/desktop-entry-spec/1.0/n");
                die("       for more information.");
            }
        }

        /* Read information from .desktop file */
        GKeyFile *kf = g_key_file_new ();
        if (!g_key_file_load_from_file (kf, desktop_file, G_KEY_FILE_KEEP_TRANSLATIONS | G_KEY_FILE_KEEP_COMMENTS, NULL))
            die(".desktop file cannot be parsed");
        if (!get_desktop_entry(kf, "Categories"))
            die(".desktop file is missing a Categories= key");
        
        if(verbose){
            fprintf (stderr,"Name: %s\n", get_desktop_entry(kf, "Name"));
            fprintf (stderr,"Icon: %s\n", get_desktop_entry(kf, "Icon"));
            fprintf (stderr,"Exec: %s\n", get_desktop_entry(kf, "Exec"));
            fprintf (stderr,"Comment: %s\n", get_desktop_entry(kf, "Comment"));
            fprintf (stderr,"Type: %s\n", get_desktop_entry(kf, "Type"));
            fprintf (stderr,"Categories: %s\n", get_desktop_entry(kf, "Categories"));
        }

        /* Determine the architecture */
        bool archs[4] = {0, 0, 0, 0};
        extract_arch_from_text(getenv("ARCH"), "Environmental variable ARCH", archs);
        if (count_archs(archs) != 1) {
            /* If no $ARCH variable is set check a file */
            /* We use the next best .so that we can find to determine the architecture */
            find_arch(source, "*.so.*", archs);
            int countArchs = count_archs(archs);
            if (countArchs != 1) {
                if (countArchs < 1)
                    fprintf(stderr, "Unable to guess the architecture of the AppDir source directory \"%s\"\n", remaining_args[0]);
                else
                    fprintf(stderr, "More than one architectures were found of the AppDir source directory \"%s\"\n", remaining_args[0]);
                fprintf(stderr, "A valid architecture with the ARCH environmental variable should be provided\ne.g. ARCH=x86_64 %s", argv[0]),
                        die(" ...");
            }
        }
        gchar* arch = getArchName(archs);
        fprintf(stderr, "Using architecture %s\n", arch);

        FILE *fp;
        char app_name_for_filename[PATH_MAX];
        sprintf(app_name_for_filename, "%s", get_desktop_entry(kf, "Name"));
        replacestr(app_name_for_filename, " ", "_");
        
        if(verbose)
            fprintf (stderr,"App name for filename: %s\n", app_name_for_filename);
        
        if (remaining_args[1]) {
            destination = remaining_args[1];
        } else {
            /* No destination has been specified, to let's construct one
            * TODO: Find out the architecture and use a $VERSION that might be around in the env */
            char dest_path[PATH_MAX];

            // if $VERSION is specified, we embed it into the filename
            if (version_env != NULL) {
                sprintf(dest_path, "%s-%s-%s.AppImage", app_name_for_filename, version_env, arch);
            } else {
                sprintf(dest_path, "%s-%s.AppImage", app_name_for_filename, arch);
            }

            destination = strdup(dest_path);
            replacestr(destination, " ", "_");
        }

        // if $VERSION is specified, we embed its value into the desktop file
        if (version_env != NULL) {
            g_key_file_set_string(kf, G_KEY_FILE_DESKTOP_GROUP, "X-AppImage-Version", version_env);

            if (!g_key_file_save_to_file(kf, desktop_file, NULL)) {
                fprintf(stderr, "Could not save modified desktop file\n");
                exit(1);
            }
        }

        fprintf (stdout, "%s should be packaged as %s\n", source, destination);
        /* Check if the Icon file is how it is expected */
        gchar* icon_name = get_desktop_entry(kf, "Icon");
        gchar* icon_file_path = NULL;
        gchar* icon_file_png;
        gchar* icon_file_svg;
        gchar* icon_file_xpm;
        icon_file_png = g_strdup_printf("%s/%s.png", source, icon_name);
        icon_file_svg = g_strdup_printf("%s/%s.svg", source, icon_name);
        icon_file_xpm = g_strdup_printf("%s/%s.xpm", source, icon_name);
        if (g_file_test(icon_file_png, G_FILE_TEST_IS_REGULAR)) {
            icon_file_path = icon_file_png;
        } else if(g_file_test(icon_file_svg, G_FILE_TEST_IS_REGULAR)) {
            icon_file_path = icon_file_svg;
        } else if(g_file_test(icon_file_xpm, G_FILE_TEST_IS_REGULAR)) {
            icon_file_path = icon_file_xpm;
        } else {
            fprintf (stderr, "%s{.png,.svg,.xpm} defined in desktop file but not found\n", icon_name);
            fprintf (stderr, "For example, you could put a 256x256 pixel png into\n");
            gchar *icon_name_with_png = g_strconcat(icon_name, ".png", NULL);
            gchar *example_path = g_build_filename(source, "/", icon_name_with_png, NULL);
            fprintf (stderr, "%s\n", example_path);
            exit(1);
        }
       
        /* Check if .DirIcon is present in source AppDir */
        gchar *diricon_path = g_build_filename(source, ".DirIcon", NULL);
        
        if (! g_file_test(diricon_path, G_FILE_TEST_EXISTS)){
            fprintf (stderr, "Deleting pre-existing .DirIcon\n");
            g_unlink(diricon_path);
        }
        if (! g_file_test(diricon_path, G_FILE_TEST_IS_REGULAR)){
            fprintf (stderr, "Creating .DirIcon symlink based on information from desktop file\n");
            int res = symlink(basename(icon_file_path), diricon_path);
            if(res)
                die("Could not symlink .DirIcon");
        }
        
        /* Check if AppStream upstream metadata is present in source AppDir */
        if(! no_appstream){
            char application_id[PATH_MAX];
            sprintf (application_id,  "%s", basename(desktop_file));
            replacestr(application_id, ".desktop", ".appdata.xml");
            gchar *appdata_path = g_build_filename(source, "/usr/share/metainfo/", application_id, NULL);
            if (! g_file_test(appdata_path, G_FILE_TEST_IS_REGULAR)){
                fprintf (stderr, "WARNING: AppStream upstream metadata is missing, please consider creating it\n");
                fprintf (stderr, "         in usr/share/metainfo/%s\n", application_id);
                fprintf (stderr, "         Please see https://www.freedesktop.org/software/appstream/docs/chap-Quickstart.html#sect-Quickstart-DesktopApps\n");
                fprintf (stderr, "         for more information or use the generator at http://output.jsbin.com/qoqukof.\n");
            } else {
                fprintf (stderr, "AppStream upstream metadata found in usr/share/metainfo/%s\n", application_id);
                /* Use ximion's appstreamcli to make sure that desktop file and appdata match together */
                if(g_find_program_in_path ("appstreamcli")) {
                    char *args[] = {
                        "appstreamcli",
                        "validate-tree",
                        source,
                        NULL
                    };
                    g_print("Trying to validate AppStream information with the appstreamcli tool\n");
                    g_print("In case of issues, please refer to https://github.com/ximion/appstream\n");
                    int ret = run_external(g_find_program_in_path ("appstreamcli"), args);
                    if (ret != 0)
                        die("Failed to validate AppStream information with appstreamcli");
                }
                /* It seems that hughsie's appstream-util does additional validations */
                if(g_find_program_in_path ("appstream-util")) {
                    char *args[] = {
                        "appstream-util",
                        "validate-relax",
                        appdata_path,
                        NULL
                    };
                    g_print("Trying to validate AppStream information with the appstream-util tool\n");
                    g_print("In case of issues, please refer to https://github.com/hughsie/appstream-glib\n");
                    int ret = run_external(g_find_program_in_path ("appstream-util"), args);
                    if (ret != 0)
                        die("Failed to validate AppStream information with appstream-util");
                }
            }
        }
        
        /* Upstream mksquashfs can currently not start writing at an offset,
        * so we need a patched one. https://github.com/plougher/squashfs-tools/pull/13
        * should hopefully change that. */

        fprintf (stderr, "Generating squashfs...\n");
        int size = 0;
        char* data = NULL;
        bool using_external_data = false;
        if (runtime_file != NULL) {
            if (!readFile(runtime_file, &size, &data))
                die("Unable to load provided runtime file");
            using_external_data = true;
        } else {
#ifdef HAVE_BINARY_RUNTIME
            /* runtime is embedded into this executable
            * http://stupefydeveloper.blogspot.de/2008/08/cc-embed-binary-data-into-elf.html */
            size = runtime_len;
            data = runtime;
#else
            die("No runtime file was provided");
#endif
        }
        if (verbose)
            printf("Size of the embedded runtime: %d bytes\n", size);
        
        int result = sfs_mksquashfs(source, destination, size);
        if(result != 0)
            die("sfs_mksquashfs error");
        
        fprintf (stderr, "Embedding ELF...\n");
        FILE *fpdst = fopen(destination, "rb+");
        if (fpdst == NULL) {
            die("Not able to open the AppImage for writing, aborting");
        }

        fseek(fpdst, 0, SEEK_SET);
        fwrite(data, size, 1, fpdst);
        fclose(fpdst);
        if (using_external_data)
            free(data);

        fprintf (stderr, "Marking the AppImage as executable...\n");
        if (chmod (destination, 0755) < 0) {
            printf("Could not set executable bit, aborting\n");
            exit(1);
        }
        
        if(bintray_user != NULL){
            if(bintray_repo != NULL){
                char buf[1024];
                sprintf(buf, "bintray-zsync|%s|%s|%s|%s-_latestVersion-%s.AppImage.zsync", bintray_user, bintray_repo, app_name_for_filename, app_name_for_filename, arch);
                updateinformation = buf;
                printf("%s\n", updateinformation);
            }
        }
        
        /* If the user has not provided update information but we know this is a Travis CI build,
         * then fill in update information based on TRAVIS_REPO_SLUG */
        if(guess_update_information){
            if(travis_repo_slug){
                if(!github_token) {
                    printf("Will not guess update information since $GITHUB_TOKEN is missing,\n");
                    if(0 != strcmp(travis_pull_request, "false")){
                        printf("please set it in the Travis CI Repository Settings for this project.\n");
                        printf("You can get one from https://github.com/settings/tokens\n");
                    } else {
                        printf("which is expected since this is a pull request\n");
                    }
                } else {
                    gchar *zsyncmake_path = g_find_program_in_path ("zsyncmake");
                    if(zsyncmake_path){
                        char buf[1024];
                        gchar **parts = g_strsplit (travis_repo_slug, "/", 2);
                        /* https://github.com/AppImage/AppImageSpec/blob/master/draft.md#github-releases 
                         * gh-releases-zsync|probono|AppImages|latest|Subsurface*-x86_64.AppImage.zsync */
                        gchar *channel = "continuous";
                            if(travis_tag != NULL){
                                if((strcmp(travis_tag, "") != 0) && (strcmp(travis_tag, "continuous") != 0)) {
                                    channel = "latest";
                                }
                            }
                        sprintf(buf, "gh-releases-zsync|%s|%s|%s|%s*-%s.AppImage.zsync", parts[0], parts[1], channel, app_name_for_filename, arch);
                        updateinformation = buf;
                        printf("Guessing update information based on $TRAVIS_TAG=%s and $TRAVIS_REPO_SLUG=%s\n", travis_tag, travis_repo_slug);
                        printf("%s\n", updateinformation);
                    } else {
                        printf("Will not guess update information since zsyncmake is missing\n");
                    }
                }
            } else if(CI_COMMIT_REF_NAME){
                // ${CI_PROJECT_URL}/-/jobs/artifacts/${CI_COMMIT_REF_NAME}/raw/QtQuickApp-x86_64.AppImage?job=${CI_JOB_NAME}
                gchar *zsyncmake_path = g_find_program_in_path ("zsyncmake");
                if(zsyncmake_path){
                    char buf[1024];
                    sprintf(buf, "zsync|%s/-/jobs/artifacts/%s/raw/%s-%s.AppImage.zsync?job=%s", CI_PROJECT_URL, CI_COMMIT_REF_NAME, app_name_for_filename, arch, CI_JOB_NAME);
                    updateinformation = buf;
                    printf("Guessing update information based on $CI_COMMIT_REF_NAME=%s and $CI_JOB_NAME=%s\n", CI_COMMIT_REF_NAME, CI_JOB_NAME);
                    printf("%s\n", updateinformation);
                } else {
                    printf("Will not guess update information since zsyncmake is missing\n");
                }
            }
        }
        
        /* If updateinformation was provided, then we check and embed it */
        if(updateinformation != NULL){
            if(!g_str_has_prefix(updateinformation,"zsync|"))
                if(!g_str_has_prefix(updateinformation,"bintray-zsync|"))
                    if(!g_str_has_prefix(updateinformation,"gh-releases-zsync|"))
                        die("The provided updateinformation is not in a recognized format");
                
            gchar **ui_type = g_strsplit_set(updateinformation, "|", -1);
                        
            if(verbose)
                printf("updateinformation type: %s\n", ui_type[0]);
            /* TODO: Further checking of the updateinformation */
            
          
            unsigned long ui_offset = 0;
            unsigned long ui_length = 0;

            bool rv = appimage_get_elf_section_offset_and_length(destination, ".upd_info", &ui_offset, &ui_length);

            if (!rv || ui_offset == 0 || ui_length == 0) {
                die("Could not find section .upd_info in runtime");
            }

            if(verbose) {
                printf("ui_offset: %lu\n", ui_offset);
                printf("ui_length: %lu\n", ui_length);
            }
            if(ui_offset == 0) {
                die("Could not determine offset for updateinformation");
            } else {
                if(strlen(updateinformation)>ui_length)
                    die("updateinformation does not fit into segment, aborting");
                FILE *fpdst2 = fopen(destination, "r+");
                if (fpdst2 == NULL)
                    die("Not able to open the destination file for writing, aborting");
                fseek(fpdst2, ui_offset, SEEK_SET);
                // fseek(fpdst2, ui_offset, SEEK_SET);
                // fwrite(0x00, 1, 1024, fpdst); // FIXME: Segfaults; why?
                // fseek(fpdst, ui_offset, SEEK_SET);
                fwrite(updateinformation, strlen(updateinformation), 1, fpdst2);
                fclose(fpdst2);
            }
        }

        // calculate and embed MD5 digest
        {
            fprintf(stderr, "Embedding MD5 digest\n");

            unsigned long digest_md5_offset = 0;
            unsigned long digest_md5_length = 0;

            bool rv = appimage_get_elf_section_offset_and_length(destination, ".digest_md5", &digest_md5_offset, &digest_md5_length);

            if (!rv || digest_md5_offset == 0 || digest_md5_length == 0) {
                die("Could not find section .digest_md5 in runtime");
            }

            static const unsigned long section_size = 16;

            if (digest_md5_length < section_size) {
                fprintf(
                    stderr,
                    ".digest_md5 section in runtime's ELF header is too small"
                    "(found %lu bytes, minimum required: %lu bytes)\n",
                    digest_md5_length, section_size
                );
                exit(1);
            }

            char digest_buffer[section_size];

            if (!appimage_type2_digest_md5(destination, digest_buffer)) {
                die("Failed to calculate MD5 digest");
            }

            FILE* destinationfp = fopen(destination, "r+");

            if (destinationfp == NULL) {
                die("Failed to open AppImage for updating");
            }

            if (fseek(destinationfp, digest_md5_offset, SEEK_SET) != 0) {
                fclose(destinationfp);
                die("Failed to embed MD5 digest: could not seek to section offset");
            }

            if (fwrite(digest_buffer, sizeof(char), section_size, destinationfp) != section_size) {
                fclose(destinationfp);
                die("Failed to embed MD5 digest: write failed");
            }

            fclose(destinationfp);
        }

        if (sign) {
            bool using_gpg = FALSE;
            bool using_shasum = FALSE;

            /* The user has indicated that he wants to sign */
            gchar* gpg2_path = g_find_program_in_path("gpg2");

            if (!gpg2_path) {
                gpg2_path = g_find_program_in_path("gpg");
                using_gpg = TRUE;
            }

            gchar* sha256sum_path = g_find_program_in_path("sha256sum");

            if (!sha256sum_path) {
                sha256sum_path = g_find_program_in_path("shasum");
                using_shasum = 1;
            }

            if (!gpg2_path) {
                fprintf(stderr, "gpg2 or gpg is not installed, cannot sign\n");
            } else if (!sha256sum_path) {
                fprintf(stderr, "sha256sum or shasum is not installed, cannot sign\n");
            } else {
                fprintf(stderr, "%s and %s are installed and user requested to sign, "
                                "hence signing\n", using_gpg ? "gpg" : "gpg2",
                    using_shasum ? "shasum" : "sha256sum");

                char* digestfile;
                digestfile = br_strcat(destination, ".digest");

                char* ascfile;
                ascfile = br_strcat(destination, ".digest.asc");

                if (g_file_test(digestfile, G_FILE_TEST_IS_REGULAR))
                    unlink(digestfile);

                if (using_shasum)
                    sprintf(command, "%s -a256 %s", sha256sum_path, destination);
                else
                    sprintf(command, "%s %s", sha256sum_path, destination);

                if (verbose)
                    fprintf(stderr, "%s\n", command);

                fp = popen(command, "r");

                if (fp == NULL)
                    die(using_shasum ? "shasum command did not succeed" : "sha256sum command did not succeed");

                char output[1024];

                fgets(output, sizeof(output) - 1, fp);


                if (pclose(fp) != 0)
                    die(using_shasum ? "shasum command did not succeed" : "sha256sum command did not succeed");

                // let's use a new scope to avoid polluting the scope too much with temporary variables
                {
                    // the output of sha256sum is always <hash> <filename>
                    // we have to split the string, the first item contains the hash then
                    gchar** split_result = g_strsplit_set(output, " ", -1);

                    // extract the first item reference, doesn't have to be free-d
                    const gchar* digest_only = split_result[0];

                    // print hash which is later signed for debugging purposes
                    printf("Signing using SHA256 digest: %s\n", digest_only);

                    FILE* fpx = fopen(digestfile, "w");

                    if (fpx != NULL) {
                        fputs(digest_only, fpx);
                        fclose(fpx);
                    }

                    g_strfreev(split_result);
                }

                fp = NULL;

                if (g_file_test(ascfile, G_FILE_TEST_IS_REGULAR))
                    unlink(ascfile);

                char* key_arg = NULL;

                if (sign_key && strlen(sign_key) > 0) {
                    key_arg = calloc(sizeof(char), strlen(sign_key) + strlen("--local-user ''"));

                    if (key_arg == NULL)
                        die("malloc() failed");

                    strcpy(key_arg, "--local-user '");
                    strcat(key_arg, sign_key);
                    strcat(key_arg, "'");
                }

                sprintf(command,
                    "%s --batch --detach-sign --armor %s %s %s",
                    gpg2_path, key_arg ? key_arg : "", sign_args ? sign_args : "", digestfile
                );

                free(key_arg);
                key_arg = NULL;

                if (verbose)
                    fprintf(stderr, "%s\n", command);

                fp = popen(command, "r");

                if (pclose(fp) != 0) {
                    fprintf(stderr, "ERROR: %s command did not succeed, could not sign, continuing\n", using_gpg ? "gpg" : "gpg2");
                } else {

                    fp = NULL;

                    FILE* destinationfp = fopen(destination, "r+");

                    // calculate signature
                    {
                        unsigned long sig_offset = 0;
                        unsigned long sig_length = 0;

                        bool rv = appimage_get_elf_section_offset_and_length(destination, ".sha256_sig", &sig_offset,
                                                                             &sig_length);

                        if (!rv || sig_offset == 0 || sig_length == 0) {
                            die("Could not find section .sha256_sig in runtime");
                        }

                        if (verbose) {
                            printf("sig_offset: %lu\n", sig_offset);
                            printf("sig_length: %lu\n", sig_length);
                        }

                        if (sig_offset == 0) {
                            die("Could not determine offset for signature");
                        }

                        if (destinationfp == NULL)
                            die("Not able to open the destination file for writing, aborting");

                        // if(strlen(updateinformation)>sig_length)
                        //     die("signature does not fit into segment, aborting");

                        fseek(destinationfp, sig_offset, SEEK_SET);

                        FILE* ascfilefp = fopen(ascfile, "rb");

                        if (ascfilefp == NULL) {
                            die("Not able to open the asc file for reading, aborting");
                        }

                        static const int bufsize = 1024;
                        char buffer[bufsize];

                        size_t totalBytesRead = 0;

                        while (!feof(ascfilefp)) {
                            size_t bytesRead = fread(buffer, sizeof(char), bufsize, ascfilefp);
                            totalBytesRead += bytesRead;

                            if (totalBytesRead > sig_length) {
                                die("Error: cannot embed key in AppImage: size exceeds reserved ELF section size");
                            }

                            size_t bytesWritten = fwrite(buffer, sizeof(char), bytesRead, destinationfp);

                            if (bytesRead != bytesWritten) {
                                char message[128];
                                sprintf(message, "Bytes read and written differ: %lu != %lu", (long unsigned) bytesRead,
                                        (long unsigned) bytesWritten);
                                die(message);
                            }
                        }

                        fclose(ascfilefp);
                        if (g_file_test(digestfile, G_FILE_TEST_IS_REGULAR))
                            unlink(digestfile);

                        if (sign_key == NULL || strlen(sign_key) > 0) {
                            // read which key was used to sign from signature
                            sprintf(command, "%s --batch --list-packets %s", gpg2_path, ascfile);

                            fp = popen(command, "r");

                            if (fp == NULL)
                                die("Failed to call gpg[2] to detect signature's key ID");

                            while (!feof(fp)) {
                                size_t bytesRead = fread(buffer, sizeof(char), bufsize, fp);

                                char* keyid_pos = strstr(buffer, "keyid");

                                if (keyid_pos == NULL)
                                    continue;

                                char* keyIDBegin = keyid_pos + strlen("keyid ");
                                char* endOfKeyID = strstr(keyIDBegin, "\n");

                                sign_key = calloc(endOfKeyID - keyIDBegin, sizeof(char));
                                memcpy(sign_key, keyIDBegin, endOfKeyID - keyIDBegin);
                            }

                            // read rest of process input to avoid broken pipe error
                            while (!feof(fp)) {
                                fread(buffer, sizeof(char), bufsize, fp);
                            }

                            int retval = pclose(fp);
                            fp = NULL;

                            if (retval != 0)
                                die("Failed to call gpg[2] to detect signature's key ID");
                        }

                        if (g_file_test(ascfile, G_FILE_TEST_IS_REGULAR))
                            unlink(ascfile);
                    }

                    // export key and write into section
                    {
                        sprintf(command, "%s --batch --export --armor %s", gpg2_path, sign_key);

                        unsigned long key_offset = 0, key_length = 0;

                        bool rv = appimage_get_elf_section_offset_and_length(destination, ".sig_key", &key_offset, &key_length);

                        if (verbose) {
                            printf("key_offset: %lu\n", key_offset);
                            printf("key_length: %lu\n", key_length);
                        }

                        if (!rv || key_offset == 0 || key_length == 0) {
                            die("Could not find section .sig_key in runtime");
                        }

                        fseek(destinationfp, key_offset, SEEK_SET);

                        fp = popen(command, "r");

                        if (fp == NULL)
                            die("Failed to call gpg[2] to export the signature key");

                        static const int bufsize = 1024;
                        char buffer[bufsize];

                        size_t totalBytesRead = 0;
                        while (!feof(fp)) {
                            size_t bytesRead = fread(buffer, sizeof(char), bufsize, fp);
                            totalBytesRead += bytesRead;

                            if (totalBytesRead > key_length) {
                                // read rest of process input to avoid broken pipe error
                                while (!feof(fp)) {
                                    fread(buffer, sizeof(char), bufsize, fp);
                                }

                                pclose(fp);
                                die("Error: cannot embed key in AppImage: size exceeds reserved ELF section size");
                            }

                            size_t bytesWritten = fwrite(buffer, sizeof(char), bytesRead, destinationfp);

                            if (bytesRead != bytesWritten) {
                                char message[128];
                                sprintf(message, "Error: Bytes read and written differ: %lu != %lu",
                                    (long unsigned) bytesRead, (long unsigned) bytesWritten);
                                die(message);
                            }
                        }

                        int exportexitcode = pclose(fp);
                        fp = NULL;

                        if (exportexitcode != 0) {
                            char message[128];
                            sprintf(message, "GPG key export failed: exit code %d", exportexitcode);
                            die(message);
                        }

                        fclose(destinationfp);
                    }
                }
            }
        }

        /* If updateinformation was provided, then we also generate the zsync file (after having signed the AppImage) */
        if (updateinformation != NULL) {
            gchar* zsyncmake_path = g_find_program_in_path("zsyncmake");
            if (!zsyncmake_path) {
                fprintf(stderr, "zsyncmake is not installed/bundled, skipping\n");
            } else {
                fprintf(stderr, "zsyncmake is available and updateinformation is provided, "
                                "hence generating zsync file\n");

                int pid = fork();

                if (verbose)
                    fprintf(stderr, "%s %s -u %s", zsyncmake_path, destination, basename(destination));

                if (pid == -1) {
                    // fork error
                    die("fork() failed");
                } else if (pid == 0) {
                    char* zsyncmake_command[] = {zsyncmake_path, destination, "-u", basename(destination), NULL};

                    // redirect stdout/stderr to /dev/null
                    int fd = open("/dev/null", O_WRONLY);

                    dup2(fd, 1);
                    dup2(fd, 2);
                    close(fd);

                    execv(zsyncmake_path, zsyncmake_command);

                    die("execv() should not return");
                } else {
                    int exitstatus;

                    if (waitpid(pid, &exitstatus, 0) == -1) {
                        perror("waitpid() failed");
                        exit(EXIT_FAILURE);
                    }

                    if (WEXITSTATUS(exitstatus) != 0)
                        die("zsyncmake command did not succeed");
                }
            }
        }

        fprintf(stderr, "Success\n\n");
        fprintf(stderr, "Please consider submitting your AppImage to AppImageHub, the crowd-sourced\n");
        fprintf(stderr, "central directory of available AppImages, by opening a pull request\n");
        fprintf(stderr, "at https://github.com/AppImage/appimage.github.io\n");

        return 0;
    } else if (g_file_test(remaining_args[0], G_FILE_TEST_IS_REGULAR)) {
        /* If the first argument is a regular file, then we assume that we should unpack it */
        fprintf(stdout, "%s is a file, assuming it is an AppImage and should be unpacked\n", remaining_args[0]);
        die("To be implemented");
        return 1;
    } else {
        fprintf(stderr, "Error: no such file or directory: %s\n", remaining_args[0]);
        return 1;
    }

    // should never be reached
    return 1;
}
