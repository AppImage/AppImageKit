/**************************************************************************
 * 
 * Copyright (c) 2004-18 Simon Peter
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

#include <stdlib.h>
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

#include "elf.h"
#include "getsection.h"

#ifdef __linux__
#define HAVE_BINARY_RUNTIME
extern int _binary_runtime_start;
extern int _binary_runtime_end;
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
static gboolean guessupdateinformation = FALSE;
gchar *bintray_user = NULL;
gchar *bintray_repo = NULL;
gchar *sqfs_comp = "gzip";
gchar *exclude_file = NULL;
gchar *runtime_file = NULL;
gchar *sign_args = NULL;
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
    
    unsigned long fs_offset = get_elf_size(image);
    
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

        args[i++] = "-mkfs-fixed-time";
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
        return "ARM";
    else if (archs[fARCH_aarch64])
        return "ARM_aarch64";
    else
        return "all";
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

void guess_arch_of_file(const gchar *archfile, bool* archs) {
    char line[PATH_MAX];
    char command[PATH_MAX];
    sprintf(command, "/usr/bin/file -L -N -b %s", archfile);
    FILE* fp = popen(command, "r");
    if (fp == NULL)
        die("Failed to run file command");
    fgets(line, sizeof (line) - 1, fp);
    pclose(fp);
    extract_arch_from_text(g_strsplit_set(line, ",", -1)[1], archfile, archs);
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

// #####################################################################

static GOptionEntry entries[] =
{
    { "list", 'l', 0, G_OPTION_ARG_NONE, &list, "List files in SOURCE AppImage", NULL },
    { "updateinformation", 'u', 0, G_OPTION_ARG_STRING, &updateinformation, "Embed update information STRING; if zsyncmake is installed, generate zsync file", NULL },
    { "guess", 'g', 0, G_OPTION_ARG_NONE, &guessupdateinformation, "Guess update information based on Travis CI environment variables", NULL },
    { "bintray-user", 0, 0, G_OPTION_ARG_STRING, &bintray_user, "Bintray user name", NULL },
    { "bintray-repo", 0, 0, G_OPTION_ARG_STRING, &bintray_repo, "Bintray repository", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &showVersionOnly, "Show version number", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Produce verbose output", NULL },
    { "sign", 's', 0, G_OPTION_ARG_NONE, &sign, "Sign with gpg[2]", NULL },
    { "comp", 0, 0, G_OPTION_ARG_STRING, &sqfs_comp, "Squashfs compression", NULL },
    { "no-appstream", 'n', 0, G_OPTION_ARG_NONE, &no_appstream, "Do not check AppStream metadata", NULL },
    { "exclude-file", 0, 0, G_OPTION_ARG_STRING, &exclude_file, _exclude_file_desc, NULL },
    { "runtime-file", 0, 0, G_OPTION_ARG_STRING, &runtime_file, "Runtime file to use", NULL },
    { "sign-args", 0, 0, G_OPTION_ARG_STRING, &sign_args, "Extra arguments to use when signing with gpg[2]", NULL},
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &remaining_args, NULL, NULL },
    { 0,0,0,0,0,0,0 }
};

int
main (int argc, char *argv[])
{
    /* Parse VERSION environment variable.
     * We cannot use g_environ_getenv (g_get_environ() since it is too new for CentOS 6 */
    char* version_env;
    version_env = getenv("VERSION");

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
    if (g_file_test (remaining_args[0], G_FILE_TEST_IS_DIR)){
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
                fprintf(stderr, "       https://standards.freedesktop.org/desktop-entry-spec/latest/\n");
                die("       for more information.");
            }
        }

        /* Read information from .desktop file */
        GKeyFile *kf = g_key_file_new ();
        if (!g_key_file_load_from_file (kf, desktop_file, 0, NULL))
            die(".desktop file cannot be parsed");
        
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
            sprintf (dest_path, "%s-%s.AppImage", app_name_for_filename, arch);
            
            if(verbose)
                fprintf (stderr,"dest_path: %s\n", dest_path);
            
            if (version_env!=NULL)
                sprintf (dest_path, "%s-%s-%s.AppImage", app_name_for_filename, version_env, arch);
            
            destination = dest_path;
            replacestr(destination, " ", "_");
        }
        fprintf (stdout, "%s should be packaged as %s\n", source, destination);
        /* Check if the Icon file is how it is expected */
        gchar* icon_name = get_desktop_entry(kf, "Icon");
        gchar* icon_file_path = NULL;
        gchar* icon_file_png;
        gchar* icon_file_svg;
        gchar* icon_file_svgz;
        gchar* icon_file_xpm;
        icon_file_png = g_strdup_printf("%s/%s.png", source, icon_name);
        icon_file_svg = g_strdup_printf("%s/%s.svg", source, icon_name);
        icon_file_svgz = g_strdup_printf("%s/%s.svgz", source, icon_name);
        icon_file_xpm = g_strdup_printf("%s/%s.xpm", source, icon_name);
        if (g_file_test(icon_file_png, G_FILE_TEST_IS_REGULAR)) {
            icon_file_path = icon_file_png;
        } else if(g_file_test(icon_file_svg, G_FILE_TEST_IS_REGULAR)) {
            icon_file_path = icon_file_svg;
        } else if(g_file_test(icon_file_svgz, G_FILE_TEST_IS_REGULAR)) {
            icon_file_path = icon_file_svgz;
        } else if(g_file_test(icon_file_xpm, G_FILE_TEST_IS_REGULAR)) {
            icon_file_path = icon_file_xpm;
        } else {
            fprintf (stderr, "%s{.png,.svg,.svgz,.xpm} defined in desktop file but not found\n", icon_name);
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
                    sprintf (command, "%s validate-tree %s", g_find_program_in_path ("appstreamcli"), source);
                    g_print("Trying to validate AppStream information with the appstreamcli tool\n");
                    g_print("In case of issues, please refer to https://github.com/ximion/appstream\n");
                    int ret = system(command);
                    if (ret != 0)
                        die("Failed to validate AppStream information with appstreamcli");
                }
                /* It seems that hughsie's appstream-util does additional validations */
                if(g_find_program_in_path ("appstream-util")) {
                    sprintf (command, "%s validate-relax %s", g_find_program_in_path ("appstream-util"), appdata_path);
                    g_print("Trying to validate AppStream information with the appstream-util tool\n");
                    g_print("In case of issues, please refer to https://github.com/hughsie/appstream-glib\n");
                    int ret = system(command);
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
            size = (int)((void *)&_binary_runtime_end - (void *)&_binary_runtime_start);
            data = (char *)&_binary_runtime_start;
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
        if(guessupdateinformation){
            if(!travis_repo_slug){
                printf("Cannot guess update information since $TRAVIS_REPO_SLUG is missing\n");
            } else if(!github_token) {
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
            get_elf_section_offset_and_length(destination, ".upd_info", &ui_offset, &ui_length);
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

        if(sign){
            bool using_gpg = FALSE;
            bool using_shasum = FALSE;
            /* The user has indicated that he wants to sign */
            gchar *gpg2_path = g_find_program_in_path ("gpg2");
            if (!gpg2_path) {
                gpg2_path = g_find_program_in_path ("gpg");
                using_gpg = TRUE;
            }
            gchar *sha256sum_path = g_find_program_in_path ("sha256sum");
            if (!sha256sum_path) {
                sha256sum_path = g_find_program_in_path ("shasum");
                using_shasum = 1;
            }
            if(!gpg2_path){
                fprintf (stderr, "gpg2 or gpg is not installed, cannot sign\n");
            }
            else if(!sha256sum_path) {
                fprintf(stderr, "sha256sum or shasum is not installed, cannot sign\n");
            } else {
                fprintf(stderr, "%s and %s are installed and user requested to sign, "
                        "hence signing\n", using_gpg ? "gpg" : "gpg2",
                        using_shasum ? "shasum" : "sha256sum");
                char *digestfile;
                digestfile = br_strcat(destination, ".digest");
                char *ascfile;
                ascfile = br_strcat(destination, ".digest.asc");
                if (g_file_test (digestfile, G_FILE_TEST_IS_REGULAR))
                    unlink(digestfile);
                if (using_shasum)
                    sprintf (command, "%s -a256 %s", sha256sum_path, destination);
                else
                    sprintf (command, "%s %s", sha256sum_path, destination);
                if(verbose)
                    fprintf (stderr, "%s\n", command);
                fp = popen(command, "r");
                if (fp == NULL)
                    die(using_shasum ? "shasum command did not succeed" : "sha256sum command did not succeed");
                char output[1024];
                fgets(output, sizeof (output) - 1, fp);
                if (verbose)
                    printf("%s: %s\n", using_shasum ? "shasum" : "sha256sum",
                        g_strsplit_set(output, " ", -1)[0]);
                FILE *fpx = fopen(digestfile, "w");
                if (fpx != NULL)
                {
                    fputs(g_strsplit_set(output, " ", -1)[0], fpx);
                    fclose(fpx);
                }
                int shasum_exit_status = pclose(fp);
                if(WEXITSTATUS(shasum_exit_status) != 0)
                    die(using_shasum ? "shasum command did not succeed" : "sha256sum command did not succeed");
                if (g_file_test (ascfile, G_FILE_TEST_IS_REGULAR))
                    unlink(ascfile);
                sprintf (command, "%s --detach-sign --armor %s %s", gpg2_path, sign_args ? sign_args : "", digestfile);
                if(verbose)
                    fprintf (stderr, "%s\n", command);
                fp = popen(command, "r");
                int gpg_exit_status = pclose(fp);
                if(WEXITSTATUS(gpg_exit_status) != 0) { 
                    fprintf (stderr, "ERROR: %s command did not succeed, could not sign, continuing\n", using_gpg ? "gpg" : "gpg2");
                } else {
                    unsigned long sig_offset = 0;
                    unsigned long sig_length = 0;
                    get_elf_section_offset_and_length(destination, ".sha256_sig", &sig_offset, &sig_length);
                    if(verbose) {
                        printf("sig_offset: %lu\n", sig_offset);
                        printf("sig_length: %lu\n", sig_length);
                    }
                    if(sig_offset == 0) {
                        die("Could not determine offset for signature");
                    } else {
                        FILE *fpdst3 = fopen(destination, "r+");
                        if (fpdst3 == NULL)
                            die("Not able to open the destination file for writing, aborting");
    //                    if(strlen(updateinformation)>sig_length)
    //                        die("signature does not fit into segment, aborting");
                        fseek(fpdst3, sig_offset, SEEK_SET);
                        FILE *fpsrc2 = fopen(ascfile, "rb");
                        if (fpsrc2 == NULL) {
                            die("Not able to open the asc file for reading, aborting");
                        }
                        char byte;
                        while (!feof(fpsrc2))
                        {
                            fread(&byte, sizeof(char), 1, fpsrc2);
                            fwrite(&byte, sizeof(char), 1, fpdst3);
                        }
                        fclose(fpsrc2);
                        fclose(fpdst3);
                    }
                    if (g_file_test (ascfile, G_FILE_TEST_IS_REGULAR))
                        unlink(ascfile);
                    if (g_file_test (digestfile, G_FILE_TEST_IS_REGULAR))
                        unlink(digestfile);
                }
            }
        }
        
        /* If updateinformation was provided, then we also generate the zsync file (after having signed the AppImage) */
        if(updateinformation != NULL){
            gchar *zsyncmake_path = g_find_program_in_path ("zsyncmake");
            if(!zsyncmake_path){
                fprintf (stderr, "zsyncmake is not installed/bundled, skipping\n");
            } else {
                fprintf (stderr, "zsyncmake is available and updateinformation is provided, "
                "hence generating zsync file\n");
                sprintf (command, "%s %s -u %s", zsyncmake_path, destination, basename(destination));
                if(verbose)
                    fprintf (stderr, "%s\n", command);
                fp = popen(command, "r");
                if (fp == NULL)
                    die("Failed to run zsyncmake command");
                int exitstatus = pclose(fp);
                if (WEXITSTATUS(exitstatus) != 0)
                    die("zsyncmake command did not succeed");
            }
         } 
         
        fprintf (stderr, "Success\n\n");
        fprintf (stderr, "Please consider submitting your AppImage to AppImageHub, the crowd-sourced\n");
        fprintf (stderr, "central directory of available AppImages, by opening a pull request\n");
        fprintf (stderr, "at https://github.com/AppImage/appimage.github.io\n");
        }
    
    /* If the first argument is a regular file, then we assume that we should unpack it */
    if (g_file_test (remaining_args[0], G_FILE_TEST_IS_REGULAR)){
        fprintf (stdout, "%s is a file, assuming it is an AppImage and should be unpacked\n", remaining_args[0]);
        die("To be implemented");
    }
    
    return 0;    
}
