
 

#include "internals.h"

static void amd_nand_decode_id(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_memory_organization *memorg;

	memorg = nanddev_get_memorg(&chip->base);

	nand_decode_ext_id(chip);

	 
	if (chip->id.data[4] != 0x00 && chip->id.data[5] == 0x00 &&
	    chip->id.data[6] == 0x00 && chip->id.data[7] == 0x00 &&
	    memorg->pagesize == 512) {
		memorg->pages_per_eraseblock = 256;
		memorg->pages_per_eraseblock <<= ((chip->id.data[3] & 0x03) << 1);
		mtd->erasesize = memorg->pages_per_eraseblock *
				 memorg->pagesize;
	}
}

static int amd_nand_init(struct nand_chip *chip)
{
	if (nand_is_slc(chip))
		 
		chip->options |= NAND_BBM_FIRSTPAGE | NAND_BBM_SECONDPAGE |
				 NAND_BBM_LASTPAGE;

	return 0;
}

const struct nand_manufacturer_ops amd_nand_manuf_ops = {
	.detect = amd_nand_decode_id,
	.init = amd_nand_init,
};
