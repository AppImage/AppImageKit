/*
	cc -o digest getsection.c digest.c -lssl -lcrypto
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include "appimage/appimage_shared.h"

typedef unsigned char byte;      

char segment_name[] = ".sha256_sig";

int sha256_file(char *path, char outputBuffer[65], int skip_offset, int skip_length)
{
    FILE *file = fopen(path, "rb");
    if(!file) return(1);
    byte hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    const int bufSize = 1024*1024;
    byte *buffer = malloc(bufSize);
    int bytesRead = 0;
    if(!buffer) {
        fclose(file);
        return ENOMEM;
    }

    int totalBytesRead = 0;
    if(skip_offset <= bufSize){
        bytesRead = fread(buffer, 1, skip_offset, file);
        totalBytesRead += bytesRead;
        // printf("totalBytesRead: %i\n", totalBytesRead);
        // printf("bytesRead: %i\n", bytesRead);
        SHA256_Update(&sha256, buffer, bytesRead);
    } else {
        int stillToRead = skip_offset-bytesRead;
        // printf("Initial stillToRead: %i\n", stillToRead);
        int readThisTime;

        if(stillToRead>bufSize){
            readThisTime = bufSize;
        } else {
            readThisTime = stillToRead;
        }
        while((bytesRead = fread(buffer, 1, readThisTime, file)))
        {
            totalBytesRead += bytesRead;
            // printf("totalBytesRead: %i\n", totalBytesRead);
            // printf("readThisTime: %i\n", readThisTime);
            // printf("bytesRead: %i\n", bytesRead);
            SHA256_Update(&sha256, buffer, bytesRead);
            stillToRead = skip_offset-totalBytesRead;
            // printf("stillToRead: %i\n", stillToRead);

            if(stillToRead>bufSize){
                readThisTime = bufSize;
            } else {
                readThisTime = stillToRead;
            }
        }
    }

    fseek(file, skip_offset+skip_length, SEEK_SET);
    
    /* Instead of the skipped area, calculate the sha256 of the same amount if 0x00s */
    int i = 0;
    for(i = 0; i < skip_length; i++) {
        SHA256_Update(&sha256, "\0", 1);
        totalBytesRead += 1;
    }
        
    while((bytesRead = fread(buffer, 1, bufSize, file)))
    {
        totalBytesRead += bytesRead;
        // printf("totalBytesRead: %i\n", totalBytesRead);
        // printf("bytesRead: %i\n", bytesRead);
        SHA256_Update(&sha256, buffer, bytesRead);
    }
    SHA256_Final(hash, &sha256);

    // fprintf(stderr, "totalBytesRead: %i\n", totalBytesRead);
    
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;

    fclose(file);
    free(buffer);
    
    return 0;
}

int main(int argc,char **argv)
{
    int res = 0;

    if(argc < 2){
        fprintf(stderr, "Usage: %s file offset length\n", argv[0]);
        fprintf(stderr, "If no offset and length are provided, the ELF section '%s' is skipped\n\n", segment_name);
        fprintf(stderr, "Calculate a sha256 of a file except a skipped area from offset to offset+length bytes\n");
        fprintf(stderr, "which is replaced with 0x00 during checksum calculation.\n");
        fprintf(stderr, "This is useful when a signature is placed in the skipped area.\n");
        exit(1);
    }

    unsigned long skip_offset = 0;
    unsigned long skip_length = 0;
    char *filename = argv[1];
        
    struct stat st;
    if (stat(filename, &st) < 0) {
        fprintf(stderr, "not existing file: %s\n", filename);
        exit(1);
    }

    if(argc < 4){
        appimage_get_elf_section_offset_and_length(filename, ".sha256_sig", &skip_offset, &skip_length);
        if(skip_length > 0)
            fprintf(stderr, "Skipping ELF section %s with offset %lu, length %lu\n", segment_name, skip_offset, skip_length);
    } else if(argc == 4) {
        skip_offset = atoi(argv[2]);
        skip_length = atoi(argv[3]);
    } else {
        exit(1);
    }

    int size = st.st_size;
    if(size < skip_offset+skip_length){
        fprintf(stderr, "offset+length cannot be less than the file size\n");
        exit(1);
    }

    static char buffer[65];
    res = sha256_file(filename, buffer, skip_offset, skip_length);
    printf("%s\n", buffer);
    return res;
}
