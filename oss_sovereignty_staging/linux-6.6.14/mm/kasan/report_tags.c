
 

#include <linux/atomic.h>

#include "kasan.h"

extern struct kasan_stack_ring stack_ring;

static const char *get_common_bug_type(struct kasan_report_info *info)
{
	 
	if (info->access_addr + info->access_size < info->access_addr)
		return "out-of-bounds";

	return "invalid-access";
}

void kasan_complete_mode_report_info(struct kasan_report_info *info)
{
	unsigned long flags;
	u64 pos;
	struct kasan_stack_ring_entry *entry;
	void *ptr;
	u32 pid;
	depot_stack_handle_t stack;
	bool is_free;
	bool alloc_found = false, free_found = false;

	if ((!info->cache || !info->object) && !info->bug_type) {
		info->bug_type = get_common_bug_type(info);
		return;
	}

	write_lock_irqsave(&stack_ring.lock, flags);

	pos = atomic64_read(&stack_ring.pos);

	 

	for (u64 i = pos - 1; i != pos - 1 - stack_ring.size; i--) {
		if (alloc_found && free_found)
			break;

		entry = &stack_ring.entries[i % stack_ring.size];

		 
		ptr = (void *)smp_load_acquire(&entry->ptr);

		if (kasan_reset_tag(ptr) != info->object ||
		    get_tag(ptr) != get_tag(info->access_addr))
			continue;

		pid = READ_ONCE(entry->pid);
		stack = READ_ONCE(entry->stack);
		is_free = READ_ONCE(entry->is_free);

		if (is_free) {
			 
			if (free_found)
				break;

			info->free_track.pid = pid;
			info->free_track.stack = stack;
			free_found = true;

			 
			if (!info->bug_type)
				info->bug_type = "slab-use-after-free";
		} else {
			 
			if (alloc_found)
				break;

			info->alloc_track.pid = pid;
			info->alloc_track.stack = stack;
			alloc_found = true;

			 
			if (!info->bug_type)
				info->bug_type = "slab-out-of-bounds";
		}
	}

	write_unlock_irqrestore(&stack_ring.lock, flags);

	 
	if (!info->bug_type)
		info->bug_type = get_common_bug_type(info);
}
