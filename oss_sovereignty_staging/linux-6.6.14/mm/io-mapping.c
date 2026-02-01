

#include <linux/mm.h>
#include <linux/io-mapping.h>

 
int io_mapping_map_user(struct io_mapping *iomap, struct vm_area_struct *vma,
		unsigned long addr, unsigned long pfn, unsigned long size)
{
	vm_flags_t expected_flags = VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;

	if (WARN_ON_ONCE((vma->vm_flags & expected_flags) != expected_flags))
		return -EINVAL;

	 
	return remap_pfn_range_notrack(vma, addr, pfn, size,
		__pgprot((pgprot_val(iomap->prot) & _PAGE_CACHE_MASK) |
			 (pgprot_val(vma->vm_page_prot) & ~_PAGE_CACHE_MASK)));
}
EXPORT_SYMBOL_GPL(io_mapping_map_user);
