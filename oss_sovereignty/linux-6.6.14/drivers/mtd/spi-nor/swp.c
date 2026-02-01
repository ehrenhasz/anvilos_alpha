
 
#include <linux/mtd/mtd.h>
#include <linux/mtd/spi-nor.h>

#include "core.h"

static u8 spi_nor_get_sr_bp_mask(struct spi_nor *nor)
{
	u8 mask = SR_BP2 | SR_BP1 | SR_BP0;

	if (nor->flags & SNOR_F_HAS_SR_BP3_BIT6)
		return mask | SR_BP3_BIT6;

	if (nor->flags & SNOR_F_HAS_4BIT_BP)
		return mask | SR_BP3;

	return mask;
}

static u8 spi_nor_get_sr_tb_mask(struct spi_nor *nor)
{
	if (nor->flags & SNOR_F_HAS_SR_TB_BIT6)
		return SR_TB_BIT6;
	else
		return SR_TB_BIT5;
}

static u64 spi_nor_get_min_prot_length_sr(struct spi_nor *nor)
{
	unsigned int bp_slots, bp_slots_needed;
	u8 mask = spi_nor_get_sr_bp_mask(nor);

	 
	bp_slots = (1 << hweight8(mask)) - 2;
	bp_slots_needed = ilog2(nor->info->n_sectors);

	if (bp_slots_needed > bp_slots)
		return nor->info->sector_size <<
			(bp_slots_needed - bp_slots);
	else
		return nor->info->sector_size;
}

static void spi_nor_get_locked_range_sr(struct spi_nor *nor, u8 sr, loff_t *ofs,
					uint64_t *len)
{
	struct mtd_info *mtd = &nor->mtd;
	u64 min_prot_len;
	u8 mask = spi_nor_get_sr_bp_mask(nor);
	u8 tb_mask = spi_nor_get_sr_tb_mask(nor);
	u8 bp, val = sr & mask;

	if (nor->flags & SNOR_F_HAS_SR_BP3_BIT6 && val & SR_BP3_BIT6)
		val = (val & ~SR_BP3_BIT6) | SR_BP3;

	bp = val >> SR_BP_SHIFT;

	if (!bp) {
		 
		*ofs = 0;
		*len = 0;
		return;
	}

	min_prot_len = spi_nor_get_min_prot_length_sr(nor);
	*len = min_prot_len << (bp - 1);

	if (*len > mtd->size)
		*len = mtd->size;

	if (nor->flags & SNOR_F_HAS_SR_TB && sr & tb_mask)
		*ofs = 0;
	else
		*ofs = mtd->size - *len;
}

 
static bool spi_nor_check_lock_status_sr(struct spi_nor *nor, loff_t ofs,
					 uint64_t len, u8 sr, bool locked)
{
	loff_t lock_offs, lock_offs_max, offs_max;
	uint64_t lock_len;

	if (!len)
		return true;

	spi_nor_get_locked_range_sr(nor, sr, &lock_offs, &lock_len);

	lock_offs_max = lock_offs + lock_len;
	offs_max = ofs + len;

	if (locked)
		 
		return (offs_max <= lock_offs_max) && (ofs >= lock_offs);
	else
		 
		return (ofs >= lock_offs_max) || (offs_max <= lock_offs);
}

static bool spi_nor_is_locked_sr(struct spi_nor *nor, loff_t ofs, uint64_t len,
				 u8 sr)
{
	return spi_nor_check_lock_status_sr(nor, ofs, len, sr, true);
}

static bool spi_nor_is_unlocked_sr(struct spi_nor *nor, loff_t ofs,
				   uint64_t len, u8 sr)
{
	return spi_nor_check_lock_status_sr(nor, ofs, len, sr, false);
}

 
static int spi_nor_sr_lock(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	struct mtd_info *mtd = &nor->mtd;
	u64 min_prot_len;
	int ret, status_old, status_new;
	u8 mask = spi_nor_get_sr_bp_mask(nor);
	u8 tb_mask = spi_nor_get_sr_tb_mask(nor);
	u8 pow, val;
	loff_t lock_len;
	bool can_be_top = true, can_be_bottom = nor->flags & SNOR_F_HAS_SR_TB;
	bool use_top;

	ret = spi_nor_read_sr(nor, nor->bouncebuf);
	if (ret)
		return ret;

	status_old = nor->bouncebuf[0];

	 
	if (spi_nor_is_locked_sr(nor, ofs, len, status_old))
		return 0;

	 
	if (!spi_nor_is_locked_sr(nor, 0, ofs, status_old))
		can_be_bottom = false;

	 
	if (!spi_nor_is_locked_sr(nor, ofs + len, mtd->size - (ofs + len),
				  status_old))
		can_be_top = false;

	if (!can_be_bottom && !can_be_top)
		return -EINVAL;

	 
	use_top = can_be_top;

	 
	if (use_top)
		lock_len = mtd->size - ofs;
	else
		lock_len = ofs + len;

	if (lock_len == mtd->size) {
		val = mask;
	} else {
		min_prot_len = spi_nor_get_min_prot_length_sr(nor);
		pow = ilog2(lock_len) - ilog2(min_prot_len) + 1;
		val = pow << SR_BP_SHIFT;

		if (nor->flags & SNOR_F_HAS_SR_BP3_BIT6 && val & SR_BP3)
			val = (val & ~SR_BP3) | SR_BP3_BIT6;

		if (val & ~mask)
			return -EINVAL;

		 
		if (!(val & mask))
			return -EINVAL;
	}

	status_new = (status_old & ~mask & ~tb_mask) | val;

	 
	if (!(nor->flags & SNOR_F_NO_WP))
		status_new |= SR_SRWD;

	if (!use_top)
		status_new |= tb_mask;

	 
	if (status_new == status_old)
		return 0;

	 
	if ((status_new & mask) < (status_old & mask))
		return -EINVAL;

	return spi_nor_write_sr_and_check(nor, status_new);
}

 
static int spi_nor_sr_unlock(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	struct mtd_info *mtd = &nor->mtd;
	u64 min_prot_len;
	int ret, status_old, status_new;
	u8 mask = spi_nor_get_sr_bp_mask(nor);
	u8 tb_mask = spi_nor_get_sr_tb_mask(nor);
	u8 pow, val;
	loff_t lock_len;
	bool can_be_top = true, can_be_bottom = nor->flags & SNOR_F_HAS_SR_TB;
	bool use_top;

	ret = spi_nor_read_sr(nor, nor->bouncebuf);
	if (ret)
		return ret;

	status_old = nor->bouncebuf[0];

	 
	if (spi_nor_is_unlocked_sr(nor, ofs, len, status_old))
		return 0;

	 
	if (!spi_nor_is_unlocked_sr(nor, 0, ofs, status_old))
		can_be_top = false;

	 
	if (!spi_nor_is_unlocked_sr(nor, ofs + len, mtd->size - (ofs + len),
				    status_old))
		can_be_bottom = false;

	if (!can_be_bottom && !can_be_top)
		return -EINVAL;

	 
	use_top = can_be_top;

	 
	if (use_top)
		lock_len = mtd->size - (ofs + len);
	else
		lock_len = ofs;

	if (lock_len == 0) {
		val = 0;  
	} else {
		min_prot_len = spi_nor_get_min_prot_length_sr(nor);
		pow = ilog2(lock_len) - ilog2(min_prot_len) + 1;
		val = pow << SR_BP_SHIFT;

		if (nor->flags & SNOR_F_HAS_SR_BP3_BIT6 && val & SR_BP3)
			val = (val & ~SR_BP3) | SR_BP3_BIT6;

		 
		if (val & ~mask)
			return -EINVAL;
	}

	status_new = (status_old & ~mask & ~tb_mask) | val;

	 
	if (lock_len == 0)
		status_new &= ~SR_SRWD;

	if (!use_top)
		status_new |= tb_mask;

	 
	if (status_new == status_old)
		return 0;

	 
	if ((status_new & mask) > (status_old & mask))
		return -EINVAL;

	return spi_nor_write_sr_and_check(nor, status_new);
}

 
static int spi_nor_sr_is_locked(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	int ret;

	ret = spi_nor_read_sr(nor, nor->bouncebuf);
	if (ret)
		return ret;

	return spi_nor_is_locked_sr(nor, ofs, len, nor->bouncebuf[0]);
}

static const struct spi_nor_locking_ops spi_nor_sr_locking_ops = {
	.lock = spi_nor_sr_lock,
	.unlock = spi_nor_sr_unlock,
	.is_locked = spi_nor_sr_is_locked,
};

void spi_nor_init_default_locking_ops(struct spi_nor *nor)
{
	nor->params->locking_ops = &spi_nor_sr_locking_ops;
}

static int spi_nor_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	int ret;

	ret = spi_nor_prep_and_lock(nor);
	if (ret)
		return ret;

	ret = nor->params->locking_ops->lock(nor, ofs, len);

	spi_nor_unlock_and_unprep(nor);
	return ret;
}

static int spi_nor_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	int ret;

	ret = spi_nor_prep_and_lock(nor);
	if (ret)
		return ret;

	ret = nor->params->locking_ops->unlock(nor, ofs, len);

	spi_nor_unlock_and_unprep(nor);
	return ret;
}

static int spi_nor_is_locked(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	int ret;

	ret = spi_nor_prep_and_lock(nor);
	if (ret)
		return ret;

	ret = nor->params->locking_ops->is_locked(nor, ofs, len);

	spi_nor_unlock_and_unprep(nor);
	return ret;
}

 
void spi_nor_try_unlock_all(struct spi_nor *nor)
{
	int ret;

	if (!(nor->flags & SNOR_F_HAS_LOCK))
		return;

	dev_dbg(nor->dev, "Unprotecting entire flash array\n");

	ret = spi_nor_unlock(&nor->mtd, 0, nor->params->size);
	if (ret)
		dev_dbg(nor->dev, "Failed to unlock the entire flash memory array\n");
}

void spi_nor_set_mtd_locking_ops(struct spi_nor *nor)
{
	struct mtd_info *mtd = &nor->mtd;

	if (!nor->params->locking_ops)
		return;

	mtd->_lock = spi_nor_lock;
	mtd->_unlock = spi_nor_unlock;
	mtd->_is_locked = spi_nor_is_locked;
}
