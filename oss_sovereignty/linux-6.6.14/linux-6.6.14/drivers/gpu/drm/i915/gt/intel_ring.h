#ifndef INTEL_RING_H
#define INTEL_RING_H
#include "i915_gem.h"  
#include "i915_request.h"
#include "intel_ring_types.h"
struct intel_engine_cs;
struct intel_ring *
intel_engine_create_ring(struct intel_engine_cs *engine, int size);
u32 *intel_ring_begin(struct i915_request *rq, unsigned int num_dwords);
int intel_ring_cacheline_align(struct i915_request *rq);
unsigned int intel_ring_update_space(struct intel_ring *ring);
void __intel_ring_pin(struct intel_ring *ring);
int intel_ring_pin(struct intel_ring *ring, struct i915_gem_ww_ctx *ww);
void intel_ring_unpin(struct intel_ring *ring);
void intel_ring_reset(struct intel_ring *ring, u32 tail);
void intel_ring_free(struct kref *ref);
static inline struct intel_ring *intel_ring_get(struct intel_ring *ring)
{
	kref_get(&ring->ref);
	return ring;
}
static inline void intel_ring_put(struct intel_ring *ring)
{
	kref_put(&ring->ref, intel_ring_free);
}
static inline void intel_ring_advance(struct i915_request *rq, u32 *cs)
{
	GEM_BUG_ON((rq->ring->vaddr + rq->ring->emit) != cs);
	GEM_BUG_ON(!IS_ALIGNED(rq->ring->emit, 8));  
}
static inline u32 intel_ring_wrap(const struct intel_ring *ring, u32 pos)
{
	return pos & (ring->size - 1);
}
static inline int intel_ring_direction(const struct intel_ring *ring,
				       u32 next, u32 prev)
{
	typecheck(typeof(ring->size), next);
	typecheck(typeof(ring->size), prev);
	return (next - prev) << ring->wrap;
}
static inline bool
intel_ring_offset_valid(const struct intel_ring *ring,
			unsigned int pos)
{
	if (pos & -ring->size)  
		return false;
	if (!IS_ALIGNED(pos, 8))  
		return false;
	return true;
}
static inline u32 intel_ring_offset(const struct i915_request *rq, void *addr)
{
	u32 offset = addr - rq->ring->vaddr;
	GEM_BUG_ON(offset > rq->ring->size);
	return intel_ring_wrap(rq->ring, offset);
}
static inline void
assert_ring_tail_valid(const struct intel_ring *ring, unsigned int tail)
{
	unsigned int head = READ_ONCE(ring->head);
	GEM_BUG_ON(!intel_ring_offset_valid(ring, tail));
#define cacheline(a) round_down(a, CACHELINE_BYTES)
	GEM_BUG_ON(cacheline(tail) == cacheline(head) && tail < head);
#undef cacheline
}
static inline unsigned int
intel_ring_set_tail(struct intel_ring *ring, unsigned int tail)
{
	assert_ring_tail_valid(ring, tail);
	ring->tail = tail;
	return tail;
}
static inline unsigned int
__intel_ring_space(unsigned int head, unsigned int tail, unsigned int size)
{
	GEM_BUG_ON(!is_power_of_2(size));
	return (head - tail - CACHELINE_BYTES) & (size - 1);
}
#endif  
