 

#include "bcma_private.h"

#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/platform_data/brcmnand.h>
#include <linux/bcma/bcma.h>

 
static const char *bcma_nflash_alt_name = "bcma_brcmnand";

struct platform_device bcma_nflash_dev = {
	.name		= "bcma_nflash",
	.num_resources	= 0,
};

static const char *probes[] = { "bcm47xxpart", NULL };

 
int bcma_nflash_init(struct bcma_drv_cc *cc)
{
	struct bcma_bus *bus = cc->core->bus;
	u32 reg;

	if (bus->chipinfo.id != BCMA_CHIP_ID_BCM4706 &&
	    cc->core->id.rev != 38) {
		bcma_err(bus, "NAND flash on unsupported board!\n");
		return -ENOTSUPP;
	}

	if (!(cc->capabilities & BCMA_CC_CAP_NFLASH)) {
		bcma_err(bus, "NAND flash not present according to ChipCommon\n");
		return -ENODEV;
	}

	cc->nflash.present = true;
	if (cc->core->id.rev == 38 &&
	    (cc->status & BCMA_CC_CHIPST_5357_NAND_BOOT)) {
		cc->nflash.boot = true;
		 
		reg = bcma_cc_read32(cc, BCMA_CC_NAND_CS_NAND_SELECT) & 0xff;
		cc->nflash.brcmnand_info.chip_select = ffs(reg) - 1;
		cc->nflash.brcmnand_info.part_probe_types = probes;
		cc->nflash.brcmnand_info.ecc_stepsize = 512;
		cc->nflash.brcmnand_info.ecc_strength = 1;
		bcma_nflash_dev.name = bcma_nflash_alt_name;
	}

	 
	bcma_nflash_dev.dev.platform_data = &cc->nflash;

	return 0;
}
