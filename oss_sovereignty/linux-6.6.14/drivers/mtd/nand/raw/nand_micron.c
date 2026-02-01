
 

#include <linux/slab.h>

#include "internals.h"

 
#define NAND_ECC_STATUS_WRITE_RECOMMENDED	BIT(3)

 
#define NAND_ECC_STATUS_MASK		(BIT(4) | BIT(3) | BIT(0))
#define NAND_ECC_STATUS_UNCORRECTABLE	BIT(0)
#define NAND_ECC_STATUS_4_6_CORRECTED	BIT(3)
#define NAND_ECC_STATUS_1_3_CORRECTED	BIT(4)
#define NAND_ECC_STATUS_7_8_CORRECTED	(BIT(4) | BIT(3))

struct nand_onfi_vendor_micron {
	u8 two_plane_read;
	u8 read_cache;
	u8 read_unique_id;
	u8 dq_imped;
	u8 dq_imped_num_settings;
	u8 dq_imped_feat_addr;
	u8 rb_pulldown_strength;
	u8 rb_pulldown_strength_feat_addr;
	u8 rb_pulldown_strength_num_settings;
	u8 otp_mode;
	u8 otp_page_start;
	u8 otp_data_prot_addr;
	u8 otp_num_pages;
	u8 otp_feat_addr;
	u8 read_retry_options;
	u8 reserved[72];
	u8 param_revision;
} __packed;

struct micron_on_die_ecc {
	bool forced;
	bool enabled;
	void *rawbuf;
};

struct micron_nand {
	struct micron_on_die_ecc ecc;
};

static int micron_nand_setup_read_retry(struct nand_chip *chip, int retry_mode)
{
	u8 feature[ONFI_SUBFEATURE_PARAM_LEN] = {retry_mode};

	return nand_set_features(chip, ONFI_FEATURE_ADDR_READ_RETRY, feature);
}

 
static int micron_nand_onfi_init(struct nand_chip *chip)
{
	struct nand_parameters *p = &chip->parameters;

	if (p->onfi) {
		struct nand_onfi_vendor_micron *micron = (void *)p->onfi->vendor;

		chip->read_retries = micron->read_retry_options;
		chip->ops.setup_read_retry = micron_nand_setup_read_retry;
	}

	if (p->supports_set_get_features) {
		set_bit(ONFI_FEATURE_ADDR_READ_RETRY, p->set_feature_list);
		set_bit(ONFI_FEATURE_ON_DIE_ECC, p->set_feature_list);
		set_bit(ONFI_FEATURE_ADDR_READ_RETRY, p->get_feature_list);
		set_bit(ONFI_FEATURE_ON_DIE_ECC, p->get_feature_list);
	}

	return 0;
}

static int micron_nand_on_die_4_ooblayout_ecc(struct mtd_info *mtd,
					      int section,
					      struct mtd_oob_region *oobregion)
{
	if (section >= 4)
		return -ERANGE;

	oobregion->offset = (section * 16) + 8;
	oobregion->length = 8;

	return 0;
}

static int micron_nand_on_die_4_ooblayout_free(struct mtd_info *mtd,
					       int section,
					       struct mtd_oob_region *oobregion)
{
	if (section >= 4)
		return -ERANGE;

	oobregion->offset = (section * 16) + 2;
	oobregion->length = 6;

	return 0;
}

static const struct mtd_ooblayout_ops micron_nand_on_die_4_ooblayout_ops = {
	.ecc = micron_nand_on_die_4_ooblayout_ecc,
	.free = micron_nand_on_die_4_ooblayout_free,
};

static int micron_nand_on_die_8_ooblayout_ecc(struct mtd_info *mtd,
					      int section,
					      struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section)
		return -ERANGE;

	oobregion->offset = mtd->oobsize - chip->ecc.total;
	oobregion->length = chip->ecc.total;

	return 0;
}

static int micron_nand_on_die_8_ooblayout_free(struct mtd_info *mtd,
					       int section,
					       struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section)
		return -ERANGE;

	oobregion->offset = 2;
	oobregion->length = mtd->oobsize - chip->ecc.total - 2;

	return 0;
}

static const struct mtd_ooblayout_ops micron_nand_on_die_8_ooblayout_ops = {
	.ecc = micron_nand_on_die_8_ooblayout_ecc,
	.free = micron_nand_on_die_8_ooblayout_free,
};

static int micron_nand_on_die_ecc_setup(struct nand_chip *chip, bool enable)
{
	struct micron_nand *micron = nand_get_manufacturer_data(chip);
	u8 feature[ONFI_SUBFEATURE_PARAM_LEN] = { 0, };
	int ret;

	if (micron->ecc.forced)
		return 0;

	if (micron->ecc.enabled == enable)
		return 0;

	if (enable)
		feature[0] |= ONFI_FEATURE_ON_DIE_ECC_EN;

	ret = nand_set_features(chip, ONFI_FEATURE_ON_DIE_ECC, feature);
	if (!ret)
		micron->ecc.enabled = enable;

	return ret;
}

static int micron_nand_on_die_ecc_status_4(struct nand_chip *chip, u8 status,
					   void *buf, int page,
					   int oob_required)
{
	struct micron_nand *micron = nand_get_manufacturer_data(chip);
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned int step, max_bitflips = 0;
	bool use_datain = false;
	int ret;

	if (!(status & NAND_ECC_STATUS_WRITE_RECOMMENDED)) {
		if (status & NAND_STATUS_FAIL)
			mtd->ecc_stats.failed++;

		return 0;
	}

	 
	if (!oob_required) {
		 
		if (!nand_has_exec_op(chip) ||
		    !nand_read_data_op(chip, chip->oob_poi, mtd->oobsize, false,
				       true))
			use_datain = true;

		if (use_datain)
			ret = nand_read_data_op(chip, chip->oob_poi,
						mtd->oobsize, false, false);
		else
			ret = nand_change_read_column_op(chip, mtd->writesize,
							 chip->oob_poi,
							 mtd->oobsize, false);
		if (ret)
			return ret;
	}

	micron_nand_on_die_ecc_setup(chip, false);

	ret = nand_read_page_op(chip, page, 0, micron->ecc.rawbuf,
				mtd->writesize + mtd->oobsize);
	if (ret)
		return ret;

	for (step = 0; step < chip->ecc.steps; step++) {
		unsigned int offs, i, nbitflips = 0;
		u8 *rawbuf, *corrbuf;

		offs = step * chip->ecc.size;
		rawbuf = micron->ecc.rawbuf + offs;
		corrbuf = buf + offs;

		for (i = 0; i < chip->ecc.size; i++)
			nbitflips += hweight8(corrbuf[i] ^ rawbuf[i]);

		offs = (step * 16) + 4;
		rawbuf = micron->ecc.rawbuf + mtd->writesize + offs;
		corrbuf = chip->oob_poi + offs;

		for (i = 0; i < chip->ecc.bytes + 4; i++)
			nbitflips += hweight8(corrbuf[i] ^ rawbuf[i]);

		if (WARN_ON(nbitflips > chip->ecc.strength))
			return -EINVAL;

		max_bitflips = max(nbitflips, max_bitflips);
		mtd->ecc_stats.corrected += nbitflips;
	}

	return max_bitflips;
}

static int micron_nand_on_die_ecc_status_8(struct nand_chip *chip, u8 status)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	 
	switch (status & NAND_ECC_STATUS_MASK) {
	case NAND_ECC_STATUS_UNCORRECTABLE:
		mtd->ecc_stats.failed++;
		return 0;
	case NAND_ECC_STATUS_1_3_CORRECTED:
		mtd->ecc_stats.corrected += 3;
		return 3;
	case NAND_ECC_STATUS_4_6_CORRECTED:
		mtd->ecc_stats.corrected += 6;
		 
		return 6;
	case NAND_ECC_STATUS_7_8_CORRECTED:
		mtd->ecc_stats.corrected += 8;
		 
		return 8;
	default:
		return 0;
	}
}

static int
micron_nand_read_page_on_die_ecc(struct nand_chip *chip, uint8_t *buf,
				 int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	bool use_datain = false;
	u8 status;
	int ret, max_bitflips = 0;

	ret = micron_nand_on_die_ecc_setup(chip, true);
	if (ret)
		return ret;

	ret = nand_read_page_op(chip, page, 0, NULL, 0);
	if (ret)
		goto out;

	ret = nand_status_op(chip, &status);
	if (ret)
		goto out;

	 
	if (!nand_has_exec_op(chip) ||
	    !nand_read_data_op(chip, buf, mtd->writesize, false, true))
		use_datain = true;

	if (use_datain) {
		ret = nand_exit_status_op(chip);
		if (ret)
			goto out;

		ret = nand_read_data_op(chip, buf, mtd->writesize, false,
					false);
		if (!ret && oob_required)
			ret = nand_read_data_op(chip, chip->oob_poi,
						mtd->oobsize, false, false);
	} else {
		ret = nand_change_read_column_op(chip, 0, buf, mtd->writesize,
						 false);
		if (!ret && oob_required)
			ret = nand_change_read_column_op(chip, mtd->writesize,
							 chip->oob_poi,
							 mtd->oobsize, false);
	}

	if (chip->ecc.strength == 4)
		max_bitflips = micron_nand_on_die_ecc_status_4(chip, status,
							       buf, page,
							       oob_required);
	else
		max_bitflips = micron_nand_on_die_ecc_status_8(chip, status);

out:
	micron_nand_on_die_ecc_setup(chip, false);

	return ret ? ret : max_bitflips;
}

static int
micron_nand_write_page_on_die_ecc(struct nand_chip *chip, const uint8_t *buf,
				  int oob_required, int page)
{
	int ret;

	ret = micron_nand_on_die_ecc_setup(chip, true);
	if (ret)
		return ret;

	ret = nand_write_page_raw(chip, buf, oob_required, page);
	micron_nand_on_die_ecc_setup(chip, false);

	return ret;
}

enum {
	 
	MICRON_ON_DIE_UNSUPPORTED,

	 
	MICRON_ON_DIE_SUPPORTED,

	 
	MICRON_ON_DIE_MANDATORY,
};

#define MICRON_ID_INTERNAL_ECC_MASK	GENMASK(1, 0)
#define MICRON_ID_ECC_ENABLED		BIT(7)

 
static int micron_supports_on_die_ecc(struct nand_chip *chip)
{
	const struct nand_ecc_props *requirements =
		nanddev_get_ecc_requirements(&chip->base);
	u8 id[5];
	int ret;

	if (!chip->parameters.onfi)
		return MICRON_ON_DIE_UNSUPPORTED;

	if (nanddev_bits_per_cell(&chip->base) != 1)
		return MICRON_ON_DIE_UNSUPPORTED;

	 
	if  (requirements->strength != 4 && requirements->strength != 8)
		return MICRON_ON_DIE_UNSUPPORTED;

	 
	if (chip->id.len != 5 ||
	    (chip->id.data[4] & MICRON_ID_INTERNAL_ECC_MASK) != 0x2)
		return MICRON_ON_DIE_UNSUPPORTED;

	 
	ret = micron_nand_on_die_ecc_setup(chip, true);
	if (ret)
		return MICRON_ON_DIE_UNSUPPORTED;

	ret = nand_readid_op(chip, 0, id, sizeof(id));
	if (ret)
		return MICRON_ON_DIE_UNSUPPORTED;

	ret = micron_nand_on_die_ecc_setup(chip, false);
	if (ret)
		return MICRON_ON_DIE_UNSUPPORTED;

	if (!(id[4] & MICRON_ID_ECC_ENABLED))
		return MICRON_ON_DIE_UNSUPPORTED;

	ret = nand_readid_op(chip, 0, id, sizeof(id));
	if (ret)
		return MICRON_ON_DIE_UNSUPPORTED;

	if (id[4] & MICRON_ID_ECC_ENABLED)
		return MICRON_ON_DIE_MANDATORY;

	 
	if  (requirements->strength != 4 && requirements->strength != 8)
		return MICRON_ON_DIE_UNSUPPORTED;

	return MICRON_ON_DIE_SUPPORTED;
}

static int micron_nand_init(struct nand_chip *chip)
{
	struct nand_device *base = &chip->base;
	const struct nand_ecc_props *requirements =
		nanddev_get_ecc_requirements(base);
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct micron_nand *micron;
	int ondie;
	int ret;

	micron = kzalloc(sizeof(*micron), GFP_KERNEL);
	if (!micron)
		return -ENOMEM;

	nand_set_manufacturer_data(chip, micron);

	ret = micron_nand_onfi_init(chip);
	if (ret)
		goto err_free_manuf_data;

	chip->options |= NAND_BBM_FIRSTPAGE;

	if (mtd->writesize == 2048)
		chip->options |= NAND_BBM_SECONDPAGE;

	ondie = micron_supports_on_die_ecc(chip);

	if (ondie == MICRON_ON_DIE_MANDATORY &&
	    chip->ecc.engine_type != NAND_ECC_ENGINE_TYPE_ON_DIE) {
		pr_err("On-die ECC forcefully enabled, not supported\n");
		ret = -EINVAL;
		goto err_free_manuf_data;
	}

	if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_DIE) {
		if (ondie == MICRON_ON_DIE_UNSUPPORTED) {
			pr_err("On-die ECC selected but not supported\n");
			ret = -EINVAL;
			goto err_free_manuf_data;
		}

		if (ondie == MICRON_ON_DIE_MANDATORY) {
			micron->ecc.forced = true;
			micron->ecc.enabled = true;
		}

		 
		if (requirements->strength == 4) {
			micron->ecc.rawbuf = kmalloc(mtd->writesize +
						     mtd->oobsize,
						     GFP_KERNEL);
			if (!micron->ecc.rawbuf) {
				ret = -ENOMEM;
				goto err_free_manuf_data;
			}
		}

		if (requirements->strength == 4)
			mtd_set_ooblayout(mtd,
					  &micron_nand_on_die_4_ooblayout_ops);
		else
			mtd_set_ooblayout(mtd,
					  &micron_nand_on_die_8_ooblayout_ops);

		chip->ecc.bytes = requirements->strength * 2;
		chip->ecc.size = 512;
		chip->ecc.strength = requirements->strength;
		chip->ecc.algo = NAND_ECC_ALGO_BCH;
		chip->ecc.read_page = micron_nand_read_page_on_die_ecc;
		chip->ecc.write_page = micron_nand_write_page_on_die_ecc;

		if (ondie == MICRON_ON_DIE_MANDATORY) {
			chip->ecc.read_page_raw = nand_read_page_raw_notsupp;
			chip->ecc.write_page_raw = nand_write_page_raw_notsupp;
		} else {
			if (!chip->ecc.read_page_raw)
				chip->ecc.read_page_raw = nand_read_page_raw;
			if (!chip->ecc.write_page_raw)
				chip->ecc.write_page_raw = nand_write_page_raw;
		}
	}

	return 0;

err_free_manuf_data:
	kfree(micron->ecc.rawbuf);
	kfree(micron);

	return ret;
}

static void micron_nand_cleanup(struct nand_chip *chip)
{
	struct micron_nand *micron = nand_get_manufacturer_data(chip);

	kfree(micron->ecc.rawbuf);
	kfree(micron);
}

static void micron_fixup_onfi_param_page(struct nand_chip *chip,
					 struct nand_onfi_params *p)
{
	 
	if (le16_to_cpu(p->revision) == 0)
		p->revision = cpu_to_le16(ONFI_VERSION_1_0);
}

const struct nand_manufacturer_ops micron_nand_manuf_ops = {
	.init = micron_nand_init,
	.cleanup = micron_nand_cleanup,
	.fixup_onfi_param_page = micron_fixup_onfi_param_page,
};
