#ifndef __INTEL_PXP_CMD_H__
#define __INTEL_PXP_CMD_H__
#include <linux/types.h>
struct intel_pxp;
int intel_pxp_terminate_session(struct intel_pxp *pxp, u32 idx);
#endif  
