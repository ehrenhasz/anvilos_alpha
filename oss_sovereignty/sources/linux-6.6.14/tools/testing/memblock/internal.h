
#ifndef _MM_INTERNAL_H
#define _MM_INTERNAL_H


#ifdef MEMBLOCK_DEBUG
static int memblock_debug = 1;
#endif

#define pr_warn_ratelimited(fmt, ...)    printf(fmt, ##__VA_ARGS__)

bool mirrored_kernelcore = false;

struct page {};

void memblock_free_pages(struct page *page, unsigned long pfn,
			 unsigned int order)
{
}

static inline void accept_memory(phys_addr_t start, phys_addr_t end)
{
}

#endif
