 
#include <nvif/disp.h>
#include <nvif/device.h>
#include <nvif/printf.h>

#include <nvif/class.h>
#include <nvif/if0010.h>

void
nvif_disp_dtor(struct nvif_disp *disp)
{
	nvif_object_dtor(&disp->object);
}

int
nvif_disp_ctor(struct nvif_device *device, const char *name, s32 oclass, struct nvif_disp *disp)
{
	static const struct nvif_mclass disps[] = {
		{ GA102_DISP, 0 },
		{ TU102_DISP, 0 },
		{ GV100_DISP, 0 },
		{ GP102_DISP, 0 },
		{ GP100_DISP, 0 },
		{ GM200_DISP, 0 },
		{ GM107_DISP, 0 },
		{ GK110_DISP, 0 },
		{ GK104_DISP, 0 },
		{ GF110_DISP, 0 },
		{ GT214_DISP, 0 },
		{ GT206_DISP, 0 },
		{ GT200_DISP, 0 },
		{   G82_DISP, 0 },
		{  NV50_DISP, 0 },
		{  NV04_DISP, 0 },
		{}
	};
	struct nvif_disp_v0 args;
	int cid, ret;

	cid = nvif_sclass(&device->object, disps, oclass);
	disp->object.client = NULL;
	if (cid < 0) {
		NVIF_ERRON(cid, &device->object, "[NEW disp%04x] not supported", oclass);
		return cid;
	}

	args.version = 0;

	ret = nvif_object_ctor(&device->object, name ?: "nvifDisp", 0,
			       disps[cid].oclass, &args, sizeof(args), &disp->object);
	NVIF_ERRON(ret, &device->object, "[NEW disp%04x]", disps[cid].oclass);
	if (ret)
		return ret;

	NVIF_DEBUG(&disp->object, "[NEW] conn_mask:%08x outp_mask:%08x head_mask:%08x",
		   args.conn_mask, args.outp_mask, args.head_mask);
	disp->conn_mask = args.conn_mask;
	disp->outp_mask = args.outp_mask;
	disp->head_mask = args.head_mask;
	return 0;
}
