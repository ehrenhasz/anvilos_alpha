
 

#include <linux/io.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/mutex.h>

#include <sound/core.h>
#include "trident.h"

 
#define __set_tlb_bus(trident,page,addr) \
	(trident)->tlb.entries[page] = cpu_to_le32((addr) & ~(SNDRV_TRIDENT_PAGE_SIZE-1))
#define __tlb_to_addr(trident,page) \
	(dma_addr_t)le32_to_cpu((trident->tlb.entries[page]) & ~(SNDRV_TRIDENT_PAGE_SIZE - 1))

#if PAGE_SIZE == 4096
 
#define ALIGN_PAGE_SIZE		PAGE_SIZE	 
#define MAX_ALIGN_PAGES		SNDRV_TRIDENT_MAX_PAGES	 
 
#define set_tlb_bus(trident,page,addr) __set_tlb_bus(trident,page,addr)
 
#define set_silent_tlb(trident,page)	__set_tlb_bus(trident, page, trident->tlb.silent_page->addr)
 
#define get_aligned_page(offset)	((offset) >> 12)
 
#define aligned_page_offset(page)	((page) << 12)
 
#define page_to_addr(trident,page)	__tlb_to_addr(trident, page)

#elif PAGE_SIZE == 8192
 
#define ALIGN_PAGE_SIZE		PAGE_SIZE
#define MAX_ALIGN_PAGES		(SNDRV_TRIDENT_MAX_PAGES / 2)
#define get_aligned_page(offset)	((offset) >> 13)
#define aligned_page_offset(page)	((page) << 13)
#define page_to_addr(trident,page)	__tlb_to_addr(trident, (page) << 1)

 
static inline void set_tlb_bus(struct snd_trident *trident, int page,
			       dma_addr_t addr)
{
	page <<= 1;
	__set_tlb_bus(trident, page, addr);
	__set_tlb_bus(trident, page+1, addr + SNDRV_TRIDENT_PAGE_SIZE);
}
static inline void set_silent_tlb(struct snd_trident *trident, int page)
{
	page <<= 1;
	__set_tlb_bus(trident, page, trident->tlb.silent_page->addr);
	__set_tlb_bus(trident, page+1, trident->tlb.silent_page->addr);
}

#else
 
#define UNIT_PAGES		(PAGE_SIZE / SNDRV_TRIDENT_PAGE_SIZE)
#define ALIGN_PAGE_SIZE		(SNDRV_TRIDENT_PAGE_SIZE * UNIT_PAGES)
#define MAX_ALIGN_PAGES		(SNDRV_TRIDENT_MAX_PAGES / UNIT_PAGES)
 
#define get_aligned_page(offset)	((offset) / ALIGN_PAGE_SIZE)
#define aligned_page_offset(page)	((page) * ALIGN_PAGE_SIZE)
#define page_to_addr(trident,page)	__tlb_to_addr(trident, (page) * UNIT_PAGES)

 
static inline void set_tlb_bus(struct snd_trident *trident, int page,
			       dma_addr_t addr)
{
	int i;
	page *= UNIT_PAGES;
	for (i = 0; i < UNIT_PAGES; i++, page++) {
		__set_tlb_bus(trident, page, addr);
		addr += SNDRV_TRIDENT_PAGE_SIZE;
	}
}
static inline void set_silent_tlb(struct snd_trident *trident, int page)
{
	int i;
	page *= UNIT_PAGES;
	for (i = 0; i < UNIT_PAGES; i++, page++)
		__set_tlb_bus(trident, page, trident->tlb.silent_page->addr);
}

#endif  

 
#define firstpg(blk)	(((struct snd_trident_memblk_arg *)snd_util_memblk_argptr(blk))->first_page)
#define lastpg(blk)	(((struct snd_trident_memblk_arg *)snd_util_memblk_argptr(blk))->last_page)

 
static struct snd_util_memblk *
search_empty(struct snd_util_memhdr *hdr, int size)
{
	struct snd_util_memblk *blk;
	int page, psize;
	struct list_head *p;

	psize = get_aligned_page(size + ALIGN_PAGE_SIZE -1);
	page = 0;
	list_for_each(p, &hdr->block) {
		blk = list_entry(p, struct snd_util_memblk, list);
		if (page + psize <= firstpg(blk))
			goto __found_pages;
		page = lastpg(blk) + 1;
	}
	if (page + psize > MAX_ALIGN_PAGES)
		return NULL;

__found_pages:
	 
	blk = __snd_util_memblk_new(hdr, psize * ALIGN_PAGE_SIZE, p->prev);
	if (blk == NULL)
		return NULL;
	blk->offset = aligned_page_offset(page);  
	firstpg(blk) = page;
	lastpg(blk) = page + psize - 1;
	return blk;
}


 
static int is_valid_page(unsigned long ptr)
{
	if (ptr & ~0x3fffffffUL) {
		snd_printk(KERN_ERR "max memory size is 1GB!!\n");
		return 0;
	}
	if (ptr & (SNDRV_TRIDENT_PAGE_SIZE-1)) {
		snd_printk(KERN_ERR "page is not aligned\n");
		return 0;
	}
	return 1;
}

 
static struct snd_util_memblk *
snd_trident_alloc_sg_pages(struct snd_trident *trident,
			   struct snd_pcm_substream *substream)
{
	struct snd_util_memhdr *hdr;
	struct snd_util_memblk *blk;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int idx, page;

	if (snd_BUG_ON(runtime->dma_bytes <= 0 ||
		       runtime->dma_bytes > SNDRV_TRIDENT_MAX_PAGES *
					SNDRV_TRIDENT_PAGE_SIZE))
		return NULL;
	hdr = trident->tlb.memhdr;
	if (snd_BUG_ON(!hdr))
		return NULL;

	

	mutex_lock(&hdr->block_mutex);
	blk = search_empty(hdr, runtime->dma_bytes);
	if (blk == NULL) {
		mutex_unlock(&hdr->block_mutex);
		return NULL;
	}
			   
	 
	idx = 0;
	for (page = firstpg(blk); page <= lastpg(blk); page++, idx++) {
		unsigned long ofs = idx << PAGE_SHIFT;
		dma_addr_t addr = snd_pcm_sgbuf_get_addr(substream, ofs);
		if (! is_valid_page(addr)) {
			__snd_util_mem_free(hdr, blk);
			mutex_unlock(&hdr->block_mutex);
			return NULL;
		}
		set_tlb_bus(trident, page, addr);
	}
	mutex_unlock(&hdr->block_mutex);
	return blk;
}

 
static struct snd_util_memblk *
snd_trident_alloc_cont_pages(struct snd_trident *trident,
			     struct snd_pcm_substream *substream)
{
	struct snd_util_memhdr *hdr;
	struct snd_util_memblk *blk;
	int page;
	struct snd_pcm_runtime *runtime = substream->runtime;
	dma_addr_t addr;

	if (snd_BUG_ON(runtime->dma_bytes <= 0 ||
		       runtime->dma_bytes > SNDRV_TRIDENT_MAX_PAGES *
					SNDRV_TRIDENT_PAGE_SIZE))
		return NULL;
	hdr = trident->tlb.memhdr;
	if (snd_BUG_ON(!hdr))
		return NULL;

	mutex_lock(&hdr->block_mutex);
	blk = search_empty(hdr, runtime->dma_bytes);
	if (blk == NULL) {
		mutex_unlock(&hdr->block_mutex);
		return NULL;
	}
			   
	 
	addr = runtime->dma_addr;
	for (page = firstpg(blk); page <= lastpg(blk); page++,
	     addr += SNDRV_TRIDENT_PAGE_SIZE) {
		if (! is_valid_page(addr)) {
			__snd_util_mem_free(hdr, blk);
			mutex_unlock(&hdr->block_mutex);
			return NULL;
		}
		set_tlb_bus(trident, page, addr);
	}
	mutex_unlock(&hdr->block_mutex);
	return blk;
}

 
struct snd_util_memblk *
snd_trident_alloc_pages(struct snd_trident *trident,
			struct snd_pcm_substream *substream)
{
	if (snd_BUG_ON(!trident || !substream))
		return NULL;
	if (substream->dma_buffer.dev.type == SNDRV_DMA_TYPE_DEV_SG)
		return snd_trident_alloc_sg_pages(trident, substream);
	else
		return snd_trident_alloc_cont_pages(trident, substream);
}


 
int snd_trident_free_pages(struct snd_trident *trident,
			   struct snd_util_memblk *blk)
{
	struct snd_util_memhdr *hdr;
	int page;

	if (snd_BUG_ON(!trident || !blk))
		return -EINVAL;

	hdr = trident->tlb.memhdr;
	mutex_lock(&hdr->block_mutex);
	 
	for (page = firstpg(blk); page <= lastpg(blk); page++)
		set_silent_tlb(trident, page);
	 
	__snd_util_mem_free(hdr, blk);
	mutex_unlock(&hdr->block_mutex);
	return 0;
}
