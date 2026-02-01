
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand-ecc-sw-hamming.h>
#include <linux/mtd/nand-ecc-sw-bch.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/mtd/partitions.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>

#include "internals.h"

static int nand_pairing_dist3_get_info(struct mtd_info *mtd, int page,
				       struct mtd_pairing_info *info)
{
	int lastpage = (mtd->erasesize / mtd->writesize) - 1;
	int dist = 3;

	if (page == lastpage)
		dist = 2;

	if (!page || (page & 1)) {
		info->group = 0;
		info->pair = (page + 1) / 2;
	} else {
		info->group = 1;
		info->pair = (page + 1 - dist) / 2;
	}

	return 0;
}

static int nand_pairing_dist3_get_wunit(struct mtd_info *mtd,
					const struct mtd_pairing_info *info)
{
	int lastpair = ((mtd->erasesize / mtd->writesize) - 1) / 2;
	int page = info->pair * 2;
	int dist = 3;

	if (!info->group && !info->pair)
		return 0;

	if (info->pair == lastpair && info->group)
		dist = 2;

	if (!info->group)
		page--;
	else if (info->pair)
		page += dist - 1;

	if (page >= mtd->erasesize / mtd->writesize)
		return -EINVAL;

	return page;
}

const struct mtd_pairing_scheme dist3_pairing_scheme = {
	.ngroups = 2,
	.get_info = nand_pairing_dist3_get_info,
	.get_wunit = nand_pairing_dist3_get_wunit,
};

static int check_offs_len(struct nand_chip *chip, loff_t ofs, uint64_t len)
{
	int ret = 0;

	 
	if (ofs & ((1ULL << chip->phys_erase_shift) - 1)) {
		pr_debug("%s: unaligned address\n", __func__);
		ret = -EINVAL;
	}

	 
	if (len & ((1ULL << chip->phys_erase_shift) - 1)) {
		pr_debug("%s: length not block aligned\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

 
void nand_extract_bits(u8 *dst, unsigned int dst_off, const u8 *src,
		       unsigned int src_off, unsigned int nbits)
{
	unsigned int tmp, n;

	dst += dst_off / 8;
	dst_off %= 8;
	src += src_off / 8;
	src_off %= 8;

	while (nbits) {
		n = min3(8 - dst_off, 8 - src_off, nbits);

		tmp = (*src >> src_off) & GENMASK(n - 1, 0);
		*dst &= ~GENMASK(n - 1 + dst_off, dst_off);
		*dst |= tmp << dst_off;

		dst_off += n;
		if (dst_off >= 8) {
			dst++;
			dst_off -= 8;
		}

		src_off += n;
		if (src_off >= 8) {
			src++;
			src_off -= 8;
		}

		nbits -= n;
	}
}
EXPORT_SYMBOL_GPL(nand_extract_bits);

 
void nand_select_target(struct nand_chip *chip, unsigned int cs)
{
	 
	if (WARN_ON(cs > nanddev_ntargets(&chip->base)))
		return;

	chip->cur_cs = cs;

	if (chip->legacy.select_chip)
		chip->legacy.select_chip(chip, cs);
}
EXPORT_SYMBOL_GPL(nand_select_target);

 
void nand_deselect_target(struct nand_chip *chip)
{
	if (chip->legacy.select_chip)
		chip->legacy.select_chip(chip, -1);

	chip->cur_cs = -1;
}
EXPORT_SYMBOL_GPL(nand_deselect_target);

 
static void nand_release_device(struct nand_chip *chip)
{
	 
	mutex_unlock(&chip->controller->lock);
	mutex_unlock(&chip->lock);
}

 
int nand_bbm_get_next_page(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int last_page = ((mtd->erasesize - mtd->writesize) >>
			 chip->page_shift) & chip->pagemask;
	unsigned int bbm_flags = NAND_BBM_FIRSTPAGE | NAND_BBM_SECONDPAGE
		| NAND_BBM_LASTPAGE;

	if (page == 0 && !(chip->options & bbm_flags))
		return 0;
	if (page == 0 && chip->options & NAND_BBM_FIRSTPAGE)
		return 0;
	if (page <= 1 && chip->options & NAND_BBM_SECONDPAGE)
		return 1;
	if (page <= last_page && chip->options & NAND_BBM_LASTPAGE)
		return last_page;

	return -EINVAL;
}

 
static int nand_block_bad(struct nand_chip *chip, loff_t ofs)
{
	int first_page, page_offset;
	int res;
	u8 bad;

	first_page = (int)(ofs >> chip->page_shift) & chip->pagemask;
	page_offset = nand_bbm_get_next_page(chip, 0);

	while (page_offset >= 0) {
		res = chip->ecc.read_oob(chip, first_page + page_offset);
		if (res < 0)
			return res;

		bad = chip->oob_poi[chip->badblockpos];

		if (likely(chip->badblockbits == 8))
			res = bad != 0xFF;
		else
			res = hweight8(bad) < chip->badblockbits;
		if (res)
			return res;

		page_offset = nand_bbm_get_next_page(chip, page_offset + 1);
	}

	return 0;
}

 
static bool nand_region_is_secured(struct nand_chip *chip, loff_t offset, u64 size)
{
	int i;

	 
	for (i = 0; i < chip->nr_secure_regions; i++) {
		const struct nand_secure_region *region = &chip->secure_regions[i];

		if (offset + size <= region->offset ||
		    offset >= region->offset + region->size)
			continue;

		pr_debug("%s: Region 0x%llx - 0x%llx is secured!",
			 __func__, offset, offset + size);

		return true;
	}

	return false;
}

static int nand_isbad_bbm(struct nand_chip *chip, loff_t ofs)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (chip->options & NAND_NO_BBM_QUIRK)
		return 0;

	 
	if (nand_region_is_secured(chip, ofs, mtd->erasesize))
		return -EIO;

	if (mtd_check_expert_analysis_mode())
		return 0;

	if (chip->legacy.block_bad)
		return chip->legacy.block_bad(chip, ofs);

	return nand_block_bad(chip, ofs);
}

 
static void nand_get_device(struct nand_chip *chip)
{
	 
	while (1) {
		mutex_lock(&chip->lock);
		if (!chip->suspended) {
			mutex_lock(&chip->controller->lock);
			return;
		}
		mutex_unlock(&chip->lock);

		wait_event(chip->resume_wq, !chip->suspended);
	}
}

 
static int nand_check_wp(struct nand_chip *chip)
{
	u8 status;
	int ret;

	 
	if (chip->options & NAND_BROKEN_XD)
		return 0;

	 
	ret = nand_status_op(chip, &status);
	if (ret)
		return ret;

	return status & NAND_STATUS_WP ? 0 : 1;
}

 
static uint8_t *nand_fill_oob(struct nand_chip *chip, uint8_t *oob, size_t len,
			      struct mtd_oob_ops *ops)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret;

	 
	memset(chip->oob_poi, 0xff, mtd->oobsize);

	switch (ops->mode) {

	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
		memcpy(chip->oob_poi + ops->ooboffs, oob, len);
		return oob + len;

	case MTD_OPS_AUTO_OOB:
		ret = mtd_ooblayout_set_databytes(mtd, oob, chip->oob_poi,
						  ops->ooboffs, len);
		BUG_ON(ret);
		return oob + len;

	default:
		BUG();
	}
	return NULL;
}

 
static int nand_do_write_oob(struct nand_chip *chip, loff_t to,
			     struct mtd_oob_ops *ops)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int chipnr, page, status, len, ret;

	pr_debug("%s: to = 0x%08x, len = %i\n",
			 __func__, (unsigned int)to, (int)ops->ooblen);

	len = mtd_oobavail(mtd, ops);

	 
	if ((ops->ooboffs + ops->ooblen) > len) {
		pr_debug("%s: attempt to write past end of page\n",
				__func__);
		return -EINVAL;
	}

	 
	if (nand_region_is_secured(chip, to, ops->ooblen))
		return -EIO;

	chipnr = (int)(to >> chip->chip_shift);

	 
	ret = nand_reset(chip, chipnr);
	if (ret)
		return ret;

	nand_select_target(chip, chipnr);

	 
	page = (int)(to >> chip->page_shift);

	 
	if (nand_check_wp(chip)) {
		nand_deselect_target(chip);
		return -EROFS;
	}

	 
	if (page == chip->pagecache.page)
		chip->pagecache.page = -1;

	nand_fill_oob(chip, ops->oobbuf, ops->ooblen, ops);

	if (ops->mode == MTD_OPS_RAW)
		status = chip->ecc.write_oob_raw(chip, page & chip->pagemask);
	else
		status = chip->ecc.write_oob(chip, page & chip->pagemask);

	nand_deselect_target(chip);

	if (status)
		return status;

	ops->oobretlen = ops->ooblen;

	return 0;
}

 
static int nand_default_block_markbad(struct nand_chip *chip, loff_t ofs)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mtd_oob_ops ops;
	uint8_t buf[2] = { 0, 0 };
	int ret = 0, res, page_offset;

	memset(&ops, 0, sizeof(ops));
	ops.oobbuf = buf;
	ops.ooboffs = chip->badblockpos;
	if (chip->options & NAND_BUSWIDTH_16) {
		ops.ooboffs &= ~0x01;
		ops.len = ops.ooblen = 2;
	} else {
		ops.len = ops.ooblen = 1;
	}
	ops.mode = MTD_OPS_PLACE_OOB;

	page_offset = nand_bbm_get_next_page(chip, 0);

	while (page_offset >= 0) {
		res = nand_do_write_oob(chip,
					ofs + (page_offset * mtd->writesize),
					&ops);

		if (!ret)
			ret = res;

		page_offset = nand_bbm_get_next_page(chip, page_offset + 1);
	}

	return ret;
}

 
int nand_markbad_bbm(struct nand_chip *chip, loff_t ofs)
{
	if (chip->legacy.block_markbad)
		return chip->legacy.block_markbad(chip, ofs);

	return nand_default_block_markbad(chip, ofs);
}

 
static int nand_block_markbad_lowlevel(struct nand_chip *chip, loff_t ofs)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int res, ret = 0;

	if (!(chip->bbt_options & NAND_BBT_NO_OOB_BBM)) {
		struct erase_info einfo;

		 
		memset(&einfo, 0, sizeof(einfo));
		einfo.addr = ofs;
		einfo.len = 1ULL << chip->phys_erase_shift;
		nand_erase_nand(chip, &einfo, 0);

		 
		nand_get_device(chip);

		ret = nand_markbad_bbm(chip, ofs);
		nand_release_device(chip);
	}

	 
	if (chip->bbt) {
		res = nand_markbad_bbt(chip, ofs);
		if (!ret)
			ret = res;
	}

	if (!ret)
		mtd->ecc_stats.badblocks++;

	return ret;
}

 
static int nand_block_isreserved(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (!chip->bbt)
		return 0;
	 
	return nand_isreserved_bbt(chip, ofs);
}

 
static int nand_block_checkbad(struct nand_chip *chip, loff_t ofs, int allowbbt)
{
	 
	if (chip->bbt)
		return nand_isbad_bbt(chip, ofs, allowbbt);

	return nand_isbad_bbm(chip, ofs);
}

 
int nand_soft_waitrdy(struct nand_chip *chip, unsigned long timeout_ms)
{
	const struct nand_interface_config *conf;
	u8 status = 0;
	int ret;

	if (!nand_has_exec_op(chip))
		return -ENOTSUPP;

	 
	conf = nand_get_interface_config(chip);
	ndelay(NAND_COMMON_TIMING_NS(conf, tWB_max));

	ret = nand_status_op(chip, NULL);
	if (ret)
		return ret;

	 
	timeout_ms = jiffies + msecs_to_jiffies(timeout_ms) + 1;
	do {
		ret = nand_read_data_op(chip, &status, sizeof(status), true,
					false);
		if (ret)
			break;

		if (status & NAND_STATUS_READY)
			break;

		 
		udelay(10);
	} while	(time_before(jiffies, timeout_ms));

	 
	nand_exit_status_op(chip);

	if (ret)
		return ret;

	return status & NAND_STATUS_READY ? 0 : -ETIMEDOUT;
};
EXPORT_SYMBOL_GPL(nand_soft_waitrdy);

 
int nand_gpio_waitrdy(struct nand_chip *chip, struct gpio_desc *gpiod,
		      unsigned long timeout_ms)
{

	 
	timeout_ms = jiffies + msecs_to_jiffies(timeout_ms) + 1;
	do {
		if (gpiod_get_value_cansleep(gpiod))
			return 0;

		cond_resched();
	} while	(time_before(jiffies, timeout_ms));

	return gpiod_get_value_cansleep(gpiod) ? 0 : -ETIMEDOUT;
};
EXPORT_SYMBOL_GPL(nand_gpio_waitrdy);

 
void panic_nand_wait(struct nand_chip *chip, unsigned long timeo)
{
	int i;
	for (i = 0; i < timeo; i++) {
		if (chip->legacy.dev_ready) {
			if (chip->legacy.dev_ready(chip))
				break;
		} else {
			int ret;
			u8 status;

			ret = nand_read_data_op(chip, &status, sizeof(status),
						true, false);
			if (ret)
				return;

			if (status & NAND_STATUS_READY)
				break;
		}
		mdelay(1);
	}
}

static bool nand_supports_get_features(struct nand_chip *chip, int addr)
{
	return (chip->parameters.supports_set_get_features &&
		test_bit(addr, chip->parameters.get_feature_list));
}

static bool nand_supports_set_features(struct nand_chip *chip, int addr)
{
	return (chip->parameters.supports_set_get_features &&
		test_bit(addr, chip->parameters.set_feature_list));
}

 
static int nand_reset_interface(struct nand_chip *chip, int chipnr)
{
	const struct nand_controller_ops *ops = chip->controller->ops;
	int ret;

	if (!nand_controller_can_setup_interface(chip))
		return 0;

	 

	chip->current_interface_config = nand_get_reset_interface_config();
	ret = ops->setup_interface(chip, chipnr,
				   chip->current_interface_config);
	if (ret)
		pr_err("Failed to configure data interface to SDR timing mode 0\n");

	return ret;
}

 
static int nand_setup_interface(struct nand_chip *chip, int chipnr)
{
	const struct nand_controller_ops *ops = chip->controller->ops;
	u8 tmode_param[ONFI_SUBFEATURE_PARAM_LEN] = { }, request;
	int ret;

	if (!nand_controller_can_setup_interface(chip))
		return 0;

	 
	if (!chip->best_interface_config)
		return 0;

	request = chip->best_interface_config->timings.mode;
	if (nand_interface_is_sdr(chip->best_interface_config))
		request |= ONFI_DATA_INTERFACE_SDR;
	else
		request |= ONFI_DATA_INTERFACE_NVDDR;
	tmode_param[0] = request;

	 
	if (nand_supports_set_features(chip, ONFI_FEATURE_ADDR_TIMING_MODE)) {
		nand_select_target(chip, chipnr);
		ret = nand_set_features(chip, ONFI_FEATURE_ADDR_TIMING_MODE,
					tmode_param);
		nand_deselect_target(chip);
		if (ret)
			return ret;
	}

	 
	ret = ops->setup_interface(chip, chipnr, chip->best_interface_config);
	if (ret)
		return ret;

	 
	if (!nand_supports_get_features(chip, ONFI_FEATURE_ADDR_TIMING_MODE))
		goto update_interface_config;

	memset(tmode_param, 0, ONFI_SUBFEATURE_PARAM_LEN);
	nand_select_target(chip, chipnr);
	ret = nand_get_features(chip, ONFI_FEATURE_ADDR_TIMING_MODE,
				tmode_param);
	nand_deselect_target(chip);
	if (ret)
		goto err_reset_chip;

	if (request != tmode_param[0]) {
		pr_warn("%s timing mode %d not acknowledged by the NAND chip\n",
			nand_interface_is_nvddr(chip->best_interface_config) ? "NV-DDR" : "SDR",
			chip->best_interface_config->timings.mode);
		pr_debug("NAND chip would work in %s timing mode %d\n",
			 tmode_param[0] & ONFI_DATA_INTERFACE_NVDDR ? "NV-DDR" : "SDR",
			 (unsigned int)ONFI_TIMING_MODE_PARAM(tmode_param[0]));
		goto err_reset_chip;
	}

update_interface_config:
	chip->current_interface_config = chip->best_interface_config;

	return 0;

err_reset_chip:
	 
	nand_reset_interface(chip, chipnr);
	nand_select_target(chip, chipnr);
	nand_reset_op(chip);
	nand_deselect_target(chip);

	return ret;
}

 
int nand_choose_best_sdr_timings(struct nand_chip *chip,
				 struct nand_interface_config *iface,
				 struct nand_sdr_timings *spec_timings)
{
	const struct nand_controller_ops *ops = chip->controller->ops;
	int best_mode = 0, mode, ret = -EOPNOTSUPP;

	iface->type = NAND_SDR_IFACE;

	if (spec_timings) {
		iface->timings.sdr = *spec_timings;
		iface->timings.mode = onfi_find_closest_sdr_mode(spec_timings);

		 
		ret = ops->setup_interface(chip, NAND_DATA_IFACE_CHECK_ONLY,
					   iface);
		if (!ret) {
			chip->best_interface_config = iface;
			return ret;
		}

		 
		best_mode = iface->timings.mode;
	} else if (chip->parameters.onfi) {
		best_mode = fls(chip->parameters.onfi->sdr_timing_modes) - 1;
	}

	for (mode = best_mode; mode >= 0; mode--) {
		onfi_fill_interface_config(chip, iface, NAND_SDR_IFACE, mode);

		ret = ops->setup_interface(chip, NAND_DATA_IFACE_CHECK_ONLY,
					   iface);
		if (!ret) {
			chip->best_interface_config = iface;
			break;
		}
	}

	return ret;
}

 
int nand_choose_best_nvddr_timings(struct nand_chip *chip,
				   struct nand_interface_config *iface,
				   struct nand_nvddr_timings *spec_timings)
{
	const struct nand_controller_ops *ops = chip->controller->ops;
	int best_mode = 0, mode, ret = -EOPNOTSUPP;

	iface->type = NAND_NVDDR_IFACE;

	if (spec_timings) {
		iface->timings.nvddr = *spec_timings;
		iface->timings.mode = onfi_find_closest_nvddr_mode(spec_timings);

		 
		ret = ops->setup_interface(chip, NAND_DATA_IFACE_CHECK_ONLY,
					   iface);
		if (!ret) {
			chip->best_interface_config = iface;
			return ret;
		}

		 
		best_mode = iface->timings.mode;
	} else if (chip->parameters.onfi) {
		best_mode = fls(chip->parameters.onfi->nvddr_timing_modes) - 1;
	}

	for (mode = best_mode; mode >= 0; mode--) {
		onfi_fill_interface_config(chip, iface, NAND_NVDDR_IFACE, mode);

		ret = ops->setup_interface(chip, NAND_DATA_IFACE_CHECK_ONLY,
					   iface);
		if (!ret) {
			chip->best_interface_config = iface;
			break;
		}
	}

	return ret;
}

 
static int nand_choose_best_timings(struct nand_chip *chip,
				    struct nand_interface_config *iface)
{
	int ret;

	 
	ret = nand_choose_best_nvddr_timings(chip, iface, NULL);
	if (!ret)
		return 0;

	 
	return nand_choose_best_sdr_timings(chip, iface, NULL);
}

 
static int nand_choose_interface_config(struct nand_chip *chip)
{
	struct nand_interface_config *iface;
	int ret;

	if (!nand_controller_can_setup_interface(chip))
		return 0;

	iface = kzalloc(sizeof(*iface), GFP_KERNEL);
	if (!iface)
		return -ENOMEM;

	if (chip->ops.choose_interface_config)
		ret = chip->ops.choose_interface_config(chip, iface);
	else
		ret = nand_choose_best_timings(chip, iface);

	if (ret)
		kfree(iface);

	return ret;
}

 
static int nand_fill_column_cycles(struct nand_chip *chip, u8 *addrs,
				   unsigned int offset_in_page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	 
	if (offset_in_page > mtd->writesize + mtd->oobsize)
		return -EINVAL;

	 
	if (mtd->writesize <= 512 && offset_in_page >= mtd->writesize)
		offset_in_page -= mtd->writesize;

	 
	if (chip->options & NAND_BUSWIDTH_16) {
		if (WARN_ON(offset_in_page % 2))
			return -EINVAL;

		offset_in_page /= 2;
	}

	addrs[0] = offset_in_page;

	 
	if (mtd->writesize <= 512)
		return 1;

	addrs[1] = offset_in_page >> 8;

	return 2;
}

static int nand_sp_exec_read_page_op(struct nand_chip *chip, unsigned int page,
				     unsigned int offset_in_page, void *buf,
				     unsigned int len)
{
	const struct nand_interface_config *conf =
		nand_get_interface_config(chip);
	struct mtd_info *mtd = nand_to_mtd(chip);
	u8 addrs[4];
	struct nand_op_instr instrs[] = {
		NAND_OP_CMD(NAND_CMD_READ0, 0),
		NAND_OP_ADDR(3, addrs, NAND_COMMON_TIMING_NS(conf, tWB_max)),
		NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tR_max),
				 NAND_COMMON_TIMING_NS(conf, tRR_min)),
		NAND_OP_DATA_IN(len, buf, 0),
	};
	struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
	int ret;

	 
	if (!len)
		op.ninstrs--;

	if (offset_in_page >= mtd->writesize)
		instrs[0].ctx.cmd.opcode = NAND_CMD_READOOB;
	else if (offset_in_page >= 256 &&
		 !(chip->options & NAND_BUSWIDTH_16))
		instrs[0].ctx.cmd.opcode = NAND_CMD_READ1;

	ret = nand_fill_column_cycles(chip, addrs, offset_in_page);
	if (ret < 0)
		return ret;

	addrs[1] = page;
	addrs[2] = page >> 8;

	if (chip->options & NAND_ROW_ADDR_3) {
		addrs[3] = page >> 16;
		instrs[1].ctx.addr.naddrs++;
	}

	return nand_exec_op(chip, &op);
}

static int nand_lp_exec_read_page_op(struct nand_chip *chip, unsigned int page,
				     unsigned int offset_in_page, void *buf,
				     unsigned int len)
{
	const struct nand_interface_config *conf =
		nand_get_interface_config(chip);
	u8 addrs[5];
	struct nand_op_instr instrs[] = {
		NAND_OP_CMD(NAND_CMD_READ0, 0),
		NAND_OP_ADDR(4, addrs, 0),
		NAND_OP_CMD(NAND_CMD_READSTART, NAND_COMMON_TIMING_NS(conf, tWB_max)),
		NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tR_max),
				 NAND_COMMON_TIMING_NS(conf, tRR_min)),
		NAND_OP_DATA_IN(len, buf, 0),
	};
	struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
	int ret;

	 
	if (!len)
		op.ninstrs--;

	ret = nand_fill_column_cycles(chip, addrs, offset_in_page);
	if (ret < 0)
		return ret;

	addrs[2] = page;
	addrs[3] = page >> 8;

	if (chip->options & NAND_ROW_ADDR_3) {
		addrs[4] = page >> 16;
		instrs[1].ctx.addr.naddrs++;
	}

	return nand_exec_op(chip, &op);
}

static int nand_lp_exec_cont_read_page_op(struct nand_chip *chip, unsigned int page,
					  unsigned int offset_in_page, void *buf,
					  unsigned int len, bool check_only)
{
	const struct nand_interface_config *conf =
		nand_get_interface_config(chip);
	u8 addrs[5];
	struct nand_op_instr start_instrs[] = {
		NAND_OP_CMD(NAND_CMD_READ0, 0),
		NAND_OP_ADDR(4, addrs, 0),
		NAND_OP_CMD(NAND_CMD_READSTART, NAND_COMMON_TIMING_NS(conf, tWB_max)),
		NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tR_max), 0),
		NAND_OP_CMD(NAND_CMD_READCACHESEQ, NAND_COMMON_TIMING_NS(conf, tWB_max)),
		NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tR_max),
				 NAND_COMMON_TIMING_NS(conf, tRR_min)),
		NAND_OP_DATA_IN(len, buf, 0),
	};
	struct nand_op_instr cont_instrs[] = {
		NAND_OP_CMD(page == chip->cont_read.last_page ?
			    NAND_CMD_READCACHEEND : NAND_CMD_READCACHESEQ,
			    NAND_COMMON_TIMING_NS(conf, tWB_max)),
		NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tR_max),
				 NAND_COMMON_TIMING_NS(conf, tRR_min)),
		NAND_OP_DATA_IN(len, buf, 0),
	};
	struct nand_operation start_op = NAND_OPERATION(chip->cur_cs, start_instrs);
	struct nand_operation cont_op = NAND_OPERATION(chip->cur_cs, cont_instrs);
	int ret;

	if (!len) {
		start_op.ninstrs--;
		cont_op.ninstrs--;
	}

	ret = nand_fill_column_cycles(chip, addrs, offset_in_page);
	if (ret < 0)
		return ret;

	addrs[2] = page;
	addrs[3] = page >> 8;

	if (chip->options & NAND_ROW_ADDR_3) {
		addrs[4] = page >> 16;
		start_instrs[1].ctx.addr.naddrs++;
	}

	 
	if (check_only) {
		if (nand_check_op(chip, &start_op) || nand_check_op(chip, &cont_op))
			return -EOPNOTSUPP;

		return 0;
	}

	if (page == chip->cont_read.first_page)
		return nand_exec_op(chip, &start_op);
	else
		return nand_exec_op(chip, &cont_op);
}

static bool rawnand_cont_read_ongoing(struct nand_chip *chip, unsigned int page)
{
	return chip->cont_read.ongoing &&
		page >= chip->cont_read.first_page &&
		page <= chip->cont_read.last_page;
}

 
int nand_read_page_op(struct nand_chip *chip, unsigned int page,
		      unsigned int offset_in_page, void *buf, unsigned int len)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (len && !buf)
		return -EINVAL;

	if (offset_in_page + len > mtd->writesize + mtd->oobsize)
		return -EINVAL;

	if (nand_has_exec_op(chip)) {
		if (mtd->writesize > 512) {
			if (rawnand_cont_read_ongoing(chip, page))
				return nand_lp_exec_cont_read_page_op(chip, page,
								      offset_in_page,
								      buf, len, false);
			else
				return nand_lp_exec_read_page_op(chip, page,
								 offset_in_page, buf,
								 len);
		}

		return nand_sp_exec_read_page_op(chip, page, offset_in_page,
						 buf, len);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_READ0, offset_in_page, page);
	if (len)
		chip->legacy.read_buf(chip, buf, len);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_read_page_op);

 
int nand_read_param_page_op(struct nand_chip *chip, u8 page, void *buf,
			    unsigned int len)
{
	unsigned int i;
	u8 *p = buf;

	if (len && !buf)
		return -EINVAL;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_PARAM, 0),
			NAND_OP_ADDR(1, &page,
				     NAND_COMMON_TIMING_NS(conf, tWB_max)),
			NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tR_max),
					 NAND_COMMON_TIMING_NS(conf, tRR_min)),
			NAND_OP_8BIT_DATA_IN(len, buf, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		 
		if (!len)
			op.ninstrs--;

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_PARAM, page, -1);
	for (i = 0; i < len; i++)
		p[i] = chip->legacy.read_byte(chip);

	return 0;
}

 
int nand_change_read_column_op(struct nand_chip *chip,
			       unsigned int offset_in_page, void *buf,
			       unsigned int len, bool force_8bit)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (len && !buf)
		return -EINVAL;

	if (offset_in_page + len > mtd->writesize + mtd->oobsize)
		return -EINVAL;

	 
	if (mtd->writesize <= 512)
		return -ENOTSUPP;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		u8 addrs[2] = {};
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_RNDOUT, 0),
			NAND_OP_ADDR(2, addrs, 0),
			NAND_OP_CMD(NAND_CMD_RNDOUTSTART,
				    NAND_COMMON_TIMING_NS(conf, tCCS_min)),
			NAND_OP_DATA_IN(len, buf, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
		int ret;

		ret = nand_fill_column_cycles(chip, addrs, offset_in_page);
		if (ret < 0)
			return ret;

		 
		if (!len)
			op.ninstrs--;

		instrs[3].ctx.data.force_8bit = force_8bit;

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_RNDOUT, offset_in_page, -1);
	if (len)
		chip->legacy.read_buf(chip, buf, len);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_change_read_column_op);

 
int nand_read_oob_op(struct nand_chip *chip, unsigned int page,
		     unsigned int offset_in_oob, void *buf, unsigned int len)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (len && !buf)
		return -EINVAL;

	if (offset_in_oob + len > mtd->oobsize)
		return -EINVAL;

	if (nand_has_exec_op(chip))
		return nand_read_page_op(chip, page,
					 mtd->writesize + offset_in_oob,
					 buf, len);

	chip->legacy.cmdfunc(chip, NAND_CMD_READOOB, offset_in_oob, page);
	if (len)
		chip->legacy.read_buf(chip, buf, len);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_read_oob_op);

static int nand_exec_prog_page_op(struct nand_chip *chip, unsigned int page,
				  unsigned int offset_in_page, const void *buf,
				  unsigned int len, bool prog)
{
	const struct nand_interface_config *conf =
		nand_get_interface_config(chip);
	struct mtd_info *mtd = nand_to_mtd(chip);
	u8 addrs[5] = {};
	struct nand_op_instr instrs[] = {
		 
		NAND_OP_CMD(NAND_CMD_READ0, 0),
		NAND_OP_CMD(NAND_CMD_SEQIN, 0),
		NAND_OP_ADDR(0, addrs, NAND_COMMON_TIMING_NS(conf, tADL_min)),
		NAND_OP_DATA_OUT(len, buf, 0),
		NAND_OP_CMD(NAND_CMD_PAGEPROG,
			    NAND_COMMON_TIMING_NS(conf, tWB_max)),
		NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tPROG_max), 0),
	};
	struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
	int naddrs = nand_fill_column_cycles(chip, addrs, offset_in_page);

	if (naddrs < 0)
		return naddrs;

	addrs[naddrs++] = page;
	addrs[naddrs++] = page >> 8;
	if (chip->options & NAND_ROW_ADDR_3)
		addrs[naddrs++] = page >> 16;

	instrs[2].ctx.addr.naddrs = naddrs;

	 
	if (!prog) {
		op.ninstrs -= 2;
		 
		if (!len)
			op.ninstrs--;
	}

	if (mtd->writesize <= 512) {
		 
		if (offset_in_page >= mtd->writesize)
			instrs[0].ctx.cmd.opcode = NAND_CMD_READOOB;
		else if (offset_in_page >= 256 &&
			 !(chip->options & NAND_BUSWIDTH_16))
			instrs[0].ctx.cmd.opcode = NAND_CMD_READ1;
	} else {
		 
		op.instrs++;
		op.ninstrs--;
	}

	return nand_exec_op(chip, &op);
}

 
int nand_prog_page_begin_op(struct nand_chip *chip, unsigned int page,
			    unsigned int offset_in_page, const void *buf,
			    unsigned int len)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (len && !buf)
		return -EINVAL;

	if (offset_in_page + len > mtd->writesize + mtd->oobsize)
		return -EINVAL;

	if (nand_has_exec_op(chip))
		return nand_exec_prog_page_op(chip, page, offset_in_page, buf,
					      len, false);

	chip->legacy.cmdfunc(chip, NAND_CMD_SEQIN, offset_in_page, page);

	if (buf)
		chip->legacy.write_buf(chip, buf, len);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_prog_page_begin_op);

 
int nand_prog_page_end_op(struct nand_chip *chip)
{
	int ret;
	u8 status;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_PAGEPROG,
				    NAND_COMMON_TIMING_NS(conf, tWB_max)),
			NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tPROG_max),
					 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		ret = nand_exec_op(chip, &op);
		if (ret)
			return ret;

		ret = nand_status_op(chip, &status);
		if (ret)
			return ret;
	} else {
		chip->legacy.cmdfunc(chip, NAND_CMD_PAGEPROG, -1, -1);
		ret = chip->legacy.waitfunc(chip);
		if (ret < 0)
			return ret;

		status = ret;
	}

	if (status & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL_GPL(nand_prog_page_end_op);

 
int nand_prog_page_op(struct nand_chip *chip, unsigned int page,
		      unsigned int offset_in_page, const void *buf,
		      unsigned int len)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	u8 status;
	int ret;

	if (!len || !buf)
		return -EINVAL;

	if (offset_in_page + len > mtd->writesize + mtd->oobsize)
		return -EINVAL;

	if (nand_has_exec_op(chip)) {
		ret = nand_exec_prog_page_op(chip, page, offset_in_page, buf,
						len, true);
		if (ret)
			return ret;

		ret = nand_status_op(chip, &status);
		if (ret)
			return ret;
	} else {
		chip->legacy.cmdfunc(chip, NAND_CMD_SEQIN, offset_in_page,
				     page);
		chip->legacy.write_buf(chip, buf, len);
		chip->legacy.cmdfunc(chip, NAND_CMD_PAGEPROG, -1, -1);
		ret = chip->legacy.waitfunc(chip);
		if (ret < 0)
			return ret;

		status = ret;
	}

	if (status & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL_GPL(nand_prog_page_op);

 
int nand_change_write_column_op(struct nand_chip *chip,
				unsigned int offset_in_page,
				const void *buf, unsigned int len,
				bool force_8bit)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (len && !buf)
		return -EINVAL;

	if (offset_in_page + len > mtd->writesize + mtd->oobsize)
		return -EINVAL;

	 
	if (mtd->writesize <= 512)
		return -ENOTSUPP;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		u8 addrs[2];
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_RNDIN, 0),
			NAND_OP_ADDR(2, addrs, NAND_COMMON_TIMING_NS(conf, tCCS_min)),
			NAND_OP_DATA_OUT(len, buf, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
		int ret;

		ret = nand_fill_column_cycles(chip, addrs, offset_in_page);
		if (ret < 0)
			return ret;

		instrs[2].ctx.data.force_8bit = force_8bit;

		 
		if (!len)
			op.ninstrs--;

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_RNDIN, offset_in_page, -1);
	if (len)
		chip->legacy.write_buf(chip, buf, len);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_change_write_column_op);

 
int nand_readid_op(struct nand_chip *chip, u8 addr, void *buf,
		   unsigned int len)
{
	unsigned int i;
	u8 *id = buf, *ddrbuf = NULL;

	if (len && !buf)
		return -EINVAL;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_READID, 0),
			NAND_OP_ADDR(1, &addr,
				     NAND_COMMON_TIMING_NS(conf, tADL_min)),
			NAND_OP_8BIT_DATA_IN(len, buf, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
		int ret;

		 
		if (len && nand_interface_is_nvddr(conf)) {
			ddrbuf = kzalloc(len * 2, GFP_KERNEL);
			if (!ddrbuf)
				return -ENOMEM;

			instrs[2].ctx.data.len *= 2;
			instrs[2].ctx.data.buf.in = ddrbuf;
		}

		 
		if (!len)
			op.ninstrs--;

		ret = nand_exec_op(chip, &op);
		if (!ret && len && nand_interface_is_nvddr(conf)) {
			for (i = 0; i < len; i++)
				id[i] = ddrbuf[i * 2];
		}

		kfree(ddrbuf);

		return ret;
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_READID, addr, -1);

	for (i = 0; i < len; i++)
		id[i] = chip->legacy.read_byte(chip);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_readid_op);

 
int nand_status_op(struct nand_chip *chip, u8 *status)
{
	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		u8 ddrstatus[2];
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_STATUS,
				    NAND_COMMON_TIMING_NS(conf, tADL_min)),
			NAND_OP_8BIT_DATA_IN(1, status, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
		int ret;

		 
		if (status && nand_interface_is_nvddr(conf)) {
			instrs[1].ctx.data.len *= 2;
			instrs[1].ctx.data.buf.in = ddrstatus;
		}

		if (!status)
			op.ninstrs--;

		ret = nand_exec_op(chip, &op);
		if (!ret && status && nand_interface_is_nvddr(conf))
			*status = ddrstatus[0];

		return ret;
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_STATUS, -1, -1);
	if (status)
		*status = chip->legacy.read_byte(chip);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_status_op);

 
int nand_exit_status_op(struct nand_chip *chip)
{
	if (nand_has_exec_op(chip)) {
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_READ0, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_READ0, -1, -1);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_exit_status_op);

 
int nand_erase_op(struct nand_chip *chip, unsigned int eraseblock)
{
	unsigned int page = eraseblock <<
			    (chip->phys_erase_shift - chip->page_shift);
	int ret;
	u8 status;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		u8 addrs[3] = {	page, page >> 8, page >> 16 };
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_ERASE1, 0),
			NAND_OP_ADDR(2, addrs, 0),
			NAND_OP_CMD(NAND_CMD_ERASE2,
				    NAND_COMMON_TIMING_NS(conf, tWB_max)),
			NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tBERS_max),
					 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		if (chip->options & NAND_ROW_ADDR_3)
			instrs[1].ctx.addr.naddrs++;

		ret = nand_exec_op(chip, &op);
		if (ret)
			return ret;

		ret = nand_status_op(chip, &status);
		if (ret)
			return ret;
	} else {
		chip->legacy.cmdfunc(chip, NAND_CMD_ERASE1, -1, page);
		chip->legacy.cmdfunc(chip, NAND_CMD_ERASE2, -1, -1);

		ret = chip->legacy.waitfunc(chip);
		if (ret < 0)
			return ret;

		status = ret;
	}

	if (status & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL_GPL(nand_erase_op);

 
static int nand_set_features_op(struct nand_chip *chip, u8 feature,
				const void *data)
{
	const u8 *params = data;
	int i, ret;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_SET_FEATURES, 0),
			NAND_OP_ADDR(1, &feature, NAND_COMMON_TIMING_NS(conf,
									tADL_min)),
			NAND_OP_8BIT_DATA_OUT(ONFI_SUBFEATURE_PARAM_LEN, data,
					      NAND_COMMON_TIMING_NS(conf,
								    tWB_max)),
			NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tFEAT_max),
					 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_SET_FEATURES, feature, -1);
	for (i = 0; i < ONFI_SUBFEATURE_PARAM_LEN; ++i)
		chip->legacy.write_byte(chip, params[i]);

	ret = chip->legacy.waitfunc(chip);
	if (ret < 0)
		return ret;

	if (ret & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}

 
static int nand_get_features_op(struct nand_chip *chip, u8 feature,
				void *data)
{
	u8 *params = data, ddrbuf[ONFI_SUBFEATURE_PARAM_LEN * 2];
	int i;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_GET_FEATURES, 0),
			NAND_OP_ADDR(1, &feature,
				     NAND_COMMON_TIMING_NS(conf, tWB_max)),
			NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tFEAT_max),
					 NAND_COMMON_TIMING_NS(conf, tRR_min)),
			NAND_OP_8BIT_DATA_IN(ONFI_SUBFEATURE_PARAM_LEN,
					     data, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
		int ret;

		 
		if (nand_interface_is_nvddr(conf)) {
			instrs[3].ctx.data.len *= 2;
			instrs[3].ctx.data.buf.in = ddrbuf;
		}

		ret = nand_exec_op(chip, &op);
		if (nand_interface_is_nvddr(conf)) {
			for (i = 0; i < ONFI_SUBFEATURE_PARAM_LEN; i++)
				params[i] = ddrbuf[i * 2];
		}

		return ret;
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_GET_FEATURES, feature, -1);
	for (i = 0; i < ONFI_SUBFEATURE_PARAM_LEN; ++i)
		params[i] = chip->legacy.read_byte(chip);

	return 0;
}

static int nand_wait_rdy_op(struct nand_chip *chip, unsigned int timeout_ms,
			    unsigned int delay_ns)
{
	if (nand_has_exec_op(chip)) {
		struct nand_op_instr instrs[] = {
			NAND_OP_WAIT_RDY(PSEC_TO_MSEC(timeout_ms),
					 PSEC_TO_NSEC(delay_ns)),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		return nand_exec_op(chip, &op);
	}

	 
	if (!chip->legacy.dev_ready)
		udelay(chip->legacy.chip_delay);
	else
		nand_wait_ready(chip);

	return 0;
}

 
int nand_reset_op(struct nand_chip *chip)
{
	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(NAND_CMD_RESET,
				    NAND_COMMON_TIMING_NS(conf, tWB_max)),
			NAND_OP_WAIT_RDY(NAND_COMMON_TIMING_MS(conf, tRST_max),
					 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_RESET, -1, -1);

	return 0;
}
EXPORT_SYMBOL_GPL(nand_reset_op);

 
int nand_read_data_op(struct nand_chip *chip, void *buf, unsigned int len,
		      bool force_8bit, bool check_only)
{
	if (!len || !buf)
		return -EINVAL;

	if (nand_has_exec_op(chip)) {
		const struct nand_interface_config *conf =
			nand_get_interface_config(chip);
		struct nand_op_instr instrs[] = {
			NAND_OP_DATA_IN(len, buf, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);
		u8 *ddrbuf = NULL;
		int ret, i;

		instrs[0].ctx.data.force_8bit = force_8bit;

		 
		if (force_8bit && nand_interface_is_nvddr(conf)) {
			ddrbuf = kzalloc(len * 2, GFP_KERNEL);
			if (!ddrbuf)
				return -ENOMEM;

			instrs[0].ctx.data.len *= 2;
			instrs[0].ctx.data.buf.in = ddrbuf;
		}

		if (check_only) {
			ret = nand_check_op(chip, &op);
			kfree(ddrbuf);
			return ret;
		}

		ret = nand_exec_op(chip, &op);
		if (!ret && force_8bit && nand_interface_is_nvddr(conf)) {
			u8 *dst = buf;

			for (i = 0; i < len; i++)
				dst[i] = ddrbuf[i * 2];
		}

		kfree(ddrbuf);

		return ret;
	}

	if (check_only)
		return 0;

	if (force_8bit) {
		u8 *p = buf;
		unsigned int i;

		for (i = 0; i < len; i++)
			p[i] = chip->legacy.read_byte(chip);
	} else {
		chip->legacy.read_buf(chip, buf, len);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(nand_read_data_op);

 
int nand_write_data_op(struct nand_chip *chip, const void *buf,
		       unsigned int len, bool force_8bit)
{
	if (!len || !buf)
		return -EINVAL;

	if (nand_has_exec_op(chip)) {
		struct nand_op_instr instrs[] = {
			NAND_OP_DATA_OUT(len, buf, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		instrs[0].ctx.data.force_8bit = force_8bit;

		return nand_exec_op(chip, &op);
	}

	if (force_8bit) {
		const u8 *p = buf;
		unsigned int i;

		for (i = 0; i < len; i++)
			chip->legacy.write_byte(chip, p[i]);
	} else {
		chip->legacy.write_buf(chip, buf, len);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(nand_write_data_op);

 
struct nand_op_parser_ctx {
	const struct nand_op_instr *instrs;
	unsigned int ninstrs;
	struct nand_subop subop;
};

 
static bool
nand_op_parser_must_split_instr(const struct nand_op_parser_pattern_elem *pat,
				const struct nand_op_instr *instr,
				unsigned int *start_offset)
{
	switch (pat->type) {
	case NAND_OP_ADDR_INSTR:
		if (!pat->ctx.addr.maxcycles)
			break;

		if (instr->ctx.addr.naddrs - *start_offset >
		    pat->ctx.addr.maxcycles) {
			*start_offset += pat->ctx.addr.maxcycles;
			return true;
		}
		break;

	case NAND_OP_DATA_IN_INSTR:
	case NAND_OP_DATA_OUT_INSTR:
		if (!pat->ctx.data.maxlen)
			break;

		if (instr->ctx.data.len - *start_offset >
		    pat->ctx.data.maxlen) {
			*start_offset += pat->ctx.data.maxlen;
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

 
static bool
nand_op_parser_match_pat(const struct nand_op_parser_pattern *pat,
			 struct nand_op_parser_ctx *ctx)
{
	unsigned int instr_offset = ctx->subop.first_instr_start_off;
	const struct nand_op_instr *end = ctx->instrs + ctx->ninstrs;
	const struct nand_op_instr *instr = ctx->subop.instrs;
	unsigned int i, ninstrs;

	for (i = 0, ninstrs = 0; i < pat->nelems && instr < end; i++) {
		 
		if (instr->type != pat->elems[i].type) {
			if (!pat->elems[i].optional)
				return false;

			continue;
		}

		 
		if (nand_op_parser_must_split_instr(&pat->elems[i], instr,
						    &instr_offset)) {
			ninstrs++;
			i++;
			break;
		}

		instr++;
		ninstrs++;
		instr_offset = 0;
	}

	 
	if (!ninstrs)
		return false;

	 
	for (; i < pat->nelems; i++) {
		if (!pat->elems[i].optional)
			return false;
	}

	 
	ctx->subop.ninstrs = ninstrs;
	ctx->subop.last_instr_end_off = instr_offset;

	return true;
}

#if IS_ENABLED(CONFIG_DYNAMIC_DEBUG) || defined(DEBUG)
static void nand_op_parser_trace(const struct nand_op_parser_ctx *ctx)
{
	const struct nand_op_instr *instr;
	char *prefix = "      ";
	unsigned int i;

	pr_debug("executing subop (CS%d):\n", ctx->subop.cs);

	for (i = 0; i < ctx->ninstrs; i++) {
		instr = &ctx->instrs[i];

		if (instr == &ctx->subop.instrs[0])
			prefix = "    ->";

		nand_op_trace(prefix, instr);

		if (instr == &ctx->subop.instrs[ctx->subop.ninstrs - 1])
			prefix = "      ";
	}
}
#else
static void nand_op_parser_trace(const struct nand_op_parser_ctx *ctx)
{
	 
}
#endif

static int nand_op_parser_cmp_ctx(const struct nand_op_parser_ctx *a,
				  const struct nand_op_parser_ctx *b)
{
	if (a->subop.ninstrs < b->subop.ninstrs)
		return -1;
	else if (a->subop.ninstrs > b->subop.ninstrs)
		return 1;

	if (a->subop.last_instr_end_off < b->subop.last_instr_end_off)
		return -1;
	else if (a->subop.last_instr_end_off > b->subop.last_instr_end_off)
		return 1;

	return 0;
}

 
int nand_op_parser_exec_op(struct nand_chip *chip,
			   const struct nand_op_parser *parser,
			   const struct nand_operation *op, bool check_only)
{
	struct nand_op_parser_ctx ctx = {
		.subop.cs = op->cs,
		.subop.instrs = op->instrs,
		.instrs = op->instrs,
		.ninstrs = op->ninstrs,
	};
	unsigned int i;

	while (ctx.subop.instrs < op->instrs + op->ninstrs) {
		const struct nand_op_parser_pattern *pattern;
		struct nand_op_parser_ctx best_ctx;
		int ret, best_pattern = -1;

		for (i = 0; i < parser->npatterns; i++) {
			struct nand_op_parser_ctx test_ctx = ctx;

			pattern = &parser->patterns[i];
			if (!nand_op_parser_match_pat(pattern, &test_ctx))
				continue;

			if (best_pattern >= 0 &&
			    nand_op_parser_cmp_ctx(&test_ctx, &best_ctx) <= 0)
				continue;

			best_pattern = i;
			best_ctx = test_ctx;
		}

		if (best_pattern < 0) {
			pr_debug("->exec_op() parser: pattern not found!\n");
			return -ENOTSUPP;
		}

		ctx = best_ctx;
		nand_op_parser_trace(&ctx);

		if (!check_only) {
			pattern = &parser->patterns[best_pattern];
			ret = pattern->exec(chip, &ctx.subop);
			if (ret)
				return ret;
		}

		 
		ctx.subop.instrs = ctx.subop.instrs + ctx.subop.ninstrs;
		if (ctx.subop.last_instr_end_off)
			ctx.subop.instrs -= 1;

		ctx.subop.first_instr_start_off = ctx.subop.last_instr_end_off;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(nand_op_parser_exec_op);

static bool nand_instr_is_data(const struct nand_op_instr *instr)
{
	return instr && (instr->type == NAND_OP_DATA_IN_INSTR ||
			 instr->type == NAND_OP_DATA_OUT_INSTR);
}

static bool nand_subop_instr_is_valid(const struct nand_subop *subop,
				      unsigned int instr_idx)
{
	return subop && instr_idx < subop->ninstrs;
}

static unsigned int nand_subop_get_start_off(const struct nand_subop *subop,
					     unsigned int instr_idx)
{
	if (instr_idx)
		return 0;

	return subop->first_instr_start_off;
}

 
unsigned int nand_subop_get_addr_start_off(const struct nand_subop *subop,
					   unsigned int instr_idx)
{
	if (WARN_ON(!nand_subop_instr_is_valid(subop, instr_idx) ||
		    subop->instrs[instr_idx].type != NAND_OP_ADDR_INSTR))
		return 0;

	return nand_subop_get_start_off(subop, instr_idx);
}
EXPORT_SYMBOL_GPL(nand_subop_get_addr_start_off);

 
unsigned int nand_subop_get_num_addr_cyc(const struct nand_subop *subop,
					 unsigned int instr_idx)
{
	int start_off, end_off;

	if (WARN_ON(!nand_subop_instr_is_valid(subop, instr_idx) ||
		    subop->instrs[instr_idx].type != NAND_OP_ADDR_INSTR))
		return 0;

	start_off = nand_subop_get_addr_start_off(subop, instr_idx);

	if (instr_idx == subop->ninstrs - 1 &&
	    subop->last_instr_end_off)
		end_off = subop->last_instr_end_off;
	else
		end_off = subop->instrs[instr_idx].ctx.addr.naddrs;

	return end_off - start_off;
}
EXPORT_SYMBOL_GPL(nand_subop_get_num_addr_cyc);

 
unsigned int nand_subop_get_data_start_off(const struct nand_subop *subop,
					   unsigned int instr_idx)
{
	if (WARN_ON(!nand_subop_instr_is_valid(subop, instr_idx) ||
		    !nand_instr_is_data(&subop->instrs[instr_idx])))
		return 0;

	return nand_subop_get_start_off(subop, instr_idx);
}
EXPORT_SYMBOL_GPL(nand_subop_get_data_start_off);

 
unsigned int nand_subop_get_data_len(const struct nand_subop *subop,
				     unsigned int instr_idx)
{
	int start_off = 0, end_off;

	if (WARN_ON(!nand_subop_instr_is_valid(subop, instr_idx) ||
		    !nand_instr_is_data(&subop->instrs[instr_idx])))
		return 0;

	start_off = nand_subop_get_data_start_off(subop, instr_idx);

	if (instr_idx == subop->ninstrs - 1 &&
	    subop->last_instr_end_off)
		end_off = subop->last_instr_end_off;
	else
		end_off = subop->instrs[instr_idx].ctx.data.len;

	return end_off - start_off;
}
EXPORT_SYMBOL_GPL(nand_subop_get_data_len);

 
int nand_reset(struct nand_chip *chip, int chipnr)
{
	int ret;

	ret = nand_reset_interface(chip, chipnr);
	if (ret)
		return ret;

	 
	nand_select_target(chip, chipnr);
	ret = nand_reset_op(chip);
	nand_deselect_target(chip);
	if (ret)
		return ret;

	ret = nand_setup_interface(chip, chipnr);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(nand_reset);

 
int nand_get_features(struct nand_chip *chip, int addr,
		      u8 *subfeature_param)
{
	if (!nand_supports_get_features(chip, addr))
		return -ENOTSUPP;

	if (chip->legacy.get_features)
		return chip->legacy.get_features(chip, addr, subfeature_param);

	return nand_get_features_op(chip, addr, subfeature_param);
}

 
int nand_set_features(struct nand_chip *chip, int addr,
		      u8 *subfeature_param)
{
	if (!nand_supports_set_features(chip, addr))
		return -ENOTSUPP;

	if (chip->legacy.set_features)
		return chip->legacy.set_features(chip, addr, subfeature_param);

	return nand_set_features_op(chip, addr, subfeature_param);
}

 
static int nand_check_erased_buf(void *buf, int len, int bitflips_threshold)
{
	const unsigned char *bitmap = buf;
	int bitflips = 0;
	int weight;

	for (; len && ((uintptr_t)bitmap) % sizeof(long);
	     len--, bitmap++) {
		weight = hweight8(*bitmap);
		bitflips += BITS_PER_BYTE - weight;
		if (unlikely(bitflips > bitflips_threshold))
			return -EBADMSG;
	}

	for (; len >= sizeof(long);
	     len -= sizeof(long), bitmap += sizeof(long)) {
		unsigned long d = *((unsigned long *)bitmap);
		if (d == ~0UL)
			continue;
		weight = hweight_long(d);
		bitflips += BITS_PER_LONG - weight;
		if (unlikely(bitflips > bitflips_threshold))
			return -EBADMSG;
	}

	for (; len > 0; len--, bitmap++) {
		weight = hweight8(*bitmap);
		bitflips += BITS_PER_BYTE - weight;
		if (unlikely(bitflips > bitflips_threshold))
			return -EBADMSG;
	}

	return bitflips;
}

 
int nand_check_erased_ecc_chunk(void *data, int datalen,
				void *ecc, int ecclen,
				void *extraoob, int extraooblen,
				int bitflips_threshold)
{
	int data_bitflips = 0, ecc_bitflips = 0, extraoob_bitflips = 0;

	data_bitflips = nand_check_erased_buf(data, datalen,
					      bitflips_threshold);
	if (data_bitflips < 0)
		return data_bitflips;

	bitflips_threshold -= data_bitflips;

	ecc_bitflips = nand_check_erased_buf(ecc, ecclen, bitflips_threshold);
	if (ecc_bitflips < 0)
		return ecc_bitflips;

	bitflips_threshold -= ecc_bitflips;

	extraoob_bitflips = nand_check_erased_buf(extraoob, extraooblen,
						  bitflips_threshold);
	if (extraoob_bitflips < 0)
		return extraoob_bitflips;

	if (data_bitflips)
		memset(data, 0xff, datalen);

	if (ecc_bitflips)
		memset(ecc, 0xff, ecclen);

	if (extraoob_bitflips)
		memset(extraoob, 0xff, extraooblen);

	return data_bitflips + ecc_bitflips + extraoob_bitflips;
}
EXPORT_SYMBOL(nand_check_erased_ecc_chunk);

 
int nand_read_page_raw_notsupp(struct nand_chip *chip, u8 *buf,
			       int oob_required, int page)
{
	return -ENOTSUPP;
}

 
int nand_read_page_raw(struct nand_chip *chip, uint8_t *buf, int oob_required,
		       int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret;

	ret = nand_read_page_op(chip, page, 0, buf, mtd->writesize);
	if (ret)
		return ret;

	if (oob_required) {
		ret = nand_read_data_op(chip, chip->oob_poi, mtd->oobsize,
					false, false);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(nand_read_page_raw);

 
int nand_monolithic_read_page_raw(struct nand_chip *chip, u8 *buf,
				  int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int size = mtd->writesize;
	u8 *read_buf = buf;
	int ret;

	if (oob_required) {
		size += mtd->oobsize;

		if (buf != chip->data_buf)
			read_buf = nand_get_data_buf(chip);
	}

	ret = nand_read_page_op(chip, page, 0, read_buf, size);
	if (ret)
		return ret;

	if (buf != chip->data_buf)
		memcpy(buf, read_buf, mtd->writesize);

	return 0;
}
EXPORT_SYMBOL(nand_monolithic_read_page_raw);

 
static int nand_read_page_raw_syndrome(struct nand_chip *chip, uint8_t *buf,
				       int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	uint8_t *oob = chip->oob_poi;
	int steps, size, ret;

	ret = nand_read_page_op(chip, page, 0, NULL, 0);
	if (ret)
		return ret;

	for (steps = chip->ecc.steps; steps > 0; steps--) {
		ret = nand_read_data_op(chip, buf, eccsize, false, false);
		if (ret)
			return ret;

		buf += eccsize;

		if (chip->ecc.prepad) {
			ret = nand_read_data_op(chip, oob, chip->ecc.prepad,
						false, false);
			if (ret)
				return ret;

			oob += chip->ecc.prepad;
		}

		ret = nand_read_data_op(chip, oob, eccbytes, false, false);
		if (ret)
			return ret;

		oob += eccbytes;

		if (chip->ecc.postpad) {
			ret = nand_read_data_op(chip, oob, chip->ecc.postpad,
						false, false);
			if (ret)
				return ret;

			oob += chip->ecc.postpad;
		}
	}

	size = mtd->oobsize - (oob - chip->oob_poi);
	if (size) {
		ret = nand_read_data_op(chip, oob, size, false, false);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int nand_read_page_swecc(struct nand_chip *chip, uint8_t *buf,
				int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i, eccsize = chip->ecc.size, ret;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->ecc.calc_buf;
	uint8_t *ecc_code = chip->ecc.code_buf;
	unsigned int max_bitflips = 0;

	chip->ecc.read_page_raw(chip, buf, 1, page);

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		chip->ecc.calculate(chip, p, &ecc_calc[i]);

	ret = mtd_ooblayout_get_eccbytes(mtd, ecc_code, chip->oob_poi, 0,
					 chip->ecc.total);
	if (ret)
		return ret;

	eccsteps = chip->ecc.steps;
	p = buf;

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = chip->ecc.correct(chip, p, &ecc_code[i], &ecc_calc[i]);
		if (stat < 0) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += stat;
			max_bitflips = max_t(unsigned int, max_bitflips, stat);
		}
	}
	return max_bitflips;
}

 
static int nand_read_subpage(struct nand_chip *chip, uint32_t data_offs,
			     uint32_t readlen, uint8_t *bufpoi, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int start_step, end_step, num_steps, ret;
	uint8_t *p;
	int data_col_addr, i, gaps = 0;
	int datafrag_len, eccfrag_len, aligned_len, aligned_pos;
	int busw = (chip->options & NAND_BUSWIDTH_16) ? 2 : 1;
	int index, section = 0;
	unsigned int max_bitflips = 0;
	struct mtd_oob_region oobregion = { };

	 
	start_step = data_offs / chip->ecc.size;
	end_step = (data_offs + readlen - 1) / chip->ecc.size;
	num_steps = end_step - start_step + 1;
	index = start_step * chip->ecc.bytes;

	 
	datafrag_len = num_steps * chip->ecc.size;
	eccfrag_len = num_steps * chip->ecc.bytes;

	data_col_addr = start_step * chip->ecc.size;
	 
	p = bufpoi + data_col_addr;
	ret = nand_read_page_op(chip, page, data_col_addr, p, datafrag_len);
	if (ret)
		return ret;

	 
	for (i = 0; i < eccfrag_len ; i += chip->ecc.bytes, p += chip->ecc.size)
		chip->ecc.calculate(chip, p, &chip->ecc.calc_buf[i]);

	 
	ret = mtd_ooblayout_find_eccregion(mtd, index, &section, &oobregion);
	if (ret)
		return ret;

	if (oobregion.length < eccfrag_len)
		gaps = 1;

	if (gaps) {
		ret = nand_change_read_column_op(chip, mtd->writesize,
						 chip->oob_poi, mtd->oobsize,
						 false);
		if (ret)
			return ret;
	} else {
		 
		aligned_pos = oobregion.offset & ~(busw - 1);
		aligned_len = eccfrag_len;
		if (oobregion.offset & (busw - 1))
			aligned_len++;
		if ((oobregion.offset + (num_steps * chip->ecc.bytes)) &
		    (busw - 1))
			aligned_len++;

		ret = nand_change_read_column_op(chip,
						 mtd->writesize + aligned_pos,
						 &chip->oob_poi[aligned_pos],
						 aligned_len, false);
		if (ret)
			return ret;
	}

	ret = mtd_ooblayout_get_eccbytes(mtd, chip->ecc.code_buf,
					 chip->oob_poi, index, eccfrag_len);
	if (ret)
		return ret;

	p = bufpoi + data_col_addr;
	for (i = 0; i < eccfrag_len ; i += chip->ecc.bytes, p += chip->ecc.size) {
		int stat;

		stat = chip->ecc.correct(chip, p, &chip->ecc.code_buf[i],
					 &chip->ecc.calc_buf[i]);
		if (stat == -EBADMSG &&
		    (chip->ecc.options & NAND_ECC_GENERIC_ERASED_CHECK)) {
			 
			stat = nand_check_erased_ecc_chunk(p, chip->ecc.size,
						&chip->ecc.code_buf[i],
						chip->ecc.bytes,
						NULL, 0,
						chip->ecc.strength);
		}

		if (stat < 0) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += stat;
			max_bitflips = max_t(unsigned int, max_bitflips, stat);
		}
	}
	return max_bitflips;
}

 
static int nand_read_page_hwecc(struct nand_chip *chip, uint8_t *buf,
				int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i, eccsize = chip->ecc.size, ret;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->ecc.calc_buf;
	uint8_t *ecc_code = chip->ecc.code_buf;
	unsigned int max_bitflips = 0;

	ret = nand_read_page_op(chip, page, 0, NULL, 0);
	if (ret)
		return ret;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(chip, NAND_ECC_READ);

		ret = nand_read_data_op(chip, p, eccsize, false, false);
		if (ret)
			return ret;

		chip->ecc.calculate(chip, p, &ecc_calc[i]);
	}

	ret = nand_read_data_op(chip, chip->oob_poi, mtd->oobsize, false,
				false);
	if (ret)
		return ret;

	ret = mtd_ooblayout_get_eccbytes(mtd, ecc_code, chip->oob_poi, 0,
					 chip->ecc.total);
	if (ret)
		return ret;

	eccsteps = chip->ecc.steps;
	p = buf;

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = chip->ecc.correct(chip, p, &ecc_code[i], &ecc_calc[i]);
		if (stat == -EBADMSG &&
		    (chip->ecc.options & NAND_ECC_GENERIC_ERASED_CHECK)) {
			 
			stat = nand_check_erased_ecc_chunk(p, eccsize,
						&ecc_code[i], eccbytes,
						NULL, 0,
						chip->ecc.strength);
		}

		if (stat < 0) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += stat;
			max_bitflips = max_t(unsigned int, max_bitflips, stat);
		}
	}
	return max_bitflips;
}

 
int nand_read_page_hwecc_oob_first(struct nand_chip *chip, uint8_t *buf,
				   int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i, eccsize = chip->ecc.size, ret;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_code = chip->ecc.code_buf;
	unsigned int max_bitflips = 0;

	 
	ret = nand_read_oob_op(chip, page, 0, chip->oob_poi, mtd->oobsize);
	if (ret)
		return ret;

	 
	ret = nand_change_read_column_op(chip, 0, NULL, 0, false);
	if (ret)
		return ret;

	ret = mtd_ooblayout_get_eccbytes(mtd, ecc_code, chip->oob_poi, 0,
					 chip->ecc.total);
	if (ret)
		return ret;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		chip->ecc.hwctl(chip, NAND_ECC_READ);

		ret = nand_read_data_op(chip, p, eccsize, false, false);
		if (ret)
			return ret;

		stat = chip->ecc.correct(chip, p, &ecc_code[i], NULL);
		if (stat == -EBADMSG &&
		    (chip->ecc.options & NAND_ECC_GENERIC_ERASED_CHECK)) {
			 
			stat = nand_check_erased_ecc_chunk(p, eccsize,
							   &ecc_code[i],
							   eccbytes, NULL, 0,
							   chip->ecc.strength);
		}

		if (stat < 0) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += stat;
			max_bitflips = max_t(unsigned int, max_bitflips, stat);
		}
	}
	return max_bitflips;
}
EXPORT_SYMBOL_GPL(nand_read_page_hwecc_oob_first);

 
static int nand_read_page_syndrome(struct nand_chip *chip, uint8_t *buf,
				   int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret, i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int eccpadbytes = eccbytes + chip->ecc.prepad + chip->ecc.postpad;
	uint8_t *p = buf;
	uint8_t *oob = chip->oob_poi;
	unsigned int max_bitflips = 0;

	ret = nand_read_page_op(chip, page, 0, NULL, 0);
	if (ret)
		return ret;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		chip->ecc.hwctl(chip, NAND_ECC_READ);

		ret = nand_read_data_op(chip, p, eccsize, false, false);
		if (ret)
			return ret;

		if (chip->ecc.prepad) {
			ret = nand_read_data_op(chip, oob, chip->ecc.prepad,
						false, false);
			if (ret)
				return ret;

			oob += chip->ecc.prepad;
		}

		chip->ecc.hwctl(chip, NAND_ECC_READSYN);

		ret = nand_read_data_op(chip, oob, eccbytes, false, false);
		if (ret)
			return ret;

		stat = chip->ecc.correct(chip, p, oob, NULL);

		oob += eccbytes;

		if (chip->ecc.postpad) {
			ret = nand_read_data_op(chip, oob, chip->ecc.postpad,
						false, false);
			if (ret)
				return ret;

			oob += chip->ecc.postpad;
		}

		if (stat == -EBADMSG &&
		    (chip->ecc.options & NAND_ECC_GENERIC_ERASED_CHECK)) {
			 
			stat = nand_check_erased_ecc_chunk(p, chip->ecc.size,
							   oob - eccpadbytes,
							   eccpadbytes,
							   NULL, 0,
							   chip->ecc.strength);
		}

		if (stat < 0) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += stat;
			max_bitflips = max_t(unsigned int, max_bitflips, stat);
		}
	}

	 
	i = mtd->oobsize - (oob - chip->oob_poi);
	if (i) {
		ret = nand_read_data_op(chip, oob, i, false, false);
		if (ret)
			return ret;
	}

	return max_bitflips;
}

 
static uint8_t *nand_transfer_oob(struct nand_chip *chip, uint8_t *oob,
				  struct mtd_oob_ops *ops, size_t len)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret;

	switch (ops->mode) {

	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
		memcpy(oob, chip->oob_poi + ops->ooboffs, len);
		return oob + len;

	case MTD_OPS_AUTO_OOB:
		ret = mtd_ooblayout_get_databytes(mtd, oob, chip->oob_poi,
						  ops->ooboffs, len);
		BUG_ON(ret);
		return oob + len;

	default:
		BUG();
	}
	return NULL;
}

static void rawnand_enable_cont_reads(struct nand_chip *chip, unsigned int page,
				      u32 readlen, int col)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (!chip->controller->supported_op.cont_read)
		return;

	if ((col && col + readlen < (3 * mtd->writesize)) ||
	    (!col && readlen < (2 * mtd->writesize))) {
		chip->cont_read.ongoing = false;
		return;
	}

	chip->cont_read.ongoing = true;
	chip->cont_read.first_page = page;
	if (col)
		chip->cont_read.first_page++;
	chip->cont_read.last_page = page + ((readlen >> chip->page_shift) & chip->pagemask);
}

 
static int nand_setup_read_retry(struct nand_chip *chip, int retry_mode)
{
	pr_debug("setting READ RETRY mode %d\n", retry_mode);

	if (retry_mode >= chip->read_retries)
		return -EINVAL;

	if (!chip->ops.setup_read_retry)
		return -EOPNOTSUPP;

	return chip->ops.setup_read_retry(chip, retry_mode);
}

static void nand_wait_readrdy(struct nand_chip *chip)
{
	const struct nand_interface_config *conf;

	if (!(chip->options & NAND_NEED_READRDY))
		return;

	conf = nand_get_interface_config(chip);
	WARN_ON(nand_wait_rdy_op(chip, NAND_COMMON_TIMING_MS(conf, tR_max), 0));
}

 
static int nand_do_read_ops(struct nand_chip *chip, loff_t from,
			    struct mtd_oob_ops *ops)
{
	int chipnr, page, realpage, col, bytes, aligned, oob_required;
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret = 0;
	uint32_t readlen = ops->len;
	uint32_t oobreadlen = ops->ooblen;
	uint32_t max_oobsize = mtd_oobavail(mtd, ops);

	uint8_t *bufpoi, *oob, *buf;
	int use_bounce_buf;
	unsigned int max_bitflips = 0;
	int retry_mode = 0;
	bool ecc_fail = false;

	 
	if (nand_region_is_secured(chip, from, readlen))
		return -EIO;

	chipnr = (int)(from >> chip->chip_shift);
	nand_select_target(chip, chipnr);

	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	col = (int)(from & (mtd->writesize - 1));

	buf = ops->datbuf;
	oob = ops->oobbuf;
	oob_required = oob ? 1 : 0;

	rawnand_enable_cont_reads(chip, page, readlen, col);

	while (1) {
		struct mtd_ecc_stats ecc_stats = mtd->ecc_stats;

		bytes = min(mtd->writesize - col, readlen);
		aligned = (bytes == mtd->writesize);

		if (!aligned)
			use_bounce_buf = 1;
		else if (chip->options & NAND_USES_DMA)
			use_bounce_buf = !virt_addr_valid(buf) ||
					 !IS_ALIGNED((unsigned long)buf,
						     chip->buf_align);
		else
			use_bounce_buf = 0;

		 
		if (realpage != chip->pagecache.page || oob) {
			bufpoi = use_bounce_buf ? chip->data_buf : buf;

			if (use_bounce_buf && aligned)
				pr_debug("%s: using read bounce buffer for buf@%p\n",
						 __func__, buf);

read_retry:
			 
			if (unlikely(ops->mode == MTD_OPS_RAW))
				ret = chip->ecc.read_page_raw(chip, bufpoi,
							      oob_required,
							      page);
			else if (!aligned && NAND_HAS_SUBPAGE_READ(chip) &&
				 !oob)
				ret = chip->ecc.read_subpage(chip, col, bytes,
							     bufpoi, page);
			else
				ret = chip->ecc.read_page(chip, bufpoi,
							  oob_required, page);
			if (ret < 0) {
				if (use_bounce_buf)
					 
					chip->pagecache.page = -1;
				break;
			}

			 
			if (use_bounce_buf) {
				if (!NAND_HAS_SUBPAGE_READ(chip) && !oob &&
				    !(mtd->ecc_stats.failed - ecc_stats.failed) &&
				    (ops->mode != MTD_OPS_RAW)) {
					chip->pagecache.page = realpage;
					chip->pagecache.bitflips = ret;
				} else {
					 
					chip->pagecache.page = -1;
				}
				memcpy(buf, bufpoi + col, bytes);
			}

			if (unlikely(oob)) {
				int toread = min(oobreadlen, max_oobsize);

				if (toread) {
					oob = nand_transfer_oob(chip, oob, ops,
								toread);
					oobreadlen -= toread;
				}
			}

			nand_wait_readrdy(chip);

			if (mtd->ecc_stats.failed - ecc_stats.failed) {
				if (retry_mode + 1 < chip->read_retries) {
					retry_mode++;
					ret = nand_setup_read_retry(chip,
							retry_mode);
					if (ret < 0)
						break;

					 
					mtd->ecc_stats = ecc_stats;
					goto read_retry;
				} else {
					 
					ecc_fail = true;
				}
			}

			buf += bytes;
			max_bitflips = max_t(unsigned int, max_bitflips, ret);
		} else {
			memcpy(buf, chip->data_buf + col, bytes);
			buf += bytes;
			max_bitflips = max_t(unsigned int, max_bitflips,
					     chip->pagecache.bitflips);
		}

		readlen -= bytes;

		 
		if (retry_mode) {
			ret = nand_setup_read_retry(chip, 0);
			if (ret < 0)
				break;
			retry_mode = 0;
		}

		if (!readlen)
			break;

		 
		col = 0;
		 
		realpage++;

		page = realpage & chip->pagemask;
		 
		if (!page) {
			chipnr++;
			nand_deselect_target(chip);
			nand_select_target(chip, chipnr);
		}
	}
	nand_deselect_target(chip);

	ops->retlen = ops->len - (size_t) readlen;
	if (oob)
		ops->oobretlen = ops->ooblen - oobreadlen;

	if (ret < 0)
		return ret;

	if (ecc_fail)
		return -EBADMSG;

	return max_bitflips;
}

 
int nand_read_oob_std(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	return nand_read_oob_op(chip, page, 0, chip->oob_poi, mtd->oobsize);
}
EXPORT_SYMBOL(nand_read_oob_std);

 
static int nand_read_oob_syndrome(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int length = mtd->oobsize;
	int chunk = chip->ecc.bytes + chip->ecc.prepad + chip->ecc.postpad;
	int eccsize = chip->ecc.size;
	uint8_t *bufpoi = chip->oob_poi;
	int i, toread, sndrnd = 0, pos, ret;

	ret = nand_read_page_op(chip, page, chip->ecc.size, NULL, 0);
	if (ret)
		return ret;

	for (i = 0; i < chip->ecc.steps; i++) {
		if (sndrnd) {
			int ret;

			pos = eccsize + i * (eccsize + chunk);
			if (mtd->writesize > 512)
				ret = nand_change_read_column_op(chip, pos,
								 NULL, 0,
								 false);
			else
				ret = nand_read_page_op(chip, page, pos, NULL,
							0);

			if (ret)
				return ret;
		} else
			sndrnd = 1;
		toread = min_t(int, length, chunk);

		ret = nand_read_data_op(chip, bufpoi, toread, false, false);
		if (ret)
			return ret;

		bufpoi += toread;
		length -= toread;
	}
	if (length > 0) {
		ret = nand_read_data_op(chip, bufpoi, length, false, false);
		if (ret)
			return ret;
	}

	return 0;
}

 
int nand_write_oob_std(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	return nand_prog_page_op(chip, page, mtd->writesize, chip->oob_poi,
				 mtd->oobsize);
}
EXPORT_SYMBOL(nand_write_oob_std);

 
static int nand_write_oob_syndrome(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int chunk = chip->ecc.bytes + chip->ecc.prepad + chip->ecc.postpad;
	int eccsize = chip->ecc.size, length = mtd->oobsize;
	int ret, i, len, pos, sndcmd = 0, steps = chip->ecc.steps;
	const uint8_t *bufpoi = chip->oob_poi;

	 
	if (!chip->ecc.prepad && !chip->ecc.postpad) {
		pos = steps * (eccsize + chunk);
		steps = 0;
	} else
		pos = eccsize;

	ret = nand_prog_page_begin_op(chip, page, pos, NULL, 0);
	if (ret)
		return ret;

	for (i = 0; i < steps; i++) {
		if (sndcmd) {
			if (mtd->writesize <= 512) {
				uint32_t fill = 0xFFFFFFFF;

				len = eccsize;
				while (len > 0) {
					int num = min_t(int, len, 4);

					ret = nand_write_data_op(chip, &fill,
								 num, false);
					if (ret)
						return ret;

					len -= num;
				}
			} else {
				pos = eccsize + i * (eccsize + chunk);
				ret = nand_change_write_column_op(chip, pos,
								  NULL, 0,
								  false);
				if (ret)
					return ret;
			}
		} else
			sndcmd = 1;
		len = min_t(int, length, chunk);

		ret = nand_write_data_op(chip, bufpoi, len, false);
		if (ret)
			return ret;

		bufpoi += len;
		length -= len;
	}
	if (length > 0) {
		ret = nand_write_data_op(chip, bufpoi, length, false);
		if (ret)
			return ret;
	}

	return nand_prog_page_end_op(chip);
}

 
static int nand_do_read_oob(struct nand_chip *chip, loff_t from,
			    struct mtd_oob_ops *ops)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int max_bitflips = 0;
	int page, realpage, chipnr;
	struct mtd_ecc_stats stats;
	int readlen = ops->ooblen;
	int len;
	uint8_t *buf = ops->oobbuf;
	int ret = 0;

	pr_debug("%s: from = 0x%08Lx, len = %i\n",
			__func__, (unsigned long long)from, readlen);

	 
	if (nand_region_is_secured(chip, from, readlen))
		return -EIO;

	stats = mtd->ecc_stats;

	len = mtd_oobavail(mtd, ops);

	chipnr = (int)(from >> chip->chip_shift);
	nand_select_target(chip, chipnr);

	 
	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	while (1) {
		if (ops->mode == MTD_OPS_RAW)
			ret = chip->ecc.read_oob_raw(chip, page);
		else
			ret = chip->ecc.read_oob(chip, page);

		if (ret < 0)
			break;

		len = min(len, readlen);
		buf = nand_transfer_oob(chip, buf, ops, len);

		nand_wait_readrdy(chip);

		max_bitflips = max_t(unsigned int, max_bitflips, ret);

		readlen -= len;
		if (!readlen)
			break;

		 
		realpage++;

		page = realpage & chip->pagemask;
		 
		if (!page) {
			chipnr++;
			nand_deselect_target(chip);
			nand_select_target(chip, chipnr);
		}
	}
	nand_deselect_target(chip);

	ops->oobretlen = ops->ooblen - readlen;

	if (ret < 0)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return max_bitflips;
}

 
static int nand_read_oob(struct mtd_info *mtd, loff_t from,
			 struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct mtd_ecc_stats old_stats;
	int ret;

	ops->retlen = 0;

	if (ops->mode != MTD_OPS_PLACE_OOB &&
	    ops->mode != MTD_OPS_AUTO_OOB &&
	    ops->mode != MTD_OPS_RAW)
		return -ENOTSUPP;

	nand_get_device(chip);

	old_stats = mtd->ecc_stats;

	if (!ops->datbuf)
		ret = nand_do_read_oob(chip, from, ops);
	else
		ret = nand_do_read_ops(chip, from, ops);

	if (ops->stats) {
		ops->stats->uncorrectable_errors +=
			mtd->ecc_stats.failed - old_stats.failed;
		ops->stats->corrected_bitflips +=
			mtd->ecc_stats.corrected - old_stats.corrected;
	}

	nand_release_device(chip);
	return ret;
}

 
int nand_write_page_raw_notsupp(struct nand_chip *chip, const u8 *buf,
				int oob_required, int page)
{
	return -ENOTSUPP;
}

 
int nand_write_page_raw(struct nand_chip *chip, const uint8_t *buf,
			int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret;

	ret = nand_prog_page_begin_op(chip, page, 0, buf, mtd->writesize);
	if (ret)
		return ret;

	if (oob_required) {
		ret = nand_write_data_op(chip, chip->oob_poi, mtd->oobsize,
					 false);
		if (ret)
			return ret;
	}

	return nand_prog_page_end_op(chip);
}
EXPORT_SYMBOL(nand_write_page_raw);

 
int nand_monolithic_write_page_raw(struct nand_chip *chip, const u8 *buf,
				   int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int size = mtd->writesize;
	u8 *write_buf = (u8 *)buf;

	if (oob_required) {
		size += mtd->oobsize;

		if (buf != chip->data_buf) {
			write_buf = nand_get_data_buf(chip);
			memcpy(write_buf, buf, mtd->writesize);
		}
	}

	return nand_prog_page_op(chip, page, 0, write_buf, size);
}
EXPORT_SYMBOL(nand_monolithic_write_page_raw);

 
static int nand_write_page_raw_syndrome(struct nand_chip *chip,
					const uint8_t *buf, int oob_required,
					int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	uint8_t *oob = chip->oob_poi;
	int steps, size, ret;

	ret = nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	if (ret)
		return ret;

	for (steps = chip->ecc.steps; steps > 0; steps--) {
		ret = nand_write_data_op(chip, buf, eccsize, false);
		if (ret)
			return ret;

		buf += eccsize;

		if (chip->ecc.prepad) {
			ret = nand_write_data_op(chip, oob, chip->ecc.prepad,
						 false);
			if (ret)
				return ret;

			oob += chip->ecc.prepad;
		}

		ret = nand_write_data_op(chip, oob, eccbytes, false);
		if (ret)
			return ret;

		oob += eccbytes;

		if (chip->ecc.postpad) {
			ret = nand_write_data_op(chip, oob, chip->ecc.postpad,
						 false);
			if (ret)
				return ret;

			oob += chip->ecc.postpad;
		}
	}

	size = mtd->oobsize - (oob - chip->oob_poi);
	if (size) {
		ret = nand_write_data_op(chip, oob, size, false);
		if (ret)
			return ret;
	}

	return nand_prog_page_end_op(chip);
}
 
static int nand_write_page_swecc(struct nand_chip *chip, const uint8_t *buf,
				 int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i, eccsize = chip->ecc.size, ret;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *ecc_calc = chip->ecc.calc_buf;
	const uint8_t *p = buf;

	 
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		chip->ecc.calculate(chip, p, &ecc_calc[i]);

	ret = mtd_ooblayout_set_eccbytes(mtd, ecc_calc, chip->oob_poi, 0,
					 chip->ecc.total);
	if (ret)
		return ret;

	return chip->ecc.write_page_raw(chip, buf, 1, page);
}

 
static int nand_write_page_hwecc(struct nand_chip *chip, const uint8_t *buf,
				 int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i, eccsize = chip->ecc.size, ret;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *ecc_calc = chip->ecc.calc_buf;
	const uint8_t *p = buf;

	ret = nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	if (ret)
		return ret;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(chip, NAND_ECC_WRITE);

		ret = nand_write_data_op(chip, p, eccsize, false);
		if (ret)
			return ret;

		chip->ecc.calculate(chip, p, &ecc_calc[i]);
	}

	ret = mtd_ooblayout_set_eccbytes(mtd, ecc_calc, chip->oob_poi, 0,
					 chip->ecc.total);
	if (ret)
		return ret;

	ret = nand_write_data_op(chip, chip->oob_poi, mtd->oobsize, false);
	if (ret)
		return ret;

	return nand_prog_page_end_op(chip);
}


 
static int nand_write_subpage_hwecc(struct nand_chip *chip, uint32_t offset,
				    uint32_t data_len, const uint8_t *buf,
				    int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	uint8_t *oob_buf  = chip->oob_poi;
	uint8_t *ecc_calc = chip->ecc.calc_buf;
	int ecc_size      = chip->ecc.size;
	int ecc_bytes     = chip->ecc.bytes;
	int ecc_steps     = chip->ecc.steps;
	uint32_t start_step = offset / ecc_size;
	uint32_t end_step   = (offset + data_len - 1) / ecc_size;
	int oob_bytes       = mtd->oobsize / ecc_steps;
	int step, ret;

	ret = nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	if (ret)
		return ret;

	for (step = 0; step < ecc_steps; step++) {
		 
		chip->ecc.hwctl(chip, NAND_ECC_WRITE);

		 
		ret = nand_write_data_op(chip, buf, ecc_size, false);
		if (ret)
			return ret;

		 
		if ((step < start_step) || (step > end_step))
			memset(ecc_calc, 0xff, ecc_bytes);
		else
			chip->ecc.calculate(chip, buf, ecc_calc);

		 
		 
		if (!oob_required || (step < start_step) || (step > end_step))
			memset(oob_buf, 0xff, oob_bytes);

		buf += ecc_size;
		ecc_calc += ecc_bytes;
		oob_buf  += oob_bytes;
	}

	 
	 
	ecc_calc = chip->ecc.calc_buf;
	ret = mtd_ooblayout_set_eccbytes(mtd, ecc_calc, chip->oob_poi, 0,
					 chip->ecc.total);
	if (ret)
		return ret;

	 
	ret = nand_write_data_op(chip, chip->oob_poi, mtd->oobsize, false);
	if (ret)
		return ret;

	return nand_prog_page_end_op(chip);
}


 
static int nand_write_page_syndrome(struct nand_chip *chip, const uint8_t *buf,
				    int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	const uint8_t *p = buf;
	uint8_t *oob = chip->oob_poi;
	int ret;

	ret = nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	if (ret)
		return ret;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(chip, NAND_ECC_WRITE);

		ret = nand_write_data_op(chip, p, eccsize, false);
		if (ret)
			return ret;

		if (chip->ecc.prepad) {
			ret = nand_write_data_op(chip, oob, chip->ecc.prepad,
						 false);
			if (ret)
				return ret;

			oob += chip->ecc.prepad;
		}

		chip->ecc.calculate(chip, p, oob);

		ret = nand_write_data_op(chip, oob, eccbytes, false);
		if (ret)
			return ret;

		oob += eccbytes;

		if (chip->ecc.postpad) {
			ret = nand_write_data_op(chip, oob, chip->ecc.postpad,
						 false);
			if (ret)
				return ret;

			oob += chip->ecc.postpad;
		}
	}

	 
	i = mtd->oobsize - (oob - chip->oob_poi);
	if (i) {
		ret = nand_write_data_op(chip, oob, i, false);
		if (ret)
			return ret;
	}

	return nand_prog_page_end_op(chip);
}

 
static int nand_write_page(struct nand_chip *chip, uint32_t offset,
			   int data_len, const uint8_t *buf, int oob_required,
			   int page, int raw)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int status, subpage;

	if (!(chip->options & NAND_NO_SUBPAGE_WRITE) &&
		chip->ecc.write_subpage)
		subpage = offset || (data_len < mtd->writesize);
	else
		subpage = 0;

	if (unlikely(raw))
		status = chip->ecc.write_page_raw(chip, buf, oob_required,
						  page);
	else if (subpage)
		status = chip->ecc.write_subpage(chip, offset, data_len, buf,
						 oob_required, page);
	else
		status = chip->ecc.write_page(chip, buf, oob_required, page);

	if (status < 0)
		return status;

	return 0;
}

#define NOTALIGNED(x)	((x & (chip->subpagesize - 1)) != 0)

 
static int nand_do_write_ops(struct nand_chip *chip, loff_t to,
			     struct mtd_oob_ops *ops)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int chipnr, realpage, page, column;
	uint32_t writelen = ops->len;

	uint32_t oobwritelen = ops->ooblen;
	uint32_t oobmaxlen = mtd_oobavail(mtd, ops);

	uint8_t *oob = ops->oobbuf;
	uint8_t *buf = ops->datbuf;
	int ret;
	int oob_required = oob ? 1 : 0;

	ops->retlen = 0;
	if (!writelen)
		return 0;

	 
	if (NOTALIGNED(to) || NOTALIGNED(ops->len)) {
		pr_notice("%s: attempt to write non page aligned data\n",
			   __func__);
		return -EINVAL;
	}

	 
	if (nand_region_is_secured(chip, to, writelen))
		return -EIO;

	column = to & (mtd->writesize - 1);

	chipnr = (int)(to >> chip->chip_shift);
	nand_select_target(chip, chipnr);

	 
	if (nand_check_wp(chip)) {
		ret = -EIO;
		goto err_out;
	}

	realpage = (int)(to >> chip->page_shift);
	page = realpage & chip->pagemask;

	 
	if (to <= ((loff_t)chip->pagecache.page << chip->page_shift) &&
	    ((loff_t)chip->pagecache.page << chip->page_shift) < (to + ops->len))
		chip->pagecache.page = -1;

	 
	if (oob && ops->ooboffs && (ops->ooboffs + ops->ooblen > oobmaxlen)) {
		ret = -EINVAL;
		goto err_out;
	}

	while (1) {
		int bytes = mtd->writesize;
		uint8_t *wbuf = buf;
		int use_bounce_buf;
		int part_pagewr = (column || writelen < mtd->writesize);

		if (part_pagewr)
			use_bounce_buf = 1;
		else if (chip->options & NAND_USES_DMA)
			use_bounce_buf = !virt_addr_valid(buf) ||
					 !IS_ALIGNED((unsigned long)buf,
						     chip->buf_align);
		else
			use_bounce_buf = 0;

		 
		if (use_bounce_buf) {
			pr_debug("%s: using write bounce buffer for buf@%p\n",
					 __func__, buf);
			if (part_pagewr)
				bytes = min_t(int, bytes - column, writelen);
			wbuf = nand_get_data_buf(chip);
			memset(wbuf, 0xff, mtd->writesize);
			memcpy(&wbuf[column], buf, bytes);
		}

		if (unlikely(oob)) {
			size_t len = min(oobwritelen, oobmaxlen);
			oob = nand_fill_oob(chip, oob, len, ops);
			oobwritelen -= len;
		} else {
			 
			memset(chip->oob_poi, 0xff, mtd->oobsize);
		}

		ret = nand_write_page(chip, column, bytes, wbuf,
				      oob_required, page,
				      (ops->mode == MTD_OPS_RAW));
		if (ret)
			break;

		writelen -= bytes;
		if (!writelen)
			break;

		column = 0;
		buf += bytes;
		realpage++;

		page = realpage & chip->pagemask;
		 
		if (!page) {
			chipnr++;
			nand_deselect_target(chip);
			nand_select_target(chip, chipnr);
		}
	}

	ops->retlen = ops->len - writelen;
	if (unlikely(oob))
		ops->oobretlen = ops->ooblen;

err_out:
	nand_deselect_target(chip);
	return ret;
}

 
static int panic_nand_write(struct mtd_info *mtd, loff_t to, size_t len,
			    size_t *retlen, const uint8_t *buf)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	int chipnr = (int)(to >> chip->chip_shift);
	struct mtd_oob_ops ops;
	int ret;

	nand_select_target(chip, chipnr);

	 
	panic_nand_wait(chip, 400);

	memset(&ops, 0, sizeof(ops));
	ops.len = len;
	ops.datbuf = (uint8_t *)buf;
	ops.mode = MTD_OPS_PLACE_OOB;

	ret = nand_do_write_ops(chip, to, &ops);

	*retlen = ops.retlen;
	return ret;
}

 
static int nand_write_oob(struct mtd_info *mtd, loff_t to,
			  struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	int ret = 0;

	ops->retlen = 0;

	nand_get_device(chip);

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = nand_do_write_oob(chip, to, ops);
	else
		ret = nand_do_write_ops(chip, to, ops);

out:
	nand_release_device(chip);
	return ret;
}

 
static int nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	return nand_erase_nand(mtd_to_nand(mtd), instr, 0);
}

 
int nand_erase_nand(struct nand_chip *chip, struct erase_info *instr,
		    int allowbbt)
{
	int page, pages_per_block, ret, chipnr;
	loff_t len;

	pr_debug("%s: start = 0x%012llx, len = %llu\n",
			__func__, (unsigned long long)instr->addr,
			(unsigned long long)instr->len);

	if (check_offs_len(chip, instr->addr, instr->len))
		return -EINVAL;

	 
	if (nand_region_is_secured(chip, instr->addr, instr->len))
		return -EIO;

	 
	nand_get_device(chip);

	 
	page = (int)(instr->addr >> chip->page_shift);
	chipnr = (int)(instr->addr >> chip->chip_shift);

	 
	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);

	 
	nand_select_target(chip, chipnr);

	 
	if (nand_check_wp(chip)) {
		pr_debug("%s: device is write protected!\n",
				__func__);
		ret = -EIO;
		goto erase_exit;
	}

	 
	len = instr->len;

	while (len) {
		loff_t ofs = (loff_t)page << chip->page_shift;

		 
		if (nand_block_checkbad(chip, ((loff_t) page) <<
					chip->page_shift, allowbbt)) {
			pr_warn("%s: attempt to erase a bad block at 0x%08llx\n",
				    __func__, (unsigned long long)ofs);
			ret = -EIO;
			goto erase_exit;
		}

		 
		if (page <= chip->pagecache.page && chip->pagecache.page <
		    (page + pages_per_block))
			chip->pagecache.page = -1;

		ret = nand_erase_op(chip, (page & chip->pagemask) >>
				    (chip->phys_erase_shift - chip->page_shift));
		if (ret) {
			pr_debug("%s: failed erase, page 0x%08x\n",
					__func__, page);
			instr->fail_addr = ofs;
			goto erase_exit;
		}

		 
		len -= (1ULL << chip->phys_erase_shift);
		page += pages_per_block;

		 
		if (len && !(page & chip->pagemask)) {
			chipnr++;
			nand_deselect_target(chip);
			nand_select_target(chip, chipnr);
		}
	}

	ret = 0;
erase_exit:

	 
	nand_deselect_target(chip);
	nand_release_device(chip);

	 
	return ret;
}

 
static void nand_sync(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	pr_debug("%s: called\n", __func__);

	 
	nand_get_device(chip);
	 
	nand_release_device(chip);
}

 
static int nand_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	int chipnr = (int)(offs >> chip->chip_shift);
	int ret;

	 
	nand_get_device(chip);

	nand_select_target(chip, chipnr);

	ret = nand_block_checkbad(chip, offs, 0);

	nand_deselect_target(chip);
	nand_release_device(chip);

	return ret;
}

 
static int nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	int ret;

	ret = nand_block_isbad(mtd, ofs);
	if (ret) {
		 
		if (ret > 0)
			return 0;
		return ret;
	}

	return nand_block_markbad_lowlevel(mtd_to_nand(mtd), ofs);
}

 
static int nand_suspend(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	int ret = 0;

	mutex_lock(&chip->lock);
	if (chip->ops.suspend)
		ret = chip->ops.suspend(chip);
	if (!ret)
		chip->suspended = 1;
	mutex_unlock(&chip->lock);

	return ret;
}

 
static void nand_resume(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	mutex_lock(&chip->lock);
	if (chip->suspended) {
		if (chip->ops.resume)
			chip->ops.resume(chip);
		chip->suspended = 0;
	} else {
		pr_err("%s called for a chip which is not in suspended state\n",
			__func__);
	}
	mutex_unlock(&chip->lock);

	wake_up_all(&chip->resume_wq);
}

 
static void nand_shutdown(struct mtd_info *mtd)
{
	nand_suspend(mtd);
}

 
static int nand_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (!chip->ops.lock_area)
		return -ENOTSUPP;

	return chip->ops.lock_area(chip, ofs, len);
}

 
static int nand_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (!chip->ops.unlock_area)
		return -ENOTSUPP;

	return chip->ops.unlock_area(chip, ofs, len);
}

 
static void nand_set_defaults(struct nand_chip *chip)
{
	 
	if (!chip->controller) {
		chip->controller = &chip->legacy.dummy_controller;
		nand_controller_init(chip->controller);
	}

	nand_legacy_set_defaults(chip);

	if (!chip->buf_align)
		chip->buf_align = 1;
}

 
void sanitize_string(uint8_t *s, size_t len)
{
	ssize_t i;

	 
	s[len - 1] = 0;

	 
	for (i = 0; i < len - 1; i++) {
		if (s[i] < ' ' || s[i] > 127)
			s[i] = '?';
	}

	 
	strim(s);
}

 
static int nand_id_has_period(u8 *id_data, int arrlen, int period)
{
	int i, j;
	for (i = 0; i < period; i++)
		for (j = i + period; j < arrlen; j += period)
			if (id_data[i] != id_data[j])
				return 0;
	return 1;
}

 
static int nand_id_len(u8 *id_data, int arrlen)
{
	int last_nonzero, period;

	 
	for (last_nonzero = arrlen - 1; last_nonzero >= 0; last_nonzero--)
		if (id_data[last_nonzero])
			break;

	 
	if (last_nonzero < 0)
		return 0;

	 
	for (period = 1; period < arrlen; period++)
		if (nand_id_has_period(id_data, arrlen, period))
			break;

	 
	if (period < arrlen)
		return period;

	 
	if (last_nonzero < arrlen - 1)
		return last_nonzero + 1;

	 
	return arrlen;
}

 
static int nand_get_bits_per_cell(u8 cellinfo)
{
	int bits;

	bits = cellinfo & NAND_CI_CELLTYPE_MSK;
	bits >>= NAND_CI_CELLTYPE_SHIFT;
	return bits + 1;
}

 
void nand_decode_ext_id(struct nand_chip *chip)
{
	struct nand_memory_organization *memorg;
	struct mtd_info *mtd = nand_to_mtd(chip);
	int extid;
	u8 *id_data = chip->id.data;

	memorg = nanddev_get_memorg(&chip->base);

	 
	memorg->bits_per_cell = nand_get_bits_per_cell(id_data[2]);
	 
	extid = id_data[3];

	 
	memorg->pagesize = 1024 << (extid & 0x03);
	mtd->writesize = memorg->pagesize;
	extid >>= 2;
	 
	memorg->oobsize = (8 << (extid & 0x01)) * (mtd->writesize >> 9);
	mtd->oobsize = memorg->oobsize;
	extid >>= 2;
	 
	memorg->pages_per_eraseblock = ((64 * 1024) << (extid & 0x03)) /
				       memorg->pagesize;
	mtd->erasesize = (64 * 1024) << (extid & 0x03);
	extid >>= 2;
	 
	if (extid & 0x1)
		chip->options |= NAND_BUSWIDTH_16;
}
EXPORT_SYMBOL_GPL(nand_decode_ext_id);

 
static void nand_decode_id(struct nand_chip *chip, struct nand_flash_dev *type)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_memory_organization *memorg;

	memorg = nanddev_get_memorg(&chip->base);

	memorg->pages_per_eraseblock = type->erasesize / type->pagesize;
	mtd->erasesize = type->erasesize;
	memorg->pagesize = type->pagesize;
	mtd->writesize = memorg->pagesize;
	memorg->oobsize = memorg->pagesize / 32;
	mtd->oobsize = memorg->oobsize;

	 
	memorg->bits_per_cell = 1;
}

 
static void nand_decode_bbm_options(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	 
	if (mtd->writesize > 512 || (chip->options & NAND_BUSWIDTH_16))
		chip->badblockpos = NAND_BBM_POS_LARGE;
	else
		chip->badblockpos = NAND_BBM_POS_SMALL;
}

static inline bool is_full_id_nand(struct nand_flash_dev *type)
{
	return type->id_len;
}

static bool find_full_id_nand(struct nand_chip *chip,
			      struct nand_flash_dev *type)
{
	struct nand_device *base = &chip->base;
	struct nand_ecc_props requirements;
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_memory_organization *memorg;
	u8 *id_data = chip->id.data;

	memorg = nanddev_get_memorg(&chip->base);

	if (!strncmp(type->id, id_data, type->id_len)) {
		memorg->pagesize = type->pagesize;
		mtd->writesize = memorg->pagesize;
		memorg->pages_per_eraseblock = type->erasesize /
					       type->pagesize;
		mtd->erasesize = type->erasesize;
		memorg->oobsize = type->oobsize;
		mtd->oobsize = memorg->oobsize;

		memorg->bits_per_cell = nand_get_bits_per_cell(id_data[2]);
		memorg->eraseblocks_per_lun =
			DIV_ROUND_DOWN_ULL((u64)type->chipsize << 20,
					   memorg->pagesize *
					   memorg->pages_per_eraseblock);
		chip->options |= type->options;
		requirements.strength = NAND_ECC_STRENGTH(type);
		requirements.step_size = NAND_ECC_STEP(type);
		nanddev_set_ecc_requirements(base, &requirements);

		chip->parameters.model = kstrdup(type->name, GFP_KERNEL);
		if (!chip->parameters.model)
			return false;

		return true;
	}
	return false;
}

 
static void nand_manufacturer_detect(struct nand_chip *chip)
{
	 
	if (chip->manufacturer.desc && chip->manufacturer.desc->ops &&
	    chip->manufacturer.desc->ops->detect) {
		struct nand_memory_organization *memorg;

		memorg = nanddev_get_memorg(&chip->base);

		 
		memorg->bits_per_cell = nand_get_bits_per_cell(chip->id.data[2]);
		chip->manufacturer.desc->ops->detect(chip);
	} else {
		nand_decode_ext_id(chip);
	}
}

 
static int nand_manufacturer_init(struct nand_chip *chip)
{
	if (!chip->manufacturer.desc || !chip->manufacturer.desc->ops ||
	    !chip->manufacturer.desc->ops->init)
		return 0;

	return chip->manufacturer.desc->ops->init(chip);
}

 
static void nand_manufacturer_cleanup(struct nand_chip *chip)
{
	 
	if (chip->manufacturer.desc && chip->manufacturer.desc->ops &&
	    chip->manufacturer.desc->ops->cleanup)
		chip->manufacturer.desc->ops->cleanup(chip);
}

static const char *
nand_manufacturer_name(const struct nand_manufacturer_desc *manufacturer_desc)
{
	return manufacturer_desc ? manufacturer_desc->name : "Unknown";
}

static void rawnand_check_data_only_read_support(struct nand_chip *chip)
{
	 
	if (!nand_read_data_op(chip, NULL, SZ_512, true, true))
		chip->controller->supported_op.data_only_read = 1;
}

static void rawnand_early_check_supported_ops(struct nand_chip *chip)
{
	 
	WARN_ON_ONCE(chip->controller->supported_op.data_only_read);

	if (!nand_has_exec_op(chip))
		return;

	rawnand_check_data_only_read_support(chip);
}

static void rawnand_check_cont_read_support(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	if (!chip->parameters.supports_read_cache)
		return;

	if (chip->read_retries)
		return;

	if (!nand_lp_exec_cont_read_page_op(chip, 0, 0, NULL,
					    mtd->writesize, true))
		chip->controller->supported_op.cont_read = 1;
}

static void rawnand_late_check_supported_ops(struct nand_chip *chip)
{
	 
	WARN_ON_ONCE(chip->controller->supported_op.cont_read);

	if (!nand_has_exec_op(chip))
		return;

	rawnand_check_cont_read_support(chip);
}

 
static int nand_detect(struct nand_chip *chip, struct nand_flash_dev *type)
{
	const struct nand_manufacturer_desc *manufacturer_desc;
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_memory_organization *memorg;
	int busw, ret;
	u8 *id_data = chip->id.data;
	u8 maf_id, dev_id;
	u64 targetsize;

	 
	memorg = nanddev_get_memorg(&chip->base);
	memorg->planes_per_lun = 1;
	memorg->luns_per_target = 1;

	 
	ret = nand_reset(chip, 0);
	if (ret)
		return ret;

	 
	nand_select_target(chip, 0);

	rawnand_early_check_supported_ops(chip);

	 
	ret = nand_readid_op(chip, 0, id_data, 2);
	if (ret)
		return ret;

	 
	maf_id = id_data[0];
	dev_id = id_data[1];

	 

	 
	ret = nand_readid_op(chip, 0, id_data, sizeof(chip->id.data));
	if (ret)
		return ret;

	if (id_data[0] != maf_id || id_data[1] != dev_id) {
		pr_info("second ID read did not match %02x,%02x against %02x,%02x\n",
			maf_id, dev_id, id_data[0], id_data[1]);
		return -ENODEV;
	}

	chip->id.len = nand_id_len(id_data, ARRAY_SIZE(chip->id.data));

	 
	manufacturer_desc = nand_get_manufacturer_desc(maf_id);
	chip->manufacturer.desc = manufacturer_desc;

	if (!type)
		type = nand_flash_ids;

	 
	busw = chip->options & NAND_BUSWIDTH_16;

	 
	chip->options &= ~NAND_BUSWIDTH_16;

	for (; type->name != NULL; type++) {
		if (is_full_id_nand(type)) {
			if (find_full_id_nand(chip, type))
				goto ident_done;
		} else if (dev_id == type->dev_id) {
			break;
		}
	}

	if (!type->name || !type->pagesize) {
		 
		ret = nand_onfi_detect(chip);
		if (ret < 0)
			return ret;
		else if (ret)
			goto ident_done;

		 
		ret = nand_jedec_detect(chip);
		if (ret < 0)
			return ret;
		else if (ret)
			goto ident_done;
	}

	if (!type->name)
		return -ENODEV;

	chip->parameters.model = kstrdup(type->name, GFP_KERNEL);
	if (!chip->parameters.model)
		return -ENOMEM;

	if (!type->pagesize)
		nand_manufacturer_detect(chip);
	else
		nand_decode_id(chip, type);

	 
	chip->options |= type->options;

	memorg->eraseblocks_per_lun =
			DIV_ROUND_DOWN_ULL((u64)type->chipsize << 20,
					   memorg->pagesize *
					   memorg->pages_per_eraseblock);

ident_done:
	if (!mtd->name)
		mtd->name = chip->parameters.model;

	if (chip->options & NAND_BUSWIDTH_AUTO) {
		WARN_ON(busw & NAND_BUSWIDTH_16);
		nand_set_defaults(chip);
	} else if (busw != (chip->options & NAND_BUSWIDTH_16)) {
		 
		pr_info("device found, Manufacturer ID: 0x%02x, Chip ID: 0x%02x\n",
			maf_id, dev_id);
		pr_info("%s %s\n", nand_manufacturer_name(manufacturer_desc),
			mtd->name);
		pr_warn("bus width %d instead of %d bits\n", busw ? 16 : 8,
			(chip->options & NAND_BUSWIDTH_16) ? 16 : 8);
		ret = -EINVAL;

		goto free_detect_allocation;
	}

	nand_decode_bbm_options(chip);

	 
	chip->page_shift = ffs(mtd->writesize) - 1;
	 
	targetsize = nanddev_target_size(&chip->base);
	chip->pagemask = (targetsize >> chip->page_shift) - 1;

	chip->bbt_erase_shift = chip->phys_erase_shift =
		ffs(mtd->erasesize) - 1;
	if (targetsize & 0xffffffff)
		chip->chip_shift = ffs((unsigned)targetsize) - 1;
	else {
		chip->chip_shift = ffs((unsigned)(targetsize >> 32));
		chip->chip_shift += 32 - 1;
	}

	if (chip->chip_shift - chip->page_shift > 16)
		chip->options |= NAND_ROW_ADDR_3;

	chip->badblockbits = 8;

	nand_legacy_adjust_cmdfunc(chip);

	pr_info("device found, Manufacturer ID: 0x%02x, Chip ID: 0x%02x\n",
		maf_id, dev_id);
	pr_info("%s %s\n", nand_manufacturer_name(manufacturer_desc),
		chip->parameters.model);
	pr_info("%d MiB, %s, erase size: %d KiB, page size: %d, OOB size: %d\n",
		(int)(targetsize >> 20), nand_is_slc(chip) ? "SLC" : "MLC",
		mtd->erasesize >> 10, mtd->writesize, mtd->oobsize);
	return 0;

free_detect_allocation:
	kfree(chip->parameters.model);

	return ret;
}

static enum nand_ecc_engine_type
of_get_rawnand_ecc_engine_type_legacy(struct device_node *np)
{
	enum nand_ecc_legacy_mode {
		NAND_ECC_INVALID,
		NAND_ECC_NONE,
		NAND_ECC_SOFT,
		NAND_ECC_SOFT_BCH,
		NAND_ECC_HW,
		NAND_ECC_HW_SYNDROME,
		NAND_ECC_ON_DIE,
	};
	const char * const nand_ecc_legacy_modes[] = {
		[NAND_ECC_NONE]		= "none",
		[NAND_ECC_SOFT]		= "soft",
		[NAND_ECC_SOFT_BCH]	= "soft_bch",
		[NAND_ECC_HW]		= "hw",
		[NAND_ECC_HW_SYNDROME]	= "hw_syndrome",
		[NAND_ECC_ON_DIE]	= "on-die",
	};
	enum nand_ecc_legacy_mode eng_type;
	const char *pm;
	int err;

	err = of_property_read_string(np, "nand-ecc-mode", &pm);
	if (err)
		return NAND_ECC_ENGINE_TYPE_INVALID;

	for (eng_type = NAND_ECC_NONE;
	     eng_type < ARRAY_SIZE(nand_ecc_legacy_modes); eng_type++) {
		if (!strcasecmp(pm, nand_ecc_legacy_modes[eng_type])) {
			switch (eng_type) {
			case NAND_ECC_NONE:
				return NAND_ECC_ENGINE_TYPE_NONE;
			case NAND_ECC_SOFT:
			case NAND_ECC_SOFT_BCH:
				return NAND_ECC_ENGINE_TYPE_SOFT;
			case NAND_ECC_HW:
			case NAND_ECC_HW_SYNDROME:
				return NAND_ECC_ENGINE_TYPE_ON_HOST;
			case NAND_ECC_ON_DIE:
				return NAND_ECC_ENGINE_TYPE_ON_DIE;
			default:
				break;
			}
		}
	}

	return NAND_ECC_ENGINE_TYPE_INVALID;
}

static enum nand_ecc_placement
of_get_rawnand_ecc_placement_legacy(struct device_node *np)
{
	const char *pm;
	int err;

	err = of_property_read_string(np, "nand-ecc-mode", &pm);
	if (!err) {
		if (!strcasecmp(pm, "hw_syndrome"))
			return NAND_ECC_PLACEMENT_INTERLEAVED;
	}

	return NAND_ECC_PLACEMENT_UNKNOWN;
}

static enum nand_ecc_algo of_get_rawnand_ecc_algo_legacy(struct device_node *np)
{
	const char *pm;
	int err;

	err = of_property_read_string(np, "nand-ecc-mode", &pm);
	if (!err) {
		if (!strcasecmp(pm, "soft"))
			return NAND_ECC_ALGO_HAMMING;
		else if (!strcasecmp(pm, "soft_bch"))
			return NAND_ECC_ALGO_BCH;
	}

	return NAND_ECC_ALGO_UNKNOWN;
}

static void of_get_nand_ecc_legacy_user_config(struct nand_chip *chip)
{
	struct device_node *dn = nand_get_flash_node(chip);
	struct nand_ecc_props *user_conf = &chip->base.ecc.user_conf;

	if (user_conf->engine_type == NAND_ECC_ENGINE_TYPE_INVALID)
		user_conf->engine_type = of_get_rawnand_ecc_engine_type_legacy(dn);

	if (user_conf->algo == NAND_ECC_ALGO_UNKNOWN)
		user_conf->algo = of_get_rawnand_ecc_algo_legacy(dn);

	if (user_conf->placement == NAND_ECC_PLACEMENT_UNKNOWN)
		user_conf->placement = of_get_rawnand_ecc_placement_legacy(dn);
}

static int of_get_nand_bus_width(struct nand_chip *chip)
{
	struct device_node *dn = nand_get_flash_node(chip);
	u32 val;
	int ret;

	ret = of_property_read_u32(dn, "nand-bus-width", &val);
	if (ret == -EINVAL)
		 
		return 0;
	else if (ret)
		return ret;

	if (val == 16)
		chip->options |= NAND_BUSWIDTH_16;
	else if (val != 8)
		return -EINVAL;
	return 0;
}

static int of_get_nand_secure_regions(struct nand_chip *chip)
{
	struct device_node *dn = nand_get_flash_node(chip);
	struct property *prop;
	int nr_elem, i, j;

	 
	prop = of_find_property(dn, "secure-regions", NULL);
	if (!prop)
		return 0;

	nr_elem = of_property_count_elems_of_size(dn, "secure-regions", sizeof(u64));
	if (nr_elem <= 0)
		return nr_elem;

	chip->nr_secure_regions = nr_elem / 2;
	chip->secure_regions = kcalloc(chip->nr_secure_regions, sizeof(*chip->secure_regions),
				       GFP_KERNEL);
	if (!chip->secure_regions)
		return -ENOMEM;

	for (i = 0, j = 0; i < chip->nr_secure_regions; i++, j += 2) {
		of_property_read_u64_index(dn, "secure-regions", j,
					   &chip->secure_regions[i].offset);
		of_property_read_u64_index(dn, "secure-regions", j + 1,
					   &chip->secure_regions[i].size);
	}

	return 0;
}

 
int rawnand_dt_parse_gpio_cs(struct device *dev, struct gpio_desc ***cs_array,
			     unsigned int *ncs_array)
{
	struct gpio_desc **descs;
	int ndescs, i;

	ndescs = gpiod_count(dev, "cs");
	if (ndescs < 0) {
		dev_dbg(dev, "No valid cs-gpios property\n");
		return 0;
	}

	descs = devm_kcalloc(dev, ndescs, sizeof(*descs), GFP_KERNEL);
	if (!descs)
		return -ENOMEM;

	for (i = 0; i < ndescs; i++) {
		descs[i] = gpiod_get_index_optional(dev, "cs", i,
						    GPIOD_OUT_HIGH);
		if (IS_ERR(descs[i]))
			return PTR_ERR(descs[i]);
	}

	*ncs_array = ndescs;
	*cs_array = descs;

	return 0;
}
EXPORT_SYMBOL(rawnand_dt_parse_gpio_cs);

static int rawnand_dt_init(struct nand_chip *chip)
{
	struct nand_device *nand = mtd_to_nanddev(nand_to_mtd(chip));
	struct device_node *dn = nand_get_flash_node(chip);
	int ret;

	if (!dn)
		return 0;

	ret = of_get_nand_bus_width(chip);
	if (ret)
		return ret;

	if (of_property_read_bool(dn, "nand-is-boot-medium"))
		chip->options |= NAND_IS_BOOT_MEDIUM;

	if (of_property_read_bool(dn, "nand-on-flash-bbt"))
		chip->bbt_options |= NAND_BBT_USE_FLASH;

	of_get_nand_ecc_user_config(nand);
	of_get_nand_ecc_legacy_user_config(chip);

	 
	nand->ecc.defaults.engine_type = NAND_ECC_ENGINE_TYPE_ON_HOST;

	 
	if (nand->ecc.user_conf.engine_type != NAND_ECC_ENGINE_TYPE_INVALID)
		chip->ecc.engine_type = nand->ecc.user_conf.engine_type;
	if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_INVALID)
		chip->ecc.engine_type = nand->ecc.defaults.engine_type;

	chip->ecc.placement = nand->ecc.user_conf.placement;
	chip->ecc.algo = nand->ecc.user_conf.algo;
	chip->ecc.strength = nand->ecc.user_conf.strength;
	chip->ecc.size = nand->ecc.user_conf.step_size;

	return 0;
}

 
static int nand_scan_ident(struct nand_chip *chip, unsigned int maxchips,
			   struct nand_flash_dev *table)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_memory_organization *memorg;
	int nand_maf_id, nand_dev_id;
	unsigned int i;
	int ret;

	memorg = nanddev_get_memorg(&chip->base);

	 
	chip->cur_cs = -1;

	mutex_init(&chip->lock);
	init_waitqueue_head(&chip->resume_wq);

	 
	chip->current_interface_config = nand_get_reset_interface_config();

	ret = rawnand_dt_init(chip);
	if (ret)
		return ret;

	if (!mtd->name && mtd->dev.parent)
		mtd->name = dev_name(mtd->dev.parent);

	 
	nand_set_defaults(chip);

	ret = nand_legacy_check_hooks(chip);
	if (ret)
		return ret;

	memorg->ntargets = maxchips;

	 
	ret = nand_detect(chip, table);
	if (ret) {
		if (!(chip->options & NAND_SCAN_SILENT_NODEV))
			pr_warn("No NAND device found\n");
		nand_deselect_target(chip);
		return ret;
	}

	nand_maf_id = chip->id.data[0];
	nand_dev_id = chip->id.data[1];

	nand_deselect_target(chip);

	 
	for (i = 1; i < maxchips; i++) {
		u8 id[2];

		 
		ret = nand_reset(chip, i);
		if (ret)
			break;

		nand_select_target(chip, i);
		 
		ret = nand_readid_op(chip, 0, id, sizeof(id));
		if (ret)
			break;
		 
		if (nand_maf_id != id[0] || nand_dev_id != id[1]) {
			nand_deselect_target(chip);
			break;
		}
		nand_deselect_target(chip);
	}
	if (i > 1)
		pr_info("%d chips detected\n", i);

	 
	memorg->ntargets = i;
	mtd->size = i * nanddev_target_size(&chip->base);

	return 0;
}

static void nand_scan_ident_cleanup(struct nand_chip *chip)
{
	kfree(chip->parameters.model);
	kfree(chip->parameters.onfi);
}

int rawnand_sw_hamming_init(struct nand_chip *chip)
{
	struct nand_ecc_sw_hamming_conf *engine_conf;
	struct nand_device *base = &chip->base;
	int ret;

	base->ecc.user_conf.engine_type = NAND_ECC_ENGINE_TYPE_SOFT;
	base->ecc.user_conf.algo = NAND_ECC_ALGO_HAMMING;
	base->ecc.user_conf.strength = chip->ecc.strength;
	base->ecc.user_conf.step_size = chip->ecc.size;

	ret = nand_ecc_sw_hamming_init_ctx(base);
	if (ret)
		return ret;

	engine_conf = base->ecc.ctx.priv;

	if (chip->ecc.options & NAND_ECC_SOFT_HAMMING_SM_ORDER)
		engine_conf->sm_order = true;

	chip->ecc.size = base->ecc.ctx.conf.step_size;
	chip->ecc.strength = base->ecc.ctx.conf.strength;
	chip->ecc.total = base->ecc.ctx.total;
	chip->ecc.steps = nanddev_get_ecc_nsteps(base);
	chip->ecc.bytes = base->ecc.ctx.total / nanddev_get_ecc_nsteps(base);

	return 0;
}
EXPORT_SYMBOL(rawnand_sw_hamming_init);

int rawnand_sw_hamming_calculate(struct nand_chip *chip,
				 const unsigned char *buf,
				 unsigned char *code)
{
	struct nand_device *base = &chip->base;

	return nand_ecc_sw_hamming_calculate(base, buf, code);
}
EXPORT_SYMBOL(rawnand_sw_hamming_calculate);

int rawnand_sw_hamming_correct(struct nand_chip *chip,
			       unsigned char *buf,
			       unsigned char *read_ecc,
			       unsigned char *calc_ecc)
{
	struct nand_device *base = &chip->base;

	return nand_ecc_sw_hamming_correct(base, buf, read_ecc, calc_ecc);
}
EXPORT_SYMBOL(rawnand_sw_hamming_correct);

void rawnand_sw_hamming_cleanup(struct nand_chip *chip)
{
	struct nand_device *base = &chip->base;

	nand_ecc_sw_hamming_cleanup_ctx(base);
}
EXPORT_SYMBOL(rawnand_sw_hamming_cleanup);

int rawnand_sw_bch_init(struct nand_chip *chip)
{
	struct nand_device *base = &chip->base;
	const struct nand_ecc_props *ecc_conf = nanddev_get_ecc_conf(base);
	int ret;

	base->ecc.user_conf.engine_type = NAND_ECC_ENGINE_TYPE_SOFT;
	base->ecc.user_conf.algo = NAND_ECC_ALGO_BCH;
	base->ecc.user_conf.step_size = chip->ecc.size;
	base->ecc.user_conf.strength = chip->ecc.strength;

	ret = nand_ecc_sw_bch_init_ctx(base);
	if (ret)
		return ret;

	chip->ecc.size = ecc_conf->step_size;
	chip->ecc.strength = ecc_conf->strength;
	chip->ecc.total = base->ecc.ctx.total;
	chip->ecc.steps = nanddev_get_ecc_nsteps(base);
	chip->ecc.bytes = base->ecc.ctx.total / nanddev_get_ecc_nsteps(base);

	return 0;
}
EXPORT_SYMBOL(rawnand_sw_bch_init);

static int rawnand_sw_bch_calculate(struct nand_chip *chip,
				    const unsigned char *buf,
				    unsigned char *code)
{
	struct nand_device *base = &chip->base;

	return nand_ecc_sw_bch_calculate(base, buf, code);
}

int rawnand_sw_bch_correct(struct nand_chip *chip, unsigned char *buf,
			   unsigned char *read_ecc, unsigned char *calc_ecc)
{
	struct nand_device *base = &chip->base;

	return nand_ecc_sw_bch_correct(base, buf, read_ecc, calc_ecc);
}
EXPORT_SYMBOL(rawnand_sw_bch_correct);

void rawnand_sw_bch_cleanup(struct nand_chip *chip)
{
	struct nand_device *base = &chip->base;

	nand_ecc_sw_bch_cleanup_ctx(base);
}
EXPORT_SYMBOL(rawnand_sw_bch_cleanup);

static int nand_set_ecc_on_host_ops(struct nand_chip *chip)
{
	struct nand_ecc_ctrl *ecc = &chip->ecc;

	switch (ecc->placement) {
	case NAND_ECC_PLACEMENT_UNKNOWN:
	case NAND_ECC_PLACEMENT_OOB:
		 
		if (!ecc->read_page)
			ecc->read_page = nand_read_page_hwecc;
		if (!ecc->write_page)
			ecc->write_page = nand_write_page_hwecc;
		if (!ecc->read_page_raw)
			ecc->read_page_raw = nand_read_page_raw;
		if (!ecc->write_page_raw)
			ecc->write_page_raw = nand_write_page_raw;
		if (!ecc->read_oob)
			ecc->read_oob = nand_read_oob_std;
		if (!ecc->write_oob)
			ecc->write_oob = nand_write_oob_std;
		if (!ecc->read_subpage)
			ecc->read_subpage = nand_read_subpage;
		if (!ecc->write_subpage && ecc->hwctl && ecc->calculate)
			ecc->write_subpage = nand_write_subpage_hwecc;
		fallthrough;

	case NAND_ECC_PLACEMENT_INTERLEAVED:
		if ((!ecc->calculate || !ecc->correct || !ecc->hwctl) &&
		    (!ecc->read_page ||
		     ecc->read_page == nand_read_page_hwecc ||
		     !ecc->write_page ||
		     ecc->write_page == nand_write_page_hwecc)) {
			WARN(1, "No ECC functions supplied; hardware ECC not possible\n");
			return -EINVAL;
		}
		 
		if (!ecc->read_page)
			ecc->read_page = nand_read_page_syndrome;
		if (!ecc->write_page)
			ecc->write_page = nand_write_page_syndrome;
		if (!ecc->read_page_raw)
			ecc->read_page_raw = nand_read_page_raw_syndrome;
		if (!ecc->write_page_raw)
			ecc->write_page_raw = nand_write_page_raw_syndrome;
		if (!ecc->read_oob)
			ecc->read_oob = nand_read_oob_syndrome;
		if (!ecc->write_oob)
			ecc->write_oob = nand_write_oob_syndrome;
		break;

	default:
		pr_warn("Invalid NAND_ECC_PLACEMENT %d\n",
			ecc->placement);
		return -EINVAL;
	}

	return 0;
}

static int nand_set_ecc_soft_ops(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_device *nanddev = mtd_to_nanddev(mtd);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int ret;

	if (WARN_ON(ecc->engine_type != NAND_ECC_ENGINE_TYPE_SOFT))
		return -EINVAL;

	switch (ecc->algo) {
	case NAND_ECC_ALGO_HAMMING:
		ecc->calculate = rawnand_sw_hamming_calculate;
		ecc->correct = rawnand_sw_hamming_correct;
		ecc->read_page = nand_read_page_swecc;
		ecc->read_subpage = nand_read_subpage;
		ecc->write_page = nand_write_page_swecc;
		if (!ecc->read_page_raw)
			ecc->read_page_raw = nand_read_page_raw;
		if (!ecc->write_page_raw)
			ecc->write_page_raw = nand_write_page_raw;
		ecc->read_oob = nand_read_oob_std;
		ecc->write_oob = nand_write_oob_std;
		if (!ecc->size)
			ecc->size = 256;
		ecc->bytes = 3;
		ecc->strength = 1;

		if (IS_ENABLED(CONFIG_MTD_NAND_ECC_SW_HAMMING_SMC))
			ecc->options |= NAND_ECC_SOFT_HAMMING_SM_ORDER;

		ret = rawnand_sw_hamming_init(chip);
		if (ret) {
			WARN(1, "Hamming ECC initialization failed!\n");
			return ret;
		}

		return 0;
	case NAND_ECC_ALGO_BCH:
		if (!IS_ENABLED(CONFIG_MTD_NAND_ECC_SW_BCH)) {
			WARN(1, "CONFIG_MTD_NAND_ECC_SW_BCH not enabled\n");
			return -EINVAL;
		}
		ecc->calculate = rawnand_sw_bch_calculate;
		ecc->correct = rawnand_sw_bch_correct;
		ecc->read_page = nand_read_page_swecc;
		ecc->read_subpage = nand_read_subpage;
		ecc->write_page = nand_write_page_swecc;
		if (!ecc->read_page_raw)
			ecc->read_page_raw = nand_read_page_raw;
		if (!ecc->write_page_raw)
			ecc->write_page_raw = nand_write_page_raw;
		ecc->read_oob = nand_read_oob_std;
		ecc->write_oob = nand_write_oob_std;

		 
		if (nanddev->ecc.user_conf.flags & NAND_ECC_MAXIMIZE_STRENGTH &&
		    mtd->ooblayout != nand_get_large_page_ooblayout())
			nanddev->ecc.user_conf.flags &= ~NAND_ECC_MAXIMIZE_STRENGTH;

		ret = rawnand_sw_bch_init(chip);
		if (ret) {
			WARN(1, "BCH ECC initialization failed!\n");
			return ret;
		}

		return 0;
	default:
		WARN(1, "Unsupported ECC algorithm!\n");
		return -EINVAL;
	}
}

 
static int
nand_check_ecc_caps(struct nand_chip *chip,
		    const struct nand_ecc_caps *caps, int oobavail)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	const struct nand_ecc_step_info *stepinfo;
	int preset_step = chip->ecc.size;
	int preset_strength = chip->ecc.strength;
	int ecc_bytes, nsteps = mtd->writesize / preset_step;
	int i, j;

	for (i = 0; i < caps->nstepinfos; i++) {
		stepinfo = &caps->stepinfos[i];

		if (stepinfo->stepsize != preset_step)
			continue;

		for (j = 0; j < stepinfo->nstrengths; j++) {
			if (stepinfo->strengths[j] != preset_strength)
				continue;

			ecc_bytes = caps->calc_ecc_bytes(preset_step,
							 preset_strength);
			if (WARN_ON_ONCE(ecc_bytes < 0))
				return ecc_bytes;

			if (ecc_bytes * nsteps > oobavail) {
				pr_err("ECC (step, strength) = (%d, %d) does not fit in OOB",
				       preset_step, preset_strength);
				return -ENOSPC;
			}

			chip->ecc.bytes = ecc_bytes;

			return 0;
		}
	}

	pr_err("ECC (step, strength) = (%d, %d) not supported on this controller",
	       preset_step, preset_strength);

	return -ENOTSUPP;
}

 
static int
nand_match_ecc_req(struct nand_chip *chip,
		   const struct nand_ecc_caps *caps, int oobavail)
{
	const struct nand_ecc_props *requirements =
		nanddev_get_ecc_requirements(&chip->base);
	struct mtd_info *mtd = nand_to_mtd(chip);
	const struct nand_ecc_step_info *stepinfo;
	int req_step = requirements->step_size;
	int req_strength = requirements->strength;
	int req_corr, step_size, strength, nsteps, ecc_bytes, ecc_bytes_total;
	int best_step = 0, best_strength = 0, best_ecc_bytes = 0;
	int best_ecc_bytes_total = INT_MAX;
	int i, j;

	 
	if (!req_step || !req_strength)
		return -ENOTSUPP;

	 
	req_corr = mtd->writesize / req_step * req_strength;

	for (i = 0; i < caps->nstepinfos; i++) {
		stepinfo = &caps->stepinfos[i];
		step_size = stepinfo->stepsize;

		for (j = 0; j < stepinfo->nstrengths; j++) {
			strength = stepinfo->strengths[j];

			 
			if (step_size < req_step && strength < req_strength)
				continue;

			if (mtd->writesize % step_size)
				continue;

			nsteps = mtd->writesize / step_size;

			ecc_bytes = caps->calc_ecc_bytes(step_size, strength);
			if (WARN_ON_ONCE(ecc_bytes < 0))
				continue;
			ecc_bytes_total = ecc_bytes * nsteps;

			if (ecc_bytes_total > oobavail ||
			    strength * nsteps < req_corr)
				continue;

			 
			if (ecc_bytes_total < best_ecc_bytes_total) {
				best_ecc_bytes_total = ecc_bytes_total;
				best_step = step_size;
				best_strength = strength;
				best_ecc_bytes = ecc_bytes;
			}
		}
	}

	if (best_ecc_bytes_total == INT_MAX)
		return -ENOTSUPP;

	chip->ecc.size = best_step;
	chip->ecc.strength = best_strength;
	chip->ecc.bytes = best_ecc_bytes;

	return 0;
}

 
static int
nand_maximize_ecc(struct nand_chip *chip,
		  const struct nand_ecc_caps *caps, int oobavail)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	const struct nand_ecc_step_info *stepinfo;
	int step_size, strength, nsteps, ecc_bytes, corr;
	int best_corr = 0;
	int best_step = 0;
	int best_strength = 0, best_ecc_bytes = 0;
	int i, j;

	for (i = 0; i < caps->nstepinfos; i++) {
		stepinfo = &caps->stepinfos[i];
		step_size = stepinfo->stepsize;

		 
		if (chip->ecc.size && step_size != chip->ecc.size)
			continue;

		for (j = 0; j < stepinfo->nstrengths; j++) {
			strength = stepinfo->strengths[j];

			if (mtd->writesize % step_size)
				continue;

			nsteps = mtd->writesize / step_size;

			ecc_bytes = caps->calc_ecc_bytes(step_size, strength);
			if (WARN_ON_ONCE(ecc_bytes < 0))
				continue;

			if (ecc_bytes * nsteps > oobavail)
				continue;

			corr = strength * nsteps;

			 
			if (corr > best_corr ||
			    (corr == best_corr && step_size > best_step)) {
				best_corr = corr;
				best_step = step_size;
				best_strength = strength;
				best_ecc_bytes = ecc_bytes;
			}
		}
	}

	if (!best_corr)
		return -ENOTSUPP;

	chip->ecc.size = best_step;
	chip->ecc.strength = best_strength;
	chip->ecc.bytes = best_ecc_bytes;

	return 0;
}

 
int nand_ecc_choose_conf(struct nand_chip *chip,
			 const struct nand_ecc_caps *caps, int oobavail)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_device *nanddev = mtd_to_nanddev(mtd);

	if (WARN_ON(oobavail < 0 || oobavail > mtd->oobsize))
		return -EINVAL;

	if (chip->ecc.size && chip->ecc.strength)
		return nand_check_ecc_caps(chip, caps, oobavail);

	if (nanddev->ecc.user_conf.flags & NAND_ECC_MAXIMIZE_STRENGTH)
		return nand_maximize_ecc(chip, caps, oobavail);

	if (!nand_match_ecc_req(chip, caps, oobavail))
		return 0;

	return nand_maximize_ecc(chip, caps, oobavail);
}
EXPORT_SYMBOL_GPL(nand_ecc_choose_conf);

static int rawnand_erase(struct nand_device *nand, const struct nand_pos *pos)
{
	struct nand_chip *chip = container_of(nand, struct nand_chip,
					      base);
	unsigned int eb = nanddev_pos_to_row(nand, pos);
	int ret;

	eb >>= nand->rowconv.eraseblock_addr_shift;

	nand_select_target(chip, pos->target);
	ret = nand_erase_op(chip, eb);
	nand_deselect_target(chip);

	return ret;
}

static int rawnand_markbad(struct nand_device *nand,
			   const struct nand_pos *pos)
{
	struct nand_chip *chip = container_of(nand, struct nand_chip,
					      base);

	return nand_markbad_bbm(chip, nanddev_pos_to_offs(nand, pos));
}

static bool rawnand_isbad(struct nand_device *nand, const struct nand_pos *pos)
{
	struct nand_chip *chip = container_of(nand, struct nand_chip,
					      base);
	int ret;

	nand_select_target(chip, pos->target);
	ret = nand_isbad_bbm(chip, nanddev_pos_to_offs(nand, pos));
	nand_deselect_target(chip);

	return ret;
}

static const struct nand_ops rawnand_ops = {
	.erase = rawnand_erase,
	.markbad = rawnand_markbad,
	.isbad = rawnand_isbad,
};

 
static int nand_scan_tail(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int ret, i;

	 
	if (WARN_ON((chip->bbt_options & NAND_BBT_NO_OOB_BBM) &&
		   !(chip->bbt_options & NAND_BBT_USE_FLASH))) {
		return -EINVAL;
	}

	chip->data_buf = kmalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	if (!chip->data_buf)
		return -ENOMEM;

	 
	nand_select_target(chip, 0);
	ret = nand_manufacturer_init(chip);
	nand_deselect_target(chip);
	if (ret)
		goto err_free_buf;

	 
	chip->oob_poi = chip->data_buf + mtd->writesize;

	 
	if (!mtd->ooblayout &&
	    !(ecc->engine_type == NAND_ECC_ENGINE_TYPE_SOFT &&
	      ecc->algo == NAND_ECC_ALGO_BCH) &&
	    !(ecc->engine_type == NAND_ECC_ENGINE_TYPE_SOFT &&
	      ecc->algo == NAND_ECC_ALGO_HAMMING)) {
		switch (mtd->oobsize) {
		case 8:
		case 16:
			mtd_set_ooblayout(mtd, nand_get_small_page_ooblayout());
			break;
		case 64:
		case 128:
			mtd_set_ooblayout(mtd,
					  nand_get_large_page_hamming_ooblayout());
			break;
		default:
			 
			if (ecc->engine_type == NAND_ECC_ENGINE_TYPE_NONE) {
				mtd_set_ooblayout(mtd,
						  nand_get_large_page_ooblayout());
				break;
			}

			WARN(1, "No oob scheme defined for oobsize %d\n",
				mtd->oobsize);
			ret = -EINVAL;
			goto err_nand_manuf_cleanup;
		}
	}

	 

	switch (ecc->engine_type) {
	case NAND_ECC_ENGINE_TYPE_ON_HOST:
		ret = nand_set_ecc_on_host_ops(chip);
		if (ret)
			goto err_nand_manuf_cleanup;

		if (mtd->writesize >= ecc->size) {
			if (!ecc->strength) {
				WARN(1, "Driver must set ecc.strength when using hardware ECC\n");
				ret = -EINVAL;
				goto err_nand_manuf_cleanup;
			}
			break;
		}
		pr_warn("%d byte HW ECC not possible on %d byte page size, fallback to SW ECC\n",
			ecc->size, mtd->writesize);
		ecc->engine_type = NAND_ECC_ENGINE_TYPE_SOFT;
		ecc->algo = NAND_ECC_ALGO_HAMMING;
		fallthrough;

	case NAND_ECC_ENGINE_TYPE_SOFT:
		ret = nand_set_ecc_soft_ops(chip);
		if (ret)
			goto err_nand_manuf_cleanup;
		break;

	case NAND_ECC_ENGINE_TYPE_ON_DIE:
		if (!ecc->read_page || !ecc->write_page) {
			WARN(1, "No ECC functions supplied; on-die ECC not possible\n");
			ret = -EINVAL;
			goto err_nand_manuf_cleanup;
		}
		if (!ecc->read_oob)
			ecc->read_oob = nand_read_oob_std;
		if (!ecc->write_oob)
			ecc->write_oob = nand_write_oob_std;
		break;

	case NAND_ECC_ENGINE_TYPE_NONE:
		pr_warn("NAND_ECC_ENGINE_TYPE_NONE selected by board driver. This is not recommended!\n");
		ecc->read_page = nand_read_page_raw;
		ecc->write_page = nand_write_page_raw;
		ecc->read_oob = nand_read_oob_std;
		ecc->read_page_raw = nand_read_page_raw;
		ecc->write_page_raw = nand_write_page_raw;
		ecc->write_oob = nand_write_oob_std;
		ecc->size = mtd->writesize;
		ecc->bytes = 0;
		ecc->strength = 0;
		break;

	default:
		WARN(1, "Invalid NAND_ECC_MODE %d\n", ecc->engine_type);
		ret = -EINVAL;
		goto err_nand_manuf_cleanup;
	}

	if (ecc->correct || ecc->calculate) {
		ecc->calc_buf = kmalloc(mtd->oobsize, GFP_KERNEL);
		ecc->code_buf = kmalloc(mtd->oobsize, GFP_KERNEL);
		if (!ecc->calc_buf || !ecc->code_buf) {
			ret = -ENOMEM;
			goto err_nand_manuf_cleanup;
		}
	}

	 
	if (!ecc->read_oob_raw)
		ecc->read_oob_raw = ecc->read_oob;
	if (!ecc->write_oob_raw)
		ecc->write_oob_raw = ecc->write_oob;

	 
	mtd->ecc_strength = ecc->strength;
	mtd->ecc_step_size = ecc->size;

	 
	if (!ecc->steps)
		ecc->steps = mtd->writesize / ecc->size;
	if (ecc->steps * ecc->size != mtd->writesize) {
		WARN(1, "Invalid ECC parameters\n");
		ret = -EINVAL;
		goto err_nand_manuf_cleanup;
	}

	if (!ecc->total) {
		ecc->total = ecc->steps * ecc->bytes;
		chip->base.ecc.ctx.total = ecc->total;
	}

	if (ecc->total > mtd->oobsize) {
		WARN(1, "Total number of ECC bytes exceeded oobsize\n");
		ret = -EINVAL;
		goto err_nand_manuf_cleanup;
	}

	 
	ret = mtd_ooblayout_count_freebytes(mtd);
	if (ret < 0)
		ret = 0;

	mtd->oobavail = ret;

	 
	if (!nand_ecc_is_strong_enough(&chip->base))
		pr_warn("WARNING: %s: the ECC used on your system (%db/%dB) is too weak compared to the one required by the NAND chip (%db/%dB)\n",
			mtd->name, chip->ecc.strength, chip->ecc.size,
			nanddev_get_ecc_requirements(&chip->base)->strength,
			nanddev_get_ecc_requirements(&chip->base)->step_size);

	 
	if (!(chip->options & NAND_NO_SUBPAGE_WRITE) && nand_is_slc(chip)) {
		switch (ecc->steps) {
		case 2:
			mtd->subpage_sft = 1;
			break;
		case 4:
		case 8:
		case 16:
			mtd->subpage_sft = 2;
			break;
		}
	}
	chip->subpagesize = mtd->writesize >> mtd->subpage_sft;

	 
	chip->pagecache.page = -1;

	 
	switch (ecc->engine_type) {
	case NAND_ECC_ENGINE_TYPE_SOFT:
		if (chip->page_shift > 9)
			chip->options |= NAND_SUBPAGE_READ;
		break;

	default:
		break;
	}

	ret = nanddev_init(&chip->base, &rawnand_ops, mtd->owner);
	if (ret)
		goto err_nand_manuf_cleanup;

	 
	if (chip->options & NAND_ROM)
		mtd->flags = MTD_CAP_ROM;

	 
	mtd->_erase = nand_erase;
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
	mtd->_panic_write = panic_nand_write;
	mtd->_read_oob = nand_read_oob;
	mtd->_write_oob = nand_write_oob;
	mtd->_sync = nand_sync;
	mtd->_lock = nand_lock;
	mtd->_unlock = nand_unlock;
	mtd->_suspend = nand_suspend;
	mtd->_resume = nand_resume;
	mtd->_reboot = nand_shutdown;
	mtd->_block_isreserved = nand_block_isreserved;
	mtd->_block_isbad = nand_block_isbad;
	mtd->_block_markbad = nand_block_markbad;
	mtd->_max_bad_blocks = nanddev_mtd_max_bad_blocks;

	 
	if (!mtd->bitflip_threshold)
		mtd->bitflip_threshold = DIV_ROUND_UP(mtd->ecc_strength * 3, 4);

	 
	ret = nand_choose_interface_config(chip);
	if (ret)
		goto err_nanddev_cleanup;

	 
	for (i = 0; i < nanddev_ntargets(&chip->base); i++) {
		ret = nand_setup_interface(chip, i);
		if (ret)
			goto err_free_interface_config;
	}

	rawnand_late_check_supported_ops(chip);

	 
	ret = of_get_nand_secure_regions(chip);
	if (ret)
		goto err_free_interface_config;

	 
	if (chip->options & NAND_SKIP_BBTSCAN)
		return 0;

	 
	ret = nand_create_bbt(chip);
	if (ret)
		goto err_free_secure_regions;

	return 0;

err_free_secure_regions:
	kfree(chip->secure_regions);

err_free_interface_config:
	kfree(chip->best_interface_config);

err_nanddev_cleanup:
	nanddev_cleanup(&chip->base);

err_nand_manuf_cleanup:
	nand_manufacturer_cleanup(chip);

err_free_buf:
	kfree(chip->data_buf);
	kfree(ecc->code_buf);
	kfree(ecc->calc_buf);

	return ret;
}

static int nand_attach(struct nand_chip *chip)
{
	if (chip->controller->ops && chip->controller->ops->attach_chip)
		return chip->controller->ops->attach_chip(chip);

	return 0;
}

static void nand_detach(struct nand_chip *chip)
{
	if (chip->controller->ops && chip->controller->ops->detach_chip)
		chip->controller->ops->detach_chip(chip);
}

 
int nand_scan_with_ids(struct nand_chip *chip, unsigned int maxchips,
		       struct nand_flash_dev *ids)
{
	int ret;

	if (!maxchips)
		return -EINVAL;

	ret = nand_scan_ident(chip, maxchips, ids);
	if (ret)
		return ret;

	ret = nand_attach(chip);
	if (ret)
		goto cleanup_ident;

	ret = nand_scan_tail(chip);
	if (ret)
		goto detach_chip;

	return 0;

detach_chip:
	nand_detach(chip);
cleanup_ident:
	nand_scan_ident_cleanup(chip);

	return ret;
}
EXPORT_SYMBOL(nand_scan_with_ids);

 
void nand_cleanup(struct nand_chip *chip)
{
	if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_SOFT) {
		if (chip->ecc.algo == NAND_ECC_ALGO_HAMMING)
			rawnand_sw_hamming_cleanup(chip);
		else if (chip->ecc.algo == NAND_ECC_ALGO_BCH)
			rawnand_sw_bch_cleanup(chip);
	}

	nanddev_cleanup(&chip->base);

	 
	kfree(chip->secure_regions);

	 
	kfree(chip->bbt);
	kfree(chip->data_buf);
	kfree(chip->ecc.code_buf);
	kfree(chip->ecc.calc_buf);

	 
	if (chip->badblock_pattern && chip->badblock_pattern->options
			& NAND_BBT_DYNAMICSTRUCT)
		kfree(chip->badblock_pattern);

	 
	kfree(chip->best_interface_config);

	 
	nand_manufacturer_cleanup(chip);

	 
	nand_detach(chip);

	 
	nand_scan_ident_cleanup(chip);
}

EXPORT_SYMBOL_GPL(nand_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven J. Hill <sjhill@realitydiluted.com>");
MODULE_AUTHOR("Thomas Gleixner <tglx@linutronix.de>");
MODULE_DESCRIPTION("Generic NAND flash driver code");
