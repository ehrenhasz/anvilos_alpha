
 

#include <sys/types.h>
#include <stddef.h>
#include <libelf.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <err.h>
#ifdef HAVE_DWARF_SUPPORT
#include <dwarf.h>
#endif

#include "genelf.h"
#include "../util/jitdump.h"
#include <linux/compiler.h>

#ifndef NT_GNU_BUILD_ID
#define NT_GNU_BUILD_ID 3
#endif

#define BUILD_ID_URANDOM  

#ifdef HAVE_LIBCRYPTO_SUPPORT

#define BUILD_ID_MD5
#undef BUILD_ID_SHA	 
#undef BUILD_ID_URANDOM  

#ifdef BUILD_ID_SHA
#include <openssl/sha.h>
#endif

#ifdef BUILD_ID_MD5
#include <openssl/evp.h>
#include <openssl/md5.h>
#endif
#endif


typedef struct {
  unsigned int namesz;   
  unsigned int descsz;   
  unsigned int type;     
  char         name[0];  
} Elf_Note;

struct options {
	char *output;
	int fd;
};

static char shd_string_table[] = {
	0,
	'.', 't', 'e', 'x', 't', 0,			 
	'.', 's', 'h', 's', 't', 'r', 't', 'a', 'b', 0,  
	'.', 's', 'y', 'm', 't', 'a', 'b', 0,		 
	'.', 's', 't', 'r', 't', 'a', 'b', 0,		 
	'.', 'n', 'o', 't', 'e', '.', 'g', 'n', 'u', '.', 'b', 'u', 'i', 'l', 'd', '-', 'i', 'd', 0,  
	'.', 'd', 'e', 'b', 'u', 'g', '_', 'l', 'i', 'n', 'e', 0,  
	'.', 'd', 'e', 'b', 'u', 'g', '_', 'i', 'n', 'f', 'o', 0,  
	'.', 'd', 'e', 'b', 'u', 'g', '_', 'a', 'b', 'b', 'r', 'e', 'v', 0,  
	'.', 'e', 'h', '_', 'f', 'r', 'a', 'm', 'e', '_', 'h', 'd', 'r', 0,  
	'.', 'e', 'h', '_', 'f', 'r', 'a', 'm', 'e', 0,  
};

static struct buildid_note {
	Elf_Note desc;		 
	char	 name[4];	 
	char	 build_id[20];
} bnote;

static Elf_Sym symtab[]={
	 
	{ .st_name  = 0,  
	  .st_info  = ELF_ST_TYPE(STT_NOTYPE),
	  .st_shndx = 0,  
	  .st_value = 0x0,
	  .st_other = ELF_ST_VIS(STV_DEFAULT),
	  .st_size  = 0,
	},
	{ .st_name  = 1,  
	  .st_info  = ELF_ST_BIND(STB_LOCAL) | ELF_ST_TYPE(STT_FUNC),
	  .st_shndx = 1,
	  .st_value = 0,  
	  .st_other = ELF_ST_VIS(STV_DEFAULT),
	  .st_size  = 0,  
	}
};

#ifdef BUILD_ID_URANDOM
static void
gen_build_id(struct buildid_note *note,
	     unsigned long load_addr __maybe_unused,
	     const void *code __maybe_unused,
	     size_t csize __maybe_unused)
{
	int fd;
	size_t sz = sizeof(note->build_id);
	ssize_t sret;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		err(1, "cannot access /dev/urandom for buildid");

	sret = read(fd, note->build_id, sz);

	close(fd);

	if (sret != (ssize_t)sz)
		memset(note->build_id, 0, sz);
}
#endif

#ifdef BUILD_ID_SHA
static void
gen_build_id(struct buildid_note *note,
	     unsigned long load_addr __maybe_unused,
	     const void *code,
	     size_t csize)
{
	if (sizeof(note->build_id) < SHA_DIGEST_LENGTH)
		errx(1, "build_id too small for SHA1");

	SHA1(code, csize, (unsigned char *)note->build_id);
}
#endif

#ifdef BUILD_ID_MD5
static void
gen_build_id(struct buildid_note *note, unsigned long load_addr, const void *code, size_t csize)
{
	EVP_MD_CTX *mdctx;

	if (sizeof(note->build_id) < 16)
		errx(1, "build_id too small for MD5");

	mdctx = EVP_MD_CTX_new();
	if (!mdctx)
		errx(2, "failed to create EVP_MD_CTX");

	EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
	EVP_DigestUpdate(mdctx, &load_addr, sizeof(load_addr));
	EVP_DigestUpdate(mdctx, code, csize);
	EVP_DigestFinal_ex(mdctx, (unsigned char *)note->build_id, NULL);
	EVP_MD_CTX_free(mdctx);
}
#endif

static int
jit_add_eh_frame_info(Elf *e, void* unwinding, uint64_t unwinding_header_size,
		      uint64_t unwinding_size, uint64_t base_offset)
{
	Elf_Data *d;
	Elf_Scn *scn;
	Elf_Shdr *shdr;
	uint64_t unwinding_table_size = unwinding_size - unwinding_header_size;

	 
	scn = elf_newscn(e);
	if (!scn) {
		warnx("cannot create section");
		return -1;
	}

	d = elf_newdata(scn);
	if (!d) {
		warnx("cannot get new data");
		return -1;
	}

	d->d_align = 8;
	d->d_off = 0LL;
	d->d_buf = unwinding;
	d->d_type = ELF_T_BYTE;
	d->d_size = unwinding_table_size;
	d->d_version = EV_CURRENT;

	shdr = elf_getshdr(scn);
	if (!shdr) {
		warnx("cannot get section header");
		return -1;
	}

	shdr->sh_name = 104;
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_addr = base_offset;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_entsize = 0;

	 
	scn = elf_newscn(e);
	if (!scn) {
		warnx("cannot create section");
		return -1;
	}

	d = elf_newdata(scn);
	if (!d) {
		warnx("cannot get new data");
		return -1;
	}

	d->d_align = 4;
	d->d_off = 0LL;
	d->d_buf = unwinding + unwinding_table_size;
	d->d_type = ELF_T_BYTE;
	d->d_size = unwinding_header_size;
	d->d_version = EV_CURRENT;

	shdr = elf_getshdr(scn);
	if (!shdr) {
		warnx("cannot get section header");
		return -1;
	}

	shdr->sh_name = 90;
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_addr = base_offset + unwinding_table_size;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_entsize = 0;

	return 0;
}

 
int
jit_write_elf(int fd, uint64_t load_addr, const char *sym,
	      const void *code, int csize,
	      void *debug __maybe_unused, int nr_debug_entries __maybe_unused,
	      void *unwinding, uint64_t unwinding_header_size, uint64_t unwinding_size)
{
	Elf *e;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Shdr *shdr;
	uint64_t eh_frame_base_offset;
	char *strsym = NULL;
	int symlen;
	int retval = -1;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		warnx("ELF initialization failed");
		return -1;
	}

	e = elf_begin(fd, ELF_C_WRITE, NULL);
	if (!e) {
		warnx("elf_begin failed");
		goto error;
	}

	 
	ehdr = elf_newehdr(e);
	if (!ehdr) {
		warnx("cannot get ehdr");
		goto error;
	}

	ehdr->e_ident[EI_DATA] = GEN_ELF_ENDIAN;
	ehdr->e_ident[EI_CLASS] = GEN_ELF_CLASS;
	ehdr->e_machine = GEN_ELF_ARCH;
	ehdr->e_type = ET_DYN;
	ehdr->e_entry = GEN_ELF_TEXT_OFFSET;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_shstrndx= unwinding ? 4 : 2;  

	 
	phdr = elf_newphdr(e, 1);
	phdr[0].p_type = PT_LOAD;
	phdr[0].p_offset = GEN_ELF_TEXT_OFFSET;
	phdr[0].p_vaddr = GEN_ELF_TEXT_OFFSET;
	phdr[0].p_paddr = GEN_ELF_TEXT_OFFSET;
	phdr[0].p_filesz = csize;
	phdr[0].p_memsz = csize;
	phdr[0].p_flags = PF_X | PF_R;
	phdr[0].p_align = 8;

	 
	scn = elf_newscn(e);
	if (!scn) {
		warnx("cannot create section");
		goto error;
	}

	d = elf_newdata(scn);
	if (!d) {
		warnx("cannot get new data");
		goto error;
	}

	d->d_align = 16;
	d->d_off = 0LL;
	d->d_buf = (void *)code;
	d->d_type = ELF_T_BYTE;
	d->d_size = csize;
	d->d_version = EV_CURRENT;

	shdr = elf_getshdr(scn);
	if (!shdr) {
		warnx("cannot get section header");
		goto error;
	}

	shdr->sh_name = 1;
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_addr = GEN_ELF_TEXT_OFFSET;
	shdr->sh_flags = SHF_EXECINSTR | SHF_ALLOC;
	shdr->sh_entsize = 0;

	 
	if (unwinding) {
		eh_frame_base_offset = ALIGN_8(GEN_ELF_TEXT_OFFSET + csize);
		retval = jit_add_eh_frame_info(e, unwinding,
					       unwinding_header_size, unwinding_size,
					       eh_frame_base_offset);
		if (retval)
			goto error;
		retval = -1;
	}

	 
	scn = elf_newscn(e);
	if (!scn) {
		warnx("cannot create section");
		goto error;
	}

	d = elf_newdata(scn);
	if (!d) {
		warnx("cannot get new data");
		goto error;
	}

	d->d_align = 1;
	d->d_off = 0LL;
	d->d_buf = shd_string_table;
	d->d_type = ELF_T_BYTE;
	d->d_size = sizeof(shd_string_table);
	d->d_version = EV_CURRENT;

	shdr = elf_getshdr(scn);
	if (!shdr) {
		warnx("cannot get section header");
		goto error;
	}

	shdr->sh_name = 7;  
	shdr->sh_type = SHT_STRTAB;
	shdr->sh_flags = 0;
	shdr->sh_entsize = 0;

	 
	symtab[1].st_size  = csize;
	symtab[1].st_value = GEN_ELF_TEXT_OFFSET;

	scn = elf_newscn(e);
	if (!scn) {
		warnx("cannot create section");
		goto error;
	}

	d = elf_newdata(scn);
	if (!d) {
		warnx("cannot get new data");
		goto error;
	}

	d->d_align = 8;
	d->d_off = 0LL;
	d->d_buf = symtab;
	d->d_type = ELF_T_SYM;
	d->d_size = sizeof(symtab);
	d->d_version = EV_CURRENT;

	shdr = elf_getshdr(scn);
	if (!shdr) {
		warnx("cannot get section header");
		goto error;
	}

	shdr->sh_name = 17;  
	shdr->sh_type = SHT_SYMTAB;
	shdr->sh_flags = 0;
	shdr->sh_entsize = sizeof(Elf_Sym);
	shdr->sh_link = unwinding ? 6 : 4;  

	 
	symlen = 2 + strlen(sym);
	strsym = calloc(1, symlen);
	if (!strsym) {
		warnx("cannot allocate strsym");
		goto error;
	}
	strcpy(strsym + 1, sym);

	scn = elf_newscn(e);
	if (!scn) {
		warnx("cannot create section");
		goto error;
	}

	d = elf_newdata(scn);
	if (!d) {
		warnx("cannot get new data");
		goto error;
	}

	d->d_align = 1;
	d->d_off = 0LL;
	d->d_buf = strsym;
	d->d_type = ELF_T_BYTE;
	d->d_size = symlen;
	d->d_version = EV_CURRENT;

	shdr = elf_getshdr(scn);
	if (!shdr) {
		warnx("cannot get section header");
		goto error;
	}

	shdr->sh_name = 25;  
	shdr->sh_type = SHT_STRTAB;
	shdr->sh_flags = 0;
	shdr->sh_entsize = 0;

	 
	scn = elf_newscn(e);
	if (!scn) {
		warnx("cannot create section");
		goto error;
	}

	d = elf_newdata(scn);
	if (!d) {
		warnx("cannot get new data");
		goto error;
	}

	 
	gen_build_id(&bnote, load_addr, code, csize);
	bnote.desc.namesz = sizeof(bnote.name);  
	bnote.desc.descsz = sizeof(bnote.build_id);
	bnote.desc.type   = NT_GNU_BUILD_ID;
	strcpy(bnote.name, "GNU");

	d->d_align = 4;
	d->d_off = 0LL;
	d->d_buf = &bnote;
	d->d_type = ELF_T_BYTE;
	d->d_size = sizeof(bnote);
	d->d_version = EV_CURRENT;

	shdr = elf_getshdr(scn);
	if (!shdr) {
		warnx("cannot get section header");
		goto error;
	}

	shdr->sh_name = 33;  
	shdr->sh_type = SHT_NOTE;
	shdr->sh_addr = 0x0;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_size = sizeof(bnote);
	shdr->sh_entsize = 0;

#ifdef HAVE_DWARF_SUPPORT
	if (debug && nr_debug_entries) {
		retval = jit_add_debug_info(e, load_addr, debug, nr_debug_entries);
		if (retval)
			goto error;
	} else
#endif
	{
		if (elf_update(e, ELF_C_WRITE) < 0) {
			warnx("elf_update 4 failed");
			goto error;
		}
	}

	retval = 0;
error:
	(void)elf_end(e);

	free(strsym);


	return retval;
}
