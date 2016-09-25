/*
	cc -o digest digest.c -lssl -lcrypto
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>

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
    if(!buffer) return ENOMEM;

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
    
    return 0;
}

int main(int argc,char **argv)	{
        if(argc < 2){
            fprintf(stderr, "Usage: %s file offset length\n", argv[0]);
            fprintf(stderr, "If no offset and length are provided, the ELF section '%s' is skipped\n\n", segment_name);            
            fprintf(stderr, "Calculate a sha256 of a file except a skipped area from offset to offset+length bytes\n");
            fprintf(stderr, "which is replaced with 0x00 during checksum calculation.\n");
            fprintf(stderr, "This is useful when a signature is placed in the skipped area.\n");
            exit(1);
        }

        int skip_offset;
        int skip_length;
        char *filename = argv[1];   
        
        if(argc < 4){
            /* TODO: replace with more robust code parsing the ELF like in elf_elf_size */
            char line[PATH_MAX];
            char command[PATH_MAX];
            sprintf (command, "/usr/bin/objdump -h %s", filename);
            FILE *fp;
            fp = popen(command, "r");
            if (fp == NULL){
                fprintf(stderr, "Failed to run objdump command\n");
                exit(1);
            }
            long ui_offset = 0;
            while(fgets(line, sizeof(line), fp) != NULL ){
                if(strstr(line, segment_name) != NULL)
                {
                    char *token = strtok(line, " \t"); // Split the line in tokens
                    token = strtok(NULL, " \t"); // We are not interested in this token
                    token = strtok(NULL, " \t");
                    skip_length = (int)strtol(token, NULL, 16);
                    token = strtok(NULL, " \t"); // We are not interested in this token
                    token = strtok(NULL, " \t"); // We are not interested in this token
                    token = strtok(NULL, " \t"); // We are not interested in this token
                    skip_offset = (int)strtol(token, NULL, 16);
                }
            }
            fclose(fp);
            if(skip_length > 0)
                fprintf(stderr, "Skipping ELF section %s with offset %i, length %i\n", segment_name, skip_offset, skip_length);
        } else if(argc == 4) {
            skip_offset = atoi(argv[2]);
            skip_length = atoi(argv[3]);
        } else {
            exit(1);
        }

    struct stat st;
    stat(filename, &st);
    int size = st.st_size;
    if(size < skip_offset+skip_length){
        fprintf(stderr, "offset+length cannot be less than the file size\n");
        exit(1);
    }
	static unsigned char buffer[65];
	int res = sha256_file(filename, buffer, skip_offset, skip_length);
	printf("%s\n", buffer);
	return res;
}
