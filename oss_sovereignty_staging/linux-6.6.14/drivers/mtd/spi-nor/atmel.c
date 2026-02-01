
 

#include <linux/mtd/spi-nor.h>

#include "core.h"

#define ATMEL_SR_GLOBAL_PROTECT_MASK GENMASK(5, 2)

 
static int at25fs_nor_lock(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	return -EOPNOTSUPP;
}

static int at25fs_nor_unlock(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	int ret;

	 
	if (ofs || len != nor->params->size)
		return -EINVAL;

	 
	ret = spi_nor_write_sr_and_check(nor, 0);
	if (ret)
		dev_dbg(nor->dev, "unable to clear BP bits, WP# asserted?\n");

	return ret;
}

static int at25fs_nor_is_locked(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	return -EOPNOTSUPP;
}

static const struct spi_nor_locking_ops at25fs_nor_locking_ops = {
	.lock = at25fs_nor_lock,
	.unlock = at25fs_nor_unlock,
	.is_locked = at25fs_nor_is_locked,
};

static int at25fs_nor_late_init(struct spi_nor *nor)
{
	nor->params->locking_ops = &at25fs_nor_locking_ops;

	return 0;
}

static const struct spi_nor_fixups at25fs_nor_fixups = {
	.late_init = at25fs_nor_late_init,
};

 
static int atmel_nor_set_global_protection(struct spi_nor *nor, loff_t ofs,
					   uint64_t len, bool is_protect)
{
	int ret;
	u8 sr;

	 
	if (ofs || len != nor->params->size)
		return -EINVAL;

	ret = spi_nor_read_sr(nor, nor->bouncebuf);
	if (ret)
		return ret;

	sr = nor->bouncebuf[0];

	 
	if (sr & SR_SRWD) {
		sr &= ~SR_SRWD;
		ret = spi_nor_write_sr_and_check(nor, sr);
		if (ret) {
			dev_dbg(nor->dev, "unable to clear SRWD bit, WP# asserted?\n");
			return ret;
		}
	}

	if (is_protect) {
		sr |= ATMEL_SR_GLOBAL_PROTECT_MASK;
		 
		sr |= SR_SRWD;
	} else {
		sr &= ~ATMEL_SR_GLOBAL_PROTECT_MASK;
	}

	nor->bouncebuf[0] = sr;

	 
	return spi_nor_write_sr(nor, nor->bouncebuf, 1);
}

static int atmel_nor_global_protect(struct spi_nor *nor, loff_t ofs,
				    uint64_t len)
{
	return atmel_nor_set_global_protection(nor, ofs, len, true);
}

static int atmel_nor_global_unprotect(struct spi_nor *nor, loff_t ofs,
				      uint64_t len)
{
	return atmel_nor_set_global_protection(nor, ofs, len, false);
}

static int atmel_nor_is_global_protected(struct spi_nor *nor, loff_t ofs,
					 uint64_t len)
{
	int ret;

	if (ofs >= nor->params->size || (ofs + len) > nor->params->size)
		return -EINVAL;

	ret = spi_nor_read_sr(nor, nor->bouncebuf);
	if (ret)
		return ret;

	return ((nor->bouncebuf[0] & ATMEL_SR_GLOBAL_PROTECT_MASK) == ATMEL_SR_GLOBAL_PROTECT_MASK);
}

static const struct spi_nor_locking_ops atmel_nor_global_protection_ops = {
	.lock = atmel_nor_global_protect,
	.unlock = atmel_nor_global_unprotect,
	.is_locked = atmel_nor_is_global_protected,
};

static int atmel_nor_global_protection_late_init(struct spi_nor *nor)
{
	nor->params->locking_ops = &atmel_nor_global_protection_ops;

	return 0;
}

static const struct spi_nor_fixups atmel_nor_global_protection_fixups = {
	.late_init = atmel_nor_global_protection_late_init,
};

static const struct flash_info atmel_nor_parts[] = {
	 
	{ "at25fs010",  INFO(0x1f6601, 0, 32 * 1024,   4)
		FLAGS(SPI_NOR_HAS_LOCK)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &at25fs_nor_fixups },
	{ "at25fs040",  INFO(0x1f6604, 0, 64 * 1024,   8)
		FLAGS(SPI_NOR_HAS_LOCK)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &at25fs_nor_fixups },
	{ "at25df041a", INFO(0x1f4401, 0, 64 * 1024,   8)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &atmel_nor_global_protection_fixups },
	{ "at25df321",  INFO(0x1f4700, 0, 64 * 1024,  64)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &atmel_nor_global_protection_fixups },
	{ "at25df321a", INFO(0x1f4701, 0, 64 * 1024,  64)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &atmel_nor_global_protection_fixups },
	{ "at25df641",  INFO(0x1f4800, 0, 64 * 1024, 128)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &atmel_nor_global_protection_fixups },
	{ "at25sl321",	INFO(0x1f4216, 0, 64 * 1024, 64)
		NO_SFDP_FLAGS(SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ) },
	{ "at26f004",   INFO(0x1f0400, 0, 64 * 1024,  8)
		NO_SFDP_FLAGS(SECT_4K) },
	{ "at26df081a", INFO(0x1f4501, 0, 64 * 1024, 16)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &atmel_nor_global_protection_fixups },
	{ "at26df161a", INFO(0x1f4601, 0, 64 * 1024, 32)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &atmel_nor_global_protection_fixups },
	{ "at26df321",  INFO(0x1f4700, 0, 64 * 1024, 64)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		NO_SFDP_FLAGS(SECT_4K)
		.fixups = &atmel_nor_global_protection_fixups },
	{ "at45db081d", INFO(0x1f2500, 0, 64 * 1024, 16)
		NO_SFDP_FLAGS(SECT_4K) },
};

const struct spi_nor_manufacturer spi_nor_atmel = {
	.name = "atmel",
	.parts = atmel_nor_parts,
	.nparts = ARRAY_SIZE(atmel_nor_parts),
};
