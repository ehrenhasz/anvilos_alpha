 
#include <nvif/user.h>
#include <nvif/device.h>

#include <nvif/class.h>

void
nvif_user_dtor(struct nvif_device *device)
{
	if (device->user.func) {
		nvif_object_dtor(&device->user.object);
		device->user.func = NULL;
	}
}

int
nvif_user_ctor(struct nvif_device *device, const char *name)
{
	struct {
		s32 oclass;
		int version;
		const struct nvif_user_func *func;
	} users[] = {
		{ AMPERE_USERMODE_A, -1, &nvif_userc361 },
		{ TURING_USERMODE_A, -1, &nvif_userc361 },
		{  VOLTA_USERMODE_A, -1, &nvif_userc361 },
		{}
	};
	int cid, ret;

	if (device->user.func)
		return 0;

	cid = nvif_mclass(&device->object, users);
	if (cid < 0)
		return cid;

	ret = nvif_object_ctor(&device->object, name ? name : "nvifUsermode",
			       0, users[cid].oclass, NULL, 0,
			       &device->user.object);
	if (ret)
		return ret;

	nvif_object_map(&device->user.object, NULL, 0);
	device->user.func = users[cid].func;
	return 0;
}
