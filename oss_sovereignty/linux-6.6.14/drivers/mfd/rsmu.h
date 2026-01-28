#ifndef __RSMU_MFD_H
#define __RSMU_MFD_H
#include <linux/mfd/rsmu.h>
#define RSMU_CM_SCSR_BASE		0x20100000
int rsmu_core_init(struct rsmu_ddata *rsmu);
void rsmu_core_exit(struct rsmu_ddata *rsmu);
#endif  
