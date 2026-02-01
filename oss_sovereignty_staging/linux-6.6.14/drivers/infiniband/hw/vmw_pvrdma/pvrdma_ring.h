 

#ifndef __PVRDMA_RING_H__
#define __PVRDMA_RING_H__

#include <linux/types.h>

#define PVRDMA_INVALID_IDX	-1	 

struct pvrdma_ring {
	atomic_t prod_tail;	 
	atomic_t cons_head;	 
};

struct pvrdma_ring_state {
	struct pvrdma_ring tx;	 
	struct pvrdma_ring rx;	 
};

static inline int pvrdma_idx_valid(__u32 idx, __u32 max_elems)
{
	 
	return (idx & ~((max_elems << 1) - 1)) == 0;
}

static inline __s32 pvrdma_idx(atomic_t *var, __u32 max_elems)
{
	const unsigned int idx = atomic_read(var);

	if (pvrdma_idx_valid(idx, max_elems))
		return idx & (max_elems - 1);
	return PVRDMA_INVALID_IDX;
}

static inline void pvrdma_idx_ring_inc(atomic_t *var, __u32 max_elems)
{
	__u32 idx = atomic_read(var) + 1;	 

	idx &= (max_elems << 1) - 1;		 
	atomic_set(var, idx);
}

static inline __s32 pvrdma_idx_ring_has_space(const struct pvrdma_ring *r,
					      __u32 max_elems, __u32 *out_tail)
{
	const __u32 tail = atomic_read(&r->prod_tail);
	const __u32 head = atomic_read(&r->cons_head);

	if (pvrdma_idx_valid(tail, max_elems) &&
	    pvrdma_idx_valid(head, max_elems)) {
		*out_tail = tail & (max_elems - 1);
		return tail != (head ^ max_elems);
	}
	return PVRDMA_INVALID_IDX;
}

static inline __s32 pvrdma_idx_ring_has_data(const struct pvrdma_ring *r,
					     __u32 max_elems, __u32 *out_head)
{
	const __u32 tail = atomic_read(&r->prod_tail);
	const __u32 head = atomic_read(&r->cons_head);

	if (pvrdma_idx_valid(tail, max_elems) &&
	    pvrdma_idx_valid(head, max_elems)) {
		*out_head = head & (max_elems - 1);
		return tail != head;
	}
	return PVRDMA_INVALID_IDX;
}

#endif  
