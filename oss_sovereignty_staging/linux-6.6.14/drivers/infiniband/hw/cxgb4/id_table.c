 
#include <linux/kernel.h>
#include <linux/random.h>
#include "iw_cxgb4.h"

#define RANDOM_SKIP 16

 
u32 c4iw_id_alloc(struct c4iw_id_table *alloc)
{
	unsigned long flags;
	u32 obj;

	spin_lock_irqsave(&alloc->lock, flags);

	obj = find_next_zero_bit(alloc->table, alloc->max, alloc->last);
	if (obj >= alloc->max)
		obj = find_first_zero_bit(alloc->table, alloc->max);

	if (obj < alloc->max) {
		if (alloc->flags & C4IW_ID_TABLE_F_RANDOM)
			alloc->last += get_random_u32_below(RANDOM_SKIP);
		else
			alloc->last = obj + 1;
		if (alloc->last >= alloc->max)
			alloc->last = 0;
		__set_bit(obj, alloc->table);
		obj += alloc->start;
	} else
		obj = -1;

	spin_unlock_irqrestore(&alloc->lock, flags);
	return obj;
}

void c4iw_id_free(struct c4iw_id_table *alloc, u32 obj)
{
	unsigned long flags;

	obj -= alloc->start;

	spin_lock_irqsave(&alloc->lock, flags);
	__clear_bit(obj, alloc->table);
	spin_unlock_irqrestore(&alloc->lock, flags);
}

int c4iw_id_table_alloc(struct c4iw_id_table *alloc, u32 start, u32 num,
			u32 reserved, u32 flags)
{
	alloc->start = start;
	alloc->flags = flags;
	if (flags & C4IW_ID_TABLE_F_RANDOM)
		alloc->last = get_random_u32_below(RANDOM_SKIP);
	else
		alloc->last = 0;
	alloc->max = num;
	spin_lock_init(&alloc->lock);
	alloc->table = bitmap_zalloc(num, GFP_KERNEL);
	if (!alloc->table)
		return -ENOMEM;

	if (!(alloc->flags & C4IW_ID_TABLE_F_EMPTY))
		bitmap_set(alloc->table, 0, reserved);

	return 0;
}

void c4iw_id_table_free(struct c4iw_id_table *alloc)
{
	bitmap_free(alloc->table);
}
