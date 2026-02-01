 
#include <nvif/fifo.h>

static int
nvif_fifo_runlists(struct nvif_device *device)
{
	struct nvif_object *object = &device->object;
	struct {
		struct nv_device_info_v1 m;
		struct {
			struct nv_device_info_v1_data runlists;
			struct nv_device_info_v1_data runlist[64];
		} v;
	} *a;
	int ret, i;

	if (device->runlist)
		return 0;

	if (!(a = kmalloc(sizeof(*a), GFP_KERNEL)))
		return -ENOMEM;
	a->m.version = 1;
	a->m.count = sizeof(a->v) / sizeof(a->v.runlists);
	a->v.runlists.mthd = NV_DEVICE_HOST_RUNLISTS;
	for (i = 0; i < ARRAY_SIZE(a->v.runlist); i++) {
		a->v.runlist[i].mthd = NV_DEVICE_HOST_RUNLIST_ENGINES;
		a->v.runlist[i].data = i;
	}

	ret = nvif_object_mthd(object, NV_DEVICE_V0_INFO, a, sizeof(*a));
	if (ret)
		goto done;

	device->runlists = fls64(a->v.runlists.data);
	device->runlist = kcalloc(device->runlists, sizeof(*device->runlist),
				  GFP_KERNEL);
	if (!device->runlist) {
		ret = -ENOMEM;
		goto done;
	}

	for (i = 0; i < device->runlists; i++) {
		if (a->v.runlist[i].mthd != NV_DEVICE_INFO_INVALID)
			device->runlist[i].engines = a->v.runlist[i].data;
	}

done:
	kfree(a);
	return ret;
}

u64
nvif_fifo_runlist(struct nvif_device *device, u64 engine)
{
	u64 runm = 0;
	int ret, i;

	if ((ret = nvif_fifo_runlists(device)))
		return runm;

	for (i = 0; i < device->runlists; i++) {
		if (device->runlist[i].engines & engine)
			runm |= BIT_ULL(i);
	}

	return runm;
}
