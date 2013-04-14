/***************************************************************************
 *   Copyright (c) 2005, 2006 by Dmitry Morozhnikov <dmiceman@mail.ru >    *
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

#ifndef _ISOFS_H
#define _ISOFS_H

#include <sys/stat.h>
#include <linux/iso_fs.h>
#include <linux/rock.h>

typedef int (*isofs_dir_fill_t) (void *buf, const char *name,
    const struct stat *stat, off_t off);

typedef struct _isofs_context {
    char *imagefile;
    int fd;
    int pd_have_rr; // 1 if primary descriptor have hierarchy with rrip extension
    struct iso_primary_descriptor pd;
    int supplementary; // 1 if supplementary descriptor found and in effect
    struct iso_supplementary_descriptor sd;
    struct iso_directory_record *root;
    int file_offset; // offset to begin of useful data (for .nrg files)
    int id_offset; // offset to CD001 inside file
    size_t block_size; // raw block size
    size_t block_offset; // offset from block start to data
    size_t data_size; // data size inside block 
    int susp; // parse susp entries
    int susp_skip; // skip bytes from susp SP entry
    int joliet_level; // joliet extension level (1, 2 or 3)
    ino_t last_ino;
} isofs_context;

typedef struct _isofs_inode {
    struct iso_directory_record *record;
    struct stat st;
    ino_t st_ino;
    time_t ctime; // cached value from record->date
    char *sl;
    size_t sl_len;
    char *nm;
    size_t nm_len;
    int cl_block;
    int pl_block;
    size_t zf_size;
    size_t real_size;
    char zf_algorithm[2];
    size_t zf_header_size;
    int zf_block_shift;
    int zf_nblocks;
    int *zf_blockptr;
    int PX; // 1 if PX entry found, st in effect
    int SL; // 1 if SL entry found, sl in effect
    int NM; // 1 if NM entry found, nm in effect
    int CL; // 1 if CL found, cl_block in effect
    int PL; // 1 if PL found, pl_block in effect
    int RE;
    int TF; // 1 if TF entry found, st in effect
    int ZF; // 1 if ZF entry found
} isofs_inode;

// borrowed from zisofs-tools
typedef struct _zf_file_header {
  char magic[8];
  char uncompressed_len[4];
  unsigned char header_size;
  unsigned char block_size;
  char reserved[2];
} zf_file_header;

// macros for iso_directory_record->flags
#define ISO_FLAGS_HIDDEN(x) (*((unsigned char *) x) & 1)
#define ISO_FLAGS_DIR(x) (*((unsigned char *) x) & (1 << 1))

// borrowed from linux kernel rock ridge code

#define SIG(A,B) ((A) | ((B) << 8)) /* isonum_721() */

// borrowed from linux kernel isofs code

/* Number conversion inlines, named after the section in ISO 9660
   they correspond to. */

#include <byteswap.h>

static inline int isonum_711(unsigned char *p)
{
    return *(unsigned char *)p;
}
// static inline int isonum_712(char *p)
// {
//     return *(s8 *)p;
// }
static inline unsigned int isonum_721(char *p)
{
#if defined(WORDS_BIGENDIAN)
    return *(unsigned short *)p;
#else
    return bswap_16(*(unsigned short *)p);
#endif
}
// static inline unsigned int isonum_722(char *p)
// {
//     return be16_to_cpu(get_unaligned((__le16 *)p));
// }
static inline unsigned int isonum_723(char *p)
{
    /* Ignore bigendian datum due to broken mastering programs */
#if defined(WORDS_BIGENDIAN)
    return bswap_16(*(unsigned short *)p);
#else
    return *(unsigned short *)p;
#endif
}
static inline unsigned int isonum_731(char *p)
{
#if defined(WORDS_BIGENDIAN)
    return bswap_32(*(unsigned int *)p);
#else
    return *(unsigned int *)p;
#endif
}
// static inline unsigned int isonum_732(char *p)
// {
//     return be32_to_cpu(get_unaligned((__le32 *)p));
// }
static inline unsigned int isonum_733(char *p)
{
    /* Ignore bigendian datum due to broken mastering programs */
#if defined(WORDS_BIGENDIAN)
    return bswap_32(*(unsigned int *)p);
#else
    return *(unsigned int *)p;
#endif
}

int isofs_real_preinit(char* imagefile, int fd);
void* isofs_real_init();

int isofs_real_opendir(const char *path);
int isofs_real_readdir(const char *path, void *filler_buf, isofs_dir_fill_t filler);
int isofs_real_getattr(const char *path, struct stat *stbuf);
int isofs_real_readlink(const char *path, char *target, size_t size);
int isofs_real_open(const char *path);
int isofs_real_read(const char *path, char *out_buf, size_t size, off_t offset);
int isofs_real_statfs(struct statfs *stbuf);

#endif // _ISOFS_H
