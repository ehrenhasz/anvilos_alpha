#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>
#include "list.h"
#include "elfconfig.h"
#undef ELF_ST_BIND
#undef ELF_ST_TYPE
#undef ELF_R_SYM
#undef ELF_R_TYPE
#if KERNEL_ELFCLASS == ELFCLASS32
#define Elf_Ehdr    Elf32_Ehdr
#define Elf_Shdr    Elf32_Shdr
#define Elf_Sym     Elf32_Sym
#define Elf_Addr    Elf32_Addr
#define Elf_Section Elf32_Half
#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE
#define Elf_Rel     Elf32_Rel
#define Elf_Rela    Elf32_Rela
#define ELF_R_SYM   ELF32_R_SYM
#define ELF_R_TYPE  ELF32_R_TYPE
#else
#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Shdr    Elf64_Shdr
#define Elf_Sym     Elf64_Sym
#define Elf_Addr    Elf64_Addr
#define Elf_Section Elf64_Half
#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE
#define Elf_Rel     Elf64_Rel
#define Elf_Rela    Elf64_Rela
#define ELF_R_SYM   ELF64_R_SYM
#define ELF_R_TYPE  ELF64_R_TYPE
#endif
#if KERNEL_ELFDATA != HOST_ELFDATA
static inline void __endian(const void *src, void *dest, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++)
		((unsigned char*)dest)[i] = ((unsigned char*)src)[size - i-1];
}
#define TO_NATIVE(x)						\
({								\
	typeof(x) __x;						\
	__endian(&(x), &(__x), sizeof(__x));			\
	__x;							\
})
#else  
#define TO_NATIVE(x) (x)
#endif
#define NOFAIL(ptr)   do_nofail((ptr), #ptr)
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
void *do_nofail(void *ptr, const char *expr);
struct buffer {
	char *p;
	int pos;
	int size;
};
void __attribute__((format(printf, 2, 3)))
buf_printf(struct buffer *buf, const char *fmt, ...);
void
buf_write(struct buffer *buf, const char *s, int len);
struct module {
	struct list_head list;
	struct list_head exported_symbols;
	struct list_head unresolved_symbols;
	bool is_gpl_compatible;
	bool from_dump;		 
	bool is_vmlinux;
	bool seen;
	bool has_init;
	bool has_cleanup;
	struct buffer dev_table_buf;
	char	     srcversion[25];
	struct list_head missing_namespaces;
	struct list_head imported_namespaces;
	char name[];
};
struct elf_info {
	size_t size;
	Elf_Ehdr     *hdr;
	Elf_Shdr     *sechdrs;
	Elf_Sym      *symtab_start;
	Elf_Sym      *symtab_stop;
	unsigned int export_symbol_secndx;	 
	char         *strtab;
	char	     *modinfo;
	unsigned int modinfo_len;
	unsigned int num_sections;  
	unsigned int secindex_strings;
	Elf32_Word   *symtab_shndx_start;
	Elf32_Word   *symtab_shndx_stop;
};
static inline unsigned int get_secindex(const struct elf_info *info,
					const Elf_Sym *sym)
{
	unsigned int index = sym->st_shndx;
	if (index == SHN_XINDEX)
		return info->symtab_shndx_start[sym - info->symtab_start];
	if (index >= SHN_LORESERVE && index <= SHN_HIRESERVE)
		return index - SHN_HIRESERVE - 1;
	return index;
}
void handle_moddevtable(struct module *mod, struct elf_info *info,
			Elf_Sym *sym, const char *symname);
void add_moddevtable(struct buffer *buf, struct module *mod);
void get_src_version(const char *modname, char sum[], unsigned sumlen);
char *read_text_file(const char *filename);
char *get_line(char **stringp);
void *sym_get_data(const struct elf_info *info, const Elf_Sym *sym);
enum loglevel {
	LOG_WARN,
	LOG_ERROR,
	LOG_FATAL
};
void modpost_log(enum loglevel loglevel, const char *fmt, ...);
#define warn(fmt, args...)	modpost_log(LOG_WARN, fmt, ##args)
#define error(fmt, args...)	modpost_log(LOG_ERROR, fmt, ##args)
#define fatal(fmt, args...)	modpost_log(LOG_FATAL, fmt, ##args)
