 

#include <linux/mm.h>
#include <linux/io-mapping.h>


#include "i915_drv.h"
#include "i915_mm.h"

struct remap_pfn {
	struct mm_struct *mm;
	unsigned long pfn;
	pgprot_t prot;

	struct sgt_iter sgt;
	resource_size_t iobase;
};

#define use_dma(io) ((io) != -1)

static inline unsigned long sgt_pfn(const struct remap_pfn *r)
{
	if (use_dma(r->iobase))
		return (r->sgt.dma + r->sgt.curr + r->iobase) >> PAGE_SHIFT;
	else
		return r->sgt.pfn + (r->sgt.curr >> PAGE_SHIFT);
}

static int remap_sg(pte_t *pte, unsigned long addr, void *data)
{
	struct remap_pfn *r = data;

	if (GEM_WARN_ON(!r->sgt.sgp))
		return -EINVAL;

	 
	set_pte_at(r->mm, addr, pte,
		   pte_mkspecial(pfn_pte(sgt_pfn(r), r->prot)));
	r->pfn++;  

	r->sgt.curr += PAGE_SIZE;
	if (r->sgt.curr >= r->sgt.max)
		r->sgt = __sgt_iter(__sg_next(r->sgt.sgp), use_dma(r->iobase));

	return 0;
}

#define EXPECTED_FLAGS (VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP)

#if IS_ENABLED(CONFIG_X86)
static int remap_pfn(pte_t *pte, unsigned long addr, void *data)
{
	struct remap_pfn *r = data;

	 
	set_pte_at(r->mm, addr, pte, pte_mkspecial(pfn_pte(r->pfn, r->prot)));
	r->pfn++;

	return 0;
}

 
int remap_io_mapping(struct vm_area_struct *vma,
		     unsigned long addr, unsigned long pfn, unsigned long size,
		     struct io_mapping *iomap)
{
	struct remap_pfn r;
	int err;

	GEM_BUG_ON((vma->vm_flags & EXPECTED_FLAGS) != EXPECTED_FLAGS);

	 
	r.mm = vma->vm_mm;
	r.pfn = pfn;
	r.prot = __pgprot((pgprot_val(iomap->prot) & _PAGE_CACHE_MASK) |
			  (pgprot_val(vma->vm_page_prot) & ~_PAGE_CACHE_MASK));

	err = apply_to_page_range(r.mm, addr, size, remap_pfn, &r);
	if (unlikely(err)) {
		zap_vma_ptes(vma, addr, (r.pfn - pfn) << PAGE_SHIFT);
		return err;
	}

	return 0;
}
#endif

 
int remap_io_sg(struct vm_area_struct *vma,
		unsigned long addr, unsigned long size,
		struct scatterlist *sgl, resource_size_t iobase)
{
	struct remap_pfn r = {
		.mm = vma->vm_mm,
		.prot = vma->vm_page_prot,
		.sgt = __sgt_iter(sgl, use_dma(iobase)),
		.iobase = iobase,
	};
	int err;

	 
	GEM_BUG_ON((vma->vm_flags & EXPECTED_FLAGS) != EXPECTED_FLAGS);

	if (!use_dma(iobase))
		flush_cache_range(vma, addr, size);

	err = apply_to_page_range(r.mm, addr, size, remap_sg, &r);
	if (unlikely(err)) {
		zap_vma_ptes(vma, addr, r.pfn << PAGE_SHIFT);
		return err;
	}

	return 0;
}
