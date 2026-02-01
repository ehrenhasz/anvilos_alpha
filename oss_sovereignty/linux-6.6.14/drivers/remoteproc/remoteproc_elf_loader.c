
 

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/remoteproc.h>
#include <linux/elf.h>

#include "remoteproc_internal.h"
#include "remoteproc_elf_helpers.h"

 
int rproc_elf_sanity_check(struct rproc *rproc, const struct firmware *fw)
{
	const char *name = rproc->firmware;
	struct device *dev = &rproc->dev;
	 
	struct elf32_hdr *ehdr;
	u32 elf_shdr_get_size;
	u64 phoff, shoff;
	char class;
	u16 phnum;

	if (!fw) {
		dev_err(dev, "failed to load %s\n", name);
		return -EINVAL;
	}

	if (fw->size < sizeof(struct elf32_hdr)) {
		dev_err(dev, "Image is too small\n");
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *)fw->data;

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(dev, "Image is corrupted (bad magic)\n");
		return -EINVAL;
	}

	class = ehdr->e_ident[EI_CLASS];
	if (class != ELFCLASS32 && class != ELFCLASS64) {
		dev_err(dev, "Unsupported class: %d\n", class);
		return -EINVAL;
	}

	if (class == ELFCLASS64 && fw->size < sizeof(struct elf64_hdr)) {
		dev_err(dev, "elf64 header is too small\n");
		return -EINVAL;
	}

	 
# ifdef __LITTLE_ENDIAN
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
# else  
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
# endif
		dev_err(dev, "Unsupported firmware endianness\n");
		return -EINVAL;
	}

	phoff = elf_hdr_get_e_phoff(class, fw->data);
	shoff = elf_hdr_get_e_shoff(class, fw->data);
	phnum =  elf_hdr_get_e_phnum(class, fw->data);
	elf_shdr_get_size = elf_size_of_shdr(class);

	if (fw->size < shoff + elf_shdr_get_size) {
		dev_err(dev, "Image is too small\n");
		return -EINVAL;
	}

	if (phnum == 0) {
		dev_err(dev, "No loadable segments\n");
		return -EINVAL;
	}

	if (phoff > fw->size) {
		dev_err(dev, "Firmware size is too small\n");
		return -EINVAL;
	}

	dev_dbg(dev, "Firmware is an elf%d file\n",
		class == ELFCLASS32 ? 32 : 64);

	return 0;
}
EXPORT_SYMBOL(rproc_elf_sanity_check);

 
u64 rproc_elf_get_boot_addr(struct rproc *rproc, const struct firmware *fw)
{
	return elf_hdr_get_e_entry(fw_elf_get_class(fw), fw->data);
}
EXPORT_SYMBOL(rproc_elf_get_boot_addr);

 
int rproc_elf_load_segments(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	const void *ehdr, *phdr;
	int i, ret = 0;
	u16 phnum;
	const u8 *elf_data = fw->data;
	u8 class = fw_elf_get_class(fw);
	u32 elf_phdr_get_size = elf_size_of_phdr(class);

	ehdr = elf_data;
	phnum = elf_hdr_get_e_phnum(class, ehdr);
	phdr = elf_data + elf_hdr_get_e_phoff(class, ehdr);

	 
	for (i = 0; i < phnum; i++, phdr += elf_phdr_get_size) {
		u64 da = elf_phdr_get_p_paddr(class, phdr);
		u64 memsz = elf_phdr_get_p_memsz(class, phdr);
		u64 filesz = elf_phdr_get_p_filesz(class, phdr);
		u64 offset = elf_phdr_get_p_offset(class, phdr);
		u32 type = elf_phdr_get_p_type(class, phdr);
		bool is_iomem = false;
		void *ptr;

		if (type != PT_LOAD || !memsz)
			continue;

		dev_dbg(dev, "phdr: type %d da 0x%llx memsz 0x%llx filesz 0x%llx\n",
			type, da, memsz, filesz);

		if (filesz > memsz) {
			dev_err(dev, "bad phdr filesz 0x%llx memsz 0x%llx\n",
				filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > fw->size) {
			dev_err(dev, "truncated fw: need 0x%llx avail 0x%zx\n",
				offset + filesz, fw->size);
			ret = -EINVAL;
			break;
		}

		if (!rproc_u64_fit_in_size_t(memsz)) {
			dev_err(dev, "size (%llx) does not fit in size_t type\n",
				memsz);
			ret = -EOVERFLOW;
			break;
		}

		 
		ptr = rproc_da_to_va(rproc, da, memsz, &is_iomem);
		if (!ptr) {
			dev_err(dev, "bad phdr da 0x%llx mem 0x%llx\n", da,
				memsz);
			ret = -EINVAL;
			break;
		}

		 
		if (filesz) {
			if (is_iomem)
				memcpy_toio((void __iomem *)ptr, elf_data + offset, filesz);
			else
				memcpy(ptr, elf_data + offset, filesz);
		}

		 
		if (memsz > filesz) {
			if (is_iomem)
				memset_io((void __iomem *)(ptr + filesz), 0, memsz - filesz);
			else
				memset(ptr + filesz, 0, memsz - filesz);
		}
	}

	return ret;
}
EXPORT_SYMBOL(rproc_elf_load_segments);

static const void *
find_table(struct device *dev, const struct firmware *fw)
{
	const void *shdr, *name_table_shdr;
	int i;
	const char *name_table;
	struct resource_table *table = NULL;
	const u8 *elf_data = (void *)fw->data;
	u8 class = fw_elf_get_class(fw);
	size_t fw_size = fw->size;
	const void *ehdr = elf_data;
	u16 shnum = elf_hdr_get_e_shnum(class, ehdr);
	u32 elf_shdr_get_size = elf_size_of_shdr(class);
	u16 shstrndx = elf_hdr_get_e_shstrndx(class, ehdr);

	 
	 
	shdr = elf_data + elf_hdr_get_e_shoff(class, ehdr);
	 
	name_table_shdr = shdr + (shstrndx * elf_shdr_get_size);
	 
	name_table = elf_data + elf_shdr_get_sh_offset(class, name_table_shdr);

	for (i = 0; i < shnum; i++, shdr += elf_shdr_get_size) {
		u64 size = elf_shdr_get_sh_size(class, shdr);
		u64 offset = elf_shdr_get_sh_offset(class, shdr);
		u32 name = elf_shdr_get_sh_name(class, shdr);

		if (strcmp(name_table + name, ".resource_table"))
			continue;

		table = (struct resource_table *)(elf_data + offset);

		 
		if (offset + size > fw_size || offset + size < size) {
			dev_err(dev, "resource table truncated\n");
			return NULL;
		}

		 
		if (sizeof(struct resource_table) > size) {
			dev_err(dev, "header-less resource table\n");
			return NULL;
		}

		 
		if (table->ver != 1) {
			dev_err(dev, "unsupported fw ver: %d\n", table->ver);
			return NULL;
		}

		 
		if (table->reserved[0] || table->reserved[1]) {
			dev_err(dev, "non zero reserved bytes\n");
			return NULL;
		}

		 
		if (struct_size(table, offset, table->num) > size) {
			dev_err(dev, "resource table incomplete\n");
			return NULL;
		}

		return shdr;
	}

	return NULL;
}

 
int rproc_elf_load_rsc_table(struct rproc *rproc, const struct firmware *fw)
{
	const void *shdr;
	struct device *dev = &rproc->dev;
	struct resource_table *table = NULL;
	const u8 *elf_data = fw->data;
	size_t tablesz;
	u8 class = fw_elf_get_class(fw);
	u64 sh_offset;

	shdr = find_table(dev, fw);
	if (!shdr)
		return -EINVAL;

	sh_offset = elf_shdr_get_sh_offset(class, shdr);
	table = (struct resource_table *)(elf_data + sh_offset);
	tablesz = elf_shdr_get_sh_size(class, shdr);

	 
	rproc->cached_table = kmemdup(table, tablesz, GFP_KERNEL);
	if (!rproc->cached_table)
		return -ENOMEM;

	rproc->table_ptr = rproc->cached_table;
	rproc->table_sz = tablesz;

	return 0;
}
EXPORT_SYMBOL(rproc_elf_load_rsc_table);

 
struct resource_table *rproc_elf_find_loaded_rsc_table(struct rproc *rproc,
						       const struct firmware *fw)
{
	const void *shdr;
	u64 sh_addr, sh_size;
	u8 class = fw_elf_get_class(fw);
	struct device *dev = &rproc->dev;

	shdr = find_table(&rproc->dev, fw);
	if (!shdr)
		return NULL;

	sh_addr = elf_shdr_get_sh_addr(class, shdr);
	sh_size = elf_shdr_get_sh_size(class, shdr);

	if (!rproc_u64_fit_in_size_t(sh_size)) {
		dev_err(dev, "size (%llx) does not fit in size_t type\n",
			sh_size);
		return NULL;
	}

	return rproc_da_to_va(rproc, sh_addr, sh_size, NULL);
}
EXPORT_SYMBOL(rproc_elf_find_loaded_rsc_table);
