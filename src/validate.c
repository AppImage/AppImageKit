/*
	cc -o validate ../getsection.c ../validate.c -lssl -lcrypto $(pkg-config --cflags glib-2.0) -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
*/

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
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "getsection.h"

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
    
    return 0;
}

int main(int argc,char **argv)	{
    if(argc < 2){
        fprintf(stderr, "Usage: %s Signed.AppImage\n", argv[0]);
        exit(1);
    }

    char *filename = argv[1]; 

    unsigned long skip_offset = 0;
    unsigned long skip_length = 0;
  
    get_elf_section_offset_and_length(filename, ".sha256_sig", &skip_offset, &skip_length);
    if(skip_length > 0) {
        fprintf(stderr, "Skipping ELF section %s with offset %lu, length %lu\n", segment_name, skip_offset, skip_length);
    } else {
        fprintf(stderr, "ELF section %s not found, is the file signed?\n", segment_name);
        exit(1);
    }
    
    char *digestfile;
    digestfile = g_strconcat("/tmp/", basename(g_strconcat(filename, ".digest", NULL)), NULL);
    char *signaturefile;
    signaturefile = g_strconcat("/tmp/", basename(g_strconcat(filename, ".sig", NULL)), NULL);

    uint8_t *data;   
    unsigned long k;
    int fd = open(filename, O_RDONLY);
    data = mmap(NULL, lseek(fd, 0, SEEK_END), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    FILE *fpdst2 = fopen(signaturefile, "w");
    if (fpdst2 == NULL) {
        fprintf(stderr, "Not able to open the signature file for writing, aborting");
        exit(1);
    }
    for (k = skip_offset; k < skip_offset + skip_length; k++) {
        fprintf(fpdst2, "%c", data[k]);
    }   
    fclose(fpdst2);   
    
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
