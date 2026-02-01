 
#include <linux/prefetch.h>

  
static inline unsigned int
iommu_fill_pdir(struct ioc *ioc, struct scatterlist *startsg, int nents, 
		unsigned long hint,
		void (*iommu_io_pdir_entry)(__le64 *, space_t, unsigned long,
					    unsigned long))
{
	struct scatterlist *dma_sg = startsg;	 
	unsigned int n_mappings = 0;
	unsigned long dma_offset = 0, dma_len = 0;
	__le64 *pdirp = NULL;

	 
	 dma_sg--;

	while (nents-- > 0) {
		unsigned long vaddr;
		long size;

		DBG_RUN_SG(" %d : %08lx %p/%05x\n", nents,
			   (unsigned long)sg_dma_address(startsg),
			   sg_virt(startsg), startsg->length
		);


		 
		
		if (sg_dma_address(startsg) & PIDE_FLAG) {
			u32 pide = sg_dma_address(startsg) & ~PIDE_FLAG;

			BUG_ON(pdirp && (dma_len != sg_dma_len(dma_sg)));

			dma_sg++;

			dma_len = sg_dma_len(startsg);
			sg_dma_len(startsg) = 0;
			dma_offset = (unsigned long) pide & ~IOVP_MASK;
			n_mappings++;
#if defined(ZX1_SUPPORT)
			 
			sg_dma_address(dma_sg) = pide | ioc->ibase;
#else
			 
			sg_dma_address(dma_sg) = pide;
#endif
			pdirp = &(ioc->pdir_base[pide >> IOVP_SHIFT]);
			prefetchw(pdirp);
		}
		
		BUG_ON(pdirp == NULL);
		
		vaddr = (unsigned long)sg_virt(startsg);
		sg_dma_len(dma_sg) += startsg->length;
		size = startsg->length + dma_offset;
		dma_offset = 0;
#ifdef IOMMU_MAP_STATS
		ioc->msg_pages += startsg->length >> IOVP_SHIFT;
#endif
		do {
			iommu_io_pdir_entry(pdirp, KERNEL_SPACE, 
					    vaddr, hint);
			vaddr += IOVP_SIZE;
			size -= IOVP_SIZE;
			pdirp++;
		} while(unlikely(size > 0));
		startsg++;
	}
	return(n_mappings);
}


 

static inline unsigned int
iommu_coalesce_chunks(struct ioc *ioc, struct device *dev,
		struct scatterlist *startsg, int nents,
		int (*iommu_alloc_range)(struct ioc *, struct device *, size_t))
{
	struct scatterlist *contig_sg;	    
	unsigned long dma_offset, dma_len;  
	unsigned int n_mappings = 0;
	unsigned int max_seg_size = min(dma_get_max_seg_size(dev),
					(unsigned)DMA_CHUNK_SIZE);
	unsigned int max_seg_boundary = dma_get_seg_boundary(dev) + 1;
	if (max_seg_boundary)	 
		max_seg_size = min(max_seg_size, max_seg_boundary);

	while (nents > 0) {

		 
		contig_sg = startsg;
		dma_len = startsg->length;
		dma_offset = startsg->offset;

		 
		sg_dma_address(startsg) = 0;
		sg_dma_len(startsg) = 0;

		 
		while(--nents > 0) {
			unsigned long prev_end, sg_start;

			prev_end = (unsigned long)sg_virt(startsg) +
							startsg->length;

			startsg++;
			sg_start = (unsigned long)sg_virt(startsg);

			 
			sg_dma_address(startsg) = 0;
			sg_dma_len(startsg) = 0;

			    
			if (unlikely(ALIGN(dma_len + dma_offset + startsg->length, IOVP_SIZE) >
				     max_seg_size))
				break;

			 
			if (unlikely((prev_end != sg_start) ||
				((prev_end | sg_start) & ~PAGE_MASK)))
				break;
			
			dma_len += startsg->length;
		}

		 
		sg_dma_len(contig_sg) = dma_len;
		dma_len = ALIGN(dma_len + dma_offset, IOVP_SIZE);
		sg_dma_address(contig_sg) =
			PIDE_FLAG 
			| (iommu_alloc_range(ioc, dev, dma_len) << IOVP_SHIFT)
			| dma_offset;
		n_mappings++;
	}

	return n_mappings;
}

