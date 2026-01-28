#ifndef __ASM_MMU_H
#define __ASM_MMU_H
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
typedef struct {
	union {
		u64 asid[NR_CPUS];
		atomic64_t mmid;
	};
	void *vdso;
	spinlock_t bd_emupage_lock;
	unsigned long *bd_emupage_allocmap;
	wait_queue_head_t bd_emupage_queue;
} mm_context_t;
#endif  
