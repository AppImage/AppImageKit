#ifndef SQFS_DLOPEN_H
#define SQFS_DLOPEN_H

//#define ENABLE_DLOPEN

#ifdef ENABLE_DLOPEN

#include <dlfcn.h>

#include <time.h>
#include <utime.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>


/*** dlopen() stuff ***/

#define LIBNAME "libfuse.so.2"

void *libhandle;
int have_libloaded;
const char *load_library_errmsg;

#define LOAD_LIBRARY \
if (have_libloaded != 1) { \
  if (!(libhandle = dlopen(LIBNAME, RTLD_LAZY))) { \
    fprintf(stderr, "dlopen(): error loading " LIBNAME "\n\n%s", load_library_errmsg ); \
    exit(1); \
  } else { \
    have_libloaded = 1; \
  } \
}

#define STRINGIFY(x) #x

#define LOAD_SYMBOL(type,x,param) \
type (*dl_##x) param; \
*(void **) (&dl_##x) = dlsym(libhandle, STRINGIFY(x)); \
if (dlerror()) { \
  fprintf(stderr, "dlsym(): error loading symbol from " LIBNAME "\n\n%s", load_library_errmsg ); \
  CLOSE_LIBRARY; \
  exit(1); \
}

#define DL(x) dl_##x
#define CLOSE_LIBRARY dlclose(libhandle);


/*** libfuse stuff ***/

#define FUSE_ROOT_ID 1
#define FUSE_ARGS_INIT(argc, argv) { argc, argv, 0 }
#define FUSE_OPT_KEY(templ, key) { templ, -1U, key }
#define FUSE_OPT_KEY_OPT -1
#define FUSE_OPT_KEY_NONOPT -2
#define FUSE_OPT_END { NULL, 0, 0 }

enum fuse_buf_flags {
  FUSE_BUF_IS_FD		= (1 << 1),
  FUSE_BUF_FD_SEEK	= (1 << 2),
  FUSE_BUF_FD_RETRY	= (1 << 3),
};

typedef unsigned long fuse_ino_t;
typedef struct fuse_req *fuse_req_t;

struct fuse_chan;
struct fuse_pollhandle;

struct fuse_args {
  int argc;
  char **argv;
  int allocated;
};

typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *stbuf, off_t off);
typedef int (*fuse_opt_proc_t)(void *data, const char *arg, int key, struct fuse_args *outargs);
typedef struct fuse_dirhandle *fuse_dirh_t;
typedef int (*fuse_dirfil_t) (fuse_dirh_t h, const char *name, int type, ino_t ino);

struct fuse_file_info {
  int flags;
  unsigned long fh_old;
  int writepage;
  unsigned int direct_io : 1;
  unsigned int keep_cache : 1;
  unsigned int flush : 1;
  unsigned int nonseekable : 1;
  unsigned int flock_release : 1;
  unsigned int padding : 27;
  uint64_t fh;
  uint64_t lock_owner;
};

struct fuse_entry_param {
  fuse_ino_t ino;
  unsigned long generation;
  struct stat attr;
  double attr_timeout;
  double entry_timeout;
};

struct fuse_opt {
  const char *templ;
  unsigned long offset;
  int value;
};

struct fuse_forget_data {
  uint64_t ino;
  uint64_t nlookup;
};

struct fuse_conn_info {
  unsigned proto_major;
  unsigned proto_minor;
  unsigned async_read;
  unsigned max_write;
  unsigned max_readahead;
  unsigned capable;
  unsigned want;
  unsigned max_background;
  unsigned congestion_threshold;
  unsigned reserved[23];
};

struct fuse_buf {
  size_t size;
  enum fuse_buf_flags flags;
  void *mem;
  int fd;
  off_t pos;
};

struct fuse_bufvec {
  size_t count;
  size_t idx;
  size_t off;
  struct fuse_buf buf[1];
};

struct fuse_context {
  struct fuse *fuse;
  uid_t uid;
  gid_t gid;
  pid_t pid;
  void *private_data;
  mode_t umask;
};

struct fuse_operations {
	int (*getattr) (const char *, struct stat *);
	int (*readlink) (const char *, char *, size_t);
	int (*getdir) (const char *, fuse_dirh_t, fuse_dirfil_t);
	int (*mknod) (const char *, mode_t, dev_t);
	int (*mkdir) (const char *, mode_t);
	int (*unlink) (const char *);
	int (*rmdir) (const char *);
	int (*symlink) (const char *, const char *);
	int (*rename) (const char *, const char *);
	int (*link) (const char *, const char *);
	int (*chmod) (const char *, mode_t);
	int (*chown) (const char *, uid_t, gid_t);
	int (*truncate) (const char *, off_t);
	int (*utime) (const char *, struct utimbuf *);
	int (*open) (const char *, struct fuse_file_info *);
	int (*read) (const char *, char *, size_t, off_t, struct fuse_file_info *);
	int (*write) (const char *, const char *, size_t, off_t, struct fuse_file_info *);
	int (*statfs) (const char *, struct statvfs *);
	int (*flush) (const char *, struct fuse_file_info *);
	int (*release) (const char *, struct fuse_file_info *);
	int (*fsync) (const char *, int, struct fuse_file_info *);
	int (*setxattr) (const char *, const char *, const char *, size_t, int);
	int (*getxattr) (const char *, const char *, char *, size_t);
	int (*listxattr) (const char *, char *, size_t);
	int (*removexattr) (const char *, const char *);
	int (*opendir) (const char *, struct fuse_file_info *);
	int (*readdir) (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
	int (*releasedir) (const char *, struct fuse_file_info *);
	int (*fsyncdir) (const char *, int, struct fuse_file_info *);
	void *(*init) (struct fuse_conn_info *conn);
	void (*destroy) (void *);
	int (*access) (const char *, int);
	int (*create) (const char *, mode_t, struct fuse_file_info *);
	int (*ftruncate) (const char *, off_t, struct fuse_file_info *);
	int (*fgetattr) (const char *, struct stat *, struct fuse_file_info *);
	int (*lock) (const char *, struct fuse_file_info *, int cmd, struct flock *);
	int (*utimens) (const char *, const struct timespec tv[2]);
	int (*bmap) (const char *, size_t blocksize, uint64_t *idx);
	unsigned int flag_nullpath_ok:1;
	unsigned int flag_nopath:1;
	unsigned int flag_utime_omit_ok:1;
	unsigned int flag_reserved:29;
	int (*ioctl) (const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
	int (*poll) (const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp);
	int (*write_buf) (const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);
	int (*read_buf) (const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *);
	int (*flock) (const char *, struct fuse_file_info *, int op);
	int (*fallocate) (const char *, int, off_t, off_t, struct fuse_file_info *);
};

struct fuse_lowlevel_ops {
  void (*init) (void *userdata, struct fuse_conn_info *conn);
  void (*destroy) (void *userdata);
  void (*lookup) (fuse_req_t req, fuse_ino_t parent, const char *name);
  void (*forget) (fuse_req_t req, fuse_ino_t ino, unsigned long nlookup);
  void (*getattr) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
  void (*setattr) (fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);
  void (*readlink) (fuse_req_t req, fuse_ino_t ino);
  void (*mknod) (fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev);
  void (*mkdir) (fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
  void (*unlink) (fuse_req_t req, fuse_ino_t parent, const char *name);
  void (*rmdir) (fuse_req_t req, fuse_ino_t parent, const char *name);
  void (*symlink) (fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);
  void (*rename) (fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname);
  void (*link) (fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
  void (*open) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
  void (*read) (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
  void (*write) (fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
  void (*flush) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
  void (*release) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
  void (*fsync) (fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
  void (*opendir) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
  void (*readdir) (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
  void (*releasedir) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
  void (*fsyncdir) (fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
  void (*statfs) (fuse_req_t req, fuse_ino_t ino);
  void (*setxattr) (fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);
  void (*getxattr) (fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
  void (*listxattr) (fuse_req_t req, fuse_ino_t ino, size_t size);
  void (*removexattr) (fuse_req_t req, fuse_ino_t ino, const char *name);
  void (*access) (fuse_req_t req, fuse_ino_t ino, int mask);
  void (*create) (fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
  void (*getlk) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock);
  void (*setlk) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sleep);
  void (*bmap) (fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx);
  void (*ioctl) (fuse_req_t req, fuse_ino_t ino, int cmd, void *arg, struct fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);
  void (*poll) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct fuse_pollhandle *ph);
  void (*write_buf) (fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *bufv, off_t off, struct fuse_file_info *fi);
  void (*retrieve_reply) (fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, struct fuse_bufvec *bufv);
  void (*forget_multi) (fuse_req_t req, size_t count, struct fuse_forget_data *forgets);
  void (*flock) (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op);
  void (*fallocate) (fuse_req_t req, fuse_ino_t ino, int mode, off_t offset, off_t length, struct fuse_file_info *fi);
};

#else  /* !ENABLE_DLOPEN */

#define LOAD_LIBRARY
#define LOAD_SYMBOL(x)
#define DL(x)
#define CLOSE_LIBRARY

#endif  /* !ENABLE_DLOPEN */

#endif /* SQFS_DLOPEN_H */

