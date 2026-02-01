 

#ifndef __I915_SELFTESTS_RANDOM_H__
#define __I915_SELFTESTS_RANDOM_H__

#include <linux/math64.h>
#include <linux/random.h>

#include "../i915_selftest.h"

#define I915_RND_STATE_INITIALIZER(x) ({				\
	struct rnd_state state__;					\
	prandom_seed_state(&state__, (x));				\
	state__;							\
})

#define I915_RND_STATE(name__) \
	struct rnd_state name__ = I915_RND_STATE_INITIALIZER(i915_selftest.random_seed)

#define I915_RND_SUBSTATE(name__, parent__) \
	struct rnd_state name__ = I915_RND_STATE_INITIALIZER(prandom_u32_state(&(parent__)))

u64 i915_prandom_u64_state(struct rnd_state *rnd);

static inline u32 i915_prandom_u32_max_state(u32 ep_ro, struct rnd_state *state)
{
	return upper_32_bits(mul_u32_u32(prandom_u32_state(state), ep_ro));
}

unsigned int *i915_random_order(unsigned int count,
				struct rnd_state *state);
void i915_random_reorder(unsigned int *order,
			 unsigned int count,
			 struct rnd_state *state);

void i915_prandom_shuffle(void *arr, size_t elsz, size_t count,
			  struct rnd_state *state);

u64 igt_random_offset(struct rnd_state *state,
		      u64 start, u64 end,
		      u64 len, u64 align);

#endif  
