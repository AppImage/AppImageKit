/***************************************************************************
 *   Copyright (c) 2005, 2006 by Dmitry Morozhnikov <dmiceman@mail.ru >    *
 *   Copyright (c) 2005-10 Simon Peter                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/stat.h>
#include <mntent.h>
#include <sys/param.h>
#include <linux/iso_fs.h>

#define FUSE_USE_VERSION 22
#include <fuse.h>
#include "isofs.h"

#ifdef __GNUC__
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

static char *imagefile = NULL;
static char *mount_point = NULL;
static int image_fd = -1;

int maintain_mount_point = 1;

char* iocharset;

char* normalize_name(const char* fname) {
    char* abs_fname = (char *) malloc(PATH_MAX);
    realpath(fname, abs_fname);
    // ignore errors from realpath()
    return abs_fname;
};

int check_mount_point() {
    struct stat st;
    int rc = lstat(mount_point, &st);
    if(rc == -1 && errno == ENOENT) {
        // directory does not exists, createcontext
        rc = mkdir(mount_point, 0777); // let`s underlying filesystem manage permissions
        if(rc != 0) {
            perror("Can't create mount point");
            return -EIO;
        };
    } else if(rc == -1) {
        perror("Can't check mount point");
        return -1;
    };
    return 0;
};

void del_mount_point() {
    int rc = rmdir(mount_point);
    if(rc != 0) {
        perror("Can't delete mount point");
    };
};

static int isofs_getattr(const char *path, struct stat *stbuf)
{
    return isofs_real_getattr(path, stbuf);
}

static int isofs_readlink(const char *path, char *target, size_t size) {
    return isofs_real_readlink(path, target, size);
};

static int isofs_open(const char *path, struct fuse_file_info *UNUSED(fi))
{
    return isofs_real_open(path);
}

static int isofs_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *UNUSED(fi))
{
    return isofs_real_read(path, buf, size, offset);
}

static int isofs_flush(const char *UNUSED(path), struct fuse_file_info *UNUSED(fi)) {
    return 0;
};

static void* isofs_init() {
    int rc;
    run_when_fuse_fs_mounted();
    return isofs_real_init();
};

static void isofs_destroy(void* param) {
    return;
};

static int isofs_opendir(const char *path, struct fuse_file_info *UNUSED(fi)) {
    return isofs_real_opendir(path);
};

static int isofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t UNUSED(offset),
    struct fuse_file_info *UNUSED(fi)) {
    return isofs_real_readdir(path, buf, filler);
};

static int isofs_statfs(const char *UNUSED(path), struct statfs *stbuf)
{
    return isofs_real_statfs(stbuf);
}

static struct fuse_operations isofs_oper = {
    .getattr    = isofs_getattr,
    .readlink   = isofs_readlink,
    .open       = isofs_open,
    .read       = isofs_read,
    .flush      = isofs_flush,
    .init       = isofs_init,
    .destroy    = isofs_destroy,
    .opendir    = isofs_opendir,
    .readdir    = isofs_readdir,
    .statfs     = isofs_statfs,
};

int ext2_main(int argc, char *argv[])
{  
    imagefile = normalize_name(argv[0]);
    image_fd = open(imagefile, O_RDONLY);
    if(image_fd == -1) {
        perror("Can't open image file");
        fprintf(stderr, "Supplied image file name: \"%s\"\n", imagefile);
        exit(EXIT_FAILURE);
    };
    
    mount_point = normalize_name(argv[1]);
    
    if(!iocharset) {
            iocharset = "UTF-8//IGNORE";
    };
    
    int rc;
    if(maintain_mount_point) {
        rc = check_mount_point();
        if(rc != 0) {
            exit(EXIT_FAILURE);
        };
    };
    if(maintain_mount_point) {
        rc = atexit(del_mount_point);
        if(rc != 0) {
            fprintf(stderr, "Can't set exit function\n");
            exit(EXIT_FAILURE);
        }
    };
    
    // will exit in case of failure
    rc = isofs_real_preinit(imagefile, image_fd);

    return fuse_main(argc, argv, &isofs_oper);
};

