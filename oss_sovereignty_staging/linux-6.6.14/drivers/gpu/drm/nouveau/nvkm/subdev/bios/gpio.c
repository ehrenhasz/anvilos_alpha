 
#include <subdev/bios.h>
#include <subdev/bios/dcb.h>
#include <subdev/bios/gpio.h>
#include <subdev/bios/xpio.h>

u16
dcb_gpio_table(struct nvkm_bios *bios, u8 *ver, u8 *hdr, u8 *cnt, u8 *len)
{
	u16 data = 0x0000;
	u16 dcb = dcb_table(bios, ver, hdr, cnt, len);
	if (dcb) {
		if (*ver >= 0x30 && *hdr >= 0x0c)
			data = nvbios_rd16(bios, dcb + 0x0a);
		else
		if (*ver >= 0x22 && nvbios_rd08(bios, dcb - 1) >= 0x13)
			data = nvbios_rd16(bios, dcb - 0x0f);

		if (data) {
			*ver = nvbios_rd08(bios, data + 0x00);
			if (*ver < 0x30) {
				*hdr = 3;
				*cnt = nvbios_rd08(bios, data + 0x02);
				*len = nvbios_rd08(bios, data + 0x01);
			} else
			if (*ver <= 0x41) {
				*hdr = nvbios_rd08(bios, data + 0x01);
				*cnt = nvbios_rd08(bios, data + 0x02);
				*len = nvbios_rd08(bios, data + 0x03);
			} else {
				data = 0x0000;
			}
		}
	}
	return data;
}

u16
dcb_gpio_entry(struct nvkm_bios *bios, int idx, int ent, u8 *ver, u8 *len)
{
	u8  hdr, cnt, xver;  
	u16 gpio;

	if (!idx--)
		gpio = dcb_gpio_table(bios, ver, &hdr, &cnt, len);
	else
		gpio = dcb_xpio_table(bios, idx, &xver, &hdr, &cnt, len);

	if (gpio && ent < cnt)
		return gpio + hdr + (ent * *len);

	return 0x0000;
}

u16
dcb_gpio_parse(struct nvkm_bios *bios, int idx, int ent, u8 *ver, u8 *len,
	       struct dcb_gpio_func *gpio)
{
	u16 data = dcb_gpio_entry(bios, idx, ent, ver, len);
	if (data) {
		if (*ver < 0x40) {
			u16 info = nvbios_rd16(bios, data);
			*gpio = (struct dcb_gpio_func) {
				.line = (info & 0x001f) >> 0,
				.func = (info & 0x07e0) >> 5,
				.log[0] = (info & 0x1800) >> 11,
				.log[1] = (info & 0x6000) >> 13,
				.param = !!(info & 0x8000),
			};
		} else
		if (*ver < 0x41) {
			u32 info = nvbios_rd32(bios, data);
			*gpio = (struct dcb_gpio_func) {
				.line = (info & 0x0000001f) >> 0,
				.func = (info & 0x0000ff00) >> 8,
				.log[0] = (info & 0x18000000) >> 27,
				.log[1] = (info & 0x60000000) >> 29,
				.param = !!(info & 0x80000000),
			};
		} else {
			u32 info = nvbios_rd32(bios, data + 0);
			u8 info1 = nvbios_rd32(bios, data + 4);
			*gpio = (struct dcb_gpio_func) {
				.line = (info & 0x0000003f) >> 0,
				.func = (info & 0x0000ff00) >> 8,
				.log[0] = (info1 & 0x30) >> 4,
				.log[1] = (info1 & 0xc0) >> 6,
				.param = !!(info & 0x80000000),
			};
		}
	}

	return data;
}

u16
dcb_gpio_match(struct nvkm_bios *bios, int idx, u8 func, u8 line,
	       u8 *ver, u8 *len, struct dcb_gpio_func *gpio)
{
	u8  hdr, cnt, i = 0;
	u16 data;

	while ((data = dcb_gpio_parse(bios, idx, i++, ver, len, gpio))) {
		if ((line == 0xff || line == gpio->line) &&
		    (func == 0xff || func == gpio->func))
			return data;
	}

	 
	if ((data = dcb_table(bios, ver, &hdr, &cnt, len))) {
		if (*ver >= 0x22 && *ver < 0x30 && func == DCB_GPIO_TVDAC0) {
			u8 conf = nvbios_rd08(bios, data - 5);
			u8 addr = nvbios_rd08(bios, data - 4);
			if (conf & 0x01) {
				*gpio = (struct dcb_gpio_func) {
					.func = DCB_GPIO_TVDAC0,
					.line = addr >> 4,
					.log[0] = !!(conf & 0x02),
					.log[1] =  !(conf & 0x02),
				};
				*ver = 0x00;
				return data;
			}
		}
	}

	return 0x0000;
}
