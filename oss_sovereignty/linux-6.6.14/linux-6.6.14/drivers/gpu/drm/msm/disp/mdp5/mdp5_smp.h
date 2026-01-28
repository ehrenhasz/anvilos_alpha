#ifndef __MDP5_SMP_H__
#define __MDP5_SMP_H__
#include <drm/drm_print.h>
#include "msm_drv.h"
struct mdp5_smp_state {
	mdp5_smp_state_t state;
	mdp5_smp_state_t client_state[MAX_CLIENTS];
	unsigned long assigned;
	unsigned long released;
};
struct mdp5_kms;
struct mdp5_smp;
struct mdp5_smp *mdp5_smp_init(struct mdp5_kms *mdp5_kms,
		const struct mdp5_smp_block *cfg);
void  mdp5_smp_destroy(struct mdp5_smp *smp);
void mdp5_smp_dump(struct mdp5_smp *smp, struct drm_printer *p);
uint32_t mdp5_smp_calculate(struct mdp5_smp *smp,
		const struct mdp_format *format,
		u32 width, bool hdecim);
int mdp5_smp_assign(struct mdp5_smp *smp, struct mdp5_smp_state *state,
		enum mdp5_pipe pipe, uint32_t blkcfg);
void mdp5_smp_release(struct mdp5_smp *smp, struct mdp5_smp_state *state,
		enum mdp5_pipe pipe);
void mdp5_smp_prepare_commit(struct mdp5_smp *smp, struct mdp5_smp_state *state);
void mdp5_smp_complete_commit(struct mdp5_smp *smp, struct mdp5_smp_state *state);
#endif  
