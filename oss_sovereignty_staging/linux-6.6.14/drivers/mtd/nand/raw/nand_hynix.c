
 

#include <linux/sizes.h>
#include <linux/slab.h>

#include "internals.h"

#define NAND_HYNIX_CMD_SET_PARAMS	0x36
#define NAND_HYNIX_CMD_APPLY_PARAMS	0x16

#define NAND_HYNIX_1XNM_RR_REPEAT	8

 
struct hynix_read_retry {
	int nregs;
	const u8 *regs;
	u8 values[];
};

 
struct hynix_nand {
	const struct hynix_read_retry *read_retry;
};

 
struct hynix_read_retry_otp {
	int nregs;
	const u8 *regs;
	const u8 *values;
	int page;
	int size;
};

static bool hynix_nand_has_valid_jedecid(struct nand_chip *chip)
{
	u8 jedecid[5] = { };
	int ret;

	ret = nand_readid_op(chip, 0x40, jedecid, sizeof(jedecid));
	if (ret)
		return false;

	return !strncmp("JEDEC", jedecid, sizeof(jedecid));
}

static int hynix_nand_cmd_op(struct nand_chip *chip, u8 cmd)
{
	if (nand_has_exec_op(chip)) {
		struct nand_op_instr instrs[] = {
			NAND_OP_CMD(cmd, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, cmd, -1, -1);

	return 0;
}

static int hynix_nand_reg_write_op(struct nand_chip *chip, u8 addr, u8 val)
{
	u16 column = ((u16)addr << 8) | addr;

	if (nand_has_exec_op(chip)) {
		struct nand_op_instr instrs[] = {
			NAND_OP_ADDR(1, &addr, 0),
			NAND_OP_8BIT_DATA_OUT(1, &val, 0),
		};
		struct nand_operation op = NAND_OPERATION(chip->cur_cs, instrs);

		return nand_exec_op(chip, &op);
	}

	chip->legacy.cmdfunc(chip, NAND_CMD_NONE, column, -1);
	chip->legacy.write_byte(chip, val);

	return 0;
}

static int hynix_nand_setup_read_retry(struct nand_chip *chip, int retry_mode)
{
	struct hynix_nand *hynix = nand_get_manufacturer_data(chip);
	const u8 *values;
	int i, ret;

	values = hynix->read_retry->values +
		 (retry_mode * hynix->read_retry->nregs);

	 
	ret = hynix_nand_cmd_op(chip, NAND_HYNIX_CMD_SET_PARAMS);
	if (ret)
		return ret;

	 
	for (i = 0; i < hynix->read_retry->nregs; i++) {
		ret = hynix_nand_reg_write_op(chip, hynix->read_retry->regs[i],
					      values[i]);
		if (ret)
			return ret;
	}

	 
	return hynix_nand_cmd_op(chip, NAND_HYNIX_CMD_APPLY_PARAMS);
}

 
static int hynix_get_majority(const u8 *in, int repeat, u8 *out)
{
	int i, j, half = repeat / 2;

	 
	for (i = 0; i < half; i++) {
		int cnt = 0;
		u8 val = in[i];

		 
		for (j = i + 1; j < repeat; j++) {
			if (in[j] == val)
				cnt++;
		}

		 
		if (cnt > half) {
			*out = val;
			return 0;
		}
	}

	return -EIO;
}

static int hynix_read_rr_otp(struct nand_chip *chip,
			     const struct hynix_read_retry_otp *info,
			     void *buf)
{
	int i, ret;

	ret = nand_reset_op(chip);
	if (ret)
		return ret;

	ret = hynix_nand_cmd_op(chip, NAND_HYNIX_CMD_SET_PARAMS);
	if (ret)
		return ret;

	for (i = 0; i < info->nregs; i++) {
		ret = hynix_nand_reg_write_op(chip, info->regs[i],
					      info->values[i]);
		if (ret)
			return ret;
	}

	ret = hynix_nand_cmd_op(chip, NAND_HYNIX_CMD_APPLY_PARAMS);
	if (ret)
		return ret;

	 
	ret = hynix_nand_cmd_op(chip, 0x17);
	if (ret)
		return ret;

	ret = hynix_nand_cmd_op(chip, 0x4);
	if (ret)
		return ret;

	ret = hynix_nand_cmd_op(chip, 0x19);
	if (ret)
		return ret;

	 
	ret = nand_read_page_op(chip, info->page, 0, buf, info->size);
	if (ret)
		return ret;

	 
	ret = nand_reset_op(chip);
	if (ret)
		return ret;

	ret = hynix_nand_cmd_op(chip, NAND_HYNIX_CMD_SET_PARAMS);
	if (ret)
		return ret;

	ret = hynix_nand_reg_write_op(chip, 0x38, 0);
	if (ret)
		return ret;

	ret = hynix_nand_cmd_op(chip, NAND_HYNIX_CMD_APPLY_PARAMS);
	if (ret)
		return ret;

	return nand_read_page_op(chip, 0, 0, NULL, 0);
}

#define NAND_HYNIX_1XNM_RR_COUNT_OFFS				0
#define NAND_HYNIX_1XNM_RR_REG_COUNT_OFFS			8
#define NAND_HYNIX_1XNM_RR_SET_OFFS(x, setsize, inv)		\
	(16 + ((((x) * 2) + ((inv) ? 1 : 0)) * (setsize)))

static int hynix_mlc_1xnm_rr_value(const u8 *buf, int nmodes, int nregs,
				   int mode, int reg, bool inv, u8 *val)
{
	u8 tmp[NAND_HYNIX_1XNM_RR_REPEAT];
	int val_offs = (mode * nregs) + reg;
	int set_size = nmodes * nregs;
	int i, ret;

	for (i = 0; i < NAND_HYNIX_1XNM_RR_REPEAT; i++) {
		int set_offs = NAND_HYNIX_1XNM_RR_SET_OFFS(i, set_size, inv);

		tmp[i] = buf[val_offs + set_offs];
	}

	ret = hynix_get_majority(tmp, NAND_HYNIX_1XNM_RR_REPEAT, val);
	if (ret)
		return ret;

	if (inv)
		*val = ~*val;

	return 0;
}

static u8 hynix_1xnm_mlc_read_retry_regs[] = {
	0xcc, 0xbf, 0xaa, 0xab, 0xcd, 0xad, 0xae, 0xaf
};

static int hynix_mlc_1xnm_rr_init(struct nand_chip *chip,
				  const struct hynix_read_retry_otp *info)
{
	struct hynix_nand *hynix = nand_get_manufacturer_data(chip);
	struct hynix_read_retry *rr = NULL;
	int ret, i, j;
	u8 nregs, nmodes;
	u8 *buf;

	buf = kmalloc(info->size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = hynix_read_rr_otp(chip, info, buf);
	if (ret)
		goto out;

	ret = hynix_get_majority(buf, NAND_HYNIX_1XNM_RR_REPEAT,
				 &nmodes);
	if (ret)
		goto out;

	ret = hynix_get_majority(buf + NAND_HYNIX_1XNM_RR_REPEAT,
				 NAND_HYNIX_1XNM_RR_REPEAT,
				 &nregs);
	if (ret)
		goto out;

	rr = kzalloc(sizeof(*rr) + (nregs * nmodes), GFP_KERNEL);
	if (!rr) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < nmodes; i++) {
		for (j = 0; j < nregs; j++) {
			u8 *val = rr->values + (i * nregs);

			ret = hynix_mlc_1xnm_rr_value(buf, nmodes, nregs, i, j,
						      false, val);
			if (!ret)
				continue;

			ret = hynix_mlc_1xnm_rr_value(buf, nmodes, nregs, i, j,
						      true, val);
			if (ret)
				goto out;
		}
	}

	rr->nregs = nregs;
	rr->regs = hynix_1xnm_mlc_read_retry_regs;
	hynix->read_retry = rr;
	chip->ops.setup_read_retry = hynix_nand_setup_read_retry;
	chip->read_retries = nmodes;

out:
	kfree(buf);

	if (ret)
		kfree(rr);

	return ret;
}

static const u8 hynix_mlc_1xnm_rr_otp_regs[] = { 0x38 };
static const u8 hynix_mlc_1xnm_rr_otp_values[] = { 0x52 };

static const struct hynix_read_retry_otp hynix_mlc_1xnm_rr_otps[] = {
	{
		.nregs = ARRAY_SIZE(hynix_mlc_1xnm_rr_otp_regs),
		.regs = hynix_mlc_1xnm_rr_otp_regs,
		.values = hynix_mlc_1xnm_rr_otp_values,
		.page = 0x21f,
		.size = 784
	},
	{
		.nregs = ARRAY_SIZE(hynix_mlc_1xnm_rr_otp_regs),
		.regs = hynix_mlc_1xnm_rr_otp_regs,
		.values = hynix_mlc_1xnm_rr_otp_values,
		.page = 0x200,
		.size = 528,
	},
};

static int hynix_nand_rr_init(struct nand_chip *chip)
{
	int i, ret = 0;
	bool valid_jedecid;

	valid_jedecid = hynix_nand_has_valid_jedecid(chip);

	 
	if (valid_jedecid) {
		u8 nand_tech = chip->id.data[5] >> 4;

		 
		if (nand_tech == 4) {
			for (i = 0; i < ARRAY_SIZE(hynix_mlc_1xnm_rr_otps);
			     i++) {
				 
				ret = hynix_mlc_1xnm_rr_init(chip,
						hynix_mlc_1xnm_rr_otps);
				if (!ret)
					break;
			}
		}
	}

	if (ret)
		pr_warn("failed to initialize read-retry infrastructure");

	return 0;
}

static void hynix_nand_extract_oobsize(struct nand_chip *chip,
				       bool valid_jedecid)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_memory_organization *memorg;
	u8 oobsize;

	memorg = nanddev_get_memorg(&chip->base);

	oobsize = ((chip->id.data[3] >> 2) & 0x3) |
		  ((chip->id.data[3] >> 4) & 0x4);

	if (valid_jedecid) {
		switch (oobsize) {
		case 0:
			memorg->oobsize = 2048;
			break;
		case 1:
			memorg->oobsize = 1664;
			break;
		case 2:
			memorg->oobsize = 1024;
			break;
		case 3:
			memorg->oobsize = 640;
			break;
		default:
			 
			WARN(1, "Invalid OOB size");
			break;
		}
	} else {
		switch (oobsize) {
		case 0:
			memorg->oobsize = 128;
			break;
		case 1:
			memorg->oobsize = 224;
			break;
		case 2:
			memorg->oobsize = 448;
			break;
		case 3:
			memorg->oobsize = 64;
			break;
		case 4:
			memorg->oobsize = 32;
			break;
		case 5:
			memorg->oobsize = 16;
			break;
		case 6:
			memorg->oobsize = 640;
			break;
		default:
			 
			WARN(1, "Invalid OOB size");
			break;
		}

		 
		if (chip->id.data[1] == 0xde)
			memorg->oobsize *= memorg->pagesize / SZ_8K;
	}

	mtd->oobsize = memorg->oobsize;
}

static void hynix_nand_extract_ecc_requirements(struct nand_chip *chip,
						bool valid_jedecid)
{
	struct nand_device *base = &chip->base;
	struct nand_ecc_props requirements = {};
	u8 ecc_level = (chip->id.data[4] >> 4) & 0x7;

	if (valid_jedecid) {
		 
		requirements.step_size = 1024;

		switch (ecc_level) {
		case 0:
			requirements.step_size = 0;
			requirements.strength = 0;
			break;
		case 1:
			requirements.strength = 4;
			break;
		case 2:
			requirements.strength = 24;
			break;
		case 3:
			requirements.strength = 32;
			break;
		case 4:
			requirements.strength = 40;
			break;
		case 5:
			requirements.strength = 50;
			break;
		case 6:
			requirements.strength = 60;
			break;
		default:
			 
			WARN(1, "Invalid ECC requirements");
		}
	} else {
		 
		u8 nand_tech = chip->id.data[5] & 0x7;

		if (nand_tech < 3) {
			 
			if (ecc_level < 5) {
				requirements.step_size = 512;
				requirements.strength = 1 << ecc_level;
			} else if (ecc_level < 7) {
				if (ecc_level == 5)
					requirements.step_size = 2048;
				else
					requirements.step_size = 1024;
				requirements.strength = 24;
			} else {
				 
				WARN(1, "Invalid ECC requirements");
			}
		} else {
			 
			if (!ecc_level) {
				requirements.step_size = 0;
				requirements.strength = 0;
			} else if (ecc_level < 5) {
				requirements.step_size = 512;
				requirements.strength = 1 << (ecc_level - 1);
			} else {
				requirements.step_size = 1024;
				requirements.strength = 24 +
							(8 * (ecc_level - 5));
			}
		}
	}

	nanddev_set_ecc_requirements(base, &requirements);
}

static void hynix_nand_extract_scrambling_requirements(struct nand_chip *chip,
						       bool valid_jedecid)
{
	u8 nand_tech;

	 
	if (nanddev_bits_per_cell(&chip->base) > 2)
		chip->options |= NAND_NEED_SCRAMBLING;

	 
	if (valid_jedecid) {
		nand_tech = chip->id.data[5] >> 4;

		 
		if (nand_tech > 0)
			chip->options |= NAND_NEED_SCRAMBLING;
	} else {
		nand_tech = chip->id.data[5] & 0x7;

		 
		if (nand_tech > 2)
			chip->options |= NAND_NEED_SCRAMBLING;
	}
}

static void hynix_nand_decode_id(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_memory_organization *memorg;
	bool valid_jedecid;
	u8 tmp;

	memorg = nanddev_get_memorg(&chip->base);

	 
	if (chip->id.len < 6 || nand_is_slc(chip)) {
		nand_decode_ext_id(chip);
		return;
	}

	 
	memorg->pagesize = 2048 << (chip->id.data[3] & 0x03);
	mtd->writesize = memorg->pagesize;

	tmp = (chip->id.data[3] >> 4) & 0x3;
	 
	if (chip->id.data[3] & 0x80) {
		memorg->pages_per_eraseblock = (SZ_1M << tmp) /
					       memorg->pagesize;
		mtd->erasesize = SZ_1M << tmp;
	} else if (tmp == 3) {
		memorg->pages_per_eraseblock = (SZ_512K + SZ_256K) /
					       memorg->pagesize;
		mtd->erasesize = SZ_512K + SZ_256K;
	} else {
		memorg->pages_per_eraseblock = (SZ_128K << tmp) /
					       memorg->pagesize;
		mtd->erasesize = SZ_128K << tmp;
	}

	 
	valid_jedecid = hynix_nand_has_valid_jedecid(chip);

	hynix_nand_extract_oobsize(chip, valid_jedecid);
	hynix_nand_extract_ecc_requirements(chip, valid_jedecid);
	hynix_nand_extract_scrambling_requirements(chip, valid_jedecid);
}

static void hynix_nand_cleanup(struct nand_chip *chip)
{
	struct hynix_nand *hynix = nand_get_manufacturer_data(chip);

	if (!hynix)
		return;

	kfree(hynix->read_retry);
	kfree(hynix);
	nand_set_manufacturer_data(chip, NULL);
}

static int
h27ucg8t2atrbc_choose_interface_config(struct nand_chip *chip,
				       struct nand_interface_config *iface)
{
	onfi_fill_interface_config(chip, iface, NAND_SDR_IFACE, 4);

	return nand_choose_best_sdr_timings(chip, iface, NULL);
}

static int h27ucg8t2etrbc_init(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	chip->options |= NAND_NEED_SCRAMBLING;
	mtd_set_pairing_scheme(mtd, &dist3_pairing_scheme);

	return 0;
}

static int hynix_nand_init(struct nand_chip *chip)
{
	struct hynix_nand *hynix;
	int ret;

	if (!nand_is_slc(chip))
		chip->options |= NAND_BBM_LASTPAGE;
	else
		chip->options |= NAND_BBM_FIRSTPAGE | NAND_BBM_SECONDPAGE;

	hynix = kzalloc(sizeof(*hynix), GFP_KERNEL);
	if (!hynix)
		return -ENOMEM;

	nand_set_manufacturer_data(chip, hynix);

	if (!strncmp("H27UCG8T2ATR-BC", chip->parameters.model,
		     sizeof("H27UCG8T2ATR-BC") - 1))
		chip->ops.choose_interface_config =
			h27ucg8t2atrbc_choose_interface_config;

	if (!strncmp("H27UCG8T2ETR-BC", chip->parameters.model,
		     sizeof("H27UCG8T2ETR-BC") - 1))
		h27ucg8t2etrbc_init(chip);

	ret = hynix_nand_rr_init(chip);
	if (ret)
		hynix_nand_cleanup(chip);

	return ret;
}

static void hynix_fixup_onfi_param_page(struct nand_chip *chip,
					struct nand_onfi_params *p)
{
	 
	p->sdr_timing_modes |= cpu_to_le16(BIT(0));
}

const struct nand_manufacturer_ops hynix_nand_manuf_ops = {
	.detect = hynix_nand_decode_id,
	.init = hynix_nand_init,
	.cleanup = hynix_nand_cleanup,
	.fixup_onfi_param_page = hynix_fixup_onfi_param_page,
};
