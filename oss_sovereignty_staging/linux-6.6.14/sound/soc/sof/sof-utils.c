









#include <asm/unaligned.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/device.h>
#include <sound/memalloc.h>
#include <linux/module.h>
#include "sof-utils.h"

 

int snd_sof_create_page_table(struct device *dev,
			      struct snd_dma_buffer *dmab,
			      unsigned char *page_table, size_t size)
{
	int i, pages;

	pages = snd_sgbuf_aligned_pages(size);

	dev_dbg(dev, "generating page table for %p size 0x%zx pages %d\n",
		dmab->area, size, pages);

	for (i = 0; i < pages; i++) {
		 
		u32 idx = (5 * i) >> 1;
		u32 pfn = snd_sgbuf_get_addr(dmab, i * PAGE_SIZE) >> PAGE_SHIFT;
		u8 *pg_table;

		pg_table = (u8 *)(page_table + idx);

		 
		if (i & 1)
			put_unaligned_le32((pg_table[0] & 0xf) | pfn << 4,
					   pg_table);
		else
			put_unaligned_le32(pfn, pg_table);
	}

	return pages;
}
EXPORT_SYMBOL(snd_sof_create_page_table);

MODULE_LICENSE("Dual BSD/GPL");
