
 

#include <asm/page.h>
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kmsan_types.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/mmzone.h>
#include <linux/percpu-defs.h>
#include <linux/preempt.h>
#include <linux/slab.h>
#include <linux/stackdepot.h>
#include <linux/stacktrace.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "../slab.h"
#include "kmsan.h"

bool kmsan_enabled __read_mostly;

 
DEFINE_PER_CPU(struct kmsan_ctx, kmsan_percpu_ctx);

void kmsan_internal_task_create(struct task_struct *task)
{
	struct kmsan_ctx *ctx = &task->kmsan_ctx;
	struct thread_info *info = current_thread_info();

	__memset(ctx, 0, sizeof(*ctx));
	ctx->allow_reporting = true;
	kmsan_internal_unpoison_memory(info, sizeof(*info), false);
}

void kmsan_internal_poison_memory(void *address, size_t size, gfp_t flags,
				  unsigned int poison_flags)
{
	u32 extra_bits =
		kmsan_extra_bits(  0, poison_flags & KMSAN_POISON_FREE);
	bool checked = poison_flags & KMSAN_POISON_CHECK;
	depot_stack_handle_t handle;

	handle = kmsan_save_stack_with_flags(flags, extra_bits);
	kmsan_internal_set_shadow_origin(address, size, -1, handle, checked);
}

void kmsan_internal_unpoison_memory(void *address, size_t size, bool checked)
{
	kmsan_internal_set_shadow_origin(address, size, 0, 0, checked);
}

depot_stack_handle_t kmsan_save_stack_with_flags(gfp_t flags,
						 unsigned int extra)
{
	unsigned long entries[KMSAN_STACK_DEPTH];
	unsigned int nr_entries;
	depot_stack_handle_t handle;

	nr_entries = stack_trace_save(entries, KMSAN_STACK_DEPTH, 0);

	 
	flags &= ~(__GFP_DIRECT_RECLAIM | __GFP_KSWAPD_RECLAIM);

	handle = __stack_depot_save(entries, nr_entries, flags, true);
	return stack_depot_set_extra_bits(handle, extra);
}

 
void kmsan_internal_memmove_metadata(void *dst, void *src, size_t n)
{
	depot_stack_handle_t old_origin = 0, new_origin = 0;
	int src_slots, dst_slots, i, iter, step, skip_bits;
	depot_stack_handle_t *origin_src, *origin_dst;
	void *shadow_src, *shadow_dst;
	u32 *align_shadow_src, shadow;
	bool backwards;

	shadow_dst = kmsan_get_metadata(dst, KMSAN_META_SHADOW);
	if (!shadow_dst)
		return;
	KMSAN_WARN_ON(!kmsan_metadata_is_contiguous(dst, n));

	shadow_src = kmsan_get_metadata(src, KMSAN_META_SHADOW);
	if (!shadow_src) {
		 
		__memset(shadow_dst, 0, n);
		return;
	}
	KMSAN_WARN_ON(!kmsan_metadata_is_contiguous(src, n));

	__memmove(shadow_dst, shadow_src, n);

	origin_dst = kmsan_get_metadata(dst, KMSAN_META_ORIGIN);
	origin_src = kmsan_get_metadata(src, KMSAN_META_ORIGIN);
	KMSAN_WARN_ON(!origin_dst || !origin_src);
	src_slots = (ALIGN((u64)src + n, KMSAN_ORIGIN_SIZE) -
		     ALIGN_DOWN((u64)src, KMSAN_ORIGIN_SIZE)) /
		    KMSAN_ORIGIN_SIZE;
	dst_slots = (ALIGN((u64)dst + n, KMSAN_ORIGIN_SIZE) -
		     ALIGN_DOWN((u64)dst, KMSAN_ORIGIN_SIZE)) /
		    KMSAN_ORIGIN_SIZE;
	KMSAN_WARN_ON((src_slots < 1) || (dst_slots < 1));
	KMSAN_WARN_ON((src_slots - dst_slots > 1) ||
		      (dst_slots - src_slots < -1));

	backwards = dst > src;
	i = backwards ? min(src_slots, dst_slots) - 1 : 0;
	iter = backwards ? -1 : 1;

	align_shadow_src =
		(u32 *)ALIGN_DOWN((u64)shadow_src, KMSAN_ORIGIN_SIZE);
	for (step = 0; step < min(src_slots, dst_slots); step++, i += iter) {
		KMSAN_WARN_ON(i < 0);
		shadow = align_shadow_src[i];
		if (i == 0) {
			 
			skip_bits = ((u64)src % KMSAN_ORIGIN_SIZE) * 8;
			shadow = (shadow >> skip_bits) << skip_bits;
		}
		if (i == src_slots - 1) {
			 
			skip_bits = (((u64)src + n) % KMSAN_ORIGIN_SIZE) * 8;
			shadow = (shadow << skip_bits) >> skip_bits;
		}
		 
		if (origin_src[i] && (origin_src[i] != old_origin) && shadow) {
			old_origin = origin_src[i];
			new_origin = kmsan_internal_chain_origin(old_origin);
			 
			if (!new_origin)
				new_origin = old_origin;
		}
		if (shadow)
			origin_dst[i] = new_origin;
		else
			origin_dst[i] = 0;
	}
	 
	if (src_slots < dst_slots) {
		if (backwards) {
			shadow = align_shadow_src[src_slots - 1];
			skip_bits = (((u64)dst + n) % KMSAN_ORIGIN_SIZE) * 8;
			shadow = (shadow << skip_bits) >> skip_bits;
			if (shadow)
				 
				origin_dst[dst_slots - 1] =
					origin_dst[dst_slots - 2];
		} else {
			shadow = align_shadow_src[0];
			skip_bits = ((u64)dst % KMSAN_ORIGIN_SIZE) * 8;
			shadow = (shadow >> skip_bits) << skip_bits;
			if (shadow)
				origin_dst[0] = origin_dst[1];
		}
	}
}

depot_stack_handle_t kmsan_internal_chain_origin(depot_stack_handle_t id)
{
	unsigned long entries[3];
	u32 extra_bits;
	int depth;
	bool uaf;
	depot_stack_handle_t handle;

	if (!id)
		return id;
	 
	BUILD_BUG_ON(
		(1 << STACK_DEPOT_EXTRA_BITS) <= (KMSAN_MAX_ORIGIN_DEPTH << 1));

	extra_bits = stack_depot_get_extra_bits(id);
	depth = kmsan_depth_from_eb(extra_bits);
	uaf = kmsan_uaf_from_eb(extra_bits);

	 
	if (depth == KMSAN_MAX_ORIGIN_DEPTH)
		return id;

	depth++;
	extra_bits = kmsan_extra_bits(depth, uaf);

	entries[0] = KMSAN_CHAIN_MAGIC_ORIGIN;
	entries[1] = kmsan_save_stack_with_flags(__GFP_HIGH, 0);
	entries[2] = id;
	 
	kmsan_internal_unpoison_memory(entries, sizeof(entries), false);
	handle = __stack_depot_save(entries, ARRAY_SIZE(entries), __GFP_HIGH,
				    true);
	return stack_depot_set_extra_bits(handle, extra_bits);
}

void kmsan_internal_set_shadow_origin(void *addr, size_t size, int b,
				      u32 origin, bool checked)
{
	u64 address = (u64)addr;
	void *shadow_start;
	u32 *origin_start;
	size_t pad = 0;

	KMSAN_WARN_ON(!kmsan_metadata_is_contiguous(addr, size));
	shadow_start = kmsan_get_metadata(addr, KMSAN_META_SHADOW);
	if (!shadow_start) {
		 
		if (checked) {
			pr_err("%s: not memsetting %ld bytes starting at %px, because the shadow is NULL\n",
			       __func__, size, addr);
			KMSAN_WARN_ON(true);
		}
		return;
	}
	__memset(shadow_start, b, size);

	if (!IS_ALIGNED(address, KMSAN_ORIGIN_SIZE)) {
		pad = address % KMSAN_ORIGIN_SIZE;
		address -= pad;
		size += pad;
	}
	size = ALIGN(size, KMSAN_ORIGIN_SIZE);
	origin_start =
		(u32 *)kmsan_get_metadata((void *)address, KMSAN_META_ORIGIN);

	for (int i = 0; i < size / KMSAN_ORIGIN_SIZE; i++)
		origin_start[i] = origin;
}

struct page *kmsan_vmalloc_to_page_or_null(void *vaddr)
{
	struct page *page;

	if (!kmsan_internal_is_vmalloc_addr(vaddr) &&
	    !kmsan_internal_is_module_addr(vaddr))
		return NULL;
	page = vmalloc_to_page(vaddr);
	if (pfn_valid(page_to_pfn(page)))
		return page;
	else
		return NULL;
}

void kmsan_internal_check_memory(void *addr, size_t size, const void *user_addr,
				 int reason)
{
	depot_stack_handle_t cur_origin = 0, new_origin = 0;
	unsigned long addr64 = (unsigned long)addr;
	depot_stack_handle_t *origin = NULL;
	unsigned char *shadow = NULL;
	int cur_off_start = -1;
	int chunk_size;
	size_t pos = 0;

	if (!size)
		return;
	KMSAN_WARN_ON(!kmsan_metadata_is_contiguous(addr, size));
	while (pos < size) {
		chunk_size = min(size - pos,
				 PAGE_SIZE - ((addr64 + pos) % PAGE_SIZE));
		shadow = kmsan_get_metadata((void *)(addr64 + pos),
					    KMSAN_META_SHADOW);
		if (!shadow) {
			 
			if (cur_origin) {
				kmsan_enter_runtime();
				kmsan_report(cur_origin, addr, size,
					     cur_off_start, pos - 1, user_addr,
					     reason);
				kmsan_leave_runtime();
			}
			cur_origin = 0;
			cur_off_start = -1;
			pos += chunk_size;
			continue;
		}
		for (int i = 0; i < chunk_size; i++) {
			if (!shadow[i]) {
				 
				if (cur_origin) {
					kmsan_enter_runtime();
					kmsan_report(cur_origin, addr, size,
						     cur_off_start, pos + i - 1,
						     user_addr, reason);
					kmsan_leave_runtime();
				}
				cur_origin = 0;
				cur_off_start = -1;
				continue;
			}
			origin = kmsan_get_metadata((void *)(addr64 + pos + i),
						    KMSAN_META_ORIGIN);
			KMSAN_WARN_ON(!origin);
			new_origin = *origin;
			 
			if (cur_origin != new_origin) {
				if (cur_origin) {
					kmsan_enter_runtime();
					kmsan_report(cur_origin, addr, size,
						     cur_off_start, pos + i - 1,
						     user_addr, reason);
					kmsan_leave_runtime();
				}
				cur_origin = new_origin;
				cur_off_start = pos + i;
			}
		}
		pos += chunk_size;
	}
	KMSAN_WARN_ON(pos != size);
	if (cur_origin) {
		kmsan_enter_runtime();
		kmsan_report(cur_origin, addr, size, cur_off_start, pos - 1,
			     user_addr, reason);
		kmsan_leave_runtime();
	}
}

bool kmsan_metadata_is_contiguous(void *addr, size_t size)
{
	char *cur_shadow = NULL, *next_shadow = NULL, *cur_origin = NULL,
	     *next_origin = NULL;
	u64 cur_addr = (u64)addr, next_addr = cur_addr + PAGE_SIZE;
	depot_stack_handle_t *origin_p;
	bool all_untracked = false;

	if (!size)
		return true;

	 
	if (ALIGN_DOWN(cur_addr + size - 1, PAGE_SIZE) ==
	    ALIGN_DOWN(cur_addr, PAGE_SIZE))
		return true;

	cur_shadow = kmsan_get_metadata((void *)cur_addr,   false);
	if (!cur_shadow)
		all_untracked = true;
	cur_origin = kmsan_get_metadata((void *)cur_addr,   true);
	if (all_untracked && cur_origin)
		goto report;

	for (; next_addr < (u64)addr + size;
	     cur_addr = next_addr, cur_shadow = next_shadow,
	     cur_origin = next_origin, next_addr += PAGE_SIZE) {
		next_shadow = kmsan_get_metadata((void *)next_addr, false);
		next_origin = kmsan_get_metadata((void *)next_addr, true);
		if (all_untracked) {
			if (next_shadow || next_origin)
				goto report;
			if (!next_shadow && !next_origin)
				continue;
		}
		if (((u64)cur_shadow == ((u64)next_shadow - PAGE_SIZE)) &&
		    ((u64)cur_origin == ((u64)next_origin - PAGE_SIZE)))
			continue;
		goto report;
	}
	return true;

report:
	pr_err("%s: attempting to access two shadow page ranges.\n", __func__);
	pr_err("Access of size %ld at %px.\n", size, addr);
	pr_err("Addresses belonging to different ranges: %px and %px\n",
	       (void *)cur_addr, (void *)next_addr);
	pr_err("page[0].shadow: %px, page[1].shadow: %px\n", cur_shadow,
	       next_shadow);
	pr_err("page[0].origin: %px, page[1].origin: %px\n", cur_origin,
	       next_origin);
	origin_p = kmsan_get_metadata(addr, KMSAN_META_ORIGIN);
	if (origin_p) {
		pr_err("Origin: %08x\n", *origin_p);
		kmsan_print_origin(*origin_p);
	} else {
		pr_err("Origin: unavailable\n");
	}
	return false;
}
