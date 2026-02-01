
 

#define pr_fmt(fmt) "efi: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/efi.h>
#include <linux/io.h>
#include <asm/early_ioremap.h>
#include <asm/efi.h>
#include <linux/memblock.h>
#include <linux/slab.h>

static phys_addr_t __init __efi_memmap_alloc_early(unsigned long size)
{
	return memblock_phys_alloc(size, SMP_CACHE_BYTES);
}

static phys_addr_t __init __efi_memmap_alloc_late(unsigned long size)
{
	unsigned int order = get_order(size);
	struct page *p = alloc_pages(GFP_KERNEL, order);

	if (!p)
		return 0;

	return PFN_PHYS(page_to_pfn(p));
}

void __init __efi_memmap_free(u64 phys, unsigned long size, unsigned long flags)
{
	if (flags & EFI_MEMMAP_MEMBLOCK) {
		if (slab_is_available())
			memblock_free_late(phys, size);
		else
			memblock_phys_free(phys, size);
	} else if (flags & EFI_MEMMAP_SLAB) {
		struct page *p = pfn_to_page(PHYS_PFN(phys));
		unsigned int order = get_order(size);

		free_pages((unsigned long) page_address(p), order);
	}
}

 
int __init efi_memmap_alloc(unsigned int num_entries,
		struct efi_memory_map_data *data)
{
	 
	WARN_ON(data->phys_map || data->size);

	data->size = num_entries * efi.memmap.desc_size;
	data->desc_version = efi.memmap.desc_version;
	data->desc_size = efi.memmap.desc_size;
	data->flags &= ~(EFI_MEMMAP_SLAB | EFI_MEMMAP_MEMBLOCK);
	data->flags |= efi.memmap.flags & EFI_MEMMAP_LATE;

	if (slab_is_available()) {
		data->flags |= EFI_MEMMAP_SLAB;
		data->phys_map = __efi_memmap_alloc_late(data->size);
	} else {
		data->flags |= EFI_MEMMAP_MEMBLOCK;
		data->phys_map = __efi_memmap_alloc_early(data->size);
	}

	if (!data->phys_map)
		return -ENOMEM;
	return 0;
}

 
int __init efi_memmap_install(struct efi_memory_map_data *data)
{
	efi_memmap_unmap();

	if (efi_enabled(EFI_PARAVIRT))
		return 0;

	return __efi_memmap_init(data);
}

 
int __init efi_memmap_split_count(efi_memory_desc_t *md, struct range *range)
{
	u64 m_start, m_end;
	u64 start, end;
	int count = 0;

	start = md->phys_addr;
	end = start + (md->num_pages << EFI_PAGE_SHIFT) - 1;

	 
	m_start = range->start;
	m_end = range->end;

	if (m_start <= start) {
		 
		if (start < m_end && m_end < end)
			count++;
	}

	if (start < m_start && m_start < end) {
		 
		if (m_end < end)
			count += 2;
		 
		if (end <= m_end)
			count++;
	}

	return count;
}

 
void __init efi_memmap_insert(struct efi_memory_map *old_memmap, void *buf,
			      struct efi_mem_range *mem)
{
	u64 m_start, m_end, m_attr;
	efi_memory_desc_t *md;
	u64 start, end;
	void *old, *new;

	 
	m_start = mem->range.start;
	m_end = mem->range.end;
	m_attr = mem->attribute;

	 
	if (!IS_ALIGNED(m_start, EFI_PAGE_SIZE) ||
	    !IS_ALIGNED(m_end + 1, EFI_PAGE_SIZE)) {
		WARN_ON(1);
		return;
	}

	for (old = old_memmap->map, new = buf;
	     old < old_memmap->map_end;
	     old += old_memmap->desc_size, new += old_memmap->desc_size) {

		 
		memcpy(new, old, old_memmap->desc_size);
		md = new;
		start = md->phys_addr;
		end = md->phys_addr + (md->num_pages << EFI_PAGE_SHIFT) - 1;

		if (m_start <= start && end <= m_end)
			md->attribute |= m_attr;

		if (m_start <= start &&
		    (start < m_end && m_end < end)) {
			 
			md->attribute |= m_attr;
			md->num_pages = (m_end - md->phys_addr + 1) >>
				EFI_PAGE_SHIFT;
			 
			new += old_memmap->desc_size;
			memcpy(new, old, old_memmap->desc_size);
			md = new;
			md->phys_addr = m_end + 1;
			md->num_pages = (end - md->phys_addr + 1) >>
				EFI_PAGE_SHIFT;
		}

		if ((start < m_start && m_start < end) && m_end < end) {
			 
			md->num_pages = (m_start - md->phys_addr) >>
				EFI_PAGE_SHIFT;
			 
			new += old_memmap->desc_size;
			memcpy(new, old, old_memmap->desc_size);
			md = new;
			md->attribute |= m_attr;
			md->phys_addr = m_start;
			md->num_pages = (m_end - m_start + 1) >>
				EFI_PAGE_SHIFT;
			 
			new += old_memmap->desc_size;
			memcpy(new, old, old_memmap->desc_size);
			md = new;
			md->phys_addr = m_end + 1;
			md->num_pages = (end - m_end) >>
				EFI_PAGE_SHIFT;
		}

		if ((start < m_start && m_start < end) &&
		    (end <= m_end)) {
			 
			md->num_pages = (m_start - md->phys_addr) >>
				EFI_PAGE_SHIFT;
			 
			new += old_memmap->desc_size;
			memcpy(new, old, old_memmap->desc_size);
			md = new;
			md->phys_addr = m_start;
			md->num_pages = (end - md->phys_addr + 1) >>
				EFI_PAGE_SHIFT;
			md->attribute |= m_attr;
		}
	}
}
