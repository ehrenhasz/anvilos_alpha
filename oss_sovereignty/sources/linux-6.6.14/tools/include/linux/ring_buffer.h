#ifndef _TOOLS_LINUX_RING_BUFFER_H_
#define _TOOLS_LINUX_RING_BUFFER_H_

#include <asm/barrier.h>
#include <linux/perf_event.h>



static inline u64 ring_buffer_read_head(struct perf_event_mmap_page *base)
{

#if defined(__x86_64__) || defined(__aarch64__) || defined(__powerpc64__) || \
    defined(__ia64__) || defined(__sparc__) && defined(__arch64__)
	return smp_load_acquire(&base->data_head);
#else
	u64 head = READ_ONCE(base->data_head);

	smp_rmb();
	return head;
#endif
}

static inline void ring_buffer_write_tail(struct perf_event_mmap_page *base,
					  u64 tail)
{
	smp_store_release(&base->data_tail, tail);
}

#endif 
