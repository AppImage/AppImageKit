/*
Calculate the size of an ELF file on disk based on the information in its header 

Example:

ls -l   126584

Calculation using the values also reported by readelf -h:
Start of section headers	e_shoff		124728
Size of section headers		e_shentsize	64
Number of section headers	e_shnum		29

e_shoff + ( e_shentsize * e_shnum ) =	126584
*/

#ifndef ELFSIZE_H
#define ELFSIZE_H

unsigned long get_elf_size(const char *fname);

#endif /* ELFSIZE_H */
