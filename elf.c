#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "light_elf.h"
#include "light_byteswap.h"


typedef Elf32_Nhdr Elf_Nhdr;

static char *fname;
static Elf64_Ehdr ehdr;

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
	Elf32_Shdr shdr32;
	off_t last_shdr_offset;
	ssize_t ret;
	unsigned long sht_end, last_section_end;

	ret = pread(fd, &ehdr32, sizeof(ehdr32), 0);
	if (ret < 0 || (size_t)ret != sizeof(ehdr32)) {
		fprintf(stderr, "Read of ELF header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_shoff		= file32_to_cpu(ehdr32.e_shoff);
	ehdr.e_shentsize	= file16_to_cpu(ehdr32.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr32.e_shnum);

	last_shdr_offset = ehdr.e_shoff + (ehdr.e_shentsize * (ehdr.e_shnum - 1));
	ret = pread(fd, &shdr32, sizeof(shdr32), last_shdr_offset);
	if (ret < 0 || (size_t)ret != sizeof(shdr32)) {
		fprintf(stderr, "Read of ELF section header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	/* ELF ends either with the table of section headers (SHT) or with a section. */
	sht_end = ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum);
	last_section_end = file64_to_cpu(shdr32.sh_offset) + file64_to_cpu(shdr32.sh_size);
	return sht_end > last_section_end ? sht_end : last_section_end;
}

static unsigned long read_elf64(int fd)
{
	Elf64_Ehdr ehdr64;
	Elf64_Shdr shdr64;
	off_t last_shdr_offset;
	ssize_t ret;
	unsigned long sht_end, last_section_end;

	ret = pread(fd, &ehdr64, sizeof(ehdr64), 0);
	if (ret < 0 || (size_t)ret != sizeof(ehdr64)) {
		fprintf(stderr, "Read of ELF header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	ehdr.e_shoff		= file64_to_cpu(ehdr64.e_shoff);
	ehdr.e_shentsize	= file16_to_cpu(ehdr64.e_shentsize);
	ehdr.e_shnum		= file16_to_cpu(ehdr64.e_shnum);

	last_shdr_offset = ehdr.e_shoff + (ehdr.e_shentsize * (ehdr.e_shnum - 1));
	ret = pread(fd, &shdr64, sizeof(shdr64), last_shdr_offset);
	if (ret < 0 || (size_t)ret != sizeof(shdr64)) {
		fprintf(stderr, "Read of ELF section header from %s failed: %s\n",
			fname, strerror(errno));
		exit(10);
	}

	/* ELF ends either with the table of section headers (SHT) or with a section. */
	sht_end = ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum);
	last_section_end = file64_to_cpu(shdr64.sh_offset) + file64_to_cpu(shdr64.sh_size);
	return sht_end > last_section_end ? sht_end : last_section_end;
}

unsigned long get_elf_size(const char *fname)
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
