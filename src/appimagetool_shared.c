#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "appimage/appimage.h"
#include "getsection.h"

/* Check if a file is an AppImage. Returns the image type if it is, or -1 if it isn't */
int appimage_get_type(const char* path, bool verbose)
{
    FILE *f = fopen(path, "rt");
    if (f != NULL)
    {
        char buffer[3] = {0};

        /* Check magic bytes at offset 8 */
        fseek(f, 8, SEEK_SET);
        fread(buffer, 1, 3, f);
        fclose(f);
        if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x01)){
#ifdef STANDALONE
            fprintf(stderr, "_________________________\n");
#endif
            if(verbose){
                fprintf(stderr, "AppImage type 1\n");
            }
            return 1;
        } else if((buffer[0] == 0x41) && (buffer[1] == 0x49) && (buffer[2] == 0x02)){
#ifdef STANDALONE
            fprintf(stderr, "_________________________\n");
#endif
            if(verbose){
                fprintf(stderr, "AppImage type 2\n");
            }
            return 2;
        } else {
            if((g_str_has_suffix(path,".AppImage")) || (g_str_has_suffix(path,".appimage"))) {
#ifdef STANDALONE
                fprintf(stderr, "_________________________\n");
#endif
                if (verbose) {
                    fprintf(stderr, "Blindly assuming AppImage type 1\n");
                    fprintf(stderr, "The author of this AppImage should embed the magic bytes, see https://github.com/AppImage/AppImageSpec\n");
                }
                return 1;
            } else {
#ifdef STANDALONE
                fprintf(stderr, "_________________________\n");
#endif
                if(verbose){
                    fprintf(stderr, "Unrecognized file '%s'\n", path);
                }
                return -1;
            }
        }
    }
    return -1;
}

bool appimage_type2_digest_md5(const char* path, char* digest) {
    // skip digest, signature and key sections in digest calculation
    unsigned long digest_md5_offset = 0, digest_md5_length = 0;
    if (get_elf_section_offset_and_length(path, ".digest_md5", &digest_md5_offset, &digest_md5_length) != 0)
        return false;

    unsigned long signature_offset = 0, signature_length = 0;
    if (get_elf_section_offset_and_length(path, ".sha256_sig", &signature_offset, &signature_length) != 0)
        return false;

    unsigned long sig_key_offset = 0, sig_key_length = 0;
    if (get_elf_section_offset_and_length(path, ".sig_key", &sig_key_offset, &sig_key_length) != 0)
        return false;

    GChecksum *checksum = g_checksum_new(G_CHECKSUM_MD5);

    // read file in chunks
    static const int chunk_size = 4096;

    FILE *fp = fopen(path, "r");

    // determine file size
    fseek(fp, 0L, SEEK_END);
    const long file_size = ftell(fp);
    rewind(fp);

    long bytes_left = file_size;

    // if a section spans over more than a single chunk, we need emulate null bytes in the following chunks
    ssize_t bytes_skip_following_chunks = 0;

    while (bytes_left > 0) {
        char buffer[chunk_size];

        long current_position = ftell(fp);

        ssize_t bytes_left_this_chunk = chunk_size;

        // first, check whether there's bytes left that need to be skipped
        if (bytes_skip_following_chunks > 0) {
            ssize_t bytes_skip_this_chunk = (bytes_skip_following_chunks % chunk_size == 0) ? chunk_size : (bytes_skip_following_chunks % chunk_size);
            bytes_left_this_chunk -= bytes_skip_this_chunk;

            // we could just set it to 0 here, but it makes more sense to use -= for debugging
            bytes_skip_following_chunks -= bytes_skip_this_chunk;

            // make sure to skip these bytes in the file
            fseek(fp, bytes_skip_this_chunk, SEEK_CUR);
        }

        // check whether there's a section in this chunk that we need to skip
        if (digest_md5_offset != 0 && digest_md5_length != 0 && digest_md5_offset - current_position > 0 && digest_md5_offset - current_position < chunk_size) {
            ssize_t begin_of_section = (digest_md5_offset - current_position) % chunk_size;
            // read chunk before section
            fread(buffer, sizeof(char), (size_t) begin_of_section, fp);

            bytes_left_this_chunk -= begin_of_section;
            bytes_left_this_chunk -= digest_md5_length;

            // if bytes_left is now < 0, the section exceeds the current chunk
            // this amount of bytes needs to be skipped in the future sections
            if (bytes_left_this_chunk < 0) {
                bytes_skip_following_chunks = (size_t) (-1 * bytes_left_this_chunk);
                bytes_left_this_chunk = 0;
            }

            // if there's bytes left to read, we need to seek the difference between chunk's end and bytes_left
            fseek(fp, (chunk_size - bytes_left_this_chunk - begin_of_section), SEEK_CUR);
        }

        // check whether there's a section in this chunk that we need to skip
        if (signature_offset != 0 && signature_length != 0 && signature_offset - current_position > 0 && signature_offset - current_position < chunk_size) {
            ssize_t begin_of_section = (signature_offset - current_position) % chunk_size;
            // read chunk before section
            fread(buffer, sizeof(char), (size_t) begin_of_section, fp);

            bytes_left_this_chunk -= begin_of_section;
            bytes_left_this_chunk -= signature_length;

            // if bytes_left is now < 0, the section exceeds the current chunk
            // this amount of bytes needs to be skipped in the future sections
            if (bytes_left_this_chunk < 0) {
                bytes_skip_following_chunks = (size_t) (-1 * bytes_left_this_chunk);
                bytes_left_this_chunk = 0;
            }

            // if there's bytes left to read, we need to seek the difference between chunk's end and bytes_left
            fseek(fp, (chunk_size - bytes_left_this_chunk - begin_of_section), SEEK_CUR);
        }

        // check whether there's a section in this chunk that we need to skip
        if (sig_key_offset != 0 && sig_key_length != 0 && sig_key_offset - current_position > 0 && sig_key_offset - current_position < chunk_size) {
            ssize_t begin_of_section = (sig_key_offset - current_position) % chunk_size;
            // read chunk before section
            fread(buffer, sizeof(char), (size_t) begin_of_section, fp);

            bytes_left_this_chunk -= begin_of_section;
            bytes_left_this_chunk -= sig_key_length;

            // if bytes_left is now < 0, the section exceeds the current chunk
            // this amount of bytes needs to be skipped in the future sections
            if (bytes_left_this_chunk < 0) {
                bytes_skip_following_chunks = (size_t) (-1 * bytes_left_this_chunk);
                bytes_left_this_chunk = 0;
            }

            // if there's bytes left to read, we need to seek the difference between chunk's end and bytes_left
            fseek(fp, (chunk_size - bytes_left_this_chunk - begin_of_section), SEEK_CUR);
        }

        // check whether we're done already
        if (bytes_left_this_chunk > 0) {
            // read data from file into buffer with the correct offset in case bytes have to be skipped
            fread(buffer + (chunk_size - bytes_left_this_chunk), sizeof(char), (size_t) bytes_left_this_chunk, fp);
        }

        // feed buffer into checksum calculation
        g_checksum_update(checksum, (const guchar*) buffer, chunk_size);

        bytes_left -= chunk_size;
    }

    gsize digest_len = 16;
    g_checksum_get_digest(checksum, (guint8*) digest, &digest_len);

    return true;
}

char* appimage_hexlify(const char* bytes, const size_t numBytes) {
    // first of all, allocate the new string
    // a hexadecimal representation works like "every byte will be represented by two chars"
    // additionally, we need to null-terminate the string
    char* hexlified = (char*) calloc((2 * numBytes + 1), sizeof(char));

    for (size_t i = 0; i < numBytes; i++) {
        char buffer[3];
        sprintf(buffer, "%02x", (unsigned char) bytes[i]);
        strcat(hexlified, buffer);
    }

    return hexlified;
}
