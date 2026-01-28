#ifndef __DRM_RANDOM_H__
#define __DRM_RANDOM_H__
#include <linux/random.h>
#define DRM_RND_STATE_INITIALIZER(seed__) ({				\
	struct rnd_state state__;					\
	prandom_seed_state(&state__, (seed__));				\
	state__;							\
})
#define DRM_RND_STATE(name__, seed__) \
	struct rnd_state name__ = DRM_RND_STATE_INITIALIZER(seed__)
unsigned int *drm_random_order(unsigned int count,
			       struct rnd_state *state);
void drm_random_reorder(unsigned int *order,
			unsigned int count,
			struct rnd_state *state);
u32 drm_prandom_u32_max_state(u32 ep_ro,
			      struct rnd_state *state);
#endif  
