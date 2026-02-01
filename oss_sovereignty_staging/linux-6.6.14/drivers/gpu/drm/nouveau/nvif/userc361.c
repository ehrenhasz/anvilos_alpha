 
#include <nvif/user.h>

static u64
nvif_userc361_time(struct nvif_user *user)
{
	u32 hi, lo;

	do {
		hi = nvif_rd32(&user->object, 0x084);
		lo = nvif_rd32(&user->object, 0x080);
	} while (hi != nvif_rd32(&user->object, 0x084));

	return ((u64)hi << 32 | lo);
}

static void
nvif_userc361_doorbell(struct nvif_user *user, u32 token)
{
	nvif_wr32(&user->object, 0x90, token);
}

const struct nvif_user_func
nvif_userc361 = {
	.doorbell = nvif_userc361_doorbell,
	.time = nvif_userc361_time,
};
