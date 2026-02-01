 
#include "priv.h"
#include "ram.h"

struct ramxlat {
	int id;
	u8 enc;
};

static inline int
ramxlat(const struct ramxlat *xlat, int id)
{
	while (xlat->id >= 0) {
		if (xlat->id == id)
			return xlat->enc;
		xlat++;
	}
	return -EINVAL;
}

static const struct ramxlat
ramddr2_cl[] = {
	{ 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 6 },
	 
	{ 7, 7 },
	{ -1 }
};

static const struct ramxlat
ramddr2_wr[] = {
	{ 2, 1 }, { 3, 2 }, { 4, 3 }, { 5, 4 }, { 6, 5 },
	 
	{ 7, 6 },
	{ -1 }
};

int
nvkm_sddr2_calc(struct nvkm_ram *ram)
{
	int CL, WR, DLL = 0, ODT = 0;

	switch (ram->next->bios.timing_ver) {
	case 0x10:
		CL  = ram->next->bios.timing_10_CL;
		WR  = ram->next->bios.timing_10_WR;
		DLL = !ram->next->bios.ramcfg_DLLoff;
		ODT = ram->next->bios.timing_10_ODT & 3;
		break;
	case 0x20:
		CL  = (ram->next->bios.timing[1] & 0x0000001f);
		WR  = (ram->next->bios.timing[2] & 0x007f0000) >> 16;
		break;
	default:
		return -ENOSYS;
	}

	if (ram->next->bios.timing_ver == 0x20 ||
	    ram->next->bios.ramcfg_timing == 0xff) {
		ODT =  (ram->mr[1] & 0x004) >> 2 |
		       (ram->mr[1] & 0x040) >> 5;
	}

	CL  = ramxlat(ramddr2_cl, CL);
	WR  = ramxlat(ramddr2_wr, WR);
	if (CL < 0 || WR < 0)
		return -EINVAL;

	ram->mr[0] &= ~0xf70;
	ram->mr[0] |= (WR & 0x07) << 9;
	ram->mr[0] |= (CL & 0x07) << 4;

	ram->mr[1] &= ~0x045;
	ram->mr[1] |= (ODT & 0x1) << 2;
	ram->mr[1] |= (ODT & 0x2) << 5;
	ram->mr[1] |= !DLL;
	return 0;
}
