 
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/cpu.h>
#include <linux/export.h>

#include "rds.h"

struct rds_page_remainder {
	struct page	*r_page;
	unsigned long	r_offset;
};

static
DEFINE_PER_CPU_SHARED_ALIGNED(struct rds_page_remainder, rds_page_remainders);

 
int rds_page_remainder_alloc(struct scatterlist *scat, unsigned long bytes,
			     gfp_t gfp)
{
	struct rds_page_remainder *rem;
	unsigned long flags;
	struct page *page;
	int ret;

	gfp |= __GFP_HIGHMEM;

	 
	if (bytes >= PAGE_SIZE) {
		page = alloc_page(gfp);
		if (!page) {
			ret = -ENOMEM;
		} else {
			sg_set_page(scat, page, PAGE_SIZE, 0);
			ret = 0;
		}
		goto out;
	}

	rem = &per_cpu(rds_page_remainders, get_cpu());
	local_irq_save(flags);

	while (1) {
		 
		if (rem->r_page && bytes > (PAGE_SIZE - rem->r_offset)) {
			rds_stats_inc(s_page_remainder_miss);
			__free_page(rem->r_page);
			rem->r_page = NULL;
		}

		 
		if (rem->r_page && bytes <= (PAGE_SIZE - rem->r_offset)) {
			sg_set_page(scat, rem->r_page, bytes, rem->r_offset);
			get_page(sg_page(scat));

			if (rem->r_offset != 0)
				rds_stats_inc(s_page_remainder_hit);

			rem->r_offset += ALIGN(bytes, 8);
			if (rem->r_offset >= PAGE_SIZE) {
				__free_page(rem->r_page);
				rem->r_page = NULL;
			}
			ret = 0;
			break;
		}

		 
		local_irq_restore(flags);
		put_cpu();

		page = alloc_page(gfp);

		rem = &per_cpu(rds_page_remainders, get_cpu());
		local_irq_save(flags);

		if (!page) {
			ret = -ENOMEM;
			break;
		}

		 
		if (rem->r_page) {
			__free_page(page);
			continue;
		}

		 
		rem->r_page = page;
		rem->r_offset = 0;
	}

	local_irq_restore(flags);
	put_cpu();
out:
	rdsdebug("bytes %lu ret %d %p %u %u\n", bytes, ret,
		 ret ? NULL : sg_page(scat), ret ? 0 : scat->offset,
		 ret ? 0 : scat->length);
	return ret;
}
EXPORT_SYMBOL_GPL(rds_page_remainder_alloc);

void rds_page_exit(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		struct rds_page_remainder *rem;

		rem = &per_cpu(rds_page_remainders, cpu);
		rdsdebug("cpu %u\n", cpu);

		if (rem->r_page)
			__free_page(rem->r_page);
		rem->r_page = NULL;
	}
}
