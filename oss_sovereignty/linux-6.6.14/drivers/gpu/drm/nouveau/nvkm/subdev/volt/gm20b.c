 

#include "priv.h"
#include "gk20a.h"

#include <core/tegra.h>

static const struct cvb_coef gm20b_cvb_coef[] = {
	 
	  { 1786666,  -85625, 1632 },
	  { 1846729,  -87525, 1632 },
	  { 1910480,  -89425, 1632 },
	  { 1977920,  -91325, 1632 },
	  { 2049049,  -93215, 1632 },
	  { 2122872,  -95095, 1632 },
	  { 2201331,  -96985, 1632 },
	  { 2283479,  -98885, 1632 },
	  { 2369315, -100785, 1632 },
	  { 2458841, -102685, 1632 },
	  { 2550821, -104555, 1632 },
	  { 2647676, -106455, 1632 },
};

static const struct cvb_coef gm20b_na_cvb_coef[] = {
	 
	  {  814294, 8144, -940, 808, -21583, 226 },
	  {  856185, 8144, -940, 808, -21583, 226 },
	  {  898077, 8144, -940, 808, -21583, 226 },
	  {  939968, 8144, -940, 808, -21583, 226 },
	  {  981860, 8144, -940, 808, -21583, 226 },
	  { 1023751, 8144, -940, 808, -21583, 226 },
	  { 1065642, 8144, -940, 808, -21583, 226 },
	  { 1107534, 8144, -940, 808, -21583, 226 },
	  { 1149425, 8144, -940, 808, -21583, 226 },
	  { 1191317, 8144, -940, 808, -21583, 226 },
	  { 1233208, 8144, -940, 808, -21583, 226 },
	  { 1275100, 8144, -940, 808, -21583, 226 },
	  { 1316991, 8144, -940, 808, -21583, 226 },
};

static const u32 speedo_to_vmin[] = {
	 
	950000, 840000, 818750, 840000, 810000,
};

int
gm20b_volt_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_volt **pvolt)
{
	struct nvkm_device_tegra *tdev = device->func->tegra(device);
	struct gk20a_volt *volt;
	u32 vmin;

	if (tdev->gpu_speedo_id >= ARRAY_SIZE(speedo_to_vmin)) {
		nvdev_error(device, "unsupported speedo %d\n",
			    tdev->gpu_speedo_id);
		return -EINVAL;
	}

	volt = kzalloc(sizeof(*volt), GFP_KERNEL);
	if (!volt)
		return -ENOMEM;
	*pvolt = &volt->base;

	vmin = speedo_to_vmin[tdev->gpu_speedo_id];

	if (tdev->gpu_speedo_id >= 1)
		return gk20a_volt_ctor(device, type, inst, gm20b_na_cvb_coef,
				       ARRAY_SIZE(gm20b_na_cvb_coef), vmin, volt);
	else
		return gk20a_volt_ctor(device, type, inst, gm20b_cvb_coef,
				       ARRAY_SIZE(gm20b_cvb_coef), vmin, volt);
}
