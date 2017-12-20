#ifndef GETSECTION_H
#define GETSECTION_H

/* Return the offset, and the length of an ELF section with a given name in a given ELF file */
int get_elf_section_offset_and_length(const char* fname, const char* section_name, unsigned long *offset, unsigned long *length);

void print_hex(char* fname, unsigned long offset, unsigned long length);

void print_binary(char* fname, unsigned long offset, unsigned long length);

#endif /* GETSECTION_H */
