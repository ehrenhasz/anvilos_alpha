 

#include <nvif/device.h>

u64
nvif_device_time(struct nvif_device *device)
{
	if (!device->user.func) {
		struct nv_device_time_v0 args = {};
		int ret = nvif_object_mthd(&device->object, NV_DEVICE_V0_TIME,
					   &args, sizeof(args));
		WARN_ON_ONCE(ret != 0);
		return args.time;
	}

	return device->user.func->time(&device->user);
}

void
nvif_device_dtor(struct nvif_device *device)
{
	nvif_user_dtor(device);
	kfree(device->runlist);
	device->runlist = NULL;
	nvif_object_dtor(&device->object);
}

int
nvif_device_ctor(struct nvif_object *parent, const char *name, u32 handle,
		 s32 oclass, void *data, u32 size, struct nvif_device *device)
{
	int ret = nvif_object_ctor(parent, name ? name : "nvifDevice", handle,
				   oclass, data, size, &device->object);
	device->runlist = NULL;
	device->user.func = NULL;
	if (ret == 0) {
		device->info.version = 0;
		ret = nvif_object_mthd(&device->object, NV_DEVICE_V0_INFO,
				       &device->info, sizeof(device->info));
	}
	return ret;
}
