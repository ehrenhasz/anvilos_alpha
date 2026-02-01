
 

#include "mdp5_kms.h"

int mdp5_pipe_assign(struct drm_atomic_state *s, struct drm_plane *plane,
		     uint32_t caps, uint32_t blkcfg,
		     struct mdp5_hw_pipe **hwpipe,
		     struct mdp5_hw_pipe **r_hwpipe)
{
	struct msm_drm_private *priv = s->dev->dev_private;
	struct mdp5_kms *mdp5_kms = to_mdp5_kms(to_mdp_kms(priv->kms));
	struct mdp5_global_state *new_global_state, *old_global_state;
	struct mdp5_hw_pipe_state *old_state, *new_state;
	int i, j;

	new_global_state = mdp5_get_global_state(s);
	if (IS_ERR(new_global_state))
		return PTR_ERR(new_global_state);

	 
	old_global_state = mdp5_get_existing_global_state(mdp5_kms);

	old_state = &old_global_state->hwpipe;
	new_state = &new_global_state->hwpipe;

	for (i = 0; i < mdp5_kms->num_hwpipes; i++) {
		struct mdp5_hw_pipe *cur = mdp5_kms->hwpipes[i];

		 
		if (new_state->hwpipe_to_plane[cur->idx] ||
				old_state->hwpipe_to_plane[cur->idx])
			continue;

		 
		if (caps & ~cur->caps)
			continue;

		 
		if (cur->caps & MDP_PIPE_CAP_CURSOR &&
				plane->type != DRM_PLANE_TYPE_CURSOR)
			continue;

		 
		if (!(*hwpipe) || (hweight_long(cur->caps & ~caps) <
				   hweight_long((*hwpipe)->caps & ~caps))) {
			bool r_found = false;

			if (r_hwpipe) {
				for (j = i + 1; j < mdp5_kms->num_hwpipes;
				     j++) {
					struct mdp5_hw_pipe *r_cur =
							mdp5_kms->hwpipes[j];

					 
					if (r_cur->caps != cur->caps)
						continue;

					 
					if (cur->pipe > r_cur->pipe)
						continue;

					*r_hwpipe = r_cur;
					r_found = true;
					break;
				}
			}

			if (!r_hwpipe || r_found)
				*hwpipe = cur;
		}
	}

	if (!(*hwpipe))
		return -ENOMEM;

	if (r_hwpipe && !(*r_hwpipe))
		return -ENOMEM;

	if (mdp5_kms->smp) {
		int ret;

		 
		WARN_ON(r_hwpipe);

		DBG("%s: alloc SMP blocks", (*hwpipe)->name);
		ret = mdp5_smp_assign(mdp5_kms->smp, &new_global_state->smp,
				(*hwpipe)->pipe, blkcfg);
		if (ret)
			return -ENOMEM;

		(*hwpipe)->blkcfg = blkcfg;
	}

	DBG("%s: assign to plane %s for caps %x",
			(*hwpipe)->name, plane->name, caps);
	new_state->hwpipe_to_plane[(*hwpipe)->idx] = plane;

	if (r_hwpipe) {
		DBG("%s: assign to right of plane %s for caps %x",
		    (*r_hwpipe)->name, plane->name, caps);
		new_state->hwpipe_to_plane[(*r_hwpipe)->idx] = plane;
	}

	return 0;
}

int mdp5_pipe_release(struct drm_atomic_state *s, struct mdp5_hw_pipe *hwpipe)
{
	struct msm_drm_private *priv = s->dev->dev_private;
	struct mdp5_kms *mdp5_kms = to_mdp5_kms(to_mdp_kms(priv->kms));
	struct mdp5_global_state *state;
	struct mdp5_hw_pipe_state *new_state;

	if (!hwpipe)
		return 0;

	state = mdp5_get_global_state(s);
	if (IS_ERR(state))
		return PTR_ERR(state);

	new_state = &state->hwpipe;

	if (WARN_ON(!new_state->hwpipe_to_plane[hwpipe->idx]))
		return -EINVAL;

	DBG("%s: release from plane %s", hwpipe->name,
		new_state->hwpipe_to_plane[hwpipe->idx]->name);

	if (mdp5_kms->smp) {
		DBG("%s: free SMP blocks", hwpipe->name);
		mdp5_smp_release(mdp5_kms->smp, &state->smp, hwpipe->pipe);
	}

	new_state->hwpipe_to_plane[hwpipe->idx] = NULL;

	return 0;
}

void mdp5_pipe_destroy(struct mdp5_hw_pipe *hwpipe)
{
	kfree(hwpipe);
}

struct mdp5_hw_pipe *mdp5_pipe_init(enum mdp5_pipe pipe,
		uint32_t reg_offset, uint32_t caps)
{
	struct mdp5_hw_pipe *hwpipe;

	hwpipe = kzalloc(sizeof(*hwpipe), GFP_KERNEL);
	if (!hwpipe)
		return ERR_PTR(-ENOMEM);

	hwpipe->name = pipe2name(pipe);
	hwpipe->pipe = pipe;
	hwpipe->reg_offset = reg_offset;
	hwpipe->caps = caps;
	hwpipe->flush_mask = mdp_ctl_flush_mask_pipe(pipe);

	return hwpipe;
}
