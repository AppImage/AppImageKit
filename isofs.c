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

// for struct tm->tm_gmtoff
#define _BSD_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <zlib.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/statfs.h>
#include <iconv.h>

#include "isofs.h"

static isofs_context context;
static GHashTable *lookup_table;
static GHashTable *negative_lookup_table;
static char *negative_value = "does not exist";
static pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;

static int isofs_check_rr(struct iso_directory_record *root_record);
static int isofs_read_raw_block(int block, char *buf);

extern char* iocharset;

// locally implement g_strv_length, this is missing in glib2 for rhel3/rhel4
// -- Chandan Dutta Chowdhury 2007-07-06
guint local_g_strv_length (gchar **str_array) {
    guint i = 0;
    g_return_val_if_fail (str_array != NULL, 0);
    while (str_array[i])
    ++i;
    return i;
};

int isofs_real_preinit( char* imagefile, int fd) {
    
    memset(& context, 0, sizeof(isofs_context));
    
    context.imagefile = imagefile;
    context.fd = fd;
    
    // trying to read all volume descriptors
    struct iso_volume_descriptor *vd = 
        (struct iso_volume_descriptor *) malloc(sizeof(struct iso_volume_descriptor));
    if(!vd) {
//        perror("Can`t malloc: ");
        exit(ENOMEM);
    };
    int vd_num = 0;
    
    // defaults for iso
    context.block_size = 2048;
    context.data_size = 2048;
    context.block_offset = 0;
    context.file_offset = 0;
    
    enum {
        IDOFF_ISO_2048 = 2048 * 16,
        IDOFF_MODE1_2352 = 2352 * 16 + 16,
        IDOFF_MODE2_2352_RAW = 2352 * 16,
        IDOFF_MODE2_2352 = 2352 * 16 + 24,
        IDOFF_MODE2_2336 = 2336 * 16 + 16,
        IDOFF_NRG = 2048 * 16 + 307200,
    };
    int iso_offsets[] = {IDOFF_ISO_2048, IDOFF_MODE2_2336, IDOFF_MODE2_2352_RAW, IDOFF_NRG};
    // try to find CD001 identifier
    int i;
    for(i = 0; i < 4; i++) {
        if(lseek(fd, iso_offsets[i], SEEK_SET) == -1) {
//            perror("can`t lseek() to next possible data start position; is it really supported file?");
	      perror("Unsupported file");
            exit(EIO);
        };
        ssize_t size = read(fd, vd, sizeof(struct iso_volume_descriptor));
        if(size != sizeof(struct iso_volume_descriptor)) {
//            fprintf(stderr, "only %d bytes read from position %d, %d required; is it really supported file?\n", 
//                size, iso_offsets[i], sizeof(struct iso_volume_descriptor));
	      perror("Unsupported file");
            exit(EIO);
        };
        char *vd_id = (char *) vd->id;
        if(strncmp("CD001", vd_id, 5) == 0) {
            // found CD001!
            // fill context with information about block size and block offsets
            context.id_offset = iso_offsets[i];
            switch(iso_offsets[i]) {
                case IDOFF_ISO_2048:
                    // normal iso file
                    // use defaults
                    break;
                case IDOFF_MODE2_2352_RAW:
                    context.block_size = 2352;
                    break;
                case IDOFF_MODE2_2336:
                    context.block_size = 2336;
                    context.block_offset = 16;
                    break;
                case IDOFF_NRG:
                    context.file_offset = 307200;
                    break;
                default:
                    break;
            };
            break;
        } else if(strncmp("CD001", vd_id + 16, 5) == 0) {
            context.id_offset = iso_offsets[i] + 16;
            context.block_size = 2352;
            context.block_offset = 16;
            break;
        } else if(strncmp("CD001", vd_id + 24, 5) == 0) {
            context.id_offset = iso_offsets[i] + 24;
            context.block_size = 2352;
            context.block_offset = 24;
            break;
        };
    };
    
/*    printf("CD001 found at %d, bs %d, boff %d, ds %d\n", 
        context.id_offset, context.block_size, context.block_offset, context.data_size);*/
    while(1) {
        if(lseek(fd, context.block_size * (16 + vd_num) + 
            context.block_offset + context.file_offset, SEEK_SET) == -1) {
//            perror("can`t lseek() to next volume descriptor");
            exit(EIO);
        };
        ssize_t size = read(fd, vd, sizeof(struct iso_volume_descriptor));
        if(size != sizeof(struct iso_volume_descriptor)) {
//            fprintf(stderr, "only %d bytes read from volume descriptor %d, %d required\n", 
//                size, vd_num, sizeof(struct iso_volume_descriptor));
            exit(EIO);
        };
        
        int vd_type = isonum_711((unsigned char *)vd->type);
//         printf("init: found volume descriptor type %d, vd_num %d\n", vd_type, vd_num);
        
        if(strncmp("CD001", vd->id, 5) != 0) {
            if(vd_num > 16) {
                // no more trying
//                fprintf(stderr, "init: wrong standard identifier in volume descriptor %d, exiting..\n", vd_num);
                exit(EIO);
            } else {
                // try to continue
//                fprintf(stderr, "init: wrong standard identifier in volume descriptor %d, skipping..\n", vd_num);
            };
        } else {
            switch(vd_type) {
                case ISO_VD_PRIMARY:
                    // check if this is only primary descriptor found
                    if(context.pd.type[0]) {
//                        fprintf(stderr, "init: primary volume descriptor already found, skipping..\n");
                    } else {
                        memcpy(& context.pd, vd, sizeof(struct iso_volume_descriptor));
                        context.root = (struct iso_directory_record *)& context.pd.root_directory_record;
                        context.data_size = isonum_723(context.pd.logical_block_size);
                        
                        if(!context.block_size) {
//                            fprintf(stderr, "init: wrong block data size %d, using default 2048\n", context.data_size);
                            context.data_size = 2048;
                        };
                        
                        if(context.block_size != 2048) {
                            // report unusual data block size
                            // later
                            // printf("Data block size: %d\n", context.block_size);
                        };
                        
                        if(isofs_check_rr(context.root)) {
                            context.pd_have_rr = 1;
                        };
                    };
                    break;
                
                case ISO_VD_SUPPLEMENTARY:
                    {
                        struct iso_supplementary_descriptor *sd = (struct iso_supplementary_descriptor *) vd;
                        
                        if(!context.pd.type[0]) {
//                            fprintf(stderr, "init: supplementary volume descriptor found, but no primary descriptor!\n");
                            exit(EIO);
                        } else {
                            int joliet_level = 0;
                            
                            if(sd->escape[0] == 0x25 && sd->escape[1] == 0x2f) {
                                switch(sd->escape[2]) {
                                    case 0x40:
                                        joliet_level = 1;
                                        break;
                                    case 0x43:
                                        joliet_level = 2;
                                        break;
                                    case 0x45:
                                        joliet_level = 3;
                                        break;
                                };
                            };
                            
                            int have_rr = 
                                isofs_check_rr((struct iso_directory_record *) sd->root_directory_record);
                            
                            // switch to SVD only if it contain RRIP or if PVD have no RRIP
                            // in other words, prefer VD with RRIP
                            if((joliet_level && have_rr) ||
                                (have_rr && !context.pd_have_rr) || 
                                (joliet_level && !context.pd_have_rr)) {
                                
                                context.joliet_level = joliet_level;
                                memcpy(& context.sd, vd, sizeof(struct iso_volume_descriptor));
                                context.supplementary = 1;
                                
                                context.root = (struct iso_directory_record *) context.sd.root_directory_record;
                                
                                // printf("init: switching to supplementary descriptor %d, joliet_level %d, have_rr %d\n", 
                                //     vd_num, context.joliet_level, have_rr);
                            } else {
                                context.joliet_level = 0;
                                // printf("init: found supplementary descriptor %d, flags %d\n", 
                                //     vd_num, isonum_711(sd->flags));
                            };
                        };
                    };
                    break;
                
                case 0:
                    // boot record, not intresting..
                    break;
                
                case ISO_VD_END:
                    free(vd);
                    goto out;
                    break;
                    
                default:
//                    fprintf(stderr, "init: unsupported volume descriptor type %d, vd_num %d\n", 
//                        vd_type, vd_num);
                    break;
            };
        };
        
        vd_num += 1;
    };
out:
    
    if(!context.pd.type[0]) {
//        fprintf(stderr, "init: primary volume descriptor not found! exiting..\n");
        exit(EIO);
    };
    
    context.susp = 0;
    context.susp_skip = 0;
    
    lookup_table = g_hash_table_new(g_str_hash, g_str_equal);
    negative_lookup_table = g_hash_table_new(g_str_hash, g_str_equal);
    
    isofs_inode *inode = (isofs_inode *) malloc(sizeof(isofs_inode));
    if(!inode) {
//        perror("Can`t malloc: ");
        exit(ENOMEM);
    };
    memset(inode, 0, sizeof(isofs_inode));
    inode->record = context.root;
    context.last_ino++; // set to 1
    inode->st_ino = context.last_ino;
    context.last_ino++;
    
    g_hash_table_insert(lookup_table, "/", inode);
    
    return 0;
};

static char* dstr(char* str, const char* src, int len) {
    int i;
    strncpy(str, src, len);
    str[len] = '\0';
    for(i = len - 1; i >= 0; --i) {
        if(str[i] == '\0' || str[i] == ' ' || str[i] == '\t' || str[i] == '\r' || str[i] == '\n') {
            str[i] = '\0';
        } else {
            return str;
        };
    };
    return str;
};

void* isofs_real_init() {
/*    if(context.file_offset == 307200) {
        printf("NRG image found\n");
    } else if(context.block_size == 2048) {
        printf("ISO9660 image found\n");
    } else if(context.block_size == 2352 && context.block_offset == 0) {
        printf("MODE2 RAW BIN image found\n");
    } else if(context.block_size == 2352 && context.block_offset == 16) {
        printf("MODE1 BIN image found (or CCD MODE1 image, or MDF image)\n");
    } else if(context.block_size == 2352 && context.block_offset == 24) {
        printf("MODE2 BIN image found (or CCD MODE2 image)\n");
    } else if(context.block_size == 2336 && context.block_offset == 16) {
        printf("MODE2/2336 BIN image found\n");
    } else {
        printf("UNKNOWN image found; probably will not work\n");
    };
    
    if(context.block_size != 2048) {
        // report unusual data block size
        printf("Data block size: %d\n", context.block_size);
    };
    
    char buf[129];
    
    printf("System Identifier                 : %s\n", dstr(buf, context.pd.system_id, 32));
    printf("Volume Identifier                 : %.32s\n", dstr(buf, context.pd.volume_id, 32));
    printf("Volume Set Identifier             : %.128s\n", dstr(buf, context.pd.volume_set_id, 128));
    printf("Publisher Identifier              : %.128s\n", dstr(buf, context.pd.publisher_id, 128));
    printf("Data Preparer Identifier          : %.128s\n", dstr(buf, context.pd.preparer_id, 128));
    printf("Application Identifier            : %.128s\n", dstr(buf, context.pd.application_id, 128));
    printf("Copyright File Identifier         : %.37s\n", dstr(buf, context.pd.copyright_file_id, 37));
    printf("Abstract File Identifier          : %.37s\n", dstr(buf, context.pd.abstract_file_id, 37));
    printf("Bibliographic File Identifier     : %.37s\n", dstr(buf, context.pd.bibliographic_file_id, 37));
    printf("Volume Creation Date and Time     : %.17s\n", dstr(buf, context.pd.creation_date, 17));
    printf("Volume Modification Date and Time : %.17s\n", dstr(buf, context.pd.modification_date, 17));
    printf("Volume Expiration Date and Time   : %.17s\n", dstr(buf, context.pd.expiration_date, 17));
    printf("Volume Effective Date and Time    : %.17s\n", dstr(buf, context.pd.effective_date, 17));
*/    
    return (void*) &context;
};

static int isofs_check_rr(struct iso_directory_record *root_record) {
    int extent = isonum_733(root_record->extent);
    char *buf = (char *) malloc(context.data_size); // can we use "standard" 2048 there?
    if(!buf) {
//        perror("Can`t malloc: ");
        return -ENOMEM;
    };
    
    int rc = isofs_read_raw_block(extent, buf);
    if(rc < 0) {
        free(buf);
        return 0;
    };
    
    struct iso_directory_record *record = (struct iso_directory_record *) buf;
    size_t record_length = isonum_711((unsigned char*) record->length);
    size_t name_len = isonum_711(record->name_len);
    size_t pad_len = ((name_len & 1) ? 0 : 1); // padding byte if name_len is even
    size_t sa_len = record_length - name_len - sizeof(struct iso_directory_record) - pad_len;
    if(record_length < sizeof(struct iso_directory_record)) {
//        fprintf(stderr, "check_rr: directory record length too small: %d\n", record_length);
        free(buf);
        return -EIO;
    };
    if(name_len != 1) {
//        fprintf(stderr, "check_rr: file name length too big for . record: %d\n", name_len);
        free(buf);
        return -EIO;
    };
    if(sa_len < 0) {
        // probably something wrong with name_len
//        fprintf(stderr, "check_rr: wrong name_len in directory entry: %d, record_length %d\n", 
//            name_len, record_length);
        free(buf);
        return -EIO;
    };
    
    
    if(sa_len >= 7) {
        struct rock_ridge *sue = (struct rock_ridge *) (((char *) record) + 
            sizeof(struct iso_directory_record) + 
            name_len + pad_len);
        
        int sue_sig = SIG(sue->signature[0], sue->signature[1]);
        int sue_len = sue->len;
        int sue_version = sue->version;
        
        if(sue_sig == SIG('S', 'P')) {
            if(sue_len != 7 || sue_version != 1 || sue->u.SP.magic[0] != 0xbe || sue->u.SP.magic[1] != 0xef) {
                // incorrect SP entry
                free(buf);
                return 0;
            } else {
                // got it!
                free(buf);
                return 1;
            };
        } else {
            // not SP record
            free(buf);
            return 0;
        };
    } else {
        // no space for SP record
        free(buf);
        return 0;
    };
    
    // should not happen
    free(buf);
    return 0;
};

static isofs_inode *isofs_lookup(const char *path) {
    if(path[0] == '\0') {
        return NULL;
    };
    isofs_inode *inode = g_hash_table_lookup(lookup_table, path);
    if(inode) {
        return inode;
    };
    if(g_hash_table_lookup(negative_lookup_table, path)) {
        return NULL;
    };
//     printf("start search for %s\n", path);
    gchar **parts = g_strsplit(path, "/", -1);
    guint parts_len = local_g_strv_length(parts);
    int partno = 1;
    gchar *rpath = g_strdup("/");
    gchar *rpath1 = "";
    gchar *part = parts[partno];
    while(part && partno < parts_len) {
        rpath1 = g_strconcat(rpath1, "/", part, NULL);
//         printf("looking for %s in %s...\n", rpath1, rpath);
        inode = g_hash_table_lookup(lookup_table, rpath1);
        if(!inode) {
//             printf("trying to load %s...\n", rpath);
            int rc = isofs_real_readdir(rpath, NULL, NULL);
            if(rc) {
//                fprintf(stderr, "lookup: error %d from readdir: %s\n", rc, strerror(-rc));
                g_strfreev(parts);
                g_free(rpath);
                return NULL;
            };
        };
        partno++;
        part = parts[partno];
        g_free(rpath);
        rpath = rpath1;
    };
    g_strfreev(parts);
    g_free(rpath);
    inode = g_hash_table_lookup(lookup_table, path);
    if(!inode) {
        g_hash_table_insert(negative_lookup_table, g_strdup(path), negative_value);
    };
    return inode;
};

static int isofs_read_raw_block(int block, char *buf) {
    off_t off = block * context.block_size + context.block_offset + context.file_offset;
    if(pthread_mutex_lock(& fd_mutex)) {
        int err = errno;
//        perror("isofs_read_raw_block: can`l lock fd_mutex");
        return -err;
    };
    if(lseek(context.fd, off, SEEK_SET) == -1) {
//        perror("isofs_read_raw_block: can`t lseek()");
        pthread_mutex_unlock(& fd_mutex);
        return -EIO;
    };
    size_t len = read(context.fd, buf, context.data_size);
    if(len != context.data_size) {
//        fprintf(stderr, "isofs_read_raw_block: can`t read full block, read only %d bytes from offset %d, %d required; errno %d, message %s\n", 
//            len, (int) off, context.data_size, errno, strerror(errno));
//        fprintf(stderr, "isofs_read_raw_block: huh? reading zeros beyond file end? someone want to save a penny?\n");
//        memset(buf + len, 0, context.data_size - len);
        // pthread_mutex_unlock(& fd_mutex);
        // return -EIO;
    };
    pthread_mutex_unlock(& fd_mutex);
    // printf("block %d, offset %d, read %d\n", block, (int) off, len);
    return len;
};

static time_t isofs_date(char *stamp, int stamp_len) {
    struct tm tm;
    memset(& tm, 0, sizeof(struct tm));
    
    if(stamp_len == 7) { // ISO9660:9.1.5
        tm.tm_year = stamp[0];
        tm.tm_mon = stamp[1] - 1;
        tm.tm_mday = stamp[2];
        tm.tm_hour = stamp[3];
        tm.tm_min = stamp[4];
        tm.tm_sec = stamp[5];
        tm.tm_isdst = -1;
        tm.tm_gmtoff = stamp[6] * 15 * 60;
    } else if(stamp_len == 17) { // ISO9660:8.4.26.1
//        fprintf(stderr, "isofs_date: 17 byte date isn`t supported for the moment, sorry\n");
        return 0;
    } else {
//        fprintf(stderr, "isofs_date: unsupported date format, stamp_len %d\n", stamp_len);
        return 0;
    };

    if(tm.tm_gmtoff) {
//         fprintf(stderr, "direntry2stat: non-zero timezone offset: %d\n", tm.tm_gmtoff);
    };
    time_t time = mktime(& tm);
    
    return time;
};

static int isofs_direntry2stat(struct stat *st, isofs_inode *inode) {
    struct iso_directory_record *record = inode->record;
    
    // fill st_ino from block number where file start
    // since there is no files begin in the same block
    // st->st_ino = isonum_733(record->extent);
    // but some iso images save space by sharing content between several files
    // so it is better to save it unique
    st->st_ino = inode->st_ino;
    
    if(inode->ZF) { // compressed file, report real (uncompressed) size
        st->st_size = inode->real_size;
    } else { // no zisofs compression
        st->st_size = isonum_733(record->size);
    };
    
    st->st_blocks = st->st_size / context.data_size; // should not be to meaningful even for zisofs compression
    st->st_blksize = context.data_size;
    st->st_nlink = 1; // always, even if rrip PX entry found
    
    if(inode->PX) { // rrip PX entry found and in effect
        st->st_mode = inode->st.st_mode;
        st->st_uid = inode->st.st_uid;
        st->st_gid = inode->st.st_gid;
    } else {
        /// TODO use hidden flag?
        if(ISO_FLAGS_DIR(record->flags)) {
            st->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | 
                S_IXUSR | S_IXGRP | S_IXOTH; // dir, readabale and browsable by everyone
        } else {
            st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH; // regular, read by everyone
        };
    };
    
    if(inode->TF) { // rrip TF entry found and in effect
        st->st_atime = inode->st.st_atime;
        st->st_ctime = inode->st.st_ctime;
        st->st_mtime = inode->st.st_mtime;
    } else {
        if(!inode->ctime) {
            inode->ctime = isofs_date(record->date, 7);
        };
        
        st->st_atime = inode->ctime;
        st->st_ctime = inode->ctime;
        st->st_mtime = inode->ctime;
    };
    
    return 0;
};

static char *isofs_fix_entry(char *entry, size_t len) {
    if(!context.joliet_level) { // iso9660 names
        char *sep2 = index(entry, ';'); // find SEPARATOR2
        if(sep2) { // remove remaining part
            *sep2 = '\0';
        };
        char *sep1 = rindex(entry, '.'); // find SEPARATOR1
        if(sep1 && sep1[1] == '\0') { // check if SEPARATOR1 is a last symbol in string
            *sep1 = '\0'; // remove it
        };
        
        // this part is borrowed from linux kernel code 
        // convert remaining ';' and '/' characters to dots
        // and lowercase characters in range A-Z
        char *p = entry;
        while(*p) {
            if(*p == ';' || *p == '/') {
                *p = '.';
            } else if(*p >= 'A' && *p <= 'Z') {
                *p |= 0x20;
            };
            
            p++;
        };
        
        return entry;
    } else {
        // initialize iconv descriptor
        iconv_t cd = iconv_open(iocharset, "UCS-2BE");
        if(cd < 0) {
//            perror("iconv");
            return NULL;
        };
        
        char *inbuf = entry;
        size_t inbytesleft = len;
        
        char *outbuf = (char *) malloc(NAME_MAX); // this should be sufficient for our purposes
        if(!outbuf) {
//            perror("Can`t malloc: ");
            return NULL;
        };
        char *outentry = outbuf;
        size_t outbytesleft = NAME_MAX;
        
        int rc = iconv(cd, & inbuf, & inbytesleft, & outbuf, & outbytesleft);
        size_t outlen = NAME_MAX - outbytesleft;
        outentry[outlen] = '\0';
        if(rc == -1) {
            // incorrect multibyte char or other iconv error -- return as much as possible anyway
//            fprintf(stderr, "iconv on '%s': %s\n", outentry, strerror(errno));
            if(outlen == 0) {
                iconv_close(cd);
                free(entry);
                free(outentry);
                return NULL;
            };
            // try to recover
        };
        
//         printf("outlen %d, outbytesleft %d, rc %d, outbuf %s\n", outlen, outbytesleft, rc, outentry);
        
        // borrowed from linux kernel isofs joliet code
        if(outlen > 2 && outentry[outlen - 2] == ';' && outentry[outlen - 1] == '1') {
            outentry[outlen - 2] = '\0';
        };
        if(outlen >= 2 && outentry[outlen - 1] == '.') {
            outentry[outlen - 1] = '\0';
        };
        
        free(entry);
        iconv_close(cd);
        
        return outentry;
    };
};

static void isofs_free_inode(isofs_inode *inode) {
    if(inode->SL && inode->sl) {
        free(inode->sl);
    };
    if(inode->NM && inode->nm) {
        free(inode->nm);
    };
    if(inode->zf_blockptr) {
        free(inode->zf_blockptr);
    };
    if(inode->record) {
        free(inode->record);
    };
    free(inode);
};

// borrowed from zisofs-tools
static const unsigned char zisofs_magic[8] = {
  0x37, 0xE4, 0x53, 0x96, 0xC9, 0xDB, 0xD6, 0x07
};

static int isofs_parse_zisofs_header(isofs_inode *inode) {
    char *buf = (char *) malloc(context.block_size);
    if(!buf) {
//        perror("Can`t malloc: ");
        return -ENOMEM;
    };
    
    int block_num = isonum_733(inode->record->extent);
    int len = isofs_read_raw_block(block_num, buf);
    if(len < 0) {
        free(buf);
        return len;
    };
    if(len < inode->zf_header_size) {
//        fprintf(stderr, "isofs_parse_zisofs_header: too small block len %d\n", len);
        free(buf);
        return -EIO;
    };
    
    zf_file_header *zf_header = (zf_file_header *) buf;
    
    if(memcmp(zf_header->magic, zisofs_magic, sizeof(zisofs_magic))) {
        // invalid compressed file header
        free(buf);
        return 1;
    };
    
    size_t block_size = 1 << inode->zf_block_shift;
    
    inode->zf_nblocks = ((inode->real_size + inode->zf_header_size - 1) / block_size) + 1;
    
    size_t block_table_size = (inode->zf_nblocks + 1) * 4;
    if(!inode->zf_blockptr) {
        inode->zf_blockptr = (int *) malloc(block_table_size);
        if(!inode->zf_blockptr) {
//            perror("Can`t malloc: ");
            return -ENOMEM;
        };
    };
    
    // copy offset table into memory buffer, maintaining iso9660 block boundaries
    
    int block_table_left = block_table_size;
    int block_table_total = 0;
    int block_table_shift = inode->zf_header_size;
    
    while(block_table_left) {
        size_t block_table_chunk = 
            (block_table_left < context.data_size - block_table_shift ? 
                block_table_left : context.data_size - block_table_shift);
        
/*        printf("isofs_parse_sa: block_table_size: %d, block_table_left: %d, block_table_total %d, block_table_shift %d, block_table_chunk %d\n", 
            block_table_size, block_table_left, block_table_total, block_table_shift, block_table_chunk);*/
        
        memcpy(((char *) inode->zf_blockptr) + block_table_total, buf + block_table_shift, block_table_chunk);
        
        block_table_left -= block_table_chunk;
        block_table_total += block_table_chunk;
        block_table_shift = 0;
        
        // read next block
        block_num += 1;
        len = isofs_read_raw_block(block_num, buf);
        if(len < 0) {
            free(buf);
            return len;
        };
        
/*        printf("isofs_parse_sa: block_num: %d, len: %d\n", 
            block_num, len);*/
    };
    
/*    printf("isofs_parse_zisofs_header: real size %d, header size %d, nblocks %d, block size %d\n", 
        inode->real_size, inode->zf_header_size,
        inode->zf_nblocks, block_size);
    int i;
    for(i = 0; i <= inode->zf_nblocks; i++) {
        printf("zf block table entry %d have value %d\n", i, inode->zf_blockptr[i]);
    };*/
    
    // all information for compressed file we have already in ZF entry
    // so just signal what this is really compressed file
    free(buf);
    return 0;
};

static int isofs_parse_sa(isofs_inode *inode, char *sa, size_t sa_len) {
    int cont_block = 0;
    int cont_offset = 0;
    int cont_size = 0;
    
    int remaining = sa_len;
    while(remaining > 4) { // susp 4.
//         printf("parse_sa: sa offset %d, remaining %d\n", sa_len - remaining, remaining);
        struct rock_ridge *sue = (struct rock_ridge *) (sa + sa_len - remaining);
        int sue_sig = SIG(sue->signature[0], sue->signature[1]);
        int sue_len = sue->len;
        int sue_version = sue->version;
/*        printf("parse_sa: signature %c%c, sue_len %d, sue_version %d\n", 
            sue->signature[0], sue->signature[1], sue_len, sue_version);*/
        
        int known_sue = 1;
        
        switch(sue_sig) {
            case SIG('S', 'P'): // susp 5.3
                if(sue_len != 7 || sue_version != 1 || sue->u.SP.magic[0] != 0xbe || sue->u.SP.magic[1] != 0xef) {
                    // incorrect SP entry
/*                    fprintf(stderr, 
                        "parse_sa: incorrect SP entry: sue_len %d, sue_version %d, magic %c%c\n", 
                        sue_len, sue_version, sue->u.SP.magic[0], sue->u.SP.magic[1]);*/
                    context.susp = 0;
                    return 0;
                } else {
                    context.susp = 1;
                    context.susp_skip = sue->u.SP.skip;
                };
//                 printf("parse_sa: SP entry, skip %d\n", sue->u.SP.skip);
                break;
            case SIG('C', 'E'): // susp 5.1
                if(sue_len != 28 || sue_version != 1) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect CE entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else if(cont_block != 0) { // CE entry already found
/*                    fprintf(stderr, 
                        "parse_sa: duplicate CE entry, skipping, sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    cont_block = isonum_733(sue->u.CE.extent);
                    cont_offset = isonum_733(sue->u.CE.offset);
                    cont_size = isonum_733(sue->u.CE.size);
                    
                    if(cont_block < 16) { 
                        // continuation area can`t be there
/*                        fprintf(stderr, 
                            "parse_sa: wrong continuation area extent: %d, cont_offset %d, cont_size %d\n", 
                            cont_block, cont_offset, cont_size);*/
                        cont_block = 0; // do not process it
                    } else if(cont_offset + cont_size > context.data_size) { 
                        // something wrong with continuation area offset and/or size
/*                        fprintf(stderr, 
                            "parse_sa: wrong continuation area: extent %d, cont_offset %d, cont_size %d\n", 
                            cont_block, cont_offset, cont_size);'/
                        cont_block = 0; // do not process it
                    } else {
/*                        printf("parse_sa: CE entry, extent %d, offset %d, size %d\n", 
                            cont_block, cont_offset, cont_size);*/
                    };
                };
                break;
            case SIG('E', 'R'): // susp 5.5
                if(sue_version != 1) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect ER entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    int len_id = sue->u.ER.len_id;
                    int len_des = sue->u.ER.len_des;
                    int len_src = sue->u.ER.len_src;
                    int ext_ver = sue->u.ER.ext_ver;
                    if(len_id + len_des + len_src + 8 > sue_len) {
/*                        fprintf(stderr, 
                            "parse_sa: incorrect ER entry: sue_len %d, sue_version %d, len_id %d, len_des %d, len_src %d, ext_ver %d\n", 
                            sue_len, sue_version, len_id, len_des, len_src, ext_ver);*/
                    } else {
                        char id[256];
                        char des[256];
                        char src[256];
                        
                        strncpy(id, sue->u.ER.data, len_id);
                        id[len_id] = '\0';
                        strncpy(des, sue->u.ER.data + len_id, len_des);
                        des[len_des] = '\0';
                        strncpy(src, sue->u.ER.data + len_id + len_des, len_src);
                        src[len_src] = '\0';
                        
/*                        printf("parse_sa: ER entry:\n\t id: %s\n\tdes: %s\n\tsrc: %s\n\tver: %d\n", 
                            id, des, src, ext_ver);*/
                    };
                };
                break;
            case SIG('R', 'R'):
                {
                    // unused
                    isonum_711((unsigned char *) sue->u.RR.flags);
/*                    printf("parse_sa: RR entry, sue_version %d, sue_len %d, flags %d\n", 
                        sue_version, sue_len, rr_flags);*/
                };
                break;
            case SIG('P', 'X'): // rrip 4.1.1
                // according to rrip 4.1.1, length of PX record should be exactly 44
                // but linux kernel and mkisofs seems to be happy with length 36, 
                // where 'serial number' are not presented
                // (or i`m looking at outdated draft.. :-)
                if((sue_len != 44 && sue_len != 36) || sue_version != 1) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect PX entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    mode_t mode = isonum_733(sue->u.PX.mode);
                    nlink_t nlink = isonum_733(sue->u.PX.n_links);
                    uid_t uid = isonum_733(sue->u.PX.uid);
                    gid_t gid = isonum_733(sue->u.PX.gid);
/*                    printf("parse_sa: PX entry, sue_version %d, sue_len %d, mode %d, nlinks %d, uid %d, gid %d\n", 
                        sue_version, sue_len, mode, nlink, uid, gid);*/
                    inode->st.st_mode = mode;
                    inode->st.st_nlink = nlink;
                    inode->st.st_uid = uid;
                    inode->st.st_gid = gid;
                    inode->PX = 1;
                    /// TODO check if entry length == 44 and set st_ino field from 'file serial number'?
                };
                break;
            case SIG('S', 'L'): // rrip 4.1.3
                if(sue_version != 1) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect SL entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else if(inode->SL) {
/*                    fprintf(stderr, 
                        "parse_sa: SL entry already in effect, sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    int sl_flags = sue->u.SL.flags;
                    int max_components = (sue_len - 5) / sizeof(struct SL_component);
                    
                    if(max_components < 1) {
/*                        fprintf(stderr, 
                            "parse_sa: SL entry found, but no components: sue_len %d, sue_version %d\n", 
                            sue_len, sue_version);*/
                    } else {
                        int c;
                        int c_offset = 0;
                        int c_errors = 0;
                        for(c = 0; c < max_components; c++) {
                            struct SL_component *comp = 
                                (struct SL_component *) 
                                    (((char *) & sue->u.SL.link) + c_offset);
                            
                            int sl_c_flags = comp->flags;
                            int sl_c_len = comp->len;
                            
                            if(c_offset + 5 >= sue_len) {
                                // according to rrip, we must stop if CONTINUE flag isn`t set
                                // according to linux kernel isofs code and images produced witj mkisofs
                                // we need to continue while there is space in SL entry for next component
                                break;
                                
                                // strict rrip code:
                                // fprintf(stderr, 
                                //     "parse_sa: invalid SL component: sue_len %d, sue_version %d, sl_flags %d, sl_c_flags %d, sl_c_len %d\n", 
                                //     sue_len, sue_version, sl_flags, sl_c_flags, sl_c_len);
                                // c_errors++;
                                // break;
                                
                                /// TODO find _working_ rrip specification
                            };
                            
                            int c_len = 0;
                            char c_separator = 0;
                            if(!inode->sl_len) { // nothing found previoustly
                            } else if(inode->sl_len == 1 && inode->sl[0] == '/') { // previous SL component was ROOT
                                // no need for separator after ROOT component
                            } else {
                                c_len += 1; // place for '/' separator
                                c_separator = '/';
                            };
                            if(sl_c_flags & (1 << 1)) { // CURRENT component
                                c_len += 1; // place for '.' character
                            } else if(sl_c_flags & (1 << 2)) { // PARENT component
                                c_len += 2; // place for ".." characters
                            } else {
                                c_len += sl_c_len;
                            };
                            
                            if(c_len + inode->sl_len + 1 > PATH_MAX) {
/*                                fprintf(stderr, 
                                    "parse_sa: too long symlink found: sue_len %d, sue_version %d, sl_flags %d, sl_c_flags %d, sl_c_len %d, c_len %d\n", 
                                    sue_len, sue_version, sl_flags, sl_c_flags, sl_c_len, c_len);*/
                                c_errors++;
                                break;
                            };
                            
                            if(!inode->sl) {
                                inode->sl = (char *) malloc(PATH_MAX);
                                if(!inode->sl) {
//                                    perror("Can`t malloc: ");
                                    return -ENOMEM;
                                };
                            };
                            
                            if(c_separator) {
                                inode->sl[inode->sl_len] = c_separator;
                                inode->sl_len += 1;
                            };
                            
                            if(sl_c_flags & (1 << 1)) { // CURRENT component
                                inode->sl[inode->sl_len] = '.';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '\0';
/*                                printf("parse_sa: SL CURRENT component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s\n", 
                                    sl_c_flags, sl_c_len, inode->sl_len, inode->sl);*/
                            } else if(sl_c_flags & (1 << 2)) { // PARENT component
                                inode->sl[inode->sl_len] = '.';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '.';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '\0';
/*                                printf("parse_sa: SL PARENT component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s\n", 
                                    sl_c_len, inode->sl_len, inode->sl);*/
                            } else if(sl_c_flags & (1 << 3)) { // ROOT component (?! does not documented at all)
                                inode->sl[inode->sl_len] = '/';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '\0';
/*                                printf("parse_sa: SL ROOT component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s\n", 
                                    sl_c_len, inode->sl_len, inode->sl);*/
                            } else {
                                strncpy(inode->sl + inode->sl_len, comp->text, sl_c_len);
                                inode->sl_len += sl_c_len;
                                inode->sl[inode->sl_len] = '\0';
/*                                printf("parse_sa: SL component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s\n", 
                                    sl_c_flags, sl_c_len, inode->sl_len, inode->sl);*/
                            };
                            
                            // according to rrip, we must stop if CONTINUE flag isn`t set
                            // according to linux kernel isofs code and images produced witj mkisofs
                            // we need to continue while there is space in SL entry for next component
                            c_offset += (2 + sl_c_len);
                            
                            // strict rrip code:
                            // if(sl_c_flags & 1) { // CONTINUE
                            //     c_offset += 2 + sl_c_len;
                            // } else {
                            //     printf("parse_sa: SL final component, sl_c_len %d, sl_c_flags %d, sl_len %d, sl %s\n", 
                            //         sl_c_len, sl_c_flags, inode->sl_len, inode->sl);
                            //     break;
                            // };
                        };
                        
                        if(c_errors) {
//                             printf("parse_sa: errors found while processing SL components, cleaning\n");
                            if(inode->sl) {
                                free(inode->sl);
                            };
                            inode->sl_len = 0;
                            inode->SL = 0;
                        } else if(!(sl_flags & 1) && inode->sl) {
/*                            printf("parse_sa: SL entry (final), sue_len %d, sue_version %d, sl_len %d, sl %s\n",
                                sue_len, sue_version, inode->sl_len, inode->sl);*/
                            inode->SL = 1;
                        } else if(!(sl_flags & 1) && !inode->sl) {
/*                            fprintf(stderr, "parse_sa: final SL entry found, but no SL components, cleaning\n");*/
                            if(inode->sl) {
                                free(inode->sl);
                            };
                            inode->sl_len = 0;
                            inode->SL = 0;
                        } else if(inode->sl) {
/*                            printf("parse_sa: SL entry, sue_len %d, sue_version %d, sl_len %d, sl %s\n", 
                                sue_len, sue_version, inode->sl_len, inode->sl);*/
                        } else {
/*                            fprintf(stderr, "parse_sa: empty SL entry?\n");*/
                        };
                    };
                };
                break;
            case SIG('N', 'M'): // rrip 4.1.4
                if(sue_version != 1) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect NM entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    int nm_flags = sue->u.NM.flags;
                    if(nm_flags & (1 << 1)) { // CURRENT bit
/*                        printf("parse_sa: NM CURRENT entry, sue_version %d, sue_len %d, flags %d\n", 
                            sue_version, sue_len, nm_flags);*/
                    } else if(nm_flags & (1 << 2)) { // PARENT bit
/*                        printf("parse_sa: NM PARENT entry, sue_version %d, sue_len %d, flags %d\n", 
                            sue_version, sue_len, nm_flags);*/
                    } else {
                        if(sue_len - 5 + inode->nm_len > NAME_MAX - 1) {
                            fprintf(stderr, 
/*                                "parse_sa: too long NM entry: %d\n", 
                                sue_len - 5 + inode->nm_len);
                        } else if(inode->NM) {
                            fprintf(stderr, 
                                "parse_sa: NM entry already in effect, sue_len %d, sue_version %d\n", */
                                sue_len, sue_version);
                        } else {
                            if(!inode->nm) {
                                inode->nm = (char *) malloc(NAME_MAX);
                                if(!inode->nm) {
//                                   perror("Can`t malloc: ");
                                    return -ENOMEM;
                                };
                            };
                            
                            strncpy(inode->nm + inode->nm_len, sue->u.NM.name, sue_len - 5);
                            inode->nm_len += sue_len - 5;
                            inode->nm[inode->nm_len] = '\0';
                            
                            if(!nm_flags & 1) { // CONTINUE bit
                                inode->NM = 1;
/*                                printf("parse_sa: NM entry (final), flags %d, len %d, name %s\n", 
                                    nm_flags, sue_len - 5, inode->nm);*/
                            } else {
/*                                printf("parse_sa: NM entry (part), flags %d, len %d, name %s\n", 
                                    nm_flags, sue_len - 5, inode->nm);*/
                            };
                        };
                    };
                };
                break;
            case SIG('C', 'L'): // rrip 4.1.5.1
                if(sue_version != 1 || sue_len != 12) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect CL entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    int cl_block = isonum_733(sue->u.CL.location);
/*                    printf("parse_sa: CL entry, block %d\n", 
                                    cl_block);*/
                    inode->CL = 1;
                    inode->cl_block = cl_block;
                    
                    // read block pointed by CL record and process first directory entry
                    char *buf = (char *) malloc(context.data_size);
                    if(!buf) {
//                        perror("Can`t malloc: ");
                        return -ENOMEM;
                    };
                    int rc = isofs_read_raw_block(inode->cl_block, buf);
                    if(rc < 0) {
                        free(buf);
                        return rc;
                    };
                    
                    struct iso_directory_record *record = (struct iso_directory_record *) buf;
                    size_t record_length = isonum_711((unsigned char *) record->length);
                    size_t name_len = isonum_711(record->name_len);
                    size_t pad_len = ((name_len & 1) ? 0 : 1); // padding byte if name_len is even
                    size_t sa_len = record_length - name_len - sizeof(struct iso_directory_record) - pad_len;
        /*            printf("boff %d, record length %d, name_len %d, pad_len %d, sa_len %d\n", 
                        (int) boff, record_length, name_len, pad_len, sa_len);*/
                    if(record_length < sizeof(struct iso_directory_record)) {
/*                        fprintf(stderr, "parse_sa: CL entry: directory record length too small: %d\n", record_length);*/
                        free(buf);
                        return -EIO;
                    };
                    if(name_len != 1) {
/*                        fprintf(stderr, "parse_sa: file name length too big for . record: %d\n", name_len);*/
                        free(buf);
                        return -EIO;
                    };
                    if(sa_len < 0) {
                        // probably something wrong with name_len
/*                        fprintf(stderr, "parse_sa: CL record: wrong name_len in directory entry: %d, record_length %d\n", 
                            name_len, record_length);*/
                        free(buf);
                        return -EIO;
                    };
                    
                    // ignoring anything from original record
                    struct iso_directory_record *cl_record = 
                        (struct iso_directory_record *) malloc(sizeof(struct iso_directory_record));
                    if(!cl_record) {
//                        perror("Can`t malloc: ");
                        return -ENOMEM;
                    };
                    memcpy(cl_record, record, sizeof(struct iso_directory_record));
                    
                    // drop existing record
                    if(inode->record) {
                        free(inode->record);
                    };
                    
                    // replace record with new one
                    inode->record = cl_record;
                    
                    // parse sa entries from relocated directory . record
                    rc = isofs_parse_sa(inode, 
                        ((char *) record) + 
                        sizeof(struct iso_directory_record) + 
                        name_len + pad_len + context.susp_skip, 
                        sa_len);
                    if(rc) {
                        free(buf);
                        return rc;
                    };
                    
                    free(buf);
                };
                break;
            case SIG('P', 'L'): // rrip 4.1.5.2
                if(sue_version != 1 || sue_len != 12) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect PL entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    int pl_block = isonum_733(sue->u.PL.location);
/*                    printf("parse_sa: PL entry, block %d\n", 
                                    pl_block);*/
                    // probably we don`t need process PL record with FUSE
                    inode->PL = 1;
                    inode->pl_block = pl_block;
                };
                break;
            case SIG('R', 'E'): // rrip 4.1.5.3
                if(sue_version != 1 || sue_len != 4) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect RE entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
//                     printf("parse_sa: RE entry\n");
                    inode->RE = 1;
                };
                break;
            case SIG('T', 'F'): // rrip 4.1.6
                if(sue_version != 1) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect TF entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    int tf_flags = sue->u.TF.flags;
                    int stamp_length;
                    if(tf_flags & TF_LONG_FORM) {
                        // iso9660:8.4.26.1 format
                        stamp_length = 17;
                    } else {
                        // iso9660:9.1.5 format
                        stamp_length = 7;
                    };
                    
                    time_t ctime = 0;
                    time_t mtime = 0;
                    time_t atime = 0;
                    
                    int stamp_no = 0;
                    // ctime can be stored as CREATION time
                    if(tf_flags & TF_CREATE) {
                        ctime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    };
                    if(tf_flags & TF_MODIFY) {
                        mtime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    };
                    if(tf_flags & TF_ACCESS) {
                        atime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    };
                    // ctime should be stored in ATTRIBUTES stamp according to rrip 4.1.6
                    if(tf_flags & TF_ATTRIBUTES) {
                        ctime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    };
                    // other fields have no meaning for us
                    
/*                    printf("parse_sa: TF entry, sue_version %d, sue_len %d, ctime %d, mtime %d, atime %d\n", 
                        sue_version, sue_len, ctime, mtime, atime);*/
                    inode->st.st_ctime = ctime;
                    inode->st.st_mtime = mtime;
                    inode->st.st_atime = atime;
                    inode->TF = 1;
                };
                break;
            case SIG('S', 'F'): // rrip 4.1.7
                if(sue_version != 1 || sue_len != 21) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect SF entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else {
                    /// TODO does anyone support SF entries? linux isofs code does not..
/*                    fprintf(stderr, 
                        "parse_sa: SF entries (sparse files) are unsupported in this version, sorry..\n");*/
                };
                break;
            case SIG('Z', 'F'): // non-standard linux extension (zisofs)
                if(sue_version != 1 || sue_len != 16) {
                    // incorrect entry, skip
/*                    fprintf(stderr, 
                        "parse_sa: incorrect ZF entry: sue_len %d, sue_version %d\n", 
                        sue_len, sue_version);*/
                } else if(SIG(sue->u.ZF.algorithm[0], sue->u.ZF.algorithm[1]) == SIG('p', 'z')) {
                    inode->zf_algorithm[0] = sue->u.ZF.algorithm[0];
                    inode->zf_algorithm[1] = sue->u.ZF.algorithm[1];
                    inode->zf_header_size = ((unsigned char) sue->u.ZF.parms[0]) << 2;
                    inode->zf_block_shift = (unsigned char) sue->u.ZF.parms[1];
                    inode->zf_size = (inode->st.st_size ? inode->st.st_size : isonum_733(inode->record->size));
                    inode->real_size = isonum_733(sue->u.ZF.real_size);
                    // check if file is really compressed, ignore ZF entry otherwise
                    int rc = isofs_parse_zisofs_header(inode);
                    if(rc == 0) {
//                         printf("parse_sa: ZF entry found, algorithm %02x%02x, header size %d, block shift %d, compressed size %d, real size %d\n", 
//                             inode->zf_algorithm[0], inode->zf_algorithm[1],
//                             inode->zf_header_size, inode->zf_block_shift,
//                             inode->zf_size, inode->real_size);
                        inode->ZF = 1;
                    } else if(rc > 0) {
/*                        fprintf(stderr, "parse_sa: ZF entry found, but file is not really compressed\n");*/
                    } else { // some error occur
                        return rc;
                    };
                } else {
/*                    fprintf(stderr, 
                        "parse_sa: unknown ZF compression algorithm %c%c, sorry..\n", 
                        sue->u.ZF.algorithm[0], sue->u.ZF.algorithm[1]);*/
                };
                break;
            default:
                known_sue = 0;
/*                fprintf(stderr, "parse_sa: unknown entry '%c%c', sue_sig %d, sue_version %d, sue_len %d\n", 
                    sue->signature[0], sue->signature[1], 
                    sue_sig, sue_version, sue_len);*/
                break;
        };
        
        if(sue_len >= 4 && known_sue) {
            remaining -= sue_len;
        } else if(known_sue) {
/*            fprintf(stderr, "parse_sa: inappropriate susp entry length: %d, signature %c%c\n", 
                sue_len, sue->signature[0], sue->signature[1]);*/
            return -EIO;
        } else { // probably there are no more susp entries
            return 0;
        };
    };
    
    // process continuation area if found
    if(cont_block) {
        char *buf = (char *) malloc(context.data_size);
        if(!buf) {
//            perror("Can`t malloc: ");
            return -ENOMEM;
        };
        int rc = isofs_read_raw_block(cont_block, buf);
        if(rc < 0) {
            free(buf);
            return rc;
        };
/*        printf("parse_sa: deep into CE, extent %d, offset %d, size %d\n", 
            cont_block, cont_offset, cont_size);*/
        rc = isofs_parse_sa(inode, buf + cont_offset, cont_size);
        if(rc) {
            free(buf);
            return rc;
        };
        
        free(buf);
    };
    
    return 0;
};

int isofs_real_opendir(const char *path) {
    isofs_inode *inode = isofs_lookup(path);
    if(!inode) {
//        fprintf(stderr, "opendir: know nothing about %s\n", path);
        return -ENOENT;
    };
    if(!ISO_FLAGS_DIR(inode->record->flags)) {
//        fprintf(stderr, "opendir: %s not a dir\n", path);
        return -ENOTDIR;
    };
    return 0;
};

int isofs_real_readdir(const char *path, void *filler_buf, isofs_dir_fill_t filler) {
    if(path[0] == '\0') {
//        fprintf(stderr, "readdir: attempt to read empty path name\n");
        return -EINVAL;
    };
    isofs_inode *current_inode = isofs_lookup(path);
    if(!current_inode) {
//        fprintf(stderr, "readdir: know nothing about %s\n", path);
        return -ENOENT;
    };
    struct iso_directory_record *current = current_inode->record;
    if(!ISO_FLAGS_DIR(current->flags)) {
//        fprintf(stderr, "readdir: %s not a dir\n", path);
        return -ENOTDIR;
    };
    
    size_t current_size = isonum_733(current->size);
    
    int block = isonum_733(current->extent);
    char *buf = (char *) malloc(context.data_size);
    if(!buf) {
//        perror("Can`t malloc: ");
        return -ENOMEM;
    };
    int rc;
    // printf("path %s, current_size %d, block %d\n", path, current_size, block);
    
    size_t total_size = 0;
    int count = 1;
    int block_count = 0;
    off_t boff = 0;
    
    while(total_size <= current_size - sizeof(struct iso_directory_record)) {
        rc = isofs_read_raw_block(block, buf);
        if(rc < 0) {
            return rc;
        };
        if(rc != context.data_size) {
            // can`t be allowed
//            fprintf(stderr, "readdir: can`t read whole block, read only %d bytes, block %d\n", rc, block);
            free(buf);
            return -EIO;
        };
        block_count++;
        
        if(boff > 0) {
            total_size += (context.data_size - boff);
            boff = 0;
        };
        
        while(boff + sizeof(struct iso_directory_record) <= context.data_size && 
            total_size <= current_size - sizeof(struct iso_directory_record)) {
            
            struct iso_directory_record *record = (struct iso_directory_record *) (buf + boff);
            size_t record_length = isonum_711((unsigned char *) record->length);
            size_t name_len = isonum_711(record->name_len);
            size_t pad_len = ((name_len & 1) ? 0 : 1); // padding byte if name_len is even
            size_t sa_len = record_length - name_len - sizeof(struct iso_directory_record) - pad_len;
            
            // printf("block %d, boff %d, total_size %d, current_size %d, record length %d, name_len %d, pad_len %d, sa_len %d\n", 
            //     block, (int) boff, total_size, current_size, record_length, name_len, pad_len, sa_len);
            if(record_length == 0) {
                // possible end of directory or end of block
                total_size += (context.data_size - boff);
                boff = 0;
                break;
            };
            if(record_length < sizeof(struct iso_directory_record)) {
                if(count > 2) { // check if . and .. is already read
                    // possible end of directory
                    // at least mkisofs does not set iso_directory_record.size correct
                    // (this is much possible it was my fault and misunderstanding -- dmiceman)
                    // but we need to try next block to be sure
                    /// TODO this is not clear: what to do if next block not contain next directory?
                    total_size += (context.data_size - boff);
                    boff = 0;
                    break;
                } else {
//                    fprintf(stderr, "readdir: directory record length too small: %d\n", record_length);
                    free(buf);
                    return -EIO;
                };
            };
            if(name_len > NAME_MAX - 1) {
//                fprintf(stderr, "readdir: file name length too big: %d\n", name_len);
                free(buf);
                return -EIO;
            };
            if(sa_len < 0) {
                // probably something wrong with name_len
//                fprintf(stderr, "readdir: wrong name_len in directory entry: %d, record_length %d\n", 
//                    name_len, record_length);
                free(buf);
                return -EIO;
            };
            if(count > 2 && name_len == 1 && record->name[0] == 0) {
                // looks like this is normal situation to meet another directory because
                // there is no clear way to find directory end
//                 fprintf(stderr, "readdir: next directory found while processing another directory! boff %d, total_size %d, current_size %d, block %d, count %d\n", 
//                     (int) boff, total_size, current_size, block, count);
                goto out;
            };
            
            isofs_inode *inode = (isofs_inode *) malloc(sizeof(isofs_inode));
            if(!inode) {
//                perror("Can`t malloc: ");
                return -ENOMEM;
            };
            memset(inode, 0, sizeof(isofs_inode));
            inode->st_ino = context.last_ino;
            context.last_ino++;
            
            struct iso_directory_record *n_rec = 
                (struct iso_directory_record *) malloc (sizeof(struct iso_directory_record));
            if(!n_rec) {
//                perror("Can`t malloc: ");
                return -ENOMEM;
            };
            memcpy(n_rec, record, sizeof(struct iso_directory_record));
            inode->record = n_rec;
            
            if(context.susp || path[1] == '\0') { // if susp is known to be present or this is a root dir ("/")
/*                printf("sa offset %d, sa_len %d\n", 
                    sizeof(struct iso_directory_record) + name_len + pad_len, sa_len);*/
                rc = isofs_parse_sa(inode, 
                    ((char *) record) + sizeof(struct iso_directory_record) + name_len + pad_len + context.susp_skip, 
                    sa_len);
                if(rc) {
                    free(buf);
                    isofs_free_inode(inode);
                    return rc;
                };
            };
            
            char *entry = (char *) malloc(NAME_MAX);
            if(!entry) {
//                perror("Can`t malloc: ");
                return -ENOMEM;
            };
            if(count == 1) { // . entry ('\0' on disk)
                strcpy(entry, ".");
            } else if(count == 2) { // .. entry ('\1' on disk)
                strcpy(entry, "..");
            } else { // regular entry
                if(inode->NM) { // rrip NM entry present and in effect
//                     printf("readdir: NM entry is in effect\n");
                    strncpy(entry, inode->nm, inode->nm_len);
                    entry[inode->nm_len] = '\0';
                } else { // regular ISO9660 filename
//                     printf("readdir: no NM entry found, name len %d\n", name_len);
                    // because there can be '\0' characters because using of UCS-2 encoding we need to use memcpy
                    memcpy(entry, (char *) record->name, name_len);
                    entry[name_len] = '\0';
                    
                    // fixup entry -- lowercase, strip leading ';', etc..
                    entry = isofs_fix_entry(entry, name_len);
                    if(!entry) {
//                        fprintf(stderr, "readdir: error during entry fixup\n");
                        isofs_free_inode(inode);
                        free(buf);
                        return -EIO;
                    };
                };
            };
            
            // fprintf(stderr, "%d -- %s\n\n", count, entry); 
            
            if(filler) {
                struct stat st;
                memset(& st, '\0', sizeof(struct stat));
                isofs_direntry2stat(& st, inode);
                rc = filler(filler_buf, entry, & st, 0);
                if(rc) {
//                    printf("readdir: filler return with %d, entry %s\n", rc, entry);
                    isofs_free_inode(inode);
                    free(buf);
                    free(entry);
                    return -rc;
                };
            };
            
            char absolute_entry[PATH_MAX];
            strcpy(absolute_entry, path);
            if(path[1] != '\0') { // not root dir
                strcat(absolute_entry, "/");
            };
            strcat(absolute_entry, entry);
            if(g_hash_table_lookup(lookup_table, absolute_entry)) {
                // already in lookup cache
                isofs_free_inode(inode);
            } else {
                g_hash_table_insert(lookup_table, g_strdup(absolute_entry), inode);
            };
            
            free(entry);
            
            boff += record_length;
            total_size += record_length;
            count++;
        };
        
        // read next block
        
        block++;
    };
out:
    
    free(buf);
    return 0;
};

int isofs_real_getattr(const char *path, struct stat *stbuf) {
    isofs_inode *inode = isofs_lookup(path);
    if(!inode) {
        // this error occur too often when browsing mounted tree with konqueror -- 
        // it check existence of .directory file with stat()
        // fprintf(stderr, "getattr: know nothing about %s\n", path);
        return -ENOENT;
    } else {
        // printf("getattr: found %s, size %d\n", path, isonum_733(inode->record->size));
    };
    memset(stbuf, 0, sizeof(struct stat));
    isofs_direntry2stat(stbuf, inode);
/*    if(ISO_FLAGS_DIR(inode->record->flags)) {
        printf("%s %i %i\n", path, (int) stbuf->st_size, stbuf->st_mode);
    };*/
    return 0;
};

int isofs_real_readlink(const char *path, char *target, size_t size) {
    isofs_inode *inode = isofs_lookup(path);
    if(!inode) {
//        fprintf(stderr, "readlink: know nothing about %s\n", path);
        return -ENOENT;
    };
    if(!inode->PX || !inode->SL || !S_ISLNK(inode->st.st_mode)) {
//        fprintf(stderr, "readlink: %s not a link\n", path);
        return -EINVAL;
    };
    strncpy(target, inode->sl, size - 1);
    target[size - 1] = '\0';
    return 0;
};

int isofs_real_open(const char *path) {
    isofs_inode *inode = isofs_lookup(path);
    if(!inode) {
//        fprintf(stderr, "open: know nothing about %s\n", path);
        return -ENOENT;
    };
    if(ISO_FLAGS_DIR(inode->record->flags)) {
//        fprintf(stderr, "open: %s not a file\n", path);
        return -EINVAL;
    };
    if(ISO_FLAGS_HIDDEN(inode->record->flags)) {
//        fprintf(stderr, "open: %s is a hidden file\n", path);
        // return -EPERM;
    };
    return 0;
};

static int isofs_real_read_zf(isofs_inode *inode, char *out_buf, size_t size, off_t offset) {
    int zf_block_size = 1 << inode->zf_block_shift;
    int zf_start = offset / zf_block_size;
    int zf_end = (offset + size) / zf_block_size;
    // protection against cornercase when read request is exactly equal to the uncompressed file size. 
    // --  Ryan Thomas 2007-06-13
    if( (offset + size ) % zf_block_size == 0 ) {
        zf_end--;
    };
    int shift = offset % zf_block_size;
    
//     printf("zf_start %d, zf_end %d, size %d, offset %d, shift %d\n", 
//         zf_start, zf_end, size, (int) offset, shift);
    
    // protection against some ununderstandable errors
    if(zf_start >= inode->zf_nblocks) {
        zf_start = inode->zf_nblocks - 1;
    };
    if(zf_end >= inode->zf_nblocks) {
        zf_end = inode->zf_nblocks - 1;
    };
    if(zf_end < 0 || zf_start < 0) {
        return 0;
    };
    
    unsigned char *cbuf = (unsigned char *) malloc(zf_block_size * 2);
    if(!cbuf) {
//        perror("Can`t malloc: ");
        return -ENOMEM;
    };
    unsigned char *ubuf = (unsigned char *) malloc(zf_block_size);
    if(!ubuf) {
//        perror("Can`t malloc: ");
        return -ENOMEM;
    };
    
    size_t total_size = 0;
    size_t size_left = size;
    
    int base_offset = isonum_733(inode->record->extent) * context.data_size;
    
    int i;
    for(i = zf_start; i <= zf_end; i++) {
        int block_offset = isonum_731((char *) (& inode->zf_blockptr[i]));
        int block_end = isonum_731((char *) (& inode->zf_blockptr[i + 1]));
        int block_size = block_end - block_offset;
        
        if(block_size == 0) { // sort of sparce file block
            size_t payload_size = (zf_block_size - shift < size_left ? zf_block_size - shift: size_left);
            memset(out_buf + total_size, 0, payload_size);
            
            total_size += payload_size;
            size_left -= payload_size;
        } else if(block_size > zf_block_size * 2) {
/*            fprintf(stderr, "isofs_real_read_zf: compressed block size bigger than uncompressed block size * 2, something is wrong there.. block size %d, uncompressed block size %d\n", 
                block_size, zf_block_size);*/
            free(cbuf);
            free(ubuf);
            return -EIO;
        } else {
            // we do not use isofs_read_raw_block() there because it is simpler to access
            // compressed blocks directly
            
            int image_off = base_offset + block_offset;
            
            if(pthread_mutex_lock(& fd_mutex)) {
                int err = errno;
//                perror("isofs_real_read_zf: can`l lock fd_mutex");
                free(cbuf);
                free(ubuf);
                return -err;
            };
            if(lseek(context.fd, image_off, SEEK_SET) == -1) {
//                perror("isofs_real_read_zf: can`t lseek()");
                pthread_mutex_unlock(& fd_mutex);
                free(cbuf);
                free(ubuf);
                return -EIO;
            };
            size_t len = read(context.fd, cbuf, block_size);
            if(len != block_size) {
/*                fprintf(stderr, "isofs_real_read_zf: can`t read full block, errno %d, message %s\n", 
                    errno, strerror(errno));*/
                pthread_mutex_unlock(& fd_mutex);
                free(cbuf);
                free(ubuf);
                return -EIO;
            };
            pthread_mutex_unlock(& fd_mutex);
            
            // compressed block is read from disk, now uncompress() it
            
            uLongf usize = zf_block_size;
            int rc = uncompress(ubuf, &usize, cbuf, block_size);
            if(rc != Z_OK) {
                free(cbuf);
                free(ubuf);
//                fprintf(stderr, "isofs_real_read_zf: uncompress() error %i: %s\n", rc, strerror(rc));
                return -rc;
            };
            
            // ubuf contain uncompressed data, usize contain uncompressed block size
            
            size_t payload_size = (usize - shift < size_left ? usize - shift : size_left);
            memcpy(out_buf + total_size, ubuf + shift, payload_size);
            
            total_size += payload_size;
            size_left -= payload_size;
        };
        
        // shift is only meaningful for first block
        shift = 0;
    };
    
    free(cbuf);
    free(ubuf);
    
//     printf("total size %d\n", total_size);
    
    return total_size;
};

int isofs_real_read(const char *path, char *out_buf, size_t size, off_t offset) {
    isofs_inode *inode = isofs_lookup(path);
    if(!inode) {
//        fprintf(stderr, "read: know nothing about %s\n", path);
        return -ENOENT;
    };
    struct iso_directory_record *record = inode->record;
    if(ISO_FLAGS_DIR(record->flags)) {
//        fprintf(stderr, "read: %s not a file\n", path);
        return -EINVAL;
    };
    if(ISO_FLAGS_HIDDEN(record->flags)) {
//        fprintf(stderr, "read: %s is a hidden file\n", path);
        // return -EPERM;
    };
    
    if(inode->ZF) { // this file is compressed, handle it specially
        return isofs_real_read_zf(inode, out_buf, size, offset);
    };
    
    size_t fsize = isonum_733(record->size);
    if(offset + size > fsize) {
        size = fsize - offset;
    };
    if(size < 1) {
        return 0;
    };
    
    int start_block = isonum_733(record->extent);
    if(start_block == 0) { // empty file
        return 0;
    };
    
    int start = offset / context.data_size;
    int end = (offset + size) / context.data_size;
    int shift = offset % context.data_size;
    int block = start_block + start;
//     printf("read: path %s, size %d, offset %d, fsize %d, start %d, end %d, shift %d\n", 
//         path, size, (int) offset, fsize, start, end, shift);
    
    char *buf = (char *) malloc(context.data_size);
    if(!buf) {
//        perror("Can't malloc: ");
        return -ENOMEM;
    };
    
    int i;
    size_t total_size = 0;
    size_t size_left = size;
    int count = 0;
    
    for(i = start; i <= end; i++) {
        int len = isofs_read_raw_block(block, buf);
        if(len < 0) {
            free(buf);
            return len;
        };
//         printf("read: block %d, len %d, size_left %d\n", block, len, size_left);
        
        if(len > size_left) {
            len = size_left;
        };
        
        total_size += len;
        
        memcpy(out_buf + count * context.data_size, buf + shift, len - shift);
        
        count++;
        shift = 0;
        size_left -= len;
        block++;
    };
    
    free(buf);
    return total_size;
};

int isofs_real_statfs(struct statfs *stbuf) {
    stbuf->f_type = ISOFS_SUPER_MAGIC;
    stbuf->f_bsize = context.data_size; // or PAGE_CACHE_SIZE?
    stbuf->f_blocks = 0; // while it is possible to calculate this, i see no reasons to do so
    stbuf->f_bfree = 0;
    stbuf->f_bavail = 0;
    stbuf->f_files = 0;
    stbuf->f_ffree = 0;
    stbuf->f_namelen = NAME_MAX - 1; // ? not sure..
    return 0;
};
