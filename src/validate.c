#include <glib.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "appimage/appimage.h"
#include "appimage/appimage_shared.h"
#include "light_elf.h"

typedef unsigned char byte;      

char segment_name[] = ".sha256_sig";
char segment_key_name[] = ".sig_key";

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
    
    return 0;
}

int main(int argc,char **argv)	{
    if(argc < 2){
        fprintf(stderr, "Usage: %s Signed.AppImage\n", argv[0]);
        exit(1);
    }

    char *filename = argv[1]; 

    unsigned long skip_offset_sig = 0;
    unsigned long skip_length_sig = 0;

    unsigned long skip_offset_key = 0;
    unsigned long skip_length_key = 0;
  
    if (!appimage_get_elf_section_offset_and_length(filename, ".sha256_sig", &skip_offset_sig, &skip_length_sig)) {
        fprintf(stderr, "Failed to read .sha256_sig section");
        exit(1);
    }
    if (!appimage_get_elf_section_offset_and_length(filename, ".sig_key", &skip_offset_key, &skip_length_key)) {
        skip_length_key = 0;
        skip_offset_key = 0;
    }

    if(skip_length_sig > 0) {
        fprintf(stderr, "Skipping ELF section %s with offset %lu, length %lu\n", segment_name, skip_offset_sig, skip_length_sig);
    } else {
        fprintf(stderr, "ELF section %s not found, is the file signed?\n", segment_name);
        exit(1);
    }
    if(skip_length_key > 0) {
        fprintf(stderr, "Skipping ELF section %s with offset %lu, length %lu\n", segment_key_name, skip_offset_key, skip_length_key);
    } else {
        fprintf(stderr, "ELF section %s not found, assuming older AppImage Standard\n", segment_key_name);
    }
    if(skip_offset_sig + skip_length_sig != skip_offset_key && skip_length_key != 0) {
        fprintf(stderr, "validate only works when .sha256_sig and .sig_key are contiguous in the ELF header\n");
        exit(0);
    }
    int skip_offset = skip_offset_sig;
    int skip_length = skip_length_sig + skip_length_key;
    
    char *digestfile;
    digestfile = g_strconcat("/tmp/", basename(g_strconcat(filename, ".digest", NULL)), NULL);
    char *signaturefile;
    signaturefile = g_strconcat("/tmp/", basename(g_strconcat(filename, ".sig", NULL)), NULL);

    uint8_t *data = malloc(skip_length_sig);
    unsigned long k;
    FILE* fd = fopen(filename, "r");
    fseek(fd, skip_offset_sig, SEEK_SET);
    fread(data, skip_length_sig, sizeof(uint8_t), fd);
    fclose(fd);
    FILE *fpdst2 = fopen(signaturefile, "w");
    if (fpdst2 == NULL) {
        fprintf(stderr, "Not able to open the signature file for writing, aborting");
        exit(1);
    }
    for (k = 0; k < skip_length_sig; k++) {
        fprintf(fpdst2, "%c", data[k]);
    }   
    fclose(fpdst2);
    free(data);
    
    struct stat st;
    stat(filename, &st);
    int size = st.st_size;
    if(size < skip_offset+skip_length){
        fprintf(stderr, "offset+length cannot be less than the file size\n");
        exit(1);
    }

    static char buffer[65];
    sha256_file(filename, buffer, skip_offset, skip_length);
    printf("%s\n", buffer);
    

    FILE *f = fopen(digestfile, "w");
    if (f == NULL){
        printf("Error opening digestfile\n");
        exit(1);
    }
    fprintf(f, "%s", buffer);
    fclose(f);
    if (! g_file_test(digestfile, G_FILE_TEST_IS_REGULAR)) {
        printf("Error writing digestfile\n");
        exit(1);        
    }
    
    char command[1024];
    gchar *gpg2_path = g_find_program_in_path ("gpg2");
    sprintf (command, "%s --verify %s %s", gpg2_path, signaturefile, digestfile);
    fprintf (stderr, "%s\n", command);
    FILE *fp = popen(command, "r");
    if (fp == NULL)
        fprintf(stderr, "gpg2 command did not succeed");
    int exitcode = WEXITSTATUS(pclose(fp));
    unlink(digestfile);
    unlink(signaturefile);
    return exitcode;
}
