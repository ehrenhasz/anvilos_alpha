#ifndef INTEL_RING_TYPES_H
#define INTEL_RING_TYPES_H
#include <linux/atomic.h>
#include <linux/kref.h>
#include <linux/types.h>
#define CACHELINE_BYTES 64
#define CACHELINE_DWORDS (CACHELINE_BYTES / sizeof(u32))
struct i915_vma;
struct intel_ring {
	struct kref ref;
	struct i915_vma *vma;
	void *vaddr;
	atomic_t pin_count;
	u32 head;  
	u32 tail;  
	u32 emit;  
	u32 space;
	u32 size;
	u32 wrap;
	u32 effective_size;
};
#endif  
