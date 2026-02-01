
 
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/radix-tree.h>
#include <stdlib.h>
#include <stdio.h>

#include "regression.h"
#include "test.h"

#define PAGECACHE_TAG_DIRTY     XA_MARK_0
#define PAGECACHE_TAG_WRITEBACK XA_MARK_1
#define PAGECACHE_TAG_TOWRITE   XA_MARK_2

static RADIX_TREE(mt_tree, GFP_KERNEL);
unsigned long page_count = 0;

struct page {
	unsigned long index;
};

static struct page *page_alloc(void)
{
	struct page *p;
	p = malloc(sizeof(struct page));
	p->index = page_count++;

	return p;
}

void regression2_test(void)
{
	int i;
	struct page *p;
	int max_slots = RADIX_TREE_MAP_SIZE;
	unsigned long int start, end;
	struct page *pages[1];

	printv(1, "running regression test 2 (should take milliseconds)\n");
	 
	for (i = 0; i <= max_slots - 1; i++) {
		p = page_alloc();
		radix_tree_insert(&mt_tree, i, p);
	}
	radix_tree_tag_set(&mt_tree, max_slots - 1, PAGECACHE_TAG_DIRTY);

	 
	start = 0;
	end = max_slots - 2;
	tag_tagged_items(&mt_tree, start, end, 1,
				PAGECACHE_TAG_DIRTY, PAGECACHE_TAG_TOWRITE);

	 
	p = page_alloc();
	radix_tree_insert(&mt_tree, max_slots, p);

	 
	radix_tree_tag_clear(&mt_tree, max_slots - 1, PAGECACHE_TAG_DIRTY);

	 
	for (i = max_slots - 1; i >= 0; i--)
		free(radix_tree_delete(&mt_tree, i));

	 
	
	
	start = 1;
	end = max_slots - 2;
	radix_tree_gang_lookup_tag_slot(&mt_tree, (void ***)pages, start, end,
		PAGECACHE_TAG_TOWRITE);

	 
	free(radix_tree_delete(&mt_tree, max_slots));

	BUG_ON(!radix_tree_empty(&mt_tree));

	printv(1, "regression test 2, done\n");
}
