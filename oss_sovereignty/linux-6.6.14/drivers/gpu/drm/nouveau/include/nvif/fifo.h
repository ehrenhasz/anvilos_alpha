#ifndef __NVIF_FIFO_H__
#define __NVIF_FIFO_H__
#include <nvif/device.h>

 
u64 nvif_fifo_runlist(struct nvif_device *, u64 engine);

 
static inline u64
nvif_fifo_runlist_ce(struct nvif_device *device)
{
	u64 runmgr = nvif_fifo_runlist(device, NV_DEVICE_HOST_RUNLIST_ENGINES_GR);
	u64 runmce = nvif_fifo_runlist(device, NV_DEVICE_HOST_RUNLIST_ENGINES_CE);
	if (runmce && !(runmce &= ~runmgr))
		runmce = runmgr;
	return runmce;
}
#endif
