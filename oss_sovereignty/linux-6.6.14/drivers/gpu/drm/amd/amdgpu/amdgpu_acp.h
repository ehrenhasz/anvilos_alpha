 

#ifndef __AMDGPU_ACP_H__
#define __AMDGPU_ACP_H__

#include <linux/mfd/core.h>

struct amdgpu_acp {
	struct device *parent;
	struct cgs_device *cgs_device;
	struct amd_acp_private *private;
	struct mfd_cell *acp_cell;
	struct resource *acp_res;
	struct acp_pm_domain *acp_genpd;
};

extern const struct amdgpu_ip_block_version acp_ip_block;

#endif  
