 

#ifndef _ACP_GFX_IF_H
#define _ACP_GFX_IF_H

#include <linux/types.h>
#include "cgs_common.h"

int amd_acp_hw_init(struct cgs_device *cgs_device,
		    unsigned acp_version_major, unsigned acp_version_minor);

#endif  
