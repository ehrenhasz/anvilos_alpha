
 
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched/task_stack.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mtd/partitions.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/dma/mxs-dma.h>
#include "gpmi-nand.h"
#include "gpmi-regs.h"
#include "bch-regs.h"

 
#define GPMI_NAND_GPMI_REGS_ADDR_RES_NAME  "gpmi-nand"
#define GPMI_NAND_BCH_REGS_ADDR_RES_NAME   "bch"
#define GPMI_NAND_BCH_INTERRUPT_RES_NAME   "bch"

 
#define TO_CYCLES(duration, period) DIV_ROUND_UP_ULL(duration, period)

#define MXS_SET_ADDR		0x4
#define MXS_CLR_ADDR		0x8
 
static int clear_poll_bit(void __iomem *addr, u32 mask)
{
	int timeout = 0x400;

	 
	writel(mask, addr + MXS_CLR_ADDR);

	 
	udelay(1);

	 
	while ((readl(addr) & mask) && --timeout)
		 ;

	return !timeout;
}

#define MODULE_CLKGATE		(1 << 30)
#define MODULE_SFTRST		(1 << 31)
 
static int gpmi_reset_block(void __iomem *reset_addr, bool just_enable)
{
	int ret;
	int timeout = 0x400;

	 
	ret = clear_poll_bit(reset_addr, MODULE_SFTRST);
	if (unlikely(ret))
		goto error;

	 
	writel(MODULE_CLKGATE, reset_addr + MXS_CLR_ADDR);

	if (!just_enable) {
		 
		writel(MODULE_SFTRST, reset_addr + MXS_SET_ADDR);
		udelay(1);

		 
		while ((!(readl(reset_addr) & MODULE_CLKGATE)) && --timeout)
			 ;
		if (unlikely(!timeout))
			goto error;
	}

	 
	ret = clear_poll_bit(reset_addr, MODULE_SFTRST);
	if (unlikely(ret))
		goto error;

	 
	ret = clear_poll_bit(reset_addr, MODULE_CLKGATE);
	if (unlikely(ret))
		goto error;

	return 0;

error:
	pr_err("%s(%p): module reset timeout\n", __func__, reset_addr);
	return -ETIMEDOUT;
}

static int __gpmi_enable_clk(struct gpmi_nand_data *this, bool v)
{
	struct clk *clk;
	int ret;
	int i;

	for (i = 0; i < GPMI_CLK_MAX; i++) {
		clk = this->resources.clock[i];
		if (!clk)
			break;

		if (v) {
			ret = clk_prepare_enable(clk);
			if (ret)
				goto err_clk;
		} else {
			clk_disable_unprepare(clk);
		}
	}
	return 0;

err_clk:
	for (; i > 0; i--)
		clk_disable_unprepare(this->resources.clock[i - 1]);
	return ret;
}

static int gpmi_init(struct gpmi_nand_data *this)
{
	struct resources *r = &this->resources;
	int ret;

	ret = pm_runtime_resume_and_get(this->dev);
	if (ret < 0)
		return ret;

	ret = gpmi_reset_block(r->gpmi_regs, false);
	if (ret)
		goto err_out;

	 
	ret = gpmi_reset_block(r->bch_regs, GPMI_IS_MXS(this));
	if (ret)
		goto err_out;

	 
	writel(BM_GPMI_CTRL1_GPMI_MODE, r->gpmi_regs + HW_GPMI_CTRL1_CLR);

	 
	writel(BM_GPMI_CTRL1_ATA_IRQRDY_POLARITY,
				r->gpmi_regs + HW_GPMI_CTRL1_SET);

	 
	writel(BM_GPMI_CTRL1_DEV_RESET, r->gpmi_regs + HW_GPMI_CTRL1_SET);

	 
	writel(BM_GPMI_CTRL1_BCH_MODE, r->gpmi_regs + HW_GPMI_CTRL1_SET);

	 
	writel(BM_GPMI_CTRL1_DECOUPLE_CS | BM_GPMI_CTRL1_GANGED_RDYBUSY,
	       r->gpmi_regs + HW_GPMI_CTRL1_SET);

err_out:
	pm_runtime_mark_last_busy(this->dev);
	pm_runtime_put_autosuspend(this->dev);
	return ret;
}

 
static void gpmi_dump_info(struct gpmi_nand_data *this)
{
	struct resources *r = &this->resources;
	struct bch_geometry *geo = &this->bch_geometry;
	u32 reg;
	int i;

	dev_err(this->dev, "Show GPMI registers :\n");
	for (i = 0; i <= HW_GPMI_DEBUG / 0x10 + 1; i++) {
		reg = readl(r->gpmi_regs + i * 0x10);
		dev_err(this->dev, "offset 0x%.3x : 0x%.8x\n", i * 0x10, reg);
	}

	 
	dev_err(this->dev, "Show BCH registers :\n");
	for (i = 0; i <= HW_BCH_VERSION / 0x10 + 1; i++) {
		reg = readl(r->bch_regs + i * 0x10);
		dev_err(this->dev, "offset 0x%.3x : 0x%.8x\n", i * 0x10, reg);
	}
	dev_err(this->dev, "BCH Geometry :\n"
		"GF length              : %u\n"
		"ECC Strength           : %u\n"
		"Page Size in Bytes     : %u\n"
		"Metadata Size in Bytes : %u\n"
		"ECC0 Chunk Size in Bytes: %u\n"
		"ECCn Chunk Size in Bytes: %u\n"
		"ECC Chunk Count        : %u\n"
		"Payload Size in Bytes  : %u\n"
		"Auxiliary Size in Bytes: %u\n"
		"Auxiliary Status Offset: %u\n"
		"Block Mark Byte Offset : %u\n"
		"Block Mark Bit Offset  : %u\n",
		geo->gf_len,
		geo->ecc_strength,
		geo->page_size,
		geo->metadata_size,
		geo->ecc0_chunk_size,
		geo->eccn_chunk_size,
		geo->ecc_chunk_count,
		geo->payload_size,
		geo->auxiliary_size,
		geo->auxiliary_status_offset,
		geo->block_mark_byte_offset,
		geo->block_mark_bit_offset);
}

static bool gpmi_check_ecc(struct gpmi_nand_data *this)
{
	struct nand_chip *chip = &this->nand;
	struct bch_geometry *geo = &this->bch_geometry;
	struct nand_device *nand = &chip->base;
	struct nand_ecc_props *conf = &nand->ecc.ctx.conf;

	conf->step_size = geo->eccn_chunk_size;
	conf->strength = geo->ecc_strength;

	 
	if (GPMI_IS_MXS(this)) {
		 
		if (geo->gf_len == 14)
			return false;
	}

	if (geo->ecc_strength > this->devdata->bch_max_ecc_strength)
		return false;

	if (!nand_ecc_is_strong_enough(nand))
		return false;

	return true;
}

 
static bool bbm_in_data_chunk(struct gpmi_nand_data *this,
			unsigned int *chunk_num)
{
	struct bch_geometry *geo = &this->bch_geometry;
	struct nand_chip *chip = &this->nand;
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int i, j;

	if (geo->ecc0_chunk_size != geo->eccn_chunk_size) {
		dev_err(this->dev,
			"The size of ecc0_chunk must equal to eccn_chunk\n");
		return false;
	}

	i = (mtd->writesize * 8 - geo->metadata_size * 8) /
		(geo->gf_len * geo->ecc_strength +
			geo->eccn_chunk_size * 8);

	j = (mtd->writesize * 8 - geo->metadata_size * 8) -
		(geo->gf_len * geo->ecc_strength +
			geo->eccn_chunk_size * 8) * i;

	if (j < geo->eccn_chunk_size * 8) {
		*chunk_num = i+1;
		dev_dbg(this->dev, "Set ecc to %d and bbm in chunk %d\n",
				geo->ecc_strength, *chunk_num);
		return true;
	}

	return false;
}

 
static int set_geometry_by_ecc_info(struct gpmi_nand_data *this,
				    unsigned int ecc_strength,
				    unsigned int ecc_step)
{
	struct bch_geometry *geo = &this->bch_geometry;
	struct nand_chip *chip = &this->nand;
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int block_mark_bit_offset;

	switch (ecc_step) {
	case SZ_512:
		geo->gf_len = 13;
		break;
	case SZ_1K:
		geo->gf_len = 14;
		break;
	default:
		dev_err(this->dev,
			"unsupported nand chip. ecc bits : %d, ecc size : %d\n",
			nanddev_get_ecc_requirements(&chip->base)->strength,
			nanddev_get_ecc_requirements(&chip->base)->step_size);
		return -EINVAL;
	}
	geo->ecc0_chunk_size = ecc_step;
	geo->eccn_chunk_size = ecc_step;
	geo->ecc_strength = round_up(ecc_strength, 2);
	if (!gpmi_check_ecc(this))
		return -EINVAL;

	 
	if (geo->eccn_chunk_size < mtd->oobsize) {
		dev_err(this->dev,
			"unsupported nand chip. ecc size: %d, oob size : %d\n",
			ecc_step, mtd->oobsize);
		return -EINVAL;
	}

	 
	geo->metadata_size = 10;

	geo->ecc_chunk_count = mtd->writesize / geo->eccn_chunk_size;

	 
	geo->page_size = mtd->writesize + geo->metadata_size +
		(geo->gf_len * geo->ecc_strength * geo->ecc_chunk_count) / 8;

	geo->payload_size = mtd->writesize;

	geo->auxiliary_status_offset = ALIGN(geo->metadata_size, 4);
	geo->auxiliary_size = ALIGN(geo->metadata_size, 4)
				+ ALIGN(geo->ecc_chunk_count, 4);

	if (!this->swap_block_mark)
		return 0;

	 
	block_mark_bit_offset = mtd->writesize * 8 -
		(geo->ecc_strength * geo->gf_len * (geo->ecc_chunk_count - 1)
				+ geo->metadata_size * 8);

	geo->block_mark_byte_offset = block_mark_bit_offset / 8;
	geo->block_mark_bit_offset  = block_mark_bit_offset % 8;
	return 0;
}

 
static inline int get_ecc_strength(struct gpmi_nand_data *this)
{
	struct bch_geometry *geo = &this->bch_geometry;
	struct mtd_info	*mtd = nand_to_mtd(&this->nand);
	int ecc_strength;

	ecc_strength = ((mtd->oobsize - geo->metadata_size) * 8)
			/ (geo->gf_len * geo->ecc_chunk_count);

	 
	return round_down(ecc_strength, 2);
}

static int set_geometry_for_large_oob(struct gpmi_nand_data *this)
{
	struct bch_geometry *geo = &this->bch_geometry;
	struct nand_chip *chip = &this->nand;
	struct mtd_info *mtd = nand_to_mtd(chip);
	const struct nand_ecc_props *requirements =
		nanddev_get_ecc_requirements(&chip->base);
	unsigned int block_mark_bit_offset;
	unsigned int max_ecc;
	unsigned int bbm_chunk;
	unsigned int i;

	 
	if (!(requirements->strength > 0 &&
	      requirements->step_size > 0))
		return -EINVAL;
	geo->ecc_strength = requirements->strength;

	 
	if (!gpmi_check_ecc(this)) {
		dev_err(this->dev,
			"unsupported NAND chip, minimum ecc required %d\n",
			geo->ecc_strength);
		return -EINVAL;
	}

	 
	geo->metadata_size = 10;
	geo->gf_len = 14;
	geo->ecc0_chunk_size = 1024;
	geo->eccn_chunk_size = 1024;
	geo->ecc_chunk_count = mtd->writesize / geo->eccn_chunk_size;
	max_ecc = min(get_ecc_strength(this),
		      this->devdata->bch_max_ecc_strength);

	 
	geo->ecc_strength = max_ecc;
	while (!(geo->ecc_strength < requirements->strength)) {
		if (bbm_in_data_chunk(this, &bbm_chunk))
			goto geo_setting;
		geo->ecc_strength -= 2;
	}

	 
	 
	geo->ecc_strength = requirements->strength;
	 
	geo->ecc0_chunk_size = 0;
	geo->ecc_chunk_count = (mtd->writesize / geo->eccn_chunk_size) + 1;
	geo->ecc_for_meta = 1;
	 
	if (mtd->oobsize * 8 < geo->metadata_size * 8 +
	    geo->gf_len * geo->ecc_strength * geo->ecc_chunk_count) {
		dev_err(this->dev, "unsupported NAND chip with new layout\n");
		return -EINVAL;
	}

	 
	bbm_chunk = (mtd->writesize * 8 - geo->metadata_size * 8 -
		     geo->gf_len * geo->ecc_strength) /
		     (geo->gf_len * geo->ecc_strength +
		     geo->eccn_chunk_size * 8) + 1;

geo_setting:

	geo->page_size = mtd->writesize + geo->metadata_size +
		(geo->gf_len * geo->ecc_strength * geo->ecc_chunk_count) / 8;
	geo->payload_size = mtd->writesize;

	 
	geo->auxiliary_status_offset = ALIGN(geo->metadata_size, 4);
	geo->auxiliary_size = ALIGN(geo->metadata_size, 4)
				    + ALIGN(geo->ecc_chunk_count, 4);

	if (!this->swap_block_mark)
		return 0;

	 
	i = (mtd->writesize / geo->eccn_chunk_size) - bbm_chunk + 1;

	block_mark_bit_offset = mtd->writesize * 8 -
		(geo->ecc_strength * geo->gf_len * (geo->ecc_chunk_count - i)
		+ geo->metadata_size * 8);

	geo->block_mark_byte_offset = block_mark_bit_offset / 8;
	geo->block_mark_bit_offset  = block_mark_bit_offset % 8;

	dev_dbg(this->dev, "BCH Geometry :\n"
		"GF length              : %u\n"
		"ECC Strength           : %u\n"
		"Page Size in Bytes     : %u\n"
		"Metadata Size in Bytes : %u\n"
		"ECC0 Chunk Size in Bytes: %u\n"
		"ECCn Chunk Size in Bytes: %u\n"
		"ECC Chunk Count        : %u\n"
		"Payload Size in Bytes  : %u\n"
		"Auxiliary Size in Bytes: %u\n"
		"Auxiliary Status Offset: %u\n"
		"Block Mark Byte Offset : %u\n"
		"Block Mark Bit Offset  : %u\n"
		"Block Mark in chunk	: %u\n"
		"Ecc for Meta data	: %u\n",
		geo->gf_len,
		geo->ecc_strength,
		geo->page_size,
		geo->metadata_size,
		geo->ecc0_chunk_size,
		geo->eccn_chunk_size,
		geo->ecc_chunk_count,
		geo->payload_size,
		geo->auxiliary_size,
		geo->auxiliary_status_offset,
		geo->block_mark_byte_offset,
		geo->block_mark_bit_offset,
		bbm_chunk,
		geo->ecc_for_meta);

	return 0;
}

static int legacy_set_geometry(struct gpmi_nand_data *this)
{
	struct bch_geometry *geo = &this->bch_geometry;
	struct mtd_info *mtd = nand_to_mtd(&this->nand);
	unsigned int metadata_size;
	unsigned int status_size;
	unsigned int block_mark_bit_offset;

	 
	geo->metadata_size = 10;

	 
	geo->gf_len = 13;

	 
	geo->ecc0_chunk_size = 512;
	geo->eccn_chunk_size = 512;
	while (geo->eccn_chunk_size < mtd->oobsize) {
		geo->ecc0_chunk_size *= 2;  
		geo->eccn_chunk_size *= 2;  
		geo->gf_len = 14;
	}

	geo->ecc_chunk_count = mtd->writesize / geo->eccn_chunk_size;

	 
	geo->ecc_strength = get_ecc_strength(this);
	if (!gpmi_check_ecc(this)) {
		dev_err(this->dev,
			"ecc strength: %d cannot be supported by the controller (%d)\n"
			"try to use minimum ecc strength that NAND chip required\n",
			geo->ecc_strength,
			this->devdata->bch_max_ecc_strength);
		return -EINVAL;
	}

	geo->page_size = mtd->writesize + geo->metadata_size +
		(geo->gf_len * geo->ecc_strength * geo->ecc_chunk_count) / 8;
	geo->payload_size = mtd->writesize;

	 
	metadata_size = ALIGN(geo->metadata_size, 4);
	status_size   = ALIGN(geo->ecc_chunk_count, 4);

	geo->auxiliary_size = metadata_size + status_size;
	geo->auxiliary_status_offset = metadata_size;

	if (!this->swap_block_mark)
		return 0;

	 
	block_mark_bit_offset = mtd->writesize * 8 -
		(geo->ecc_strength * geo->gf_len * (geo->ecc_chunk_count - 1)
				+ geo->metadata_size * 8);

	geo->block_mark_byte_offset = block_mark_bit_offset / 8;
	geo->block_mark_bit_offset  = block_mark_bit_offset % 8;
	return 0;
}

static int common_nfc_set_geometry(struct gpmi_nand_data *this)
{
	struct nand_chip *chip = &this->nand;
	struct mtd_info *mtd = nand_to_mtd(&this->nand);
	const struct nand_ecc_props *requirements =
		nanddev_get_ecc_requirements(&chip->base);
	bool use_minimun_ecc;
	int err;

	use_minimun_ecc = of_property_read_bool(this->dev->of_node,
						"fsl,use-minimum-ecc");

	 
	if ((!use_minimun_ecc && mtd->oobsize < 1024) ||
	    !(requirements->strength > 0 && requirements->step_size > 0)) {
		dev_dbg(this->dev, "use legacy bch geometry\n");
		err = legacy_set_geometry(this);
		if (!err)
			return 0;
	}

	 
	if (mtd->oobsize > 1024) {
		dev_dbg(this->dev, "use large oob bch geometry\n");
		err = set_geometry_for_large_oob(this);
		if (!err)
			return 0;
	}

	 
	dev_dbg(this->dev, "use minimum ecc bch geometry\n");
	err = set_geometry_by_ecc_info(this, requirements->strength,
					requirements->step_size);
	if (err)
		dev_err(this->dev, "none of the bch geometry setting works\n");

	return err;
}

 
static int bch_set_geometry(struct gpmi_nand_data *this)
{
	struct resources *r = &this->resources;
	int ret;

	ret = common_nfc_set_geometry(this);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(this->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(this->dev);
		return ret;
	}

	 
	ret = gpmi_reset_block(r->bch_regs, GPMI_IS_MXS(this));
	if (ret)
		goto err_out;

	 
	writel(0, r->bch_regs + HW_BCH_LAYOUTSELECT);

	ret = 0;
err_out:
	pm_runtime_mark_last_busy(this->dev);
	pm_runtime_put_autosuspend(this->dev);

	return ret;
}

 
static int gpmi_nfc_compute_timings(struct gpmi_nand_data *this,
				    const struct nand_sdr_timings *sdr)
{
	struct gpmi_nfc_hardware_timing *hw = &this->hw;
	struct resources *r = &this->resources;
	unsigned int dll_threshold_ps = this->devdata->max_chain_delay;
	unsigned int period_ps, reference_period_ps;
	unsigned int data_setup_cycles, data_hold_cycles, addr_setup_cycles;
	unsigned int tRP_ps;
	bool use_half_period;
	int sample_delay_ps, sample_delay_factor;
	unsigned int busy_timeout_cycles;
	u8 wrn_dly_sel;
	unsigned long clk_rate, min_rate;
	u64 busy_timeout_ps;

	if (sdr->tRC_min >= 30000) {
		 
		hw->clk_rate = 22000000;
		min_rate = 0;
		wrn_dly_sel = BV_GPMI_CTRL1_WRN_DLY_SEL_4_TO_8NS;
	} else if (sdr->tRC_min >= 25000) {
		 
		hw->clk_rate = 80000000;
		min_rate = 22000000;
		wrn_dly_sel = BV_GPMI_CTRL1_WRN_DLY_SEL_NO_DELAY;
	} else {
		 
		hw->clk_rate = 100000000;
		min_rate = 80000000;
		wrn_dly_sel = BV_GPMI_CTRL1_WRN_DLY_SEL_NO_DELAY;
	}

	clk_rate = clk_round_rate(r->clock[0], hw->clk_rate);
	if (clk_rate <= min_rate) {
		dev_err(this->dev, "clock setting: expected %ld, got %ld\n",
			hw->clk_rate, clk_rate);
		return -ENOTSUPP;
	}

	hw->clk_rate = clk_rate;
	 
	period_ps = div_u64((u64)NSEC_PER_SEC * 1000, hw->clk_rate);

	addr_setup_cycles = TO_CYCLES(sdr->tALS_min, period_ps);
	data_setup_cycles = TO_CYCLES(sdr->tDS_min, period_ps);
	data_hold_cycles = TO_CYCLES(sdr->tDH_min, period_ps);
	busy_timeout_ps = max(sdr->tBERS_max, sdr->tPROG_max);
	busy_timeout_cycles = TO_CYCLES(busy_timeout_ps, period_ps);

	hw->timing0 = BF_GPMI_TIMING0_ADDRESS_SETUP(addr_setup_cycles) |
		      BF_GPMI_TIMING0_DATA_HOLD(data_hold_cycles) |
		      BF_GPMI_TIMING0_DATA_SETUP(data_setup_cycles);
	hw->timing1 = BF_GPMI_TIMING1_BUSY_TIMEOUT(DIV_ROUND_UP(busy_timeout_cycles, 4096));

	 
	if (period_ps > dll_threshold_ps) {
		use_half_period = true;
		reference_period_ps = period_ps / 2;
	} else {
		use_half_period = false;
		reference_period_ps = period_ps;
	}

	tRP_ps = data_setup_cycles * period_ps;
	sample_delay_ps = (sdr->tREA_max + 4000 - tRP_ps) * 8;
	if (sample_delay_ps > 0)
		sample_delay_factor = sample_delay_ps / reference_period_ps;
	else
		sample_delay_factor = 0;

	hw->ctrl1n = BF_GPMI_CTRL1_WRN_DLY_SEL(wrn_dly_sel);
	if (sample_delay_factor)
		hw->ctrl1n |= BF_GPMI_CTRL1_RDN_DELAY(sample_delay_factor) |
			      BM_GPMI_CTRL1_DLL_ENABLE |
			      (use_half_period ? BM_GPMI_CTRL1_HALF_PERIOD : 0);
	return 0;
}

static int gpmi_nfc_apply_timings(struct gpmi_nand_data *this)
{
	struct gpmi_nfc_hardware_timing *hw = &this->hw;
	struct resources *r = &this->resources;
	void __iomem *gpmi_regs = r->gpmi_regs;
	unsigned int dll_wait_time_us;
	int ret;

	 
	if (GPMI_IS_MX6Q(this) || GPMI_IS_MX6SX(this))
		clk_disable_unprepare(r->clock[0]);

	ret = clk_set_rate(r->clock[0], hw->clk_rate);
	if (ret) {
		dev_err(this->dev, "cannot set clock rate to %lu Hz: %d\n", hw->clk_rate, ret);
		return ret;
	}

	if (GPMI_IS_MX6Q(this) || GPMI_IS_MX6SX(this)) {
		ret = clk_prepare_enable(r->clock[0]);
		if (ret)
			return ret;
	}

	writel(hw->timing0, gpmi_regs + HW_GPMI_TIMING0);
	writel(hw->timing1, gpmi_regs + HW_GPMI_TIMING1);

	 
	writel(BM_GPMI_CTRL1_CLEAR_MASK, gpmi_regs + HW_GPMI_CTRL1_CLR);
	writel(hw->ctrl1n, gpmi_regs + HW_GPMI_CTRL1_SET);

	 
	dll_wait_time_us = USEC_PER_SEC / hw->clk_rate * 64;
	if (!dll_wait_time_us)
		dll_wait_time_us = 1;

	 
	udelay(dll_wait_time_us);

	return 0;
}

static int gpmi_setup_interface(struct nand_chip *chip, int chipnr,
				const struct nand_interface_config *conf)
{
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	const struct nand_sdr_timings *sdr;
	int ret;

	 
	sdr = nand_get_sdr_timings(conf);
	if (IS_ERR(sdr))
		return PTR_ERR(sdr);

	 
	if (sdr->tRC_min <= 25000 && !GPMI_IS_MX28(this) && !GPMI_IS_MX6(this))
		return -ENOTSUPP;

	 
	if (chipnr < 0)
		return 0;

	 
	ret = gpmi_nfc_compute_timings(this, sdr);
	if (ret)
		return ret;

	this->hw.must_apply_timings = true;

	return 0;
}

 
static void gpmi_clear_bch(struct gpmi_nand_data *this)
{
	struct resources *r = &this->resources;
	writel(BM_BCH_CTRL_COMPLETE_IRQ, r->bch_regs + HW_BCH_CTRL_CLR);
}

static struct dma_chan *get_dma_chan(struct gpmi_nand_data *this)
{
	 
	return this->dma_chans[0];
}

 
static void dma_irq_callback(void *param)
{
	struct gpmi_nand_data *this = param;
	struct completion *dma_c = &this->dma_done;

	complete(dma_c);
}

static irqreturn_t bch_irq(int irq, void *cookie)
{
	struct gpmi_nand_data *this = cookie;

	gpmi_clear_bch(this);
	complete(&this->bch_done);
	return IRQ_HANDLED;
}

static int gpmi_raw_len_to_len(struct gpmi_nand_data *this, int raw_len)
{
	 
	if (this->bch)
		return ALIGN_DOWN(raw_len, this->bch_geometry.eccn_chunk_size);
	else
		return raw_len;
}

 
static bool prepare_data_dma(struct gpmi_nand_data *this, const void *buf,
			     int raw_len, struct scatterlist *sgl,
			     enum dma_data_direction dr)
{
	int ret;
	int len = gpmi_raw_len_to_len(this, raw_len);

	 
	if (virt_addr_valid(buf) && !object_is_on_stack(buf)) {
		sg_init_one(sgl, buf, len);
		ret = dma_map_sg(this->dev, sgl, 1, dr);
		if (ret == 0)
			goto map_fail;

		return true;
	}

map_fail:
	 
	sg_init_one(sgl, this->data_buffer_dma, len);

	if (dr == DMA_TO_DEVICE && buf != this->data_buffer_dma)
		memcpy(this->data_buffer_dma, buf, len);

	dma_map_sg(this->dev, sgl, 1, dr);

	return false;
}

 
static uint8_t scan_ff_pattern[] = { 0xff };
static struct nand_bbt_descr gpmi_bbt_descr = {
	.options	= 0,
	.offs		= 0,
	.len		= 1,
	.pattern	= scan_ff_pattern
};

 
static int gpmi_ooblayout_ecc(struct mtd_info *mtd, int section,
			      struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct bch_geometry *geo = &this->bch_geometry;

	if (section)
		return -ERANGE;

	oobregion->offset = 0;
	oobregion->length = geo->page_size - mtd->writesize;

	return 0;
}

static int gpmi_ooblayout_free(struct mtd_info *mtd, int section,
			       struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct bch_geometry *geo = &this->bch_geometry;

	if (section)
		return -ERANGE;

	 
	if (geo->page_size < mtd->writesize + mtd->oobsize) {
		oobregion->offset = geo->page_size - mtd->writesize;
		oobregion->length = mtd->oobsize - oobregion->offset;
	}

	return 0;
}

static const char * const gpmi_clks_for_mx2x[] = {
	"gpmi_io",
};

static const struct mtd_ooblayout_ops gpmi_ooblayout_ops = {
	.ecc = gpmi_ooblayout_ecc,
	.free = gpmi_ooblayout_free,
};

static const struct gpmi_devdata gpmi_devdata_imx23 = {
	.type = IS_MX23,
	.bch_max_ecc_strength = 20,
	.max_chain_delay = 16000,
	.clks = gpmi_clks_for_mx2x,
	.clks_count = ARRAY_SIZE(gpmi_clks_for_mx2x),
};

static const struct gpmi_devdata gpmi_devdata_imx28 = {
	.type = IS_MX28,
	.bch_max_ecc_strength = 20,
	.max_chain_delay = 16000,
	.clks = gpmi_clks_for_mx2x,
	.clks_count = ARRAY_SIZE(gpmi_clks_for_mx2x),
};

static const char * const gpmi_clks_for_mx6[] = {
	"gpmi_io", "gpmi_apb", "gpmi_bch", "gpmi_bch_apb", "per1_bch",
};

static const struct gpmi_devdata gpmi_devdata_imx6q = {
	.type = IS_MX6Q,
	.bch_max_ecc_strength = 40,
	.max_chain_delay = 12000,
	.clks = gpmi_clks_for_mx6,
	.clks_count = ARRAY_SIZE(gpmi_clks_for_mx6),
};

static const struct gpmi_devdata gpmi_devdata_imx6sx = {
	.type = IS_MX6SX,
	.bch_max_ecc_strength = 62,
	.max_chain_delay = 12000,
	.clks = gpmi_clks_for_mx6,
	.clks_count = ARRAY_SIZE(gpmi_clks_for_mx6),
};

static const char * const gpmi_clks_for_mx7d[] = {
	"gpmi_io", "gpmi_bch_apb",
};

static const struct gpmi_devdata gpmi_devdata_imx7d = {
	.type = IS_MX7D,
	.bch_max_ecc_strength = 62,
	.max_chain_delay = 12000,
	.clks = gpmi_clks_for_mx7d,
	.clks_count = ARRAY_SIZE(gpmi_clks_for_mx7d),
};

static int acquire_register_block(struct gpmi_nand_data *this,
				  const char *res_name)
{
	struct platform_device *pdev = this->pdev;
	struct resources *res = &this->resources;
	void __iomem *p;

	p = devm_platform_ioremap_resource_byname(pdev, res_name);
	if (IS_ERR(p))
		return PTR_ERR(p);

	if (!strcmp(res_name, GPMI_NAND_GPMI_REGS_ADDR_RES_NAME))
		res->gpmi_regs = p;
	else if (!strcmp(res_name, GPMI_NAND_BCH_REGS_ADDR_RES_NAME))
		res->bch_regs = p;
	else
		dev_err(this->dev, "unknown resource name : %s\n", res_name);

	return 0;
}

static int acquire_bch_irq(struct gpmi_nand_data *this, irq_handler_t irq_h)
{
	struct platform_device *pdev = this->pdev;
	const char *res_name = GPMI_NAND_BCH_INTERRUPT_RES_NAME;
	int err;

	err = platform_get_irq_byname(pdev, res_name);
	if (err < 0)
		return err;

	err = devm_request_irq(this->dev, err, irq_h, 0, res_name, this);
	if (err)
		dev_err(this->dev, "error requesting BCH IRQ\n");

	return err;
}

static void release_dma_channels(struct gpmi_nand_data *this)
{
	unsigned int i;
	for (i = 0; i < DMA_CHANS; i++)
		if (this->dma_chans[i]) {
			dma_release_channel(this->dma_chans[i]);
			this->dma_chans[i] = NULL;
		}
}

static int acquire_dma_channels(struct gpmi_nand_data *this)
{
	struct platform_device *pdev = this->pdev;
	struct dma_chan *dma_chan;
	int ret = 0;

	 
	dma_chan = dma_request_chan(&pdev->dev, "rx-tx");
	if (IS_ERR(dma_chan)) {
		ret = dev_err_probe(this->dev, PTR_ERR(dma_chan),
				    "DMA channel request failed\n");
		release_dma_channels(this);
	} else {
		this->dma_chans[0] = dma_chan;
	}

	return ret;
}

static int gpmi_get_clks(struct gpmi_nand_data *this)
{
	struct resources *r = &this->resources;
	struct clk *clk;
	int err, i;

	for (i = 0; i < this->devdata->clks_count; i++) {
		clk = devm_clk_get(this->dev, this->devdata->clks[i]);
		if (IS_ERR(clk)) {
			err = PTR_ERR(clk);
			goto err_clock;
		}

		r->clock[i] = clk;
	}

	return 0;

err_clock:
	dev_dbg(this->dev, "failed in finding the clocks.\n");
	return err;
}

static int acquire_resources(struct gpmi_nand_data *this)
{
	int ret;

	ret = acquire_register_block(this, GPMI_NAND_GPMI_REGS_ADDR_RES_NAME);
	if (ret)
		goto exit_regs;

	ret = acquire_register_block(this, GPMI_NAND_BCH_REGS_ADDR_RES_NAME);
	if (ret)
		goto exit_regs;

	ret = acquire_bch_irq(this, bch_irq);
	if (ret)
		goto exit_regs;

	ret = acquire_dma_channels(this);
	if (ret)
		goto exit_regs;

	ret = gpmi_get_clks(this);
	if (ret)
		goto exit_clock;
	return 0;

exit_clock:
	release_dma_channels(this);
exit_regs:
	return ret;
}

static void release_resources(struct gpmi_nand_data *this)
{
	release_dma_channels(this);
}

static void gpmi_free_dma_buffer(struct gpmi_nand_data *this)
{
	struct device *dev = this->dev;
	struct bch_geometry *geo = &this->bch_geometry;

	if (this->auxiliary_virt && virt_addr_valid(this->auxiliary_virt))
		dma_free_coherent(dev, geo->auxiliary_size,
					this->auxiliary_virt,
					this->auxiliary_phys);
	kfree(this->data_buffer_dma);
	kfree(this->raw_buffer);

	this->data_buffer_dma	= NULL;
	this->raw_buffer	= NULL;
}

 
static int gpmi_alloc_dma_buffer(struct gpmi_nand_data *this)
{
	struct bch_geometry *geo = &this->bch_geometry;
	struct device *dev = this->dev;
	struct mtd_info *mtd = nand_to_mtd(&this->nand);

	 
	this->data_buffer_dma = kzalloc(mtd->writesize ?: PAGE_SIZE,
					GFP_DMA | GFP_KERNEL);
	if (this->data_buffer_dma == NULL)
		goto error_alloc;

	this->auxiliary_virt = dma_alloc_coherent(dev, geo->auxiliary_size,
					&this->auxiliary_phys, GFP_DMA);
	if (!this->auxiliary_virt)
		goto error_alloc;

	this->raw_buffer = kzalloc((mtd->writesize ?: PAGE_SIZE) + mtd->oobsize, GFP_KERNEL);
	if (!this->raw_buffer)
		goto error_alloc;

	return 0;

error_alloc:
	gpmi_free_dma_buffer(this);
	return -ENOMEM;
}

 
static void block_mark_swapping(struct gpmi_nand_data *this,
				void *payload, void *auxiliary)
{
	struct bch_geometry *nfc_geo = &this->bch_geometry;
	unsigned char *p;
	unsigned char *a;
	unsigned int  bit;
	unsigned char mask;
	unsigned char from_data;
	unsigned char from_oob;

	if (!this->swap_block_mark)
		return;

	 
	bit = nfc_geo->block_mark_bit_offset;
	p   = payload + nfc_geo->block_mark_byte_offset;
	a   = auxiliary;

	 
	from_data = (p[0] >> bit) | (p[1] << (8 - bit));

	 
	from_oob = a[0];

	 
	a[0] = from_data;

	mask = (0x1 << bit) - 1;
	p[0] = (p[0] & mask) | (from_oob << bit);

	mask = ~0 << bit;
	p[1] = (p[1] & mask) | (from_oob >> (8 - bit));
}

static int gpmi_count_bitflips(struct nand_chip *chip, void *buf, int first,
			       int last, int meta)
{
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct bch_geometry *nfc_geo = &this->bch_geometry;
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i;
	unsigned char *status;
	unsigned int max_bitflips = 0;

	 
	status = this->auxiliary_virt + ALIGN(meta, 4);

	for (i = first; i < last; i++, status++) {
		if ((*status == STATUS_GOOD) || (*status == STATUS_ERASED))
			continue;

		if (*status == STATUS_UNCORRECTABLE) {
			int eccbits = nfc_geo->ecc_strength * nfc_geo->gf_len;
			u8 *eccbuf = this->raw_buffer;
			int offset, bitoffset;
			int eccbytes;
			int flips;

			 
			offset = nfc_geo->metadata_size * 8;
			offset += ((8 * nfc_geo->eccn_chunk_size) + eccbits) * (i + 1);
			offset -= eccbits;
			bitoffset = offset % 8;
			eccbytes = DIV_ROUND_UP(offset + eccbits, 8);
			offset /= 8;
			eccbytes -= offset;
			nand_change_read_column_op(chip, offset, eccbuf,
						   eccbytes, false);

			 
			if (bitoffset)
				eccbuf[0] |= GENMASK(bitoffset - 1, 0);

			bitoffset = (bitoffset + eccbits) % 8;
			if (bitoffset)
				eccbuf[eccbytes - 1] |= GENMASK(7, bitoffset);

			 
			if (i == 0) {
				 
				flips = nand_check_erased_ecc_chunk(
						buf + i * nfc_geo->eccn_chunk_size,
						nfc_geo->eccn_chunk_size,
						eccbuf, eccbytes,
						this->auxiliary_virt,
						nfc_geo->metadata_size,
						nfc_geo->ecc_strength);
			} else {
				flips = nand_check_erased_ecc_chunk(
						buf + i * nfc_geo->eccn_chunk_size,
						nfc_geo->eccn_chunk_size,
						eccbuf, eccbytes,
						NULL, 0,
						nfc_geo->ecc_strength);
			}

			if (flips > 0) {
				max_bitflips = max_t(unsigned int, max_bitflips,
						     flips);
				mtd->ecc_stats.corrected += flips;
				continue;
			}

			mtd->ecc_stats.failed++;
			continue;
		}

		mtd->ecc_stats.corrected += *status;
		max_bitflips = max_t(unsigned int, max_bitflips, *status);
	}

	return max_bitflips;
}

static void gpmi_bch_layout_std(struct gpmi_nand_data *this)
{
	struct bch_geometry *geo = &this->bch_geometry;
	unsigned int ecc_strength = geo->ecc_strength >> 1;
	unsigned int gf_len = geo->gf_len;
	unsigned int block0_size = geo->ecc0_chunk_size;
	unsigned int blockn_size = geo->eccn_chunk_size;

	this->bch_flashlayout0 =
		BF_BCH_FLASH0LAYOUT0_NBLOCKS(geo->ecc_chunk_count - 1) |
		BF_BCH_FLASH0LAYOUT0_META_SIZE(geo->metadata_size) |
		BF_BCH_FLASH0LAYOUT0_ECC0(ecc_strength, this) |
		BF_BCH_FLASH0LAYOUT0_GF(gf_len, this) |
		BF_BCH_FLASH0LAYOUT0_DATA0_SIZE(block0_size, this);

	this->bch_flashlayout1 =
		BF_BCH_FLASH0LAYOUT1_PAGE_SIZE(geo->page_size) |
		BF_BCH_FLASH0LAYOUT1_ECCN(ecc_strength, this) |
		BF_BCH_FLASH0LAYOUT1_GF(gf_len, this) |
		BF_BCH_FLASH0LAYOUT1_DATAN_SIZE(blockn_size, this);
}

static int gpmi_ecc_read_page(struct nand_chip *chip, uint8_t *buf,
			      int oob_required, int page)
{
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct bch_geometry *geo = &this->bch_geometry;
	unsigned int max_bitflips;
	int ret;

	gpmi_bch_layout_std(this);
	this->bch = true;

	ret = nand_read_page_op(chip, page, 0, buf, geo->page_size);
	if (ret)
		return ret;

	max_bitflips = gpmi_count_bitflips(chip, buf, 0,
					   geo->ecc_chunk_count,
					   geo->auxiliary_status_offset);

	 
	block_mark_swapping(this, buf, this->auxiliary_virt);

	if (oob_required) {
		 
		memset(chip->oob_poi, ~0, mtd->oobsize);
		chip->oob_poi[0] = ((uint8_t *)this->auxiliary_virt)[0];
	}

	return max_bitflips;
}

 
static int gpmi_ecc_read_subpage(struct nand_chip *chip, uint32_t offs,
				 uint32_t len, uint8_t *buf, int page)
{
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct bch_geometry *geo = &this->bch_geometry;
	int size = chip->ecc.size;  
	int meta, n, page_size;
	unsigned int max_bitflips;
	unsigned int ecc_strength;
	int first, last, marker_pos;
	int ecc_parity_size;
	int col = 0;
	int ret;

	 
	ecc_parity_size = geo->gf_len * geo->ecc_strength / 8;

	 
	first = offs / size;
	last = (offs + len - 1) / size;

	if (this->swap_block_mark) {
		 
		marker_pos = geo->block_mark_byte_offset / size;
		if (last >= marker_pos && first <= marker_pos) {
			dev_dbg(this->dev,
				"page:%d, first:%d, last:%d, marker at:%d\n",
				page, first, last, marker_pos);
			return gpmi_ecc_read_page(chip, buf, 0, page);
		}
	}

	 

	meta = geo->metadata_size;
	if (first) {
		if (geo->ecc_for_meta)
			col = meta + ecc_parity_size
				+ (size + ecc_parity_size) * first;
		else
			col = meta + (size + ecc_parity_size) * first;

		meta = 0;
		buf = buf + first * size;
	}

	ecc_parity_size = geo->gf_len * geo->ecc_strength / 8;
	n = last - first + 1;

	if (geo->ecc_for_meta && meta)
		page_size = meta + ecc_parity_size
			    + (size + ecc_parity_size) * n;
	else
		page_size = meta + (size + ecc_parity_size) * n;

	ecc_strength = geo->ecc_strength >> 1;

	this->bch_flashlayout0 = BF_BCH_FLASH0LAYOUT0_NBLOCKS(
		(geo->ecc_for_meta ? n : n - 1)) |
		BF_BCH_FLASH0LAYOUT0_META_SIZE(meta) |
		BF_BCH_FLASH0LAYOUT0_ECC0(ecc_strength, this) |
		BF_BCH_FLASH0LAYOUT0_GF(geo->gf_len, this) |
		BF_BCH_FLASH0LAYOUT0_DATA0_SIZE((geo->ecc_for_meta ?
		0 : geo->ecc0_chunk_size), this);

	this->bch_flashlayout1 = BF_BCH_FLASH0LAYOUT1_PAGE_SIZE(page_size) |
		BF_BCH_FLASH0LAYOUT1_ECCN(ecc_strength, this) |
		BF_BCH_FLASH0LAYOUT1_GF(geo->gf_len, this) |
		BF_BCH_FLASH0LAYOUT1_DATAN_SIZE(geo->eccn_chunk_size, this);

	this->bch = true;

	ret = nand_read_page_op(chip, page, col, buf, page_size);
	if (ret)
		return ret;

	dev_dbg(this->dev, "page:%d(%d:%d)%d, chunk:(%d:%d), BCH PG size:%d\n",
		page, offs, len, col, first, n, page_size);

	max_bitflips = gpmi_count_bitflips(chip, buf, first, last, meta);

	return max_bitflips;
}

static int gpmi_ecc_write_page(struct nand_chip *chip, const uint8_t *buf,
			       int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct bch_geometry *nfc_geo = &this->bch_geometry;

	dev_dbg(this->dev, "ecc write page.\n");

	gpmi_bch_layout_std(this);
	this->bch = true;

	memcpy(this->auxiliary_virt, chip->oob_poi, nfc_geo->auxiliary_size);

	if (this->swap_block_mark) {
		 
		memcpy(this->data_buffer_dma, buf, mtd->writesize);
		buf = this->data_buffer_dma;
		block_mark_swapping(this, this->data_buffer_dma,
				    this->auxiliary_virt);
	}

	return nand_prog_page_op(chip, page, 0, buf, nfc_geo->page_size);
}

 
static int gpmi_ecc_read_oob(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	int ret;

	 
	memset(chip->oob_poi, ~0, mtd->oobsize);

	 
	ret = nand_read_page_op(chip, page, mtd->writesize, chip->oob_poi,
				mtd->oobsize);
	if (ret)
		return ret;

	 
	if (GPMI_IS_MX23(this)) {
		 
		ret = nand_read_page_op(chip, page, 0, chip->oob_poi, 1);
		if (ret)
			return ret;
	}

	return 0;
}

static int gpmi_ecc_write_oob(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mtd_oob_region of = { };

	 
	mtd_ooblayout_free(mtd, 0, &of);
	if (!of.length)
		return -EPERM;

	if (!nand_is_slc(chip))
		return -EPERM;

	return nand_prog_page_op(chip, page, mtd->writesize + of.offset,
				 chip->oob_poi + of.offset, of.length);
}

 
static int gpmi_ecc_read_page_raw(struct nand_chip *chip, uint8_t *buf,
				  int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct bch_geometry *nfc_geo = &this->bch_geometry;
	int eccsize = nfc_geo->eccn_chunk_size;
	int eccbits = nfc_geo->ecc_strength * nfc_geo->gf_len;
	u8 *tmp_buf = this->raw_buffer;
	size_t src_bit_off;
	size_t oob_bit_off;
	size_t oob_byte_off;
	uint8_t *oob = chip->oob_poi;
	int step;
	int ret;

	ret = nand_read_page_op(chip, page, 0, tmp_buf,
				mtd->writesize + mtd->oobsize);
	if (ret)
		return ret;

	 
	if (this->swap_block_mark)
		swap(tmp_buf[0], tmp_buf[mtd->writesize]);

	 
	if (oob_required)
		memcpy(oob, tmp_buf, nfc_geo->metadata_size);

	oob_bit_off = nfc_geo->metadata_size * 8;
	src_bit_off = oob_bit_off;

	 
	for (step = 0; step < nfc_geo->ecc_chunk_count; step++) {
		if (buf)
			nand_extract_bits(buf, step * eccsize * 8, tmp_buf,
					  src_bit_off, eccsize * 8);
		src_bit_off += eccsize * 8;

		 
		if (step == nfc_geo->ecc_chunk_count - 1 &&
		    (oob_bit_off + eccbits) % 8)
			eccbits += 8 - ((oob_bit_off + eccbits) % 8);

		if (oob_required)
			nand_extract_bits(oob, oob_bit_off, tmp_buf,
					  src_bit_off, eccbits);

		src_bit_off += eccbits;
		oob_bit_off += eccbits;
	}

	if (oob_required) {
		oob_byte_off = oob_bit_off / 8;

		if (oob_byte_off < mtd->oobsize)
			memcpy(oob + oob_byte_off,
			       tmp_buf + mtd->writesize + oob_byte_off,
			       mtd->oobsize - oob_byte_off);
	}

	return 0;
}

 
static int gpmi_ecc_write_page_raw(struct nand_chip *chip, const uint8_t *buf,
				   int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct bch_geometry *nfc_geo = &this->bch_geometry;
	int eccsize = nfc_geo->eccn_chunk_size;
	int eccbits = nfc_geo->ecc_strength * nfc_geo->gf_len;
	u8 *tmp_buf = this->raw_buffer;
	uint8_t *oob = chip->oob_poi;
	size_t dst_bit_off;
	size_t oob_bit_off;
	size_t oob_byte_off;
	int step;

	 
	if (!buf || !oob_required)
		memset(tmp_buf, 0xff, mtd->writesize + mtd->oobsize);

	 
	memcpy(tmp_buf, oob, nfc_geo->metadata_size);
	oob_bit_off = nfc_geo->metadata_size * 8;
	dst_bit_off = oob_bit_off;

	 
	for (step = 0; step < nfc_geo->ecc_chunk_count; step++) {
		if (buf)
			nand_extract_bits(tmp_buf, dst_bit_off, buf,
					  step * eccsize * 8, eccsize * 8);
		dst_bit_off += eccsize * 8;

		 
		if (step == nfc_geo->ecc_chunk_count - 1 &&
		    (oob_bit_off + eccbits) % 8)
			eccbits += 8 - ((oob_bit_off + eccbits) % 8);

		if (oob_required)
			nand_extract_bits(tmp_buf, dst_bit_off, oob,
					  oob_bit_off, eccbits);

		dst_bit_off += eccbits;
		oob_bit_off += eccbits;
	}

	oob_byte_off = oob_bit_off / 8;

	if (oob_required && oob_byte_off < mtd->oobsize)
		memcpy(tmp_buf + mtd->writesize + oob_byte_off,
		       oob + oob_byte_off, mtd->oobsize - oob_byte_off);

	 
	if (this->swap_block_mark)
		swap(tmp_buf[0], tmp_buf[mtd->writesize]);

	return nand_prog_page_op(chip, page, 0, tmp_buf,
				 mtd->writesize + mtd->oobsize);
}

static int gpmi_ecc_read_oob_raw(struct nand_chip *chip, int page)
{
	return gpmi_ecc_read_page_raw(chip, NULL, 1, page);
}

static int gpmi_ecc_write_oob_raw(struct nand_chip *chip, int page)
{
	return gpmi_ecc_write_page_raw(chip, NULL, 1, page);
}

static int gpmi_block_markbad(struct nand_chip *chip, loff_t ofs)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	int ret = 0;
	uint8_t *block_mark;
	int column, page, chipnr;

	chipnr = (int)(ofs >> chip->chip_shift);
	nand_select_target(chip, chipnr);

	column = !GPMI_IS_MX23(this) ? mtd->writesize : 0;

	 
	block_mark = this->data_buffer_dma;
	block_mark[0] = 0;  

	 
	page = (int)(ofs >> chip->page_shift);

	ret = nand_prog_page_op(chip, page, column, block_mark, 1);

	nand_deselect_target(chip);

	return ret;
}

static int nand_boot_set_geometry(struct gpmi_nand_data *this)
{
	struct boot_rom_geometry *geometry = &this->rom_geometry;

	 
	geometry->stride_size_in_pages = 64;

	 
	geometry->search_area_stride_exponent = 2;
	return 0;
}

static const char  *fingerprint = "STMP";
static int mx23_check_transcription_stamp(struct gpmi_nand_data *this)
{
	struct boot_rom_geometry *rom_geo = &this->rom_geometry;
	struct device *dev = this->dev;
	struct nand_chip *chip = &this->nand;
	unsigned int search_area_size_in_strides;
	unsigned int stride;
	unsigned int page;
	u8 *buffer = nand_get_data_buf(chip);
	int found_an_ncb_fingerprint = false;
	int ret;

	 
	search_area_size_in_strides = 1 << rom_geo->search_area_stride_exponent;

	nand_select_target(chip, 0);

	 
	dev_dbg(dev, "Scanning for an NCB fingerprint...\n");

	for (stride = 0; stride < search_area_size_in_strides; stride++) {
		 
		page = stride * rom_geo->stride_size_in_pages;

		dev_dbg(dev, "Looking for a fingerprint in page 0x%x\n", page);

		 
		ret = nand_read_page_op(chip, page, 12, buffer,
					strlen(fingerprint));
		if (ret)
			continue;

		 
		if (!memcmp(buffer, fingerprint, strlen(fingerprint))) {
			found_an_ncb_fingerprint = true;
			break;
		}

	}

	nand_deselect_target(chip);

	if (found_an_ncb_fingerprint)
		dev_dbg(dev, "\tFound a fingerprint\n");
	else
		dev_dbg(dev, "\tNo fingerprint found\n");
	return found_an_ncb_fingerprint;
}

 
static int mx23_write_transcription_stamp(struct gpmi_nand_data *this)
{
	struct device *dev = this->dev;
	struct boot_rom_geometry *rom_geo = &this->rom_geometry;
	struct nand_chip *chip = &this->nand;
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int block_size_in_pages;
	unsigned int search_area_size_in_strides;
	unsigned int search_area_size_in_pages;
	unsigned int search_area_size_in_blocks;
	unsigned int block;
	unsigned int stride;
	unsigned int page;
	u8 *buffer = nand_get_data_buf(chip);
	int status;

	 
	block_size_in_pages = mtd->erasesize / mtd->writesize;
	search_area_size_in_strides = 1 << rom_geo->search_area_stride_exponent;
	search_area_size_in_pages = search_area_size_in_strides *
					rom_geo->stride_size_in_pages;
	search_area_size_in_blocks =
		  (search_area_size_in_pages + (block_size_in_pages - 1)) /
				    block_size_in_pages;

	dev_dbg(dev, "Search Area Geometry :\n");
	dev_dbg(dev, "\tin Blocks : %u\n", search_area_size_in_blocks);
	dev_dbg(dev, "\tin Strides: %u\n", search_area_size_in_strides);
	dev_dbg(dev, "\tin Pages  : %u\n", search_area_size_in_pages);

	nand_select_target(chip, 0);

	 
	dev_dbg(dev, "Erasing the search area...\n");

	for (block = 0; block < search_area_size_in_blocks; block++) {
		 
		dev_dbg(dev, "\tErasing block 0x%x\n", block);
		status = nand_erase_op(chip, block);
		if (status)
			dev_err(dev, "[%s] Erase failed.\n", __func__);
	}

	 
	memset(buffer, ~0, mtd->writesize);
	memcpy(buffer + 12, fingerprint, strlen(fingerprint));

	 
	dev_dbg(dev, "Writing NCB fingerprints...\n");
	for (stride = 0; stride < search_area_size_in_strides; stride++) {
		 
		page = stride * rom_geo->stride_size_in_pages;

		 
		dev_dbg(dev, "Writing an NCB fingerprint in page 0x%x\n", page);

		status = chip->ecc.write_page_raw(chip, buffer, 0, page);
		if (status)
			dev_err(dev, "[%s] Write failed.\n", __func__);
	}

	nand_deselect_target(chip);

	return 0;
}

static int mx23_boot_init(struct gpmi_nand_data  *this)
{
	struct device *dev = this->dev;
	struct nand_chip *chip = &this->nand;
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int block_count;
	unsigned int block;
	int     chipnr;
	int     page;
	loff_t  byte;
	uint8_t block_mark;
	int     ret = 0;

	 
	if (mx23_check_transcription_stamp(this))
		return 0;

	 
	dev_dbg(dev, "Transcribing bad block marks...\n");

	 
	block_count = nanddev_eraseblocks_per_target(&chip->base);

	 
	for (block = 0; block < block_count; block++) {
		 
		chipnr = block >> (chip->chip_shift - chip->phys_erase_shift);
		page = block << (chip->phys_erase_shift - chip->page_shift);
		byte = block <<  chip->phys_erase_shift;

		 
		nand_select_target(chip, chipnr);
		ret = nand_read_page_op(chip, page, mtd->writesize, &block_mark,
					1);
		nand_deselect_target(chip);

		if (ret)
			continue;

		 
		if (block_mark != 0xff) {
			dev_dbg(dev, "Transcribing mark in block %u\n", block);
			ret = chip->legacy.block_markbad(chip, byte);
			if (ret)
				dev_err(dev,
					"Failed to mark block bad with ret %d\n",
					ret);
		}
	}

	 
	mx23_write_transcription_stamp(this);
	return 0;
}

static int nand_boot_init(struct gpmi_nand_data  *this)
{
	nand_boot_set_geometry(this);

	 
	if (GPMI_IS_MX23(this))
		return mx23_boot_init(this);
	return 0;
}

static int gpmi_set_geometry(struct gpmi_nand_data *this)
{
	int ret;

	 
	gpmi_free_dma_buffer(this);

	 
	ret = bch_set_geometry(this);
	if (ret) {
		dev_err(this->dev, "Error setting BCH geometry : %d\n", ret);
		return ret;
	}

	 
	return gpmi_alloc_dma_buffer(this);
}

static int gpmi_init_last(struct gpmi_nand_data *this)
{
	struct nand_chip *chip = &this->nand;
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	struct bch_geometry *bch_geo = &this->bch_geometry;
	int ret;

	 
	ret = gpmi_set_geometry(this);
	if (ret)
		return ret;

	 
	ecc->read_page	= gpmi_ecc_read_page;
	ecc->write_page	= gpmi_ecc_write_page;
	ecc->read_oob	= gpmi_ecc_read_oob;
	ecc->write_oob	= gpmi_ecc_write_oob;
	ecc->read_page_raw = gpmi_ecc_read_page_raw;
	ecc->write_page_raw = gpmi_ecc_write_page_raw;
	ecc->read_oob_raw = gpmi_ecc_read_oob_raw;
	ecc->write_oob_raw = gpmi_ecc_write_oob_raw;
	ecc->engine_type = NAND_ECC_ENGINE_TYPE_ON_HOST;
	ecc->size	= bch_geo->eccn_chunk_size;
	ecc->strength	= bch_geo->ecc_strength;
	mtd_set_ooblayout(mtd, &gpmi_ooblayout_ops);

	 
	if (GPMI_IS_MX6(this) &&
		((bch_geo->gf_len * bch_geo->ecc_strength) % 8) == 0) {
		ecc->read_subpage = gpmi_ecc_read_subpage;
		chip->options |= NAND_SUBPAGE_READ;
	}

	return 0;
}

static int gpmi_nand_attach_chip(struct nand_chip *chip)
{
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	int ret;

	if (chip->bbt_options & NAND_BBT_USE_FLASH) {
		chip->bbt_options |= NAND_BBT_NO_OOB;

		if (of_property_read_bool(this->dev->of_node,
					  "fsl,no-blockmark-swap"))
			this->swap_block_mark = false;
	}
	dev_dbg(this->dev, "Blockmark swapping %sabled\n",
		this->swap_block_mark ? "en" : "dis");

	ret = gpmi_init_last(this);
	if (ret)
		return ret;

	chip->options |= NAND_SKIP_BBTSCAN;

	return 0;
}

static struct gpmi_transfer *get_next_transfer(struct gpmi_nand_data *this)
{
	struct gpmi_transfer *transfer = &this->transfers[this->ntransfers];

	this->ntransfers++;

	if (this->ntransfers == GPMI_MAX_TRANSFERS)
		return NULL;

	return transfer;
}

static struct dma_async_tx_descriptor *gpmi_chain_command(
	struct gpmi_nand_data *this, u8 cmd, const u8 *addr, int naddr)
{
	struct dma_chan *channel = get_dma_chan(this);
	struct dma_async_tx_descriptor *desc;
	struct gpmi_transfer *transfer;
	int chip = this->nand.cur_cs;
	u32 pio[3];

	 
	pio[0] = BF_GPMI_CTRL0_COMMAND_MODE(BV_GPMI_CTRL0_COMMAND_MODE__WRITE)
		| BM_GPMI_CTRL0_WORD_LENGTH
		| BF_GPMI_CTRL0_CS(chip, this)
		| BF_GPMI_CTRL0_LOCK_CS(LOCK_CS_ENABLE, this)
		| BF_GPMI_CTRL0_ADDRESS(BV_GPMI_CTRL0_ADDRESS__NAND_CLE)
		| BM_GPMI_CTRL0_ADDRESS_INCREMENT
		| BF_GPMI_CTRL0_XFER_COUNT(naddr + 1);
	pio[1] = 0;
	pio[2] = 0;
	desc = mxs_dmaengine_prep_pio(channel, pio, ARRAY_SIZE(pio),
				      DMA_TRANS_NONE, 0);
	if (!desc)
		return NULL;

	transfer = get_next_transfer(this);
	if (!transfer)
		return NULL;

	transfer->cmdbuf[0] = cmd;
	if (naddr)
		memcpy(&transfer->cmdbuf[1], addr, naddr);

	sg_init_one(&transfer->sgl, transfer->cmdbuf, naddr + 1);
	dma_map_sg(this->dev, &transfer->sgl, 1, DMA_TO_DEVICE);

	transfer->direction = DMA_TO_DEVICE;

	desc = dmaengine_prep_slave_sg(channel, &transfer->sgl, 1, DMA_MEM_TO_DEV,
				       MXS_DMA_CTRL_WAIT4END);
	return desc;
}

static struct dma_async_tx_descriptor *gpmi_chain_wait_ready(
	struct gpmi_nand_data *this)
{
	struct dma_chan *channel = get_dma_chan(this);
	u32 pio[2];

	pio[0] =  BF_GPMI_CTRL0_COMMAND_MODE(BV_GPMI_CTRL0_COMMAND_MODE__WAIT_FOR_READY)
		| BM_GPMI_CTRL0_WORD_LENGTH
		| BF_GPMI_CTRL0_CS(this->nand.cur_cs, this)
		| BF_GPMI_CTRL0_LOCK_CS(LOCK_CS_ENABLE, this)
		| BF_GPMI_CTRL0_ADDRESS(BV_GPMI_CTRL0_ADDRESS__NAND_DATA)
		| BF_GPMI_CTRL0_XFER_COUNT(0);
	pio[1] = 0;

	return mxs_dmaengine_prep_pio(channel, pio, 2, DMA_TRANS_NONE,
				MXS_DMA_CTRL_WAIT4END | MXS_DMA_CTRL_WAIT4RDY);
}

static struct dma_async_tx_descriptor *gpmi_chain_data_read(
	struct gpmi_nand_data *this, void *buf, int raw_len, bool *direct)
{
	struct dma_async_tx_descriptor *desc;
	struct dma_chan *channel = get_dma_chan(this);
	struct gpmi_transfer *transfer;
	u32 pio[6] = {};

	transfer = get_next_transfer(this);
	if (!transfer)
		return NULL;

	transfer->direction = DMA_FROM_DEVICE;

	*direct = prepare_data_dma(this, buf, raw_len, &transfer->sgl,
				   DMA_FROM_DEVICE);

	pio[0] =  BF_GPMI_CTRL0_COMMAND_MODE(BV_GPMI_CTRL0_COMMAND_MODE__READ)
		| BM_GPMI_CTRL0_WORD_LENGTH
		| BF_GPMI_CTRL0_CS(this->nand.cur_cs, this)
		| BF_GPMI_CTRL0_LOCK_CS(LOCK_CS_ENABLE, this)
		| BF_GPMI_CTRL0_ADDRESS(BV_GPMI_CTRL0_ADDRESS__NAND_DATA)
		| BF_GPMI_CTRL0_XFER_COUNT(raw_len);

	if (this->bch) {
		pio[2] =  BM_GPMI_ECCCTRL_ENABLE_ECC
			| BF_GPMI_ECCCTRL_ECC_CMD(BV_GPMI_ECCCTRL_ECC_CMD__BCH_DECODE)
			| BF_GPMI_ECCCTRL_BUFFER_MASK(BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_PAGE
				| BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_AUXONLY);
		pio[3] = raw_len;
		pio[4] = transfer->sgl.dma_address;
		pio[5] = this->auxiliary_phys;
	}

	desc = mxs_dmaengine_prep_pio(channel, pio, ARRAY_SIZE(pio),
				      DMA_TRANS_NONE, 0);
	if (!desc)
		return NULL;

	if (!this->bch)
		desc = dmaengine_prep_slave_sg(channel, &transfer->sgl, 1,
					     DMA_DEV_TO_MEM,
					     MXS_DMA_CTRL_WAIT4END);

	return desc;
}

static struct dma_async_tx_descriptor *gpmi_chain_data_write(
	struct gpmi_nand_data *this, const void *buf, int raw_len)
{
	struct dma_chan *channel = get_dma_chan(this);
	struct dma_async_tx_descriptor *desc;
	struct gpmi_transfer *transfer;
	u32 pio[6] = {};

	transfer = get_next_transfer(this);
	if (!transfer)
		return NULL;

	transfer->direction = DMA_TO_DEVICE;

	prepare_data_dma(this, buf, raw_len, &transfer->sgl, DMA_TO_DEVICE);

	pio[0] = BF_GPMI_CTRL0_COMMAND_MODE(BV_GPMI_CTRL0_COMMAND_MODE__WRITE)
		| BM_GPMI_CTRL0_WORD_LENGTH
		| BF_GPMI_CTRL0_CS(this->nand.cur_cs, this)
		| BF_GPMI_CTRL0_LOCK_CS(LOCK_CS_ENABLE, this)
		| BF_GPMI_CTRL0_ADDRESS(BV_GPMI_CTRL0_ADDRESS__NAND_DATA)
		| BF_GPMI_CTRL0_XFER_COUNT(raw_len);

	if (this->bch) {
		pio[2] = BM_GPMI_ECCCTRL_ENABLE_ECC
			| BF_GPMI_ECCCTRL_ECC_CMD(BV_GPMI_ECCCTRL_ECC_CMD__BCH_ENCODE)
			| BF_GPMI_ECCCTRL_BUFFER_MASK(BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_PAGE |
					BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_AUXONLY);
		pio[3] = raw_len;
		pio[4] = transfer->sgl.dma_address;
		pio[5] = this->auxiliary_phys;
	}

	desc = mxs_dmaengine_prep_pio(channel, pio, ARRAY_SIZE(pio),
				      DMA_TRANS_NONE,
				      (this->bch ? MXS_DMA_CTRL_WAIT4END : 0));
	if (!desc)
		return NULL;

	if (!this->bch)
		desc = dmaengine_prep_slave_sg(channel, &transfer->sgl, 1,
					       DMA_MEM_TO_DEV,
					       MXS_DMA_CTRL_WAIT4END);

	return desc;
}

static int gpmi_nfc_exec_op(struct nand_chip *chip,
			     const struct nand_operation *op,
			     bool check_only)
{
	const struct nand_op_instr *instr;
	struct gpmi_nand_data *this = nand_get_controller_data(chip);
	struct dma_async_tx_descriptor *desc = NULL;
	int i, ret, buf_len = 0, nbufs = 0;
	u8 cmd = 0;
	void *buf_read = NULL;
	const void *buf_write = NULL;
	bool direct = false;
	struct completion *dma_completion, *bch_completion;
	unsigned long to;

	if (check_only)
		return 0;

	this->ntransfers = 0;
	for (i = 0; i < GPMI_MAX_TRANSFERS; i++)
		this->transfers[i].direction = DMA_NONE;

	ret = pm_runtime_resume_and_get(this->dev);
	if (ret < 0)
		return ret;

	 
	if (this->hw.must_apply_timings) {
		this->hw.must_apply_timings = false;
		ret = gpmi_nfc_apply_timings(this);
		if (ret)
			goto out_pm;
	}

	dev_dbg(this->dev, "%s: %d instructions\n", __func__, op->ninstrs);

	for (i = 0; i < op->ninstrs; i++) {
		instr = &op->instrs[i];

		nand_op_trace("  ", instr);

		switch (instr->type) {
		case NAND_OP_WAITRDY_INSTR:
			desc = gpmi_chain_wait_ready(this);
			break;
		case NAND_OP_CMD_INSTR:
			cmd = instr->ctx.cmd.opcode;

			 
			if (i + 1 != op->ninstrs &&
			    op->instrs[i + 1].type == NAND_OP_ADDR_INSTR)
				continue;

			desc = gpmi_chain_command(this, cmd, NULL, 0);

			break;
		case NAND_OP_ADDR_INSTR:
			desc = gpmi_chain_command(this, cmd, instr->ctx.addr.addrs,
						  instr->ctx.addr.naddrs);
			break;
		case NAND_OP_DATA_OUT_INSTR:
			buf_write = instr->ctx.data.buf.out;
			buf_len = instr->ctx.data.len;
			nbufs++;

			desc = gpmi_chain_data_write(this, buf_write, buf_len);

			break;
		case NAND_OP_DATA_IN_INSTR:
			if (!instr->ctx.data.len)
				break;
			buf_read = instr->ctx.data.buf.in;
			buf_len = instr->ctx.data.len;
			nbufs++;

			desc = gpmi_chain_data_read(this, buf_read, buf_len,
						   &direct);
			break;
		}

		if (!desc) {
			ret = -ENXIO;
			goto unmap;
		}
	}

	dev_dbg(this->dev, "%s setup done\n", __func__);

	if (nbufs > 1) {
		dev_err(this->dev, "Multiple data instructions not supported\n");
		ret = -EINVAL;
		goto unmap;
	}

	if (this->bch) {
		writel(this->bch_flashlayout0,
		       this->resources.bch_regs + HW_BCH_FLASH0LAYOUT0);
		writel(this->bch_flashlayout1,
		       this->resources.bch_regs + HW_BCH_FLASH0LAYOUT1);
	}

	desc->callback = dma_irq_callback;
	desc->callback_param = this;
	dma_completion = &this->dma_done;
	bch_completion = NULL;

	init_completion(dma_completion);

	if (this->bch && buf_read) {
		writel(BM_BCH_CTRL_COMPLETE_IRQ_EN,
		       this->resources.bch_regs + HW_BCH_CTRL_SET);
		bch_completion = &this->bch_done;
		init_completion(bch_completion);
	}

	dmaengine_submit(desc);
	dma_async_issue_pending(get_dma_chan(this));

	to = wait_for_completion_timeout(dma_completion, msecs_to_jiffies(1000));
	if (!to) {
		dev_err(this->dev, "DMA timeout, last DMA\n");
		gpmi_dump_info(this);
		ret = -ETIMEDOUT;
		goto unmap;
	}

	if (this->bch && buf_read) {
		to = wait_for_completion_timeout(bch_completion, msecs_to_jiffies(1000));
		if (!to) {
			dev_err(this->dev, "BCH timeout, last DMA\n");
			gpmi_dump_info(this);
			ret = -ETIMEDOUT;
			goto unmap;
		}
	}

	writel(BM_BCH_CTRL_COMPLETE_IRQ_EN,
	       this->resources.bch_regs + HW_BCH_CTRL_CLR);
	gpmi_clear_bch(this);

	ret = 0;

unmap:
	for (i = 0; i < this->ntransfers; i++) {
		struct gpmi_transfer *transfer = &this->transfers[i];

		if (transfer->direction != DMA_NONE)
			dma_unmap_sg(this->dev, &transfer->sgl, 1,
				     transfer->direction);
	}

	if (!ret && buf_read && !direct)
		memcpy(buf_read, this->data_buffer_dma,
		       gpmi_raw_len_to_len(this, buf_len));

	this->bch = false;

out_pm:
	pm_runtime_mark_last_busy(this->dev);
	pm_runtime_put_autosuspend(this->dev);

	return ret;
}

static const struct nand_controller_ops gpmi_nand_controller_ops = {
	.attach_chip = gpmi_nand_attach_chip,
	.setup_interface = gpmi_setup_interface,
	.exec_op = gpmi_nfc_exec_op,
};

static int gpmi_nand_init(struct gpmi_nand_data *this)
{
	struct nand_chip *chip = &this->nand;
	struct mtd_info  *mtd = nand_to_mtd(chip);
	int ret;

	 
	mtd->name		= "gpmi-nand";
	mtd->dev.parent		= this->dev;

	 
	nand_set_controller_data(chip, this);
	nand_set_flash_node(chip, this->pdev->dev.of_node);
	chip->legacy.block_markbad = gpmi_block_markbad;
	chip->badblock_pattern	= &gpmi_bbt_descr;
	chip->options		|= NAND_NO_SUBPAGE_WRITE;

	 
	this->swap_block_mark = !GPMI_IS_MX23(this);

	 
	this->bch_geometry.payload_size = 1024;
	this->bch_geometry.auxiliary_size = 128;
	ret = gpmi_alloc_dma_buffer(this);
	if (ret)
		return ret;

	nand_controller_init(&this->base);
	this->base.ops = &gpmi_nand_controller_ops;
	chip->controller = &this->base;

	ret = nand_scan(chip, GPMI_IS_MX6(this) ? 2 : 1);
	if (ret)
		goto err_out;

	ret = nand_boot_init(this);
	if (ret)
		goto err_nand_cleanup;
	ret = nand_create_bbt(chip);
	if (ret)
		goto err_nand_cleanup;

	ret = mtd_device_register(mtd, NULL, 0);
	if (ret)
		goto err_nand_cleanup;
	return 0;

err_nand_cleanup:
	nand_cleanup(chip);
err_out:
	gpmi_free_dma_buffer(this);
	return ret;
}

static const struct of_device_id gpmi_nand_id_table[] = {
	{ .compatible = "fsl,imx23-gpmi-nand", .data = &gpmi_devdata_imx23, },
	{ .compatible = "fsl,imx28-gpmi-nand", .data = &gpmi_devdata_imx28, },
	{ .compatible = "fsl,imx6q-gpmi-nand", .data = &gpmi_devdata_imx6q, },
	{ .compatible = "fsl,imx6sx-gpmi-nand", .data = &gpmi_devdata_imx6sx, },
	{ .compatible = "fsl,imx7d-gpmi-nand", .data = &gpmi_devdata_imx7d,},
	{}
};
MODULE_DEVICE_TABLE(of, gpmi_nand_id_table);

static int gpmi_nand_probe(struct platform_device *pdev)
{
	struct gpmi_nand_data *this;
	int ret;

	this = devm_kzalloc(&pdev->dev, sizeof(*this), GFP_KERNEL);
	if (!this)
		return -ENOMEM;

	this->devdata = of_device_get_match_data(&pdev->dev);
	platform_set_drvdata(pdev, this);
	this->pdev  = pdev;
	this->dev   = &pdev->dev;

	ret = acquire_resources(this);
	if (ret)
		goto exit_acquire_resources;

	ret = __gpmi_enable_clk(this, true);
	if (ret)
		goto exit_acquire_resources;

	pm_runtime_set_autosuspend_delay(&pdev->dev, 500);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	ret = gpmi_init(this);
	if (ret)
		goto exit_nfc_init;

	ret = gpmi_nand_init(this);
	if (ret)
		goto exit_nfc_init;

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	dev_info(this->dev, "driver registered.\n");

	return 0;

exit_nfc_init:
	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	release_resources(this);
exit_acquire_resources:

	return ret;
}

static void gpmi_nand_remove(struct platform_device *pdev)
{
	struct gpmi_nand_data *this = platform_get_drvdata(pdev);
	struct nand_chip *chip = &this->nand;
	int ret;

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	ret = mtd_device_unregister(nand_to_mtd(chip));
	WARN_ON(ret);
	nand_cleanup(chip);
	gpmi_free_dma_buffer(this);
	release_resources(this);
}

#ifdef CONFIG_PM_SLEEP
static int gpmi_pm_suspend(struct device *dev)
{
	struct gpmi_nand_data *this = dev_get_drvdata(dev);

	release_dma_channels(this);
	return 0;
}

static int gpmi_pm_resume(struct device *dev)
{
	struct gpmi_nand_data *this = dev_get_drvdata(dev);
	int ret;

	ret = acquire_dma_channels(this);
	if (ret < 0)
		return ret;

	 
	ret = gpmi_init(this);
	if (ret) {
		dev_err(this->dev, "Error setting GPMI : %d\n", ret);
		return ret;
	}

	 
	if (this->hw.clk_rate)
		this->hw.must_apply_timings = true;

	 
	ret = bch_set_geometry(this);
	if (ret) {
		dev_err(this->dev, "Error setting BCH : %d\n", ret);
		return ret;
	}

	return 0;
}
#endif  

static int __maybe_unused gpmi_runtime_suspend(struct device *dev)
{
	struct gpmi_nand_data *this = dev_get_drvdata(dev);

	return __gpmi_enable_clk(this, false);
}

static int __maybe_unused gpmi_runtime_resume(struct device *dev)
{
	struct gpmi_nand_data *this = dev_get_drvdata(dev);

	return __gpmi_enable_clk(this, true);
}

static const struct dev_pm_ops gpmi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(gpmi_pm_suspend, gpmi_pm_resume)
	SET_RUNTIME_PM_OPS(gpmi_runtime_suspend, gpmi_runtime_resume, NULL)
};

static struct platform_driver gpmi_nand_driver = {
	.driver = {
		.name = "gpmi-nand",
		.pm = &gpmi_pm_ops,
		.of_match_table = gpmi_nand_id_table,
	},
	.probe   = gpmi_nand_probe,
	.remove_new = gpmi_nand_remove,
};
module_platform_driver(gpmi_nand_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("i.MX GPMI NAND Flash Controller Driver");
MODULE_LICENSE("GPL");
