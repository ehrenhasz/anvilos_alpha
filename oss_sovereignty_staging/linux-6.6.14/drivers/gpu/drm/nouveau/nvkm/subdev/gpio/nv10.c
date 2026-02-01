 
#include "priv.h"

static int
nv10_gpio_sense(struct nvkm_gpio *gpio, int line)
{
	struct nvkm_device *device = gpio->subdev.device;
	if (line < 2) {
		line = line * 16;
		line = nvkm_rd32(device, 0x600818) >> line;
		return !!(line & 0x0100);
	} else
	if (line < 10) {
		line = (line - 2) * 4;
		line = nvkm_rd32(device, 0x60081c) >> line;
		return !!(line & 0x04);
	} else
	if (line < 14) {
		line = (line - 10) * 4;
		line = nvkm_rd32(device, 0x600850) >> line;
		return !!(line & 0x04);
	}

	return -EINVAL;
}

static int
nv10_gpio_drive(struct nvkm_gpio *gpio, int line, int dir, int out)
{
	struct nvkm_device *device = gpio->subdev.device;
	u32 reg, mask, data;

	if (line < 2) {
		line = line * 16;
		reg  = 0x600818;
		mask = 0x00000011;
		data = (dir << 4) | out;
	} else
	if (line < 10) {
		line = (line - 2) * 4;
		reg  = 0x60081c;
		mask = 0x00000003;
		data = (dir << 1) | out;
	} else
	if (line < 14) {
		line = (line - 10) * 4;
		reg  = 0x600850;
		mask = 0x00000003;
		data = (dir << 1) | out;
	} else {
		return -EINVAL;
	}

	nvkm_mask(device, reg, mask << line, data << line);
	return 0;
}

static void
nv10_gpio_intr_stat(struct nvkm_gpio *gpio, u32 *hi, u32 *lo)
{
	struct nvkm_device *device = gpio->subdev.device;
	u32 intr = nvkm_rd32(device, 0x001104);
	u32 stat = nvkm_rd32(device, 0x001144) & intr;
	*lo = (stat & 0xffff0000) >> 16;
	*hi = (stat & 0x0000ffff);
	nvkm_wr32(device, 0x001104, intr);
}

static void
nv10_gpio_intr_mask(struct nvkm_gpio *gpio, u32 type, u32 mask, u32 data)
{
	struct nvkm_device *device = gpio->subdev.device;
	u32 inte = nvkm_rd32(device, 0x001144);
	if (type & NVKM_GPIO_LO)
		inte = (inte & ~(mask << 16)) | (data << 16);
	if (type & NVKM_GPIO_HI)
		inte = (inte & ~mask) | data;
	nvkm_wr32(device, 0x001144, inte);
}

static const struct nvkm_gpio_func
nv10_gpio = {
	.lines = 16,
	.intr_stat = nv10_gpio_intr_stat,
	.intr_mask = nv10_gpio_intr_mask,
	.drive = nv10_gpio_drive,
	.sense = nv10_gpio_sense,
};

int
nv10_gpio_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_gpio **pgpio)
{
	return nvkm_gpio_new_(&nv10_gpio, device, type, inst, pgpio);
}
