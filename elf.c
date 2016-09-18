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


#include <elf.h>
#include <byteswap.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>


typedef Elf32_Nhdr Elf_Nhdr;

static char *fname;
static Elf64_Ehdr ehdr;
static Elf64_Phdr *phdr;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATANATIVE ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATANATIVE ELFDATA2MSB
#else
#error "Unknown machine endian"
#endif

static uint16_t file16_to_cpu(uint16_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_16(val);
	return val;
}

static uint32_t file32_to_cpu(uint32_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_32(val);
	return val;
}

static uint64_t file64_to_cpu(uint64_t val)
{
	if (ehdr.e_ident[EI_DATA] != ELFDATANATIVE)
		val = bswap_64(val);
	return val;
}

static unsigned long read_elf32(int fd)
{
	Elf32_Ehdr ehdr32;
	ssize_t ret, i;

	ret = pread(fd, &ehdr32, sizeof(ehdr32), 0);
	if (ret < 0 || (size_t)ret != sizeof(ehdr)) {
		fprintf(stderr, "Read of ELF header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_shoff		= file32_to_cpu(ehdr32.e_shoff);
	ehdr.e_shentsize	= file16_to_cpu(ehdr32.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr32.e_shnum);

	return(ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum));
}

static unsigned long read_elf64(int fd)
{
	Elf64_Ehdr ehdr64;
	ssize_t ret, i;

	ret = pread(fd, &ehdr64, sizeof(ehdr64), 0);
	if (ret < 0 || (size_t)ret != sizeof(ehdr)) {
		fprintf(stderr, "Read of ELF header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_shoff		= file64_to_cpu(ehdr64.e_shoff);
	ehdr.e_shentsize	= file16_to_cpu(ehdr64.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr64.e_shnum);

	return(ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum));
}

unsigned long get_elf_size(char *fname)
/* TODO, FIXME: This assumes that the section header table (SHT) is
the last part of the ELF. This is usually the case but
it could also be that the last section is the last part
of the ELF. This should be checked for.
*/
{
	ssize_t ret;
	int fd;
	static unsigned long size = 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n",
			fname, strerror(errno));
		return(1);
	}
	ret = pread(fd, ehdr.e_ident, EI_NIDENT, 0);
	if (ret != EI_NIDENT) {
		fprintf(stderr, "Read of e_ident from %s failed: %s\n",
			fname, strerror(errno));
		return(1);
	}
	if ((ehdr.e_ident[EI_DATA] != ELFDATA2LSB) &&
	    (ehdr.e_ident[EI_DATA] != ELFDATA2MSB))
	{
		fprintf(stderr, "Unkown ELF data order %u\n",
			ehdr.e_ident[EI_DATA]);
		return(1);
	}
	if(ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
		size = read_elf32(fd);
	} else if(ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
		size = read_elf64(fd);
	} else {
		fprintf(stderr, "Unknown ELF class %u\n", ehdr.e_ident[EI_CLASS]);
		return(1);
	}

	close(fd);
	return size;
}
