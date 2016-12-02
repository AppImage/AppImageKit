#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

/* Return the offset, and the length of an ELF section with a given name in a given ELF file */
int get_elf_section_offset_and_lenghth(char* fname, char* section_name, unsigned long *offset, unsigned long *length)
{
    uint8_t *data;   
    int i;  
    int fd = open(fname, O_RDONLY);
    data = mmap(NULL, lseek(fd, 0, SEEK_END), PROT_READ, MAP_SHARED, fd, 0);

#ifdef __i386__
    Elf32_Ehdr *elf;
    Elf32_Shdr *shdr;
    elf = (Elf32_Ehdr *) data;
    shdr = (Elf32_Shdr *) (data + elf->e_shoff);
#else // Default to x86_64
    Elf64_Ehdr *elf;
    Elf64_Shdr *shdr;
    elf = (Elf64_Ehdr *) data;
    shdr = (Elf64_Shdr *) (data + elf->e_shoff);
#endif

    char *strTab = (char *)(data + shdr[elf->e_shstrndx].sh_offset);
    for(i = 0; i <  elf->e_shnum; i++) {
        if(strcmp(&strTab[shdr[i].sh_name], section_name) == 0) {
        *offset = shdr[i].sh_offset;
        *length = shdr[i].sh_size;
        }
    }
    close(fd);
    return(0);
}

void print_hex(char* fname, unsigned long offset, unsigned long length){
    uint8_t *data;   
    unsigned long k;
    int fd = open(fname, O_RDONLY);
    data = mmap(NULL, lseek(fd, 0, SEEK_END), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    for (k = offset; k < offset + length; k++) {
        printf("%x", data[k]);
        }   
        printf("\n");
}

void print_binary(char* fname, unsigned long offset, unsigned long length){
    uint8_t *data;   
    unsigned long k;
    int fd = open(fname, O_RDONLY);
    data = mmap(NULL, lseek(fd, 0, SEEK_END), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    for (k = offset; k < offset + length; k++) {
        printf("%c", data[k]);
        }   
        printf("\n");
}

/*
int main(int ac, char **av)
{
    unsigned long offset = 0;
    unsigned long length = 0; // Where the function will store the result
    char* segment_name;
    segment_name = ".upd_info";
    int res = get_elf_section_offset_and_lenghth(av[1], segment_name, &offset, &length);
    printf("segment_name: %s\n", segment_name);
    printf("offset: %lu\n", offset);
    printf("length: %lu\n", length);
    print_hex(av[1], offset, length);
    print_binary(av[1], offset, length);
    segment_name = ".sha256_sig";
    printf("segment_name: %s\n", segment_name);
    res = get_elf_section_offset_and_lenghth(av[1], segment_name, &offset, &length);
    printf("offset: %lu\n", offset);
    printf("length: %lu\n", length);
    print_hex(av[1], offset, length);
    print_binary(av[1], offset, length);
    return res;
}
*/
