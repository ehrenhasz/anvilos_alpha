 
#include "priv.h"

int
ga100_flcn_fw_signature(struct nvkm_falcon_fw *fw, u32 *src_base_src)
{
	struct nvkm_falcon *falcon = fw->falcon;
	struct nvkm_device *device = falcon->owner->device;
	u32 reg_fuse_version;
	int idx;

	FLCN_DBG(falcon, "brom: %08x %08x", fw->engine_id, fw->ucode_id);
	FLCN_DBG(falcon, "fuse_version: %d", fw->fuse_ver);

	if (fw->engine_id & 0x00000001) {
		reg_fuse_version = nvkm_rd32(device, 0x824140 + (fw->ucode_id - 1) * 4);
	} else
	if (fw->engine_id & 0x00000004) {
		reg_fuse_version = nvkm_rd32(device, 0x824100 + (fw->ucode_id - 1) * 4);
	} else
	if (fw->engine_id & 0x00000400) {
		reg_fuse_version = nvkm_rd32(device, 0x8241c0 + (fw->ucode_id - 1) * 4);
	} else {
		WARN_ON(1);
		return -ENOSYS;
	}

	FLCN_DBG(falcon, "reg_fuse_version: %08x", reg_fuse_version);
	if (reg_fuse_version) {
		reg_fuse_version = fls(reg_fuse_version);
		FLCN_DBG(falcon, "reg_fuse_version: %d", reg_fuse_version);

		if (WARN_ON(fw->fuse_ver < reg_fuse_version))
			return -EINVAL;

		idx = fw->fuse_ver - reg_fuse_version;
	} else {
		idx = fw->sig_nr - 1;
	}

	return idx;
}
