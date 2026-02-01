
 

#include <linux/log2.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/spi-nor.h>

#include "core.h"

#define spi_nor_otp_region_len(nor) ((nor)->params->otp.org->len)
#define spi_nor_otp_n_regions(nor) ((nor)->params->otp.org->n_regions)

 
int spi_nor_otp_read_secr(struct spi_nor *nor, loff_t addr, size_t len, u8 *buf)
{
	u8 addr_nbytes, read_opcode, read_dummy;
	struct spi_mem_dirmap_desc *rdesc;
	enum spi_nor_protocol read_proto;
	int ret;

	read_opcode = nor->read_opcode;
	addr_nbytes = nor->addr_nbytes;
	read_dummy = nor->read_dummy;
	read_proto = nor->read_proto;
	rdesc = nor->dirmap.rdesc;

	nor->read_opcode = SPINOR_OP_RSECR;
	nor->read_dummy = 8;
	nor->read_proto = SNOR_PROTO_1_1_1;
	nor->dirmap.rdesc = NULL;

	ret = spi_nor_read_data(nor, addr, len, buf);

	nor->read_opcode = read_opcode;
	nor->addr_nbytes = addr_nbytes;
	nor->read_dummy = read_dummy;
	nor->read_proto = read_proto;
	nor->dirmap.rdesc = rdesc;

	return ret;
}

 
int spi_nor_otp_write_secr(struct spi_nor *nor, loff_t addr, size_t len,
			   const u8 *buf)
{
	enum spi_nor_protocol write_proto;
	struct spi_mem_dirmap_desc *wdesc;
	u8 addr_nbytes, program_opcode;
	int ret, written;

	program_opcode = nor->program_opcode;
	addr_nbytes = nor->addr_nbytes;
	write_proto = nor->write_proto;
	wdesc = nor->dirmap.wdesc;

	nor->program_opcode = SPINOR_OP_PSECR;
	nor->write_proto = SNOR_PROTO_1_1_1;
	nor->dirmap.wdesc = NULL;

	 
	ret = spi_nor_write_enable(nor);
	if (ret)
		goto out;

	written = spi_nor_write_data(nor, addr, len, buf);
	if (written < 0)
		goto out;

	ret = spi_nor_wait_till_ready(nor);

out:
	nor->program_opcode = program_opcode;
	nor->addr_nbytes = addr_nbytes;
	nor->write_proto = write_proto;
	nor->dirmap.wdesc = wdesc;

	return ret ?: written;
}

 
int spi_nor_otp_erase_secr(struct spi_nor *nor, loff_t addr)
{
	u8 erase_opcode = nor->erase_opcode;
	int ret;

	ret = spi_nor_write_enable(nor);
	if (ret)
		return ret;

	nor->erase_opcode = SPINOR_OP_ESECR;
	ret = spi_nor_erase_sector(nor, addr);
	nor->erase_opcode = erase_opcode;
	if (ret)
		return ret;

	return spi_nor_wait_till_ready(nor);
}

static int spi_nor_otp_lock_bit_cr(unsigned int region)
{
	static const int lock_bits[] = { SR2_LB1, SR2_LB2, SR2_LB3 };

	if (region >= ARRAY_SIZE(lock_bits))
		return -EINVAL;

	return lock_bits[region];
}

 
int spi_nor_otp_lock_sr2(struct spi_nor *nor, unsigned int region)
{
	u8 *cr = nor->bouncebuf;
	int ret, lock_bit;

	lock_bit = spi_nor_otp_lock_bit_cr(region);
	if (lock_bit < 0)
		return lock_bit;

	ret = spi_nor_read_cr(nor, cr);
	if (ret)
		return ret;

	 
	if (cr[0] & lock_bit)
		return 0;

	cr[0] |= lock_bit;

	return spi_nor_write_16bit_cr_and_check(nor, cr[0]);
}

 
int spi_nor_otp_is_locked_sr2(struct spi_nor *nor, unsigned int region)
{
	u8 *cr = nor->bouncebuf;
	int ret, lock_bit;

	lock_bit = spi_nor_otp_lock_bit_cr(region);
	if (lock_bit < 0)
		return lock_bit;

	ret = spi_nor_read_cr(nor, cr);
	if (ret)
		return ret;

	return cr[0] & lock_bit;
}

static loff_t spi_nor_otp_region_start(const struct spi_nor *nor, unsigned int region)
{
	const struct spi_nor_otp_organization *org = nor->params->otp.org;

	return org->base + region * org->offset;
}

static size_t spi_nor_otp_size(struct spi_nor *nor)
{
	return spi_nor_otp_n_regions(nor) * spi_nor_otp_region_len(nor);
}

 
static loff_t spi_nor_otp_region_to_offset(struct spi_nor *nor, unsigned int region)
{
	return region * spi_nor_otp_region_len(nor);
}

static unsigned int spi_nor_otp_offset_to_region(struct spi_nor *nor, loff_t ofs)
{
	return div64_u64(ofs, spi_nor_otp_region_len(nor));
}

static int spi_nor_mtd_otp_info(struct mtd_info *mtd, size_t len,
				size_t *retlen, struct otp_info *buf)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	const struct spi_nor_otp_ops *ops = nor->params->otp.ops;
	unsigned int n_regions = spi_nor_otp_n_regions(nor);
	unsigned int i;
	int ret, locked;

	if (len < n_regions * sizeof(*buf))
		return -ENOSPC;

	ret = spi_nor_prep_and_lock(nor);
	if (ret)
		return ret;

	for (i = 0; i < n_regions; i++) {
		buf->start = spi_nor_otp_region_to_offset(nor, i);
		buf->length = spi_nor_otp_region_len(nor);

		locked = ops->is_locked(nor, i);
		if (locked < 0) {
			ret = locked;
			goto out;
		}

		buf->locked = !!locked;
		buf++;
	}

	*retlen = n_regions * sizeof(*buf);

out:
	spi_nor_unlock_and_unprep(nor);

	return ret;
}

static int spi_nor_mtd_otp_range_is_locked(struct spi_nor *nor, loff_t ofs,
					   size_t len)
{
	const struct spi_nor_otp_ops *ops = nor->params->otp.ops;
	unsigned int region;
	int locked;

	 
	for (region = spi_nor_otp_offset_to_region(nor, ofs);
	     region <= spi_nor_otp_offset_to_region(nor, ofs + len - 1);
	     region++) {
		locked = ops->is_locked(nor, region);
		 
		if (locked)
			return locked;
	}

	return 0;
}

static int spi_nor_mtd_otp_read_write(struct mtd_info *mtd, loff_t ofs,
				      size_t total_len, size_t *retlen,
				      const u8 *buf, bool is_write)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	const struct spi_nor_otp_ops *ops = nor->params->otp.ops;
	const size_t rlen = spi_nor_otp_region_len(nor);
	loff_t rstart, rofs;
	unsigned int region;
	size_t len;
	int ret;

	if (ofs < 0 || ofs >= spi_nor_otp_size(nor))
		return 0;

	 
	total_len = min_t(size_t, total_len, spi_nor_otp_size(nor) - ofs);

	if (!total_len)
		return 0;

	ret = spi_nor_prep_and_lock(nor);
	if (ret)
		return ret;

	if (is_write) {
		ret = spi_nor_mtd_otp_range_is_locked(nor, ofs, total_len);
		if (ret < 0) {
			goto out;
		} else if (ret) {
			ret = -EROFS;
			goto out;
		}
	}

	while (total_len) {
		 
		region = spi_nor_otp_offset_to_region(nor, ofs);
		rstart = spi_nor_otp_region_start(nor, region);

		 
		rofs = ofs & (rlen - 1);

		 
		len = min_t(size_t, total_len, rlen - rofs);

		if (is_write)
			ret = ops->write(nor, rstart + rofs, len, buf);
		else
			ret = ops->read(nor, rstart + rofs, len, (u8 *)buf);
		if (ret == 0)
			ret = -EIO;
		if (ret < 0)
			goto out;

		*retlen += ret;
		ofs += ret;
		buf += ret;
		total_len -= ret;
	}
	ret = 0;

out:
	spi_nor_unlock_and_unprep(nor);
	return ret;
}

static int spi_nor_mtd_otp_read(struct mtd_info *mtd, loff_t from, size_t len,
				size_t *retlen, u8 *buf)
{
	return spi_nor_mtd_otp_read_write(mtd, from, len, retlen, buf, false);
}

static int spi_nor_mtd_otp_write(struct mtd_info *mtd, loff_t to, size_t len,
				 size_t *retlen, const u8 *buf)
{
	return spi_nor_mtd_otp_read_write(mtd, to, len, retlen, buf, true);
}

static int spi_nor_mtd_otp_erase(struct mtd_info *mtd, loff_t from, size_t len)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	const struct spi_nor_otp_ops *ops = nor->params->otp.ops;
	const size_t rlen = spi_nor_otp_region_len(nor);
	unsigned int region;
	loff_t rstart;
	int ret;

	 
	if (!ops->erase)
		return -EOPNOTSUPP;

	if (!len)
		return 0;

	if (from < 0 || (from + len) > spi_nor_otp_size(nor))
		return -EINVAL;

	 
	if (!IS_ALIGNED(len, rlen) || !IS_ALIGNED(from, rlen))
		return -EINVAL;

	ret = spi_nor_prep_and_lock(nor);
	if (ret)
		return ret;

	ret = spi_nor_mtd_otp_range_is_locked(nor, from, len);
	if (ret < 0) {
		goto out;
	} else if (ret) {
		ret = -EROFS;
		goto out;
	}

	while (len) {
		region = spi_nor_otp_offset_to_region(nor, from);
		rstart = spi_nor_otp_region_start(nor, region);

		ret = ops->erase(nor, rstart);
		if (ret)
			goto out;

		len -= rlen;
		from += rlen;
	}

out:
	spi_nor_unlock_and_unprep(nor);

	return ret;
}

static int spi_nor_mtd_otp_lock(struct mtd_info *mtd, loff_t from, size_t len)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	const struct spi_nor_otp_ops *ops = nor->params->otp.ops;
	const size_t rlen = spi_nor_otp_region_len(nor);
	unsigned int region;
	int ret;

	if (from < 0 || (from + len) > spi_nor_otp_size(nor))
		return -EINVAL;

	 
	if (!IS_ALIGNED(len, rlen) || !IS_ALIGNED(from, rlen))
		return -EINVAL;

	ret = spi_nor_prep_and_lock(nor);
	if (ret)
		return ret;

	while (len) {
		region = spi_nor_otp_offset_to_region(nor, from);
		ret = ops->lock(nor, region);
		if (ret)
			goto out;

		len -= rlen;
		from += rlen;
	}

out:
	spi_nor_unlock_and_unprep(nor);

	return ret;
}

void spi_nor_set_mtd_otp_ops(struct spi_nor *nor)
{
	struct mtd_info *mtd = &nor->mtd;

	if (!nor->params->otp.ops)
		return;

	if (WARN_ON(!is_power_of_2(spi_nor_otp_region_len(nor))))
		return;

	 
	mtd->_get_user_prot_info = spi_nor_mtd_otp_info;
	mtd->_read_user_prot_reg = spi_nor_mtd_otp_read;
	mtd->_write_user_prot_reg = spi_nor_mtd_otp_write;
	mtd->_lock_user_prot_reg = spi_nor_mtd_otp_lock;
	mtd->_erase_user_prot_reg = spi_nor_mtd_otp_erase;
}
