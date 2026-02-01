 
 

#ifndef __RCAR_DU_GROUP_H__
#define __RCAR_DU_GROUP_H__

#include <linux/mutex.h>

#include "rcar_du_plane.h"

struct rcar_du_device;

 
struct rcar_du_group {
	struct rcar_du_device *dev;
	unsigned int mmio_offset;
	unsigned int index;

	unsigned int channels_mask;
	unsigned int cmms_mask;
	unsigned int num_crtcs;
	unsigned int use_count;
	unsigned int used_crtcs;

	struct mutex lock;
	unsigned int dptsr_planes;

	unsigned int num_planes;
	struct rcar_du_plane planes[RCAR_DU_NUM_KMS_PLANES];
	bool need_restart;
};

u32 rcar_du_group_read(struct rcar_du_group *rgrp, u32 reg);
void rcar_du_group_write(struct rcar_du_group *rgrp, u32 reg, u32 data);

int rcar_du_group_get(struct rcar_du_group *rgrp);
void rcar_du_group_put(struct rcar_du_group *rgrp);
void rcar_du_group_start_stop(struct rcar_du_group *rgrp, bool start);
void rcar_du_group_restart(struct rcar_du_group *rgrp);
int rcar_du_group_set_routing(struct rcar_du_group *rgrp);

int rcar_du_set_dpad0_vsp1_routing(struct rcar_du_device *rcdu);

#endif  
