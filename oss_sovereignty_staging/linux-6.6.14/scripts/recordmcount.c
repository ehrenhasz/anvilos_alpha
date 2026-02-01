
 

 

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <getopt.h>
#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef EM_AARCH64
#define EM_AARCH64	183
#define R_AARCH64_NONE		0
#define R_AARCH64_ABS64	257
#endif

#ifndef EM_LOONGARCH
#define EM_LOONGARCH		258
#define R_LARCH_32			1
#define R_LARCH_64			2
#define R_LARCH_MARK_LA			20
#define R_LARCH_SOP_PUSH_PLT_PCREL	29
#endif

#define R_ARM_PC24		1
#define R_ARM_THM_CALL		10
#define R_ARM_CALL		28

#define R_AARCH64_CALL26	283

static int fd_map;	 
static int mmap_failed;  
static char gpfx;	 
static struct stat sb;	 
static const char *altmcount;	 
static int warn_on_notrace_sect;  
static void *file_map;	 
static void *file_end;	 
static int file_updated;  
static void *file_ptr;	 

static void *file_append;  
static size_t file_append_size;  

 
static void file_append_cleanup(void)
{
	free(file_append);
	file_append = NULL;
	file_append_size = 0;
	file_updated = 0;
}

static void mmap_cleanup(void)
{
	if (!mmap_failed)
		munmap(file_map, sb.st_size);
	else
		free(file_map);
	file_map = NULL;
}

 

static off_t ulseek(off_t const offset, int const whence)
{
	switch (whence) {
	case SEEK_SET:
		file_ptr = file_map + offset;
		break;
	case SEEK_CUR:
		file_ptr += offset;
		break;
	case SEEK_END:
		file_ptr = file_map + (sb.st_size - offset);
		break;
	}
	if (file_ptr < file_map) {
		fprintf(stderr, "lseek: seek before file\n");
		return -1;
	}
	return file_ptr - file_map;
}

static ssize_t uwrite(void const *const buf, size_t const count)
{
	size_t cnt = count;
	off_t idx = 0;
	void *p = NULL;

	file_updated = 1;

	if (file_ptr + count >= file_end) {
		off_t aoffset = (file_ptr + count) - file_end;

		if (aoffset > file_append_size) {
			p = realloc(file_append, aoffset);
			if (!p)
				free(file_append);
			file_append = p;
			file_append_size = aoffset;
		}
		if (!file_append) {
			perror("write");
			file_append_cleanup();
			mmap_cleanup();
			return -1;
		}
		if (file_ptr < file_end) {
			cnt = file_end - file_ptr;
		} else {
			cnt = 0;
			idx = aoffset - count;
		}
	}

	if (cnt)
		memcpy(file_ptr, buf, cnt);

	if (cnt < count)
		memcpy(file_append + idx, buf + cnt, count - cnt);

	file_ptr += count;
	return count;
}

static void * umalloc(size_t size)
{
	void *const addr = malloc(size);
	if (addr == 0) {
		fprintf(stderr, "malloc failed: %zu bytes\n", size);
		file_append_cleanup();
		mmap_cleanup();
		return NULL;
	}
	return addr;
}

 
static void *mmap_file(char const *fname)
{
	 
	fd_map = -1;
	mmap_failed = 1;
	file_map = NULL;
	file_ptr = NULL;
	file_updated = 0;
	sb.st_size = 0;

	fd_map = open(fname, O_RDONLY);
	if (fd_map < 0) {
		perror(fname);
		return NULL;
	}
	if (fstat(fd_map, &sb) < 0) {
		perror(fname);
		goto out;
	}
	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "not a regular file: %s\n", fname);
		goto out;
	}
	file_map = mmap(0, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE,
			fd_map, 0);
	if (file_map == MAP_FAILED) {
		mmap_failed = 1;
		file_map = umalloc(sb.st_size);
		if (!file_map) {
			perror(fname);
			goto out;
		}
		if (read(fd_map, file_map, sb.st_size) != sb.st_size) {
			perror(fname);
			free(file_map);
			file_map = NULL;
			goto out;
		}
	} else
		mmap_failed = 0;
out:
	close(fd_map);
	fd_map = -1;

	file_end = file_map + sb.st_size;

	return file_map;
}


static unsigned char ideal_nop5_x86_64[5] = { 0x0f, 0x1f, 0x44, 0x00, 0x00 };
static unsigned char ideal_nop5_x86_32[5] = { 0x3e, 0x8d, 0x74, 0x26, 0x00 };
static unsigned char *ideal_nop;

static char rel_type_nop;

static int (*make_nop)(void *map, size_t const offset);

static int make_nop_x86(void *map, size_t const offset)
{
	uint32_t *ptr;
	unsigned char *op;

	 
	ptr = map + offset;
	if (*ptr != 0)
		return -1;

	op = map + offset - 1;
	if (*op != 0xe8)
		return -1;

	 
	if (ulseek(offset - 1, SEEK_SET) < 0)
		return -1;
	if (uwrite(ideal_nop, 5) < 0)
		return -1;
	return 0;
}

static unsigned char ideal_nop4_arm_le[4] = { 0x00, 0x00, 0xa0, 0xe1 };  
static unsigned char ideal_nop4_arm_be[4] = { 0xe1, 0xa0, 0x00, 0x00 };  
static unsigned char *ideal_nop4_arm;

static unsigned char bl_mcount_arm_le[4] = { 0xfe, 0xff, 0xff, 0xeb };  
static unsigned char bl_mcount_arm_be[4] = { 0xeb, 0xff, 0xff, 0xfe };  
static unsigned char *bl_mcount_arm;

static unsigned char push_arm_le[4] = { 0x04, 0xe0, 0x2d, 0xe5 };  
static unsigned char push_arm_be[4] = { 0xe5, 0x2d, 0xe0, 0x04 };  
static unsigned char *push_arm;

static unsigned char ideal_nop2_thumb_le[2] = { 0x00, 0xbf };  
static unsigned char ideal_nop2_thumb_be[2] = { 0xbf, 0x00 };  
static unsigned char *ideal_nop2_thumb;

static unsigned char push_bl_mcount_thumb_le[6] = { 0x00, 0xb5, 0xff, 0xf7, 0xfe, 0xff };  
static unsigned char push_bl_mcount_thumb_be[6] = { 0xb5, 0x00, 0xf7, 0xff, 0xff, 0xfe };  
static unsigned char *push_bl_mcount_thumb;

static int make_nop_arm(void *map, size_t const offset)
{
	char *ptr;
	int cnt = 1;
	int nop_size;
	size_t off = offset;

	ptr = map + offset;
	if (memcmp(ptr, bl_mcount_arm, 4) == 0) {
		if (memcmp(ptr - 4, push_arm, 4) == 0) {
			off -= 4;
			cnt = 2;
		}
		ideal_nop = ideal_nop4_arm;
		nop_size = 4;
	} else if (memcmp(ptr - 2, push_bl_mcount_thumb, 6) == 0) {
		cnt = 3;
		nop_size = 2;
		off -= 2;
		ideal_nop = ideal_nop2_thumb;
	} else
		return -1;

	 
	if (ulseek(off, SEEK_SET) < 0)
		return -1;

	do {
		if (uwrite(ideal_nop, nop_size) < 0)
			return -1;
	} while (--cnt > 0);

	return 0;
}

static unsigned char ideal_nop4_arm64[4] = {0x1f, 0x20, 0x03, 0xd5};
static int make_nop_arm64(void *map, size_t const offset)
{
	uint32_t *ptr;

	ptr = map + offset;
	 
	if (*ptr != 0x94000000)
		return -1;

	 
	if (ulseek(offset, SEEK_SET) < 0)
		return -1;
	if (uwrite(ideal_nop, 4) < 0)
		return -1;
	return 0;
}

static int write_file(const char *fname)
{
	char tmp_file[strlen(fname) + 4];
	size_t n;

	if (!file_updated)
		return 0;

	sprintf(tmp_file, "%s.rc", fname);

	 
	fd_map = open(tmp_file, O_WRONLY | O_TRUNC | O_CREAT, sb.st_mode);
	if (fd_map < 0) {
		perror(fname);
		return -1;
	}
	n = write(fd_map, file_map, sb.st_size);
	if (n != sb.st_size) {
		perror("write");
		close(fd_map);
		return -1;
	}
	if (file_append_size) {
		n = write(fd_map, file_append, file_append_size);
		if (n != file_append_size) {
			perror("write");
			close(fd_map);
			return -1;
		}
	}
	close(fd_map);
	if (rename(tmp_file, fname) < 0) {
		perror(fname);
		return -1;
	}
	return 0;
}

 

static uint64_t w8rev(uint64_t const x)
{
	return   ((0xff & (x >> (0 * 8))) << (7 * 8))
	       | ((0xff & (x >> (1 * 8))) << (6 * 8))
	       | ((0xff & (x >> (2 * 8))) << (5 * 8))
	       | ((0xff & (x >> (3 * 8))) << (4 * 8))
	       | ((0xff & (x >> (4 * 8))) << (3 * 8))
	       | ((0xff & (x >> (5 * 8))) << (2 * 8))
	       | ((0xff & (x >> (6 * 8))) << (1 * 8))
	       | ((0xff & (x >> (7 * 8))) << (0 * 8));
}

static uint32_t w4rev(uint32_t const x)
{
	return   ((0xff & (x >> (0 * 8))) << (3 * 8))
	       | ((0xff & (x >> (1 * 8))) << (2 * 8))
	       | ((0xff & (x >> (2 * 8))) << (1 * 8))
	       | ((0xff & (x >> (3 * 8))) << (0 * 8));
}

static uint32_t w2rev(uint16_t const x)
{
	return   ((0xff & (x >> (0 * 8))) << (1 * 8))
	       | ((0xff & (x >> (1 * 8))) << (0 * 8));
}

static uint64_t w8nat(uint64_t const x)
{
	return x;
}

static uint32_t w4nat(uint32_t const x)
{
	return x;
}

static uint32_t w2nat(uint16_t const x)
{
	return x;
}

static uint64_t (*w8)(uint64_t);
static uint32_t (*w)(uint32_t);
static uint32_t (*w2)(uint16_t);

 
static int is_mcounted_section_name(char const *const txtname)
{
	return strncmp(".text",          txtname, 5) == 0 ||
		strcmp(".init.text",     txtname) == 0 ||
		strcmp(".ref.text",      txtname) == 0 ||
		strcmp(".sched.text",    txtname) == 0 ||
		strcmp(".spinlock.text", txtname) == 0 ||
		strcmp(".irqentry.text", txtname) == 0 ||
		strcmp(".softirqentry.text", txtname) == 0 ||
		strcmp(".kprobes.text", txtname) == 0 ||
		strcmp(".cpuidle.text", txtname) == 0;
}

static char const *already_has_rel_mcount = "success";  

 
#include "recordmcount.h"
#define RECORD_MCOUNT_64
#include "recordmcount.h"

static int arm_is_fake_mcount(Elf32_Rel const *rp)
{
	switch (ELF32_R_TYPE(w(rp->r_info))) {
	case R_ARM_THM_CALL:
	case R_ARM_CALL:
	case R_ARM_PC24:
		return 0;
	}

	return 1;
}

static int arm64_is_fake_mcount(Elf64_Rel const *rp)
{
	return ELF64_R_TYPE(w8(rp->r_info)) != R_AARCH64_CALL26;
}

static int LARCH32_is_fake_mcount(Elf32_Rel const *rp)
{
	switch (ELF64_R_TYPE(w(rp->r_info))) {
	case R_LARCH_MARK_LA:
	case R_LARCH_SOP_PUSH_PLT_PCREL:
		return 0;
	}

	return 1;
}

static int LARCH64_is_fake_mcount(Elf64_Rel const *rp)
{
	switch (ELF64_R_TYPE(w(rp->r_info))) {
	case R_LARCH_MARK_LA:
	case R_LARCH_SOP_PUSH_PLT_PCREL:
		return 0;
	}

	return 1;
}

 

typedef uint8_t myElf64_Byte;		 

union mips_r_info {
	Elf64_Xword r_info;
	struct {
		Elf64_Word r_sym;		 
		myElf64_Byte r_ssym;		 
		myElf64_Byte r_type3;		 
		myElf64_Byte r_type2;		 
		myElf64_Byte r_type;		 
	} r_mips;
};

static uint64_t MIPS64_r_sym(Elf64_Rel const *rp)
{
	return w(((union mips_r_info){ .r_info = rp->r_info }).r_mips.r_sym);
}

static void MIPS64_r_info(Elf64_Rel *const rp, unsigned sym, unsigned type)
{
	rp->r_info = ((union mips_r_info){
		.r_mips = { .r_sym = w(sym), .r_type = type }
	}).r_info;
}

static int do_file(char const *const fname)
{
	unsigned int reltype = 0;
	Elf32_Ehdr *ehdr;
	int rc = -1;

	ehdr = mmap_file(fname);
	if (!ehdr)
		goto out;

	w = w4nat;
	w2 = w2nat;
	w8 = w8nat;
	switch (ehdr->e_ident[EI_DATA]) {
		static unsigned int const endian = 1;
	default:
		fprintf(stderr, "unrecognized ELF data encoding %d: %s\n",
			ehdr->e_ident[EI_DATA], fname);
		goto out;
	case ELFDATA2LSB:
		if (*(unsigned char const *)&endian != 1) {
			 
			w = w4rev;
			w2 = w2rev;
			w8 = w8rev;
		}
		ideal_nop4_arm = ideal_nop4_arm_le;
		bl_mcount_arm = bl_mcount_arm_le;
		push_arm = push_arm_le;
		ideal_nop2_thumb = ideal_nop2_thumb_le;
		push_bl_mcount_thumb = push_bl_mcount_thumb_le;
		break;
	case ELFDATA2MSB:
		if (*(unsigned char const *)&endian != 0) {
			 
			w = w4rev;
			w2 = w2rev;
			w8 = w8rev;
		}
		ideal_nop4_arm = ideal_nop4_arm_be;
		bl_mcount_arm = bl_mcount_arm_be;
		push_arm = push_arm_be;
		ideal_nop2_thumb = ideal_nop2_thumb_be;
		push_bl_mcount_thumb = push_bl_mcount_thumb_be;
		break;
	}   
	if (memcmp(ELFMAG, ehdr->e_ident, SELFMAG) != 0 ||
	    w2(ehdr->e_type) != ET_REL ||
	    ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
		fprintf(stderr, "unrecognized ET_REL file %s\n", fname);
		goto out;
	}

	gpfx = '_';
	switch (w2(ehdr->e_machine)) {
	default:
		fprintf(stderr, "unrecognized e_machine %u %s\n",
			w2(ehdr->e_machine), fname);
		goto out;
	case EM_386:
		reltype = R_386_32;
		rel_type_nop = R_386_NONE;
		make_nop = make_nop_x86;
		ideal_nop = ideal_nop5_x86_32;
		mcount_adjust_32 = -1;
		gpfx = 0;
		break;
	case EM_ARM:
		reltype = R_ARM_ABS32;
		altmcount = "__gnu_mcount_nc";
		make_nop = make_nop_arm;
		rel_type_nop = R_ARM_NONE;
		is_fake_mcount32 = arm_is_fake_mcount;
		gpfx = 0;
		break;
	case EM_AARCH64:
		reltype = R_AARCH64_ABS64;
		make_nop = make_nop_arm64;
		rel_type_nop = R_AARCH64_NONE;
		ideal_nop = ideal_nop4_arm64;
		is_fake_mcount64 = arm64_is_fake_mcount;
		break;
	case EM_IA_64:	reltype = R_IA64_IMM64; break;
	case EM_MIPS:	  break;
	case EM_LOONGARCH:	  break;
	case EM_PPC:	reltype = R_PPC_ADDR32; break;
	case EM_PPC64:	reltype = R_PPC64_ADDR64; break;
	case EM_S390:	  break;
	case EM_SH:	reltype = R_SH_DIR32; gpfx = 0; break;
	case EM_SPARCV9: reltype = R_SPARC_64; break;
	case EM_X86_64:
		make_nop = make_nop_x86;
		ideal_nop = ideal_nop5_x86_64;
		reltype = R_X86_64_64;
		rel_type_nop = R_X86_64_NONE;
		mcount_adjust_64 = -1;
		gpfx = 0;
		break;
	}   

	switch (ehdr->e_ident[EI_CLASS]) {
	default:
		fprintf(stderr, "unrecognized ELF class %d %s\n",
			ehdr->e_ident[EI_CLASS], fname);
		goto out;
	case ELFCLASS32:
		if (w2(ehdr->e_ehsize) != sizeof(Elf32_Ehdr)
		||  w2(ehdr->e_shentsize) != sizeof(Elf32_Shdr)) {
			fprintf(stderr,
				"unrecognized ET_REL file: %s\n", fname);
			goto out;
		}
		if (w2(ehdr->e_machine) == EM_MIPS) {
			reltype = R_MIPS_32;
			is_fake_mcount32 = MIPS32_is_fake_mcount;
		}
		if (w2(ehdr->e_machine) == EM_LOONGARCH) {
			reltype = R_LARCH_32;
			is_fake_mcount32 = LARCH32_is_fake_mcount;
		}
		if (do32(ehdr, fname, reltype) < 0)
			goto out;
		break;
	case ELFCLASS64: {
		Elf64_Ehdr *const ghdr = (Elf64_Ehdr *)ehdr;
		if (w2(ghdr->e_ehsize) != sizeof(Elf64_Ehdr)
		||  w2(ghdr->e_shentsize) != sizeof(Elf64_Shdr)) {
			fprintf(stderr,
				"unrecognized ET_REL file: %s\n", fname);
			goto out;
		}
		if (w2(ghdr->e_machine) == EM_S390) {
			reltype = R_390_64;
			mcount_adjust_64 = -14;
		}
		if (w2(ghdr->e_machine) == EM_MIPS) {
			reltype = R_MIPS_64;
			Elf64_r_sym = MIPS64_r_sym;
			Elf64_r_info = MIPS64_r_info;
			is_fake_mcount64 = MIPS64_is_fake_mcount;
		}
		if (w2(ghdr->e_machine) == EM_LOONGARCH) {
			reltype = R_LARCH_64;
			is_fake_mcount64 = LARCH64_is_fake_mcount;
		}
		if (do64(ghdr, fname, reltype) < 0)
			goto out;
		break;
	}
	}   

	rc = write_file(fname);
out:
	file_append_cleanup();
	mmap_cleanup();
	return rc;
}

int main(int argc, char *argv[])
{
	const char ftrace[] = "/ftrace.o";
	int ftrace_size = sizeof(ftrace) - 1;
	int n_error = 0;   
	int c;
	int i;

	while ((c = getopt(argc, argv, "w")) >= 0) {
		switch (c) {
		case 'w':
			warn_on_notrace_sect = 1;
			break;
		default:
			fprintf(stderr, "usage: recordmcount [-w] file.o...\n");
			return 0;
		}
	}

	if ((argc - optind) < 1) {
		fprintf(stderr, "usage: recordmcount [-w] file.o...\n");
		return 0;
	}

	 
	for (i = optind; i < argc; i++) {
		char *file = argv[i];
		int len;

		 
		len = strlen(file);
		if (len >= ftrace_size &&
		    strcmp(file + (len - ftrace_size), ftrace) == 0)
			continue;

		if (do_file(file)) {
			fprintf(stderr, "%s: failed\n", file);
			++n_error;
		}
	}
	return !!n_error;
}
