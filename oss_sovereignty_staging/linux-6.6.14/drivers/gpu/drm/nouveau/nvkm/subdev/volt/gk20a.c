 
#define gk20a_volt(p) container_of((p), struct gk20a_volt, base)
#include "priv.h"

#include <core/tegra.h>

#include "gk20a.h"

static const struct cvb_coef gk20a_cvb_coef[] = {
	 
	  { 1209886, -36468,  515,   417, -13123,  203},
	  { 1130804, -27659,  296,   298, -10834,  221},
	  { 1162871, -27110,  247,   238, -10681,  268},
	  { 1220458, -28654,  247,   179, -10376,  298},
	  { 1280953, -30204,  247,   119,  -9766,  304},
	  { 1344547, -31777,  247,   119,  -8545,  292},
	  { 1420168, -34227,  269,    60,  -7172,  256},
	  { 1490757, -35955,  274,    60,  -5188,  197},
	  { 1599112, -42583,  398,     0,  -1831,  119},
	  { 1366986, -16459, -274,     0,  -3204,   72},
	  { 1391884, -17078, -274,   -60,  -1526,   30},
	  { 1415522, -17497, -274,   -60,   -458,    0},
	  { 1464061, -18331, -274,  -119,   1831,  -72},
	  { 1524225, -20064, -254,  -119,   4272, -155},
	  { 1608418, -21643, -269,     0,    763,  -48},
};

 
static inline int
gk20a_volt_get_cvb_voltage(int speedo, int s_scale, const struct cvb_coef *coef)
{
	int mv;

	mv = DIV_ROUND_CLOSEST(coef->c2 * speedo, s_scale);
	mv = DIV_ROUND_CLOSEST((mv + coef->c1) * speedo, s_scale) + coef->c0;
	return mv;
}

 
static inline int
gk20a_volt_get_cvb_t_voltage(int speedo, int temp, int s_scale, int t_scale,
			     const struct cvb_coef *coef)
{
	int cvb_mv, mv;

	cvb_mv = gk20a_volt_get_cvb_voltage(speedo, s_scale, coef);

	mv = DIV_ROUND_CLOSEST(coef->c3 * speedo, s_scale) + coef->c4 +
		DIV_ROUND_CLOSEST(coef->c5 * temp, t_scale);
	mv = DIV_ROUND_CLOSEST(mv * temp, t_scale) + cvb_mv;
	return mv;
}

static int
gk20a_volt_calc_voltage(const struct cvb_coef *coef, int speedo)
{
	static const int v_scale = 1000;
	int mv;

	mv = gk20a_volt_get_cvb_t_voltage(speedo, -10, 100, 10, coef);
	mv = DIV_ROUND_UP(mv, v_scale);

	return mv * 1000;
}

static int
gk20a_volt_vid_get(struct nvkm_volt *base)
{
	struct gk20a_volt *volt = gk20a_volt(base);
	int i, uv;

	uv = regulator_get_voltage(volt->vdd);

	for (i = 0; i < volt->base.vid_nr; i++)
		if (volt->base.vid[i].uv >= uv)
			return i;

	return -EINVAL;
}

static int
gk20a_volt_vid_set(struct nvkm_volt *base, u8 vid)
{
	struct gk20a_volt *volt = gk20a_volt(base);
	struct nvkm_subdev *subdev = &volt->base.subdev;

	nvkm_debug(subdev, "set voltage as %duv\n", volt->base.vid[vid].uv);
	return regulator_set_voltage(volt->vdd, volt->base.vid[vid].uv, 1200000);
}

static int
gk20a_volt_set_id(struct nvkm_volt *base, u8 id, int condition)
{
	struct gk20a_volt *volt = gk20a_volt(base);
	struct nvkm_subdev *subdev = &volt->base.subdev;
	int prev_uv = regulator_get_voltage(volt->vdd);
	int target_uv = volt->base.vid[id].uv;
	int ret;

	nvkm_debug(subdev, "prev=%d, target=%d, condition=%d\n",
		   prev_uv, target_uv, condition);
	if (!condition ||
		(condition < 0 && target_uv < prev_uv) ||
		(condition > 0 && target_uv > prev_uv)) {
		ret = gk20a_volt_vid_set(&volt->base, volt->base.vid[id].vid);
	} else {
		ret = 0;
	}

	return ret;
}

static const struct nvkm_volt_func
gk20a_volt = {
	.vid_get = gk20a_volt_vid_get,
	.vid_set = gk20a_volt_vid_set,
	.set_id = gk20a_volt_set_id,
};

int
gk20a_volt_ctor(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		const struct cvb_coef *coefs, int nb_coefs,
		int vmin, struct gk20a_volt *volt)
{
	struct nvkm_device_tegra *tdev = device->func->tegra(device);
	int i, uv;

	nvkm_volt_ctor(&gk20a_volt, device, type, inst, &volt->base);

	uv = regulator_get_voltage(tdev->vdd);
	nvkm_debug(&volt->base.subdev, "the default voltage is %duV\n", uv);

	volt->vdd = tdev->vdd;

	volt->base.vid_nr = nb_coefs;
	for (i = 0; i < volt->base.vid_nr; i++) {
		volt->base.vid[i].vid = i;
		volt->base.vid[i].uv = max(
			gk20a_volt_calc_voltage(&coefs[i], tdev->gpu_speedo),
			vmin);
		nvkm_debug(&volt->base.subdev, "%2d: vid=%d, uv=%d\n", i,
			   volt->base.vid[i].vid, volt->base.vid[i].uv);
	}

	return 0;
}

int
gk20a_volt_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_volt **pvolt)
{
	struct gk20a_volt *volt;

	volt = kzalloc(sizeof(*volt), GFP_KERNEL);
	if (!volt)
		return -ENOMEM;
	*pvolt = &volt->base;

	return gk20a_volt_ctor(device, type, inst, gk20a_cvb_coef,
			       ARRAY_SIZE(gk20a_cvb_coef), 0, volt);
}
