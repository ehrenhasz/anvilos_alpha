 
 

#ifndef _ZYNQMP_DP_H_
#define _ZYNQMP_DP_H_

struct platform_device;
struct zynqmp_dp;
struct zynqmp_dpsub;

void zynqmp_dp_enable_vblank(struct zynqmp_dp *dp);
void zynqmp_dp_disable_vblank(struct zynqmp_dp *dp);

int zynqmp_dp_probe(struct zynqmp_dpsub *dpsub);
void zynqmp_dp_remove(struct zynqmp_dpsub *dpsub);

#endif  
