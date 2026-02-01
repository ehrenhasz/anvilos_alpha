 
 

#ifndef __MDP5_CTL_H__
#define __MDP5_CTL_H__

#include "msm_drv.h"

 
struct mdp5_ctl_manager;
struct mdp5_ctl_manager *mdp5_ctlm_init(struct drm_device *dev,
		void __iomem *mmio_base, struct mdp5_cfg_handler *cfg_hnd);
void mdp5_ctlm_hw_reset(struct mdp5_ctl_manager *ctlm);
void mdp5_ctlm_destroy(struct mdp5_ctl_manager *ctlm);

 
struct mdp5_ctl *mdp5_ctlm_request(struct mdp5_ctl_manager *ctlm, int intf_num);

int mdp5_ctl_get_ctl_id(struct mdp5_ctl *ctl);

struct mdp5_interface;
struct mdp5_pipeline;
int mdp5_ctl_set_pipeline(struct mdp5_ctl *ctl, struct mdp5_pipeline *p);
int mdp5_ctl_set_encoder_state(struct mdp5_ctl *ctl, struct mdp5_pipeline *p,
			       bool enabled);

int mdp5_ctl_set_cursor(struct mdp5_ctl *ctl, struct mdp5_pipeline *pipeline,
			int cursor_id, bool enable);
int mdp5_ctl_pair(struct mdp5_ctl *ctlx, struct mdp5_ctl *ctly, bool enable);

#define MAX_PIPE_STAGE		2

 
#define MDP5_CTL_BLEND_OP_FLAG_BORDER_OUT	BIT(0)
int mdp5_ctl_blend(struct mdp5_ctl *ctl, struct mdp5_pipeline *pipeline,
		   enum mdp5_pipe stage[][MAX_PIPE_STAGE],
		   enum mdp5_pipe r_stage[][MAX_PIPE_STAGE],
		   u32 stage_cnt, u32 ctl_blend_op_flags);

 
u32 mdp_ctl_flush_mask_lm(int lm);
u32 mdp_ctl_flush_mask_pipe(enum mdp5_pipe pipe);
u32 mdp_ctl_flush_mask_cursor(int cursor_id);
u32 mdp_ctl_flush_mask_encoder(struct mdp5_interface *intf);

 
u32 mdp5_ctl_commit(struct mdp5_ctl *ctl, struct mdp5_pipeline *pipeline,
		    u32 flush_mask, bool start);
u32 mdp5_ctl_get_commit_status(struct mdp5_ctl *ctl);



#endif  
