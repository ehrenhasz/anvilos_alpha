
 

#include "test_util.h"

#include <bits/endian.h>
#include <linux/elf.h>

#include "kvm_util.h"

static void elfhdr_get(const char *filename, Elf64_Ehdr *hdrp)
{
	off_t offset_rv;

	 
	int fd;
	fd = open(filename, O_RDONLY);
	TEST_ASSERT(fd >= 0, "Failed to open ELF file,\n"
		"  filename: %s\n"
		"  rv: %i errno: %i", filename, fd, errno);

	 
	unsigned char ident[EI_NIDENT];
	test_read(fd, ident, sizeof(ident));
	TEST_ASSERT((ident[EI_MAG0] == ELFMAG0) && (ident[EI_MAG1] == ELFMAG1)
		&& (ident[EI_MAG2] == ELFMAG2) && (ident[EI_MAG3] == ELFMAG3),
		"ELF MAGIC Mismatch,\n"
		"  filename: %s\n"
		"  ident[EI_MAG0 - EI_MAG3]: %02x %02x %02x %02x\n"
		"  Expected: %02x %02x %02x %02x",
		filename,
		ident[EI_MAG0], ident[EI_MAG1], ident[EI_MAG2], ident[EI_MAG3],
		ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3);
	TEST_ASSERT(ident[EI_CLASS] == ELFCLASS64,
		"Current implementation only able to handle ELFCLASS64,\n"
		"  filename: %s\n"
		"  ident[EI_CLASS]: %02x\n"
		"  expected: %02x",
		filename,
		ident[EI_CLASS], ELFCLASS64);
	TEST_ASSERT(((BYTE_ORDER == LITTLE_ENDIAN)
			&& (ident[EI_DATA] == ELFDATA2LSB))
		|| ((BYTE_ORDER == BIG_ENDIAN)
			&& (ident[EI_DATA] == ELFDATA2MSB)), "Current "
		"implementation only able to handle\n"
		"cases where the host and ELF file endianness\n"
		"is the same:\n"
		"  host BYTE_ORDER: %u\n"
		"  host LITTLE_ENDIAN: %u\n"
		"  host BIG_ENDIAN: %u\n"
		"  ident[EI_DATA]: %u\n"
		"  ELFDATA2LSB: %u\n"
		"  ELFDATA2MSB: %u",
		BYTE_ORDER, LITTLE_ENDIAN, BIG_ENDIAN,
		ident[EI_DATA], ELFDATA2LSB, ELFDATA2MSB);
	TEST_ASSERT(ident[EI_VERSION] == EV_CURRENT,
		"Current implementation only able to handle current "
		"ELF version,\n"
		"  filename: %s\n"
		"  ident[EI_VERSION]: %02x\n"
		"  expected: %02x",
		filename, ident[EI_VERSION], EV_CURRENT);

	 
	offset_rv = lseek(fd, 0, SEEK_SET);
	TEST_ASSERT(offset_rv == 0, "Seek to ELF header failed,\n"
		"  rv: %zi expected: %i", offset_rv, 0);
	test_read(fd, hdrp, sizeof(*hdrp));
	TEST_ASSERT(hdrp->e_phentsize == sizeof(Elf64_Phdr),
		"Unexpected physical header size,\n"
		"  hdrp->e_phentsize: %x\n"
		"  expected: %zx",
		hdrp->e_phentsize, sizeof(Elf64_Phdr));
	TEST_ASSERT(hdrp->e_shentsize == sizeof(Elf64_Shdr),
		"Unexpected section header size,\n"
		"  hdrp->e_shentsize: %x\n"
		"  expected: %zx",
		hdrp->e_shentsize, sizeof(Elf64_Shdr));
	close(fd);
}

 
void kvm_vm_elf_load(struct kvm_vm *vm, const char *filename)
{
	off_t offset, offset_rv;
	Elf64_Ehdr hdr;

	 
	int fd;
	fd = open(filename, O_RDONLY);
	TEST_ASSERT(fd >= 0, "Failed to open ELF file,\n"
		"  filename: %s\n"
		"  rv: %i errno: %i", filename, fd, errno);

	 
	elfhdr_get(filename, &hdr);

	 
	for (unsigned int n1 = 0; n1 < hdr.e_phnum; n1++) {
		 
		offset = hdr.e_phoff + (n1 * hdr.e_phentsize);
		offset_rv = lseek(fd, offset, SEEK_SET);
		TEST_ASSERT(offset_rv == offset,
			"Failed to seek to beginning of program header %u,\n"
			"  filename: %s\n"
			"  rv: %jd errno: %i",
			n1, filename, (intmax_t) offset_rv, errno);

		 
		Elf64_Phdr phdr;
		test_read(fd, &phdr, sizeof(phdr));

		 
		if (phdr.p_type != PT_LOAD)
			continue;

		 
		TEST_ASSERT(phdr.p_memsz > 0, "Unexpected loadable segment "
			"memsize of 0,\n"
			"  phdr index: %u p_memsz: 0x%" PRIx64,
			n1, (uint64_t) phdr.p_memsz);
		vm_vaddr_t seg_vstart = align_down(phdr.p_vaddr, vm->page_size);
		vm_vaddr_t seg_vend = phdr.p_vaddr + phdr.p_memsz - 1;
		seg_vend |= vm->page_size - 1;
		size_t seg_size = seg_vend - seg_vstart + 1;

		vm_vaddr_t vaddr = __vm_vaddr_alloc(vm, seg_size, seg_vstart,
						    MEM_REGION_CODE);
		TEST_ASSERT(vaddr == seg_vstart, "Unable to allocate "
			"virtual memory for segment at requested min addr,\n"
			"  segment idx: %u\n"
			"  seg_vstart: 0x%lx\n"
			"  vaddr: 0x%lx",
			n1, seg_vstart, vaddr);
		memset(addr_gva2hva(vm, vaddr), 0, seg_size);
		 

		 
		if (phdr.p_filesz) {
			offset_rv = lseek(fd, phdr.p_offset, SEEK_SET);
			TEST_ASSERT(offset_rv == phdr.p_offset,
				"Seek to program segment offset failed,\n"
				"  program header idx: %u errno: %i\n"
				"  offset_rv: 0x%jx\n"
				"  expected: 0x%jx\n",
				n1, errno, (intmax_t) offset_rv,
				(intmax_t) phdr.p_offset);
			test_read(fd, addr_gva2hva(vm, phdr.p_vaddr),
				phdr.p_filesz);
		}
	}
	close(fd);
}
