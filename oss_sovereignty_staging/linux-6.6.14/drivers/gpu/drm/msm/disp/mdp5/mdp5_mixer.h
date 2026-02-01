 
 

#ifndef __MDP5_LM_H__
#define __MDP5_LM_H__

 
struct mdp5_hw_mixer {
	int idx;

	const char *name;

	int lm;			 
	uint32_t caps;
	int pp;
	int dspp;

	uint32_t flush_mask;       
};

 
struct mdp5_hw_mixer_state {
	struct drm_crtc *hwmixer_to_crtc[8];
};

struct mdp5_hw_mixer *mdp5_mixer_init(const struct mdp5_lm_instance *lm);
void mdp5_mixer_destroy(struct mdp5_hw_mixer *lm);
int mdp5_mixer_assign(struct drm_atomic_state *s, struct drm_crtc *crtc,
		      uint32_t caps, struct mdp5_hw_mixer **mixer,
		      struct mdp5_hw_mixer **r_mixer);
int mdp5_mixer_release(struct drm_atomic_state *s,
		       struct mdp5_hw_mixer *mixer);

#endif  
