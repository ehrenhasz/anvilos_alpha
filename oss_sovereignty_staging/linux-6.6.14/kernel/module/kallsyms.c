
 

#include <linux/module.h>
#include <linux/module_symbol.h>
#include <linux/kallsyms.h>
#include <linux/buildid.h>
#include <linux/bsearch.h>
#include "internal.h"

 
static const struct kernel_symbol *lookup_exported_symbol(const char *name,
							  const struct kernel_symbol *start,
							  const struct kernel_symbol *stop)
{
	return bsearch(name, start, stop - start,
			sizeof(struct kernel_symbol), cmp_name);
}

static int is_exported(const char *name, unsigned long value,
		       const struct module *mod)
{
	const struct kernel_symbol *ks;

	if (!mod)
		ks = lookup_exported_symbol(name, __start___ksymtab, __stop___ksymtab);
	else
		ks = lookup_exported_symbol(name, mod->syms, mod->syms + mod->num_syms);

	return ks && kernel_symbol_value(ks) == value;
}

 
static char elf_type(const Elf_Sym *sym, const struct load_info *info)
{
	const Elf_Shdr *sechdrs = info->sechdrs;

	if (ELF_ST_BIND(sym->st_info) == STB_WEAK) {
		if (ELF_ST_TYPE(sym->st_info) == STT_OBJECT)
			return 'v';
		else
			return 'w';
	}
	if (sym->st_shndx == SHN_UNDEF)
		return 'U';
	if (sym->st_shndx == SHN_ABS || sym->st_shndx == info->index.pcpu)
		return 'a';
	if (sym->st_shndx >= SHN_LORESERVE)
		return '?';
	if (sechdrs[sym->st_shndx].sh_flags & SHF_EXECINSTR)
		return 't';
	if (sechdrs[sym->st_shndx].sh_flags & SHF_ALLOC &&
	    sechdrs[sym->st_shndx].sh_type != SHT_NOBITS) {
		if (!(sechdrs[sym->st_shndx].sh_flags & SHF_WRITE))
			return 'r';
		else if (sechdrs[sym->st_shndx].sh_flags & ARCH_SHF_SMALL)
			return 'g';
		else
			return 'd';
	}
	if (sechdrs[sym->st_shndx].sh_type == SHT_NOBITS) {
		if (sechdrs[sym->st_shndx].sh_flags & ARCH_SHF_SMALL)
			return 's';
		else
			return 'b';
	}
	if (strstarts(info->secstrings + sechdrs[sym->st_shndx].sh_name,
		      ".debug")) {
		return 'n';
	}
	return '?';
}

static bool is_core_symbol(const Elf_Sym *src, const Elf_Shdr *sechdrs,
			   unsigned int shnum, unsigned int pcpundx)
{
	const Elf_Shdr *sec;
	enum mod_mem_type type;

	if (src->st_shndx == SHN_UNDEF ||
	    src->st_shndx >= shnum ||
	    !src->st_name)
		return false;

#ifdef CONFIG_KALLSYMS_ALL
	if (src->st_shndx == pcpundx)
		return true;
#endif

	sec = sechdrs + src->st_shndx;
	type = sec->sh_entsize >> SH_ENTSIZE_TYPE_SHIFT;
	if (!(sec->sh_flags & SHF_ALLOC)
#ifndef CONFIG_KALLSYMS_ALL
	    || !(sec->sh_flags & SHF_EXECINSTR)
#endif
	    || mod_mem_type_is_init(type))
		return false;

	return true;
}

 
void layout_symtab(struct module *mod, struct load_info *info)
{
	Elf_Shdr *symsect = info->sechdrs + info->index.sym;
	Elf_Shdr *strsect = info->sechdrs + info->index.str;
	const Elf_Sym *src;
	unsigned int i, nsrc, ndst, strtab_size = 0;
	struct module_memory *mod_mem_data = &mod->mem[MOD_DATA];
	struct module_memory *mod_mem_init_data = &mod->mem[MOD_INIT_DATA];

	 
	symsect->sh_flags |= SHF_ALLOC;
	symsect->sh_entsize = module_get_offset_and_type(mod, MOD_INIT_DATA,
							 symsect, info->index.sym);
	pr_debug("\t%s\n", info->secstrings + symsect->sh_name);

	src = (void *)info->hdr + symsect->sh_offset;
	nsrc = symsect->sh_size / sizeof(*src);

	 
	for (ndst = i = 0; i < nsrc; i++) {
		if (i == 0 || is_livepatch_module(mod) ||
		    is_core_symbol(src + i, info->sechdrs, info->hdr->e_shnum,
				   info->index.pcpu)) {
			strtab_size += strlen(&info->strtab[src[i].st_name]) + 1;
			ndst++;
		}
	}

	 
	info->symoffs = ALIGN(mod_mem_data->size, symsect->sh_addralign ?: 1);
	info->stroffs = mod_mem_data->size = info->symoffs + ndst * sizeof(Elf_Sym);
	mod_mem_data->size += strtab_size;
	 
	info->core_typeoffs = mod_mem_data->size;
	mod_mem_data->size += ndst * sizeof(char);

	 
	strsect->sh_flags |= SHF_ALLOC;
	strsect->sh_entsize = module_get_offset_and_type(mod, MOD_INIT_DATA,
							 strsect, info->index.str);
	pr_debug("\t%s\n", info->secstrings + strsect->sh_name);

	 
	mod_mem_init_data->size = ALIGN(mod_mem_init_data->size,
					__alignof__(struct mod_kallsyms));
	info->mod_kallsyms_init_off = mod_mem_init_data->size;

	mod_mem_init_data->size += sizeof(struct mod_kallsyms);
	info->init_typeoffs = mod_mem_init_data->size;
	mod_mem_init_data->size += nsrc * sizeof(char);
}

 
void add_kallsyms(struct module *mod, const struct load_info *info)
{
	unsigned int i, ndst;
	const Elf_Sym *src;
	Elf_Sym *dst;
	char *s;
	Elf_Shdr *symsec = &info->sechdrs[info->index.sym];
	unsigned long strtab_size;
	void *data_base = mod->mem[MOD_DATA].base;
	void *init_data_base = mod->mem[MOD_INIT_DATA].base;

	 
	mod->kallsyms = (void __rcu *)init_data_base +
		info->mod_kallsyms_init_off;

	rcu_read_lock();
	 
	rcu_dereference(mod->kallsyms)->symtab = (void *)symsec->sh_addr;
	rcu_dereference(mod->kallsyms)->num_symtab = symsec->sh_size / sizeof(Elf_Sym);
	 
	rcu_dereference(mod->kallsyms)->strtab =
		(void *)info->sechdrs[info->index.str].sh_addr;
	rcu_dereference(mod->kallsyms)->typetab = init_data_base + info->init_typeoffs;

	 
	mod->core_kallsyms.symtab = dst = data_base + info->symoffs;
	mod->core_kallsyms.strtab = s = data_base + info->stroffs;
	mod->core_kallsyms.typetab = data_base + info->core_typeoffs;
	strtab_size = info->core_typeoffs - info->stroffs;
	src = rcu_dereference(mod->kallsyms)->symtab;
	for (ndst = i = 0; i < rcu_dereference(mod->kallsyms)->num_symtab; i++) {
		rcu_dereference(mod->kallsyms)->typetab[i] = elf_type(src + i, info);
		if (i == 0 || is_livepatch_module(mod) ||
		    is_core_symbol(src + i, info->sechdrs, info->hdr->e_shnum,
				   info->index.pcpu)) {
			ssize_t ret;

			mod->core_kallsyms.typetab[ndst] =
			    rcu_dereference(mod->kallsyms)->typetab[i];
			dst[ndst] = src[i];
			dst[ndst++].st_name = s - mod->core_kallsyms.strtab;
			ret = strscpy(s,
				      &rcu_dereference(mod->kallsyms)->strtab[src[i].st_name],
				      strtab_size);
			if (ret < 0)
				break;
			s += ret + 1;
			strtab_size -= ret + 1;
		}
	}
	rcu_read_unlock();
	mod->core_kallsyms.num_symtab = ndst;
}

#if IS_ENABLED(CONFIG_STACKTRACE_BUILD_ID)
void init_build_id(struct module *mod, const struct load_info *info)
{
	const Elf_Shdr *sechdr;
	unsigned int i;

	for (i = 0; i < info->hdr->e_shnum; i++) {
		sechdr = &info->sechdrs[i];
		if (!sect_empty(sechdr) && sechdr->sh_type == SHT_NOTE &&
		    !build_id_parse_buf((void *)sechdr->sh_addr, mod->build_id,
					sechdr->sh_size))
			break;
	}
}
#else
void init_build_id(struct module *mod, const struct load_info *info)
{
}
#endif

static const char *kallsyms_symbol_name(struct mod_kallsyms *kallsyms, unsigned int symnum)
{
	return kallsyms->strtab + kallsyms->symtab[symnum].st_name;
}

 
static const char *find_kallsyms_symbol(struct module *mod,
					unsigned long addr,
					unsigned long *size,
					unsigned long *offset)
{
	unsigned int i, best = 0;
	unsigned long nextval, bestval;
	struct mod_kallsyms *kallsyms = rcu_dereference_sched(mod->kallsyms);
	struct module_memory *mod_mem;

	 
	if (within_module_init(addr, mod))
		mod_mem = &mod->mem[MOD_INIT_TEXT];
	else
		mod_mem = &mod->mem[MOD_TEXT];

	nextval = (unsigned long)mod_mem->base + mod_mem->size;

	bestval = kallsyms_symbol_value(&kallsyms->symtab[best]);

	 
	for (i = 1; i < kallsyms->num_symtab; i++) {
		const Elf_Sym *sym = &kallsyms->symtab[i];
		unsigned long thisval = kallsyms_symbol_value(sym);

		if (sym->st_shndx == SHN_UNDEF)
			continue;

		 
		if (*kallsyms_symbol_name(kallsyms, i) == '\0' ||
		    is_mapping_symbol(kallsyms_symbol_name(kallsyms, i)))
			continue;

		if (thisval <= addr && thisval > bestval) {
			best = i;
			bestval = thisval;
		}
		if (thisval > addr && thisval < nextval)
			nextval = thisval;
	}

	if (!best)
		return NULL;

	if (size)
		*size = nextval - bestval;
	if (offset)
		*offset = addr - bestval;

	return kallsyms_symbol_name(kallsyms, best);
}

void * __weak dereference_module_function_descriptor(struct module *mod,
						     void *ptr)
{
	return ptr;
}

 
const char *module_address_lookup(unsigned long addr,
				  unsigned long *size,
			    unsigned long *offset,
			    char **modname,
			    const unsigned char **modbuildid,
			    char *namebuf)
{
	const char *ret = NULL;
	struct module *mod;

	preempt_disable();
	mod = __module_address(addr);
	if (mod) {
		if (modname)
			*modname = mod->name;
		if (modbuildid) {
#if IS_ENABLED(CONFIG_STACKTRACE_BUILD_ID)
			*modbuildid = mod->build_id;
#else
			*modbuildid = NULL;
#endif
		}

		ret = find_kallsyms_symbol(mod, addr, size, offset);
	}
	 
	if (ret) {
		strncpy(namebuf, ret, KSYM_NAME_LEN - 1);
		ret = namebuf;
	}
	preempt_enable();

	return ret;
}

int lookup_module_symbol_name(unsigned long addr, char *symname)
{
	struct module *mod;

	preempt_disable();
	list_for_each_entry_rcu(mod, &modules, list) {
		if (mod->state == MODULE_STATE_UNFORMED)
			continue;
		if (within_module(addr, mod)) {
			const char *sym;

			sym = find_kallsyms_symbol(mod, addr, NULL, NULL);
			if (!sym)
				goto out;

			strscpy(symname, sym, KSYM_NAME_LEN);
			preempt_enable();
			return 0;
		}
	}
out:
	preempt_enable();
	return -ERANGE;
}

int module_get_kallsym(unsigned int symnum, unsigned long *value, char *type,
		       char *name, char *module_name, int *exported)
{
	struct module *mod;

	preempt_disable();
	list_for_each_entry_rcu(mod, &modules, list) {
		struct mod_kallsyms *kallsyms;

		if (mod->state == MODULE_STATE_UNFORMED)
			continue;
		kallsyms = rcu_dereference_sched(mod->kallsyms);
		if (symnum < kallsyms->num_symtab) {
			const Elf_Sym *sym = &kallsyms->symtab[symnum];

			*value = kallsyms_symbol_value(sym);
			*type = kallsyms->typetab[symnum];
			strscpy(name, kallsyms_symbol_name(kallsyms, symnum), KSYM_NAME_LEN);
			strscpy(module_name, mod->name, MODULE_NAME_LEN);
			*exported = is_exported(name, *value, mod);
			preempt_enable();
			return 0;
		}
		symnum -= kallsyms->num_symtab;
	}
	preempt_enable();
	return -ERANGE;
}

 
static unsigned long __find_kallsyms_symbol_value(struct module *mod, const char *name)
{
	unsigned int i;
	struct mod_kallsyms *kallsyms = rcu_dereference_sched(mod->kallsyms);

	for (i = 0; i < kallsyms->num_symtab; i++) {
		const Elf_Sym *sym = &kallsyms->symtab[i];

		if (strcmp(name, kallsyms_symbol_name(kallsyms, i)) == 0 &&
		    sym->st_shndx != SHN_UNDEF)
			return kallsyms_symbol_value(sym);
	}
	return 0;
}

static unsigned long __module_kallsyms_lookup_name(const char *name)
{
	struct module *mod;
	char *colon;

	colon = strnchr(name, MODULE_NAME_LEN, ':');
	if (colon) {
		mod = find_module_all(name, colon - name, false);
		if (mod)
			return __find_kallsyms_symbol_value(mod, colon + 1);
		return 0;
	}

	list_for_each_entry_rcu(mod, &modules, list) {
		unsigned long ret;

		if (mod->state == MODULE_STATE_UNFORMED)
			continue;
		ret = __find_kallsyms_symbol_value(mod, name);
		if (ret)
			return ret;
	}
	return 0;
}

 
unsigned long module_kallsyms_lookup_name(const char *name)
{
	unsigned long ret;

	 
	preempt_disable();
	ret = __module_kallsyms_lookup_name(name);
	preempt_enable();
	return ret;
}

unsigned long find_kallsyms_symbol_value(struct module *mod, const char *name)
{
	unsigned long ret;

	preempt_disable();
	ret = __find_kallsyms_symbol_value(mod, name);
	preempt_enable();
	return ret;
}

int module_kallsyms_on_each_symbol(const char *modname,
				   int (*fn)(void *, const char *, unsigned long),
				   void *data)
{
	struct module *mod;
	unsigned int i;
	int ret = 0;

	mutex_lock(&module_mutex);
	list_for_each_entry(mod, &modules, list) {
		struct mod_kallsyms *kallsyms;

		if (mod->state == MODULE_STATE_UNFORMED)
			continue;

		if (modname && strcmp(modname, mod->name))
			continue;

		 
		preempt_disable();
		kallsyms = rcu_dereference_sched(mod->kallsyms);
		preempt_enable();

		for (i = 0; i < kallsyms->num_symtab; i++) {
			const Elf_Sym *sym = &kallsyms->symtab[i];

			if (sym->st_shndx == SHN_UNDEF)
				continue;

			ret = fn(data, kallsyms_symbol_name(kallsyms, i),
				 kallsyms_symbol_value(sym));
			if (ret != 0)
				goto out;
		}

		 
		if (modname)
			break;
	}
out:
	mutex_unlock(&module_mutex);
	return ret;
}
