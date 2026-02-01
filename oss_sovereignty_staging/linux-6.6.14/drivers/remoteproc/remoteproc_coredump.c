
 

#include <linux/completion.h>
#include <linux/devcoredump.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/remoteproc.h>
#include "remoteproc_internal.h"
#include "remoteproc_elf_helpers.h"

struct rproc_coredump_state {
	struct rproc *rproc;
	void *header;
	struct completion dump_done;
};

 
void rproc_coredump_cleanup(struct rproc *rproc)
{
	struct rproc_dump_segment *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, &rproc->dump_segments, node) {
		list_del(&entry->node);
		kfree(entry);
	}
}
EXPORT_SYMBOL_GPL(rproc_coredump_cleanup);

 
int rproc_coredump_add_segment(struct rproc *rproc, dma_addr_t da, size_t size)
{
	struct rproc_dump_segment *segment;

	segment = kzalloc(sizeof(*segment), GFP_KERNEL);
	if (!segment)
		return -ENOMEM;

	segment->da = da;
	segment->size = size;

	list_add_tail(&segment->node, &rproc->dump_segments);

	return 0;
}
EXPORT_SYMBOL(rproc_coredump_add_segment);

 
int rproc_coredump_add_custom_segment(struct rproc *rproc,
				      dma_addr_t da, size_t size,
				      void (*dumpfn)(struct rproc *rproc,
						     struct rproc_dump_segment *segment,
						     void *dest, size_t offset,
						     size_t size),
				      void *priv)
{
	struct rproc_dump_segment *segment;

	segment = kzalloc(sizeof(*segment), GFP_KERNEL);
	if (!segment)
		return -ENOMEM;

	segment->da = da;
	segment->size = size;
	segment->priv = priv;
	segment->dump = dumpfn;

	list_add_tail(&segment->node, &rproc->dump_segments);

	return 0;
}
EXPORT_SYMBOL(rproc_coredump_add_custom_segment);

 
int rproc_coredump_set_elf_info(struct rproc *rproc, u8 class, u16 machine)
{
	if (class != ELFCLASS64 && class != ELFCLASS32)
		return -EINVAL;

	rproc->elf_class = class;
	rproc->elf_machine = machine;

	return 0;
}
EXPORT_SYMBOL(rproc_coredump_set_elf_info);

static void rproc_coredump_free(void *data)
{
	struct rproc_coredump_state *dump_state = data;

	vfree(dump_state->header);
	complete(&dump_state->dump_done);
}

static void *rproc_coredump_find_segment(loff_t user_offset,
					 struct list_head *segments,
					 size_t *data_left)
{
	struct rproc_dump_segment *segment;

	list_for_each_entry(segment, segments, node) {
		if (user_offset < segment->size) {
			*data_left = segment->size - user_offset;
			return segment;
		}
		user_offset -= segment->size;
	}

	*data_left = 0;
	return NULL;
}

static void rproc_copy_segment(struct rproc *rproc, void *dest,
			       struct rproc_dump_segment *segment,
			       size_t offset, size_t size)
{
	bool is_iomem = false;
	void *ptr;

	if (segment->dump) {
		segment->dump(rproc, segment, dest, offset, size);
	} else {
		ptr = rproc_da_to_va(rproc, segment->da + offset, size, &is_iomem);
		if (!ptr) {
			dev_err(&rproc->dev,
				"invalid copy request for segment %pad with offset %zu and size %zu)\n",
				&segment->da, offset, size);
			memset(dest, 0xff, size);
		} else {
			if (is_iomem)
				memcpy_fromio(dest, (void const __iomem *)ptr, size);
			else
				memcpy(dest, ptr, size);
		}
	}
}

static ssize_t rproc_coredump_read(char *buffer, loff_t offset, size_t count,
				   void *data, size_t header_sz)
{
	size_t seg_data, bytes_left = count;
	ssize_t copy_sz;
	struct rproc_dump_segment *seg;
	struct rproc_coredump_state *dump_state = data;
	struct rproc *rproc = dump_state->rproc;
	void *elfcore = dump_state->header;

	 
	if (offset < header_sz) {
		copy_sz = memory_read_from_buffer(buffer, count, &offset,
						  elfcore, header_sz);

		return copy_sz;
	}

	 
	while (bytes_left) {
		seg = rproc_coredump_find_segment(offset - header_sz,
						  &rproc->dump_segments,
						  &seg_data);
		 
		if (!seg) {
			dev_info(&rproc->dev, "Ramdump done, %lld bytes read",
				 offset);
			break;
		}

		copy_sz = min_t(size_t, bytes_left, seg_data);

		rproc_copy_segment(rproc, buffer, seg, seg->size - seg_data,
				   copy_sz);

		offset += copy_sz;
		buffer += copy_sz;
		bytes_left -= copy_sz;
	}

	return count - bytes_left;
}

 
void rproc_coredump(struct rproc *rproc)
{
	struct rproc_dump_segment *segment;
	void *phdr;
	void *ehdr;
	size_t data_size;
	size_t offset;
	void *data;
	u8 class = rproc->elf_class;
	int phnum = 0;
	struct rproc_coredump_state dump_state;
	enum rproc_dump_mechanism dump_conf = rproc->dump_conf;

	if (list_empty(&rproc->dump_segments) ||
	    dump_conf == RPROC_COREDUMP_DISABLED)
		return;

	if (class == ELFCLASSNONE) {
		dev_err(&rproc->dev, "ELF class is not set\n");
		return;
	}

	data_size = elf_size_of_hdr(class);
	list_for_each_entry(segment, &rproc->dump_segments, node) {
		 
		data_size += elf_size_of_phdr(class);
		if (dump_conf == RPROC_COREDUMP_ENABLED)
			data_size += segment->size;

		phnum++;
	}

	data = vmalloc(data_size);
	if (!data)
		return;

	ehdr = data;

	memset(ehdr, 0, elf_size_of_hdr(class));
	 
	elf_hdr_init_ident(ehdr, class);

	elf_hdr_set_e_type(class, ehdr, ET_CORE);
	elf_hdr_set_e_machine(class, ehdr, rproc->elf_machine);
	elf_hdr_set_e_version(class, ehdr, EV_CURRENT);
	elf_hdr_set_e_entry(class, ehdr, rproc->bootaddr);
	elf_hdr_set_e_phoff(class, ehdr, elf_size_of_hdr(class));
	elf_hdr_set_e_ehsize(class, ehdr, elf_size_of_hdr(class));
	elf_hdr_set_e_phentsize(class, ehdr, elf_size_of_phdr(class));
	elf_hdr_set_e_phnum(class, ehdr, phnum);

	phdr = data + elf_hdr_get_e_phoff(class, ehdr);
	offset = elf_hdr_get_e_phoff(class, ehdr);
	offset += elf_size_of_phdr(class) * elf_hdr_get_e_phnum(class, ehdr);

	list_for_each_entry(segment, &rproc->dump_segments, node) {
		memset(phdr, 0, elf_size_of_phdr(class));
		elf_phdr_set_p_type(class, phdr, PT_LOAD);
		elf_phdr_set_p_offset(class, phdr, offset);
		elf_phdr_set_p_vaddr(class, phdr, segment->da);
		elf_phdr_set_p_paddr(class, phdr, segment->da);
		elf_phdr_set_p_filesz(class, phdr, segment->size);
		elf_phdr_set_p_memsz(class, phdr, segment->size);
		elf_phdr_set_p_flags(class, phdr, PF_R | PF_W | PF_X);
		elf_phdr_set_p_align(class, phdr, 0);

		if (dump_conf == RPROC_COREDUMP_ENABLED)
			rproc_copy_segment(rproc, data + offset, segment, 0,
					   segment->size);

		offset += elf_phdr_get_p_filesz(class, phdr);
		phdr += elf_size_of_phdr(class);
	}
	if (dump_conf == RPROC_COREDUMP_ENABLED) {
		dev_coredumpv(&rproc->dev, data, data_size, GFP_KERNEL);
		return;
	}

	 
	dump_state.rproc = rproc;
	dump_state.header = data;
	init_completion(&dump_state.dump_done);

	dev_coredumpm(&rproc->dev, NULL, &dump_state, data_size, GFP_KERNEL,
		      rproc_coredump_read, rproc_coredump_free);

	 
	wait_for_completion(&dump_state.dump_done);
}
EXPORT_SYMBOL_GPL(rproc_coredump);

 
void rproc_coredump_using_sections(struct rproc *rproc)
{
	struct rproc_dump_segment *segment;
	void *shdr;
	void *ehdr;
	size_t data_size;
	size_t strtbl_size = 0;
	size_t strtbl_index = 1;
	size_t offset;
	void *data;
	u8 class = rproc->elf_class;
	int shnum;
	struct rproc_coredump_state dump_state;
	unsigned int dump_conf = rproc->dump_conf;
	char *str_tbl = "STR_TBL";

	if (list_empty(&rproc->dump_segments) ||
	    dump_conf == RPROC_COREDUMP_DISABLED)
		return;

	if (class == ELFCLASSNONE) {
		dev_err(&rproc->dev, "ELF class is not set\n");
		return;
	}

	 
	data_size = elf_size_of_hdr(class) + 2 * elf_size_of_shdr(class);
	shnum = 2;

	 
	strtbl_size += strlen(str_tbl) + 2;

	list_for_each_entry(segment, &rproc->dump_segments, node) {
		data_size += elf_size_of_shdr(class);
		strtbl_size += strlen(segment->priv) + 1;
		if (dump_conf == RPROC_COREDUMP_ENABLED)
			data_size += segment->size;
		shnum++;
	}

	data_size += strtbl_size;

	data = vmalloc(data_size);
	if (!data)
		return;

	ehdr = data;
	memset(ehdr, 0, elf_size_of_hdr(class));
	 
	elf_hdr_init_ident(ehdr, class);

	elf_hdr_set_e_type(class, ehdr, ET_CORE);
	elf_hdr_set_e_machine(class, ehdr, rproc->elf_machine);
	elf_hdr_set_e_version(class, ehdr, EV_CURRENT);
	elf_hdr_set_e_entry(class, ehdr, rproc->bootaddr);
	elf_hdr_set_e_shoff(class, ehdr, elf_size_of_hdr(class));
	elf_hdr_set_e_ehsize(class, ehdr, elf_size_of_hdr(class));
	elf_hdr_set_e_shentsize(class, ehdr, elf_size_of_shdr(class));
	elf_hdr_set_e_shnum(class, ehdr, shnum);
	elf_hdr_set_e_shstrndx(class, ehdr, 1);

	 
	shdr = data + elf_hdr_get_e_shoff(class, ehdr);
	memset(shdr, 0, elf_size_of_shdr(class));
	shdr += elf_size_of_shdr(class);

	 
	offset = elf_hdr_get_e_shoff(class, ehdr) +
		 elf_size_of_shdr(class) * elf_hdr_get_e_shnum(class, ehdr);
	memset(data + offset, 0, strtbl_size);

	 
	memset(shdr, 0, elf_size_of_shdr(class));
	elf_shdr_set_sh_type(class, shdr, SHT_STRTAB);
	elf_shdr_set_sh_offset(class, shdr, offset);
	elf_shdr_set_sh_size(class, shdr, strtbl_size);
	elf_shdr_set_sh_entsize(class, shdr, 0);
	elf_shdr_set_sh_flags(class, shdr, 0);
	elf_shdr_set_sh_name(class, shdr, elf_strtbl_add(str_tbl, ehdr, class, &strtbl_index));
	offset += elf_shdr_get_sh_size(class, shdr);
	shdr += elf_size_of_shdr(class);

	list_for_each_entry(segment, &rproc->dump_segments, node) {
		memset(shdr, 0, elf_size_of_shdr(class));
		elf_shdr_set_sh_type(class, shdr, SHT_PROGBITS);
		elf_shdr_set_sh_offset(class, shdr, offset);
		elf_shdr_set_sh_addr(class, shdr, segment->da);
		elf_shdr_set_sh_size(class, shdr, segment->size);
		elf_shdr_set_sh_entsize(class, shdr, 0);
		elf_shdr_set_sh_flags(class, shdr, SHF_WRITE);
		elf_shdr_set_sh_name(class, shdr,
				     elf_strtbl_add(segment->priv, ehdr, class, &strtbl_index));

		 
		if (dump_conf == RPROC_COREDUMP_ENABLED)
			rproc_copy_segment(rproc, data + offset, segment, 0,
					   segment->size);
		offset += elf_shdr_get_sh_size(class, shdr);
		shdr += elf_size_of_shdr(class);
	}

	if (dump_conf == RPROC_COREDUMP_ENABLED) {
		dev_coredumpv(&rproc->dev, data, data_size, GFP_KERNEL);
		return;
	}

	 
	dump_state.rproc = rproc;
	dump_state.header = data;
	init_completion(&dump_state.dump_done);

	dev_coredumpm(&rproc->dev, NULL, &dump_state, data_size, GFP_KERNEL,
		      rproc_coredump_read, rproc_coredump_free);

	 
	wait_for_completion(&dump_state.dump_done);
}
EXPORT_SYMBOL(rproc_coredump_using_sections);
