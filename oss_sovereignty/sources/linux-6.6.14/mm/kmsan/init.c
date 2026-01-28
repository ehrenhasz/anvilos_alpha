


#include "kmsan.h"

#include <asm/sections.h>
#include <linux/mm.h>
#include <linux/memblock.h>

#include "../internal.h"

#define NUM_FUTURE_RANGES 128
struct start_end_pair {
	u64 start, end;
};

static struct start_end_pair start_end_pairs[NUM_FUTURE_RANGES] __initdata;
static int future_index __initdata;


static void __init kmsan_record_future_shadow_range(void *start, void *end)
{
	u64 nstart = (u64)start, nend = (u64)end, cstart, cend;
	bool merged = false;

	KMSAN_WARN_ON(future_index == NUM_FUTURE_RANGES);
	KMSAN_WARN_ON((nstart >= nend) || !nstart || !nend);
	nstart = ALIGN_DOWN(nstart, PAGE_SIZE);
	nend = ALIGN(nend, PAGE_SIZE);

	
	for (int i = 0; i < future_index; i++) {
		cstart = start_end_pairs[i].start;
		cend = start_end_pairs[i].end;
		if ((cstart < nstart && cend < nstart) ||
		    (cstart > nend && cend > nend))
			
			continue;
		start_end_pairs[i].start = min(nstart, cstart);
		start_end_pairs[i].end = max(nend, cend);
		merged = true;
		break;
	}
	if (merged)
		return;
	start_end_pairs[future_index].start = nstart;
	start_end_pairs[future_index].end = nend;
	future_index++;
}


void __init kmsan_init_shadow(void)
{
	const size_t nd_size = roundup(sizeof(pg_data_t), PAGE_SIZE);
	phys_addr_t p_start, p_end;
	u64 loop;
	int nid;

	for_each_reserved_mem_range(loop, &p_start, &p_end)
		kmsan_record_future_shadow_range(phys_to_virt(p_start),
						 phys_to_virt(p_end));
	
	kmsan_record_future_shadow_range(_sdata, _edata);

	for_each_online_node(nid)
		kmsan_record_future_shadow_range(
			NODE_DATA(nid), (char *)NODE_DATA(nid) + nd_size);

	for (int i = 0; i < future_index; i++)
		kmsan_init_alloc_meta_for_range(
			(void *)start_end_pairs[i].start,
			(void *)start_end_pairs[i].end);
}

struct metadata_page_pair {
	struct page *shadow, *origin;
};
static struct metadata_page_pair held_back[MAX_ORDER + 1] __initdata;


bool kmsan_memblock_free_pages(struct page *page, unsigned int order)
{
	struct page *shadow, *origin;

	if (!held_back[order].shadow) {
		held_back[order].shadow = page;
		return false;
	}
	if (!held_back[order].origin) {
		held_back[order].origin = page;
		return false;
	}
	shadow = held_back[order].shadow;
	origin = held_back[order].origin;
	kmsan_setup_meta(page, shadow, origin, order);

	held_back[order].shadow = NULL;
	held_back[order].origin = NULL;
	return true;
}

#define MAX_BLOCKS 8
struct smallstack {
	struct page *items[MAX_BLOCKS];
	int index;
	int order;
};

static struct smallstack collect = {
	.index = 0,
	.order = MAX_ORDER,
};

static void smallstack_push(struct smallstack *stack, struct page *pages)
{
	KMSAN_WARN_ON(stack->index == MAX_BLOCKS);
	stack->items[stack->index] = pages;
	stack->index++;
}
#undef MAX_BLOCKS

static struct page *smallstack_pop(struct smallstack *stack)
{
	struct page *ret;

	KMSAN_WARN_ON(stack->index == 0);
	stack->index--;
	ret = stack->items[stack->index];
	stack->items[stack->index] = NULL;
	return ret;
}

static void do_collection(void)
{
	struct page *page, *shadow, *origin;

	while (collect.index >= 3) {
		page = smallstack_pop(&collect);
		shadow = smallstack_pop(&collect);
		origin = smallstack_pop(&collect);
		kmsan_setup_meta(page, shadow, origin, collect.order);
		__free_pages_core(page, collect.order);
	}
}

static void collect_split(void)
{
	struct smallstack tmp = {
		.order = collect.order - 1,
		.index = 0,
	};
	struct page *page;

	if (!collect.order)
		return;
	while (collect.index) {
		page = smallstack_pop(&collect);
		smallstack_push(&tmp, &page[0]);
		smallstack_push(&tmp, &page[1 << tmp.order]);
	}
	__memcpy(&collect, &tmp, sizeof(tmp));
}


static void kmsan_memblock_discard(void)
{
	
	collect.order = MAX_ORDER;
	for (int i = MAX_ORDER; i >= 0; i--) {
		if (held_back[i].shadow)
			smallstack_push(&collect, held_back[i].shadow);
		if (held_back[i].origin)
			smallstack_push(&collect, held_back[i].origin);
		held_back[i].shadow = NULL;
		held_back[i].origin = NULL;
		do_collection();
		collect_split();
	}
}

void __init kmsan_init_runtime(void)
{
	
	kmsan_internal_task_create(current);
	kmsan_memblock_discard();
	pr_info("Starting KernelMemorySanitizer\n");
	pr_info("ATTENTION: KMSAN is a debugging tool! Do not use it on production machines!\n");
	kmsan_enabled = true;
}
