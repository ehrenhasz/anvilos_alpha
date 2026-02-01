
 

#include <linux/err.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/pm_runtime.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <linux/sysfs.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>

#include "core.h"
#include "card.h"
#include "host.h"
#include "bus.h"
#include "mmc_ops.h"
#include "sd.h"
#include "sd_ops.h"

static const unsigned int tran_exp[] = {
	10000,		100000,		1000000,	10000000,
	0,		0,		0,		0
};

static const unsigned char tran_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

static const unsigned int taac_exp[] = {
	1,	10,	100,	1000,	10000,	100000,	1000000, 10000000,
};

static const unsigned int taac_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

static const unsigned int sd_au_size[] = {
	0,		SZ_16K / 512,		SZ_32K / 512,	SZ_64K / 512,
	SZ_128K / 512,	SZ_256K / 512,		SZ_512K / 512,	SZ_1M / 512,
	SZ_2M / 512,	SZ_4M / 512,		SZ_8M / 512,	(SZ_8M + SZ_4M) / 512,
	SZ_16M / 512,	(SZ_16M + SZ_8M) / 512,	SZ_32M / 512,	SZ_64M / 512,
};

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

#define SD_POWEROFF_NOTIFY_TIMEOUT_MS 1000
#define SD_WRITE_EXTR_SINGLE_TIMEOUT_MS 1000

struct sd_busy_data {
	struct mmc_card *card;
	u8 *reg_buf;
};

 
void mmc_decode_cid(struct mmc_card *card)
{
	u32 *resp = card->raw_cid;

	 
	add_device_randomness(&card->raw_cid, sizeof(card->raw_cid));

	 
	card->cid.manfid		= UNSTUFF_BITS(resp, 120, 8);
	card->cid.oemid			= UNSTUFF_BITS(resp, 104, 16);
	card->cid.prod_name[0]		= UNSTUFF_BITS(resp, 96, 8);
	card->cid.prod_name[1]		= UNSTUFF_BITS(resp, 88, 8);
	card->cid.prod_name[2]		= UNSTUFF_BITS(resp, 80, 8);
	card->cid.prod_name[3]		= UNSTUFF_BITS(resp, 72, 8);
	card->cid.prod_name[4]		= UNSTUFF_BITS(resp, 64, 8);
	card->cid.hwrev			= UNSTUFF_BITS(resp, 60, 4);
	card->cid.fwrev			= UNSTUFF_BITS(resp, 56, 4);
	card->cid.serial		= UNSTUFF_BITS(resp, 24, 32);
	card->cid.year			= UNSTUFF_BITS(resp, 12, 8);
	card->cid.month			= UNSTUFF_BITS(resp, 8, 4);

	card->cid.year += 2000;  
}

 
static int mmc_decode_csd(struct mmc_card *card)
{
	struct mmc_csd *csd = &card->csd;
	unsigned int e, m, csd_struct;
	u32 *resp = card->raw_csd;

	csd_struct = UNSTUFF_BITS(resp, 126, 2);

	switch (csd_struct) {
	case 0:
		m = UNSTUFF_BITS(resp, 115, 4);
		e = UNSTUFF_BITS(resp, 112, 3);
		csd->taac_ns	 = (taac_exp[e] * taac_mant[m] + 9) / 10;
		csd->taac_clks	 = UNSTUFF_BITS(resp, 104, 8) * 100;

		m = UNSTUFF_BITS(resp, 99, 4);
		e = UNSTUFF_BITS(resp, 96, 3);
		csd->max_dtr	  = tran_exp[e] * tran_mant[m];
		csd->cmdclass	  = UNSTUFF_BITS(resp, 84, 12);

		e = UNSTUFF_BITS(resp, 47, 3);
		m = UNSTUFF_BITS(resp, 62, 12);
		csd->capacity	  = (1 + m) << (e + 2);

		csd->read_blkbits = UNSTUFF_BITS(resp, 80, 4);
		csd->read_partial = UNSTUFF_BITS(resp, 79, 1);
		csd->write_misalign = UNSTUFF_BITS(resp, 78, 1);
		csd->read_misalign = UNSTUFF_BITS(resp, 77, 1);
		csd->dsr_imp = UNSTUFF_BITS(resp, 76, 1);
		csd->r2w_factor = UNSTUFF_BITS(resp, 26, 3);
		csd->write_blkbits = UNSTUFF_BITS(resp, 22, 4);
		csd->write_partial = UNSTUFF_BITS(resp, 21, 1);

		if (UNSTUFF_BITS(resp, 46, 1)) {
			csd->erase_size = 1;
		} else if (csd->write_blkbits >= 9) {
			csd->erase_size = UNSTUFF_BITS(resp, 39, 7) + 1;
			csd->erase_size <<= csd->write_blkbits - 9;
		}

		if (UNSTUFF_BITS(resp, 13, 1))
			mmc_card_set_readonly(card);
		break;
	case 1:
		 
		mmc_card_set_blockaddr(card);

		csd->taac_ns	 = 0;  
		csd->taac_clks	 = 0;  

		m = UNSTUFF_BITS(resp, 99, 4);
		e = UNSTUFF_BITS(resp, 96, 3);
		csd->max_dtr	  = tran_exp[e] * tran_mant[m];
		csd->cmdclass	  = UNSTUFF_BITS(resp, 84, 12);
		csd->c_size	  = UNSTUFF_BITS(resp, 48, 22);

		 
		if (csd->c_size >= 0xFFFF)
			mmc_card_set_ext_capacity(card);

		m = UNSTUFF_BITS(resp, 48, 22);
		csd->capacity     = (1 + m) << 10;

		csd->read_blkbits = 9;
		csd->read_partial = 0;
		csd->write_misalign = 0;
		csd->read_misalign = 0;
		csd->r2w_factor = 4;  
		csd->write_blkbits = 9;
		csd->write_partial = 0;
		csd->erase_size = 1;

		if (UNSTUFF_BITS(resp, 13, 1))
			mmc_card_set_readonly(card);
		break;
	default:
		pr_err("%s: unrecognised CSD structure version %d\n",
			mmc_hostname(card->host), csd_struct);
		return -EINVAL;
	}

	card->erase_size = csd->erase_size;

	return 0;
}

 
static int mmc_decode_scr(struct mmc_card *card)
{
	struct sd_scr *scr = &card->scr;
	unsigned int scr_struct;
	u32 resp[4];

	resp[3] = card->raw_scr[1];
	resp[2] = card->raw_scr[0];

	scr_struct = UNSTUFF_BITS(resp, 60, 4);
	if (scr_struct != 0) {
		pr_err("%s: unrecognised SCR structure version %d\n",
			mmc_hostname(card->host), scr_struct);
		return -EINVAL;
	}

	scr->sda_vsn = UNSTUFF_BITS(resp, 56, 4);
	scr->bus_widths = UNSTUFF_BITS(resp, 48, 4);
	if (scr->sda_vsn == SCR_SPEC_VER_2)
		 
		scr->sda_spec3 = UNSTUFF_BITS(resp, 47, 1);

	if (scr->sda_spec3) {
		scr->sda_spec4 = UNSTUFF_BITS(resp, 42, 1);
		scr->sda_specx = UNSTUFF_BITS(resp, 38, 4);
	}

	if (UNSTUFF_BITS(resp, 55, 1))
		card->erased_byte = 0xFF;
	else
		card->erased_byte = 0x0;

	if (scr->sda_spec4)
		scr->cmds = UNSTUFF_BITS(resp, 32, 4);
	else if (scr->sda_spec3)
		scr->cmds = UNSTUFF_BITS(resp, 32, 2);

	 
	if (!(scr->bus_widths & SD_SCR_BUS_WIDTH_1) ||
	    !(scr->bus_widths & SD_SCR_BUS_WIDTH_4)) {
		pr_err("%s: invalid bus width\n", mmc_hostname(card->host));
		return -EINVAL;
	}

	return 0;
}

 
static int mmc_read_ssr(struct mmc_card *card)
{
	unsigned int au, es, et, eo;
	__be32 *raw_ssr;
	u32 resp[4] = {};
	u8 discard_support;
	int i;

	if (!(card->csd.cmdclass & CCC_APP_SPEC)) {
		pr_warn("%s: card lacks mandatory SD Status function\n",
			mmc_hostname(card->host));
		return 0;
	}

	raw_ssr = kmalloc(sizeof(card->raw_ssr), GFP_KERNEL);
	if (!raw_ssr)
		return -ENOMEM;

	if (mmc_app_sd_status(card, raw_ssr)) {
		pr_warn("%s: problem reading SD Status register\n",
			mmc_hostname(card->host));
		kfree(raw_ssr);
		return 0;
	}

	for (i = 0; i < 16; i++)
		card->raw_ssr[i] = be32_to_cpu(raw_ssr[i]);

	kfree(raw_ssr);

	 
	au = UNSTUFF_BITS(card->raw_ssr, 428 - 384, 4);
	if (au) {
		if (au <= 9 || card->scr.sda_spec3) {
			card->ssr.au = sd_au_size[au];
			es = UNSTUFF_BITS(card->raw_ssr, 408 - 384, 16);
			et = UNSTUFF_BITS(card->raw_ssr, 402 - 384, 6);
			if (es && et) {
				eo = UNSTUFF_BITS(card->raw_ssr, 400 - 384, 2);
				card->ssr.erase_timeout = (et * 1000) / es;
				card->ssr.erase_offset = eo * 1000;
			}
		} else {
			pr_warn("%s: SD Status: Invalid Allocation Unit size\n",
				mmc_hostname(card->host));
		}
	}

	 
	resp[3] = card->raw_ssr[6];
	discard_support = UNSTUFF_BITS(resp, 313 - 288, 1);
	card->erase_arg = (card->scr.sda_specx && discard_support) ?
			    SD_DISCARD_ARG : SD_ERASE_ARG;

	return 0;
}

 
static int mmc_read_switch(struct mmc_card *card)
{
	int err;
	u8 *status;

	if (card->scr.sda_vsn < SCR_SPEC_VER_1)
		return 0;

	if (!(card->csd.cmdclass & CCC_SWITCH)) {
		pr_warn("%s: card lacks mandatory switch function, performance might suffer\n",
			mmc_hostname(card->host));
		return 0;
	}

	status = kmalloc(64, GFP_KERNEL);
	if (!status)
		return -ENOMEM;

	 
	err = mmc_sd_switch(card, 0, 0, 0, status);
	if (err) {
		 
		if (err != -EINVAL && err != -ENOSYS && err != -EFAULT)
			goto out;

		pr_warn("%s: problem reading Bus Speed modes\n",
			mmc_hostname(card->host));
		err = 0;

		goto out;
	}

	if (status[13] & SD_MODE_HIGH_SPEED)
		card->sw_caps.hs_max_dtr = HIGH_SPEED_MAX_DTR;

	if (card->scr.sda_spec3) {
		card->sw_caps.sd3_bus_mode = status[13];
		 
		card->sw_caps.sd3_drv_type = status[9];
		card->sw_caps.sd3_curr_limit = status[7] | status[6] << 8;
	}

out:
	kfree(status);

	return err;
}

 
int mmc_sd_switch_hs(struct mmc_card *card)
{
	int err;
	u8 *status;

	if (card->scr.sda_vsn < SCR_SPEC_VER_1)
		return 0;

	if (!(card->csd.cmdclass & CCC_SWITCH))
		return 0;

	if (!(card->host->caps & MMC_CAP_SD_HIGHSPEED))
		return 0;

	if (card->sw_caps.hs_max_dtr == 0)
		return 0;

	status = kmalloc(64, GFP_KERNEL);
	if (!status)
		return -ENOMEM;

	err = mmc_sd_switch(card, 1, 0, HIGH_SPEED_BUS_SPEED, status);
	if (err)
		goto out;

	if ((status[16] & 0xF) != HIGH_SPEED_BUS_SPEED) {
		pr_warn("%s: Problem switching card into high-speed mode!\n",
			mmc_hostname(card->host));
		err = 0;
	} else {
		err = 1;
	}

out:
	kfree(status);

	return err;
}

static int sd_select_driver_type(struct mmc_card *card, u8 *status)
{
	int card_drv_type, drive_strength, drv_type;
	int err;

	card->drive_strength = 0;

	card_drv_type = card->sw_caps.sd3_drv_type | SD_DRIVER_TYPE_B;

	drive_strength = mmc_select_drive_strength(card,
						   card->sw_caps.uhs_max_dtr,
						   card_drv_type, &drv_type);

	if (drive_strength) {
		err = mmc_sd_switch(card, 1, 2, drive_strength, status);
		if (err)
			return err;
		if ((status[15] & 0xF) != drive_strength) {
			pr_warn("%s: Problem setting drive strength!\n",
				mmc_hostname(card->host));
			return 0;
		}
		card->drive_strength = drive_strength;
	}

	if (drv_type)
		mmc_set_driver_type(card->host, drv_type);

	return 0;
}

static void sd_update_bus_speed_mode(struct mmc_card *card)
{
	 
	if (!mmc_host_uhs(card->host)) {
		card->sd_bus_speed = 0;
		return;
	}

	if ((card->host->caps & MMC_CAP_UHS_SDR104) &&
	    (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR104)) {
			card->sd_bus_speed = UHS_SDR104_BUS_SPEED;
	} else if ((card->host->caps & MMC_CAP_UHS_DDR50) &&
		   (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_DDR50)) {
			card->sd_bus_speed = UHS_DDR50_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50)) && (card->sw_caps.sd3_bus_mode &
		    SD_MODE_UHS_SDR50)) {
			card->sd_bus_speed = UHS_SDR50_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR25)) &&
		   (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR25)) {
			card->sd_bus_speed = UHS_SDR25_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR25 |
		    MMC_CAP_UHS_SDR12)) && (card->sw_caps.sd3_bus_mode &
		    SD_MODE_UHS_SDR12)) {
			card->sd_bus_speed = UHS_SDR12_BUS_SPEED;
	}
}

static int sd_set_bus_speed_mode(struct mmc_card *card, u8 *status)
{
	int err;
	unsigned int timing = 0;

	switch (card->sd_bus_speed) {
	case UHS_SDR104_BUS_SPEED:
		timing = MMC_TIMING_UHS_SDR104;
		card->sw_caps.uhs_max_dtr = UHS_SDR104_MAX_DTR;
		break;
	case UHS_DDR50_BUS_SPEED:
		timing = MMC_TIMING_UHS_DDR50;
		card->sw_caps.uhs_max_dtr = UHS_DDR50_MAX_DTR;
		break;
	case UHS_SDR50_BUS_SPEED:
		timing = MMC_TIMING_UHS_SDR50;
		card->sw_caps.uhs_max_dtr = UHS_SDR50_MAX_DTR;
		break;
	case UHS_SDR25_BUS_SPEED:
		timing = MMC_TIMING_UHS_SDR25;
		card->sw_caps.uhs_max_dtr = UHS_SDR25_MAX_DTR;
		break;
	case UHS_SDR12_BUS_SPEED:
		timing = MMC_TIMING_UHS_SDR12;
		card->sw_caps.uhs_max_dtr = UHS_SDR12_MAX_DTR;
		break;
	default:
		return 0;
	}

	err = mmc_sd_switch(card, 1, 0, card->sd_bus_speed, status);
	if (err)
		return err;

	if ((status[16] & 0xF) != card->sd_bus_speed)
		pr_warn("%s: Problem setting bus speed mode!\n",
			mmc_hostname(card->host));
	else {
		mmc_set_timing(card->host, timing);
		mmc_set_clock(card->host, card->sw_caps.uhs_max_dtr);
	}

	return 0;
}

 
static u32 sd_get_host_max_current(struct mmc_host *host)
{
	u32 voltage, max_current;

	voltage = 1 << host->ios.vdd;
	switch (voltage) {
	case MMC_VDD_165_195:
		max_current = host->max_current_180;
		break;
	case MMC_VDD_29_30:
	case MMC_VDD_30_31:
		max_current = host->max_current_300;
		break;
	case MMC_VDD_32_33:
	case MMC_VDD_33_34:
		max_current = host->max_current_330;
		break;
	default:
		max_current = 0;
	}

	return max_current;
}

static int sd_set_current_limit(struct mmc_card *card, u8 *status)
{
	int current_limit = SD_SET_CURRENT_NO_CHANGE;
	int err;
	u32 max_current;

	 
	if ((card->sd_bus_speed != UHS_SDR50_BUS_SPEED) &&
	    (card->sd_bus_speed != UHS_SDR104_BUS_SPEED) &&
	    (card->sd_bus_speed != UHS_DDR50_BUS_SPEED))
		return 0;

	 
	max_current = sd_get_host_max_current(card->host);

	 
	if (max_current >= 800 &&
	    card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_800)
		current_limit = SD_SET_CURRENT_LIMIT_800;
	else if (max_current >= 600 &&
		 card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_600)
		current_limit = SD_SET_CURRENT_LIMIT_600;
	else if (max_current >= 400 &&
		 card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_400)
		current_limit = SD_SET_CURRENT_LIMIT_400;
	else if (max_current >= 200 &&
		 card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_200)
		current_limit = SD_SET_CURRENT_LIMIT_200;

	if (current_limit != SD_SET_CURRENT_NO_CHANGE) {
		err = mmc_sd_switch(card, 1, 3, current_limit, status);
		if (err)
			return err;

		if (((status[15] >> 4) & 0x0F) != current_limit)
			pr_warn("%s: Problem setting current limit!\n",
				mmc_hostname(card->host));

	}

	return 0;
}

 
static int mmc_sd_init_uhs_card(struct mmc_card *card)
{
	int err;
	u8 *status;

	if (!(card->csd.cmdclass & CCC_SWITCH))
		return 0;

	status = kmalloc(64, GFP_KERNEL);
	if (!status)
		return -ENOMEM;

	 
	err = mmc_app_set_bus_width(card, MMC_BUS_WIDTH_4);
	if (err)
		goto out;

	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_4);

	 
	sd_update_bus_speed_mode(card);

	 
	err = sd_select_driver_type(card, status);
	if (err)
		goto out;

	 
	err = sd_set_current_limit(card, status);
	if (err)
		goto out;

	 
	err = sd_set_bus_speed_mode(card, status);
	if (err)
		goto out;

	 
	if (!mmc_host_is_spi(card->host) &&
		(card->host->ios.timing == MMC_TIMING_UHS_SDR50 ||
		 card->host->ios.timing == MMC_TIMING_UHS_DDR50 ||
		 card->host->ios.timing == MMC_TIMING_UHS_SDR104)) {
		err = mmc_execute_tuning(card);

		 
		if (err && card->host->ios.timing == MMC_TIMING_UHS_DDR50) {
			pr_warn("%s: ddr50 tuning failed\n",
				mmc_hostname(card->host));
			err = 0;
		}
	}

out:
	kfree(status);

	return err;
}

MMC_DEV_ATTR(cid, "%08x%08x%08x%08x\n", card->raw_cid[0], card->raw_cid[1],
	card->raw_cid[2], card->raw_cid[3]);
MMC_DEV_ATTR(csd, "%08x%08x%08x%08x\n", card->raw_csd[0], card->raw_csd[1],
	card->raw_csd[2], card->raw_csd[3]);
MMC_DEV_ATTR(scr, "%08x%08x\n", card->raw_scr[0], card->raw_scr[1]);
MMC_DEV_ATTR(ssr,
	"%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x\n",
		card->raw_ssr[0], card->raw_ssr[1], card->raw_ssr[2],
		card->raw_ssr[3], card->raw_ssr[4], card->raw_ssr[5],
		card->raw_ssr[6], card->raw_ssr[7], card->raw_ssr[8],
		card->raw_ssr[9], card->raw_ssr[10], card->raw_ssr[11],
		card->raw_ssr[12], card->raw_ssr[13], card->raw_ssr[14],
		card->raw_ssr[15]);
MMC_DEV_ATTR(date, "%02d/%04d\n", card->cid.month, card->cid.year);
MMC_DEV_ATTR(erase_size, "%u\n", card->erase_size << 9);
MMC_DEV_ATTR(preferred_erase_size, "%u\n", card->pref_erase << 9);
MMC_DEV_ATTR(fwrev, "0x%x\n", card->cid.fwrev);
MMC_DEV_ATTR(hwrev, "0x%x\n", card->cid.hwrev);
MMC_DEV_ATTR(manfid, "0x%06x\n", card->cid.manfid);
MMC_DEV_ATTR(name, "%s\n", card->cid.prod_name);
MMC_DEV_ATTR(oemid, "0x%04x\n", card->cid.oemid);
MMC_DEV_ATTR(serial, "0x%08x\n", card->cid.serial);
MMC_DEV_ATTR(ocr, "0x%08x\n", card->ocr);
MMC_DEV_ATTR(rca, "0x%04x\n", card->rca);


static ssize_t mmc_dsr_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mmc_card *card = mmc_dev_to_card(dev);
	struct mmc_host *host = card->host;

	if (card->csd.dsr_imp && host->dsr_req)
		return sysfs_emit(buf, "0x%x\n", host->dsr);
	 
	return sysfs_emit(buf, "0x%x\n", 0x404);
}

static DEVICE_ATTR(dsr, S_IRUGO, mmc_dsr_show, NULL);

MMC_DEV_ATTR(vendor, "0x%04x\n", card->cis.vendor);
MMC_DEV_ATTR(device, "0x%04x\n", card->cis.device);
MMC_DEV_ATTR(revision, "%u.%u\n", card->major_rev, card->minor_rev);

#define sdio_info_attr(num)									\
static ssize_t info##num##_show(struct device *dev, struct device_attribute *attr, char *buf)	\
{												\
	struct mmc_card *card = mmc_dev_to_card(dev);						\
												\
	if (num > card->num_info)								\
		return -ENODATA;								\
	if (!card->info[num - 1][0])								\
		return 0;									\
	return sysfs_emit(buf, "%s\n", card->info[num - 1]);					\
}												\
static DEVICE_ATTR_RO(info##num)

sdio_info_attr(1);
sdio_info_attr(2);
sdio_info_attr(3);
sdio_info_attr(4);

static struct attribute *sd_std_attrs[] = {
	&dev_attr_vendor.attr,
	&dev_attr_device.attr,
	&dev_attr_revision.attr,
	&dev_attr_info1.attr,
	&dev_attr_info2.attr,
	&dev_attr_info3.attr,
	&dev_attr_info4.attr,
	&dev_attr_cid.attr,
	&dev_attr_csd.attr,
	&dev_attr_scr.attr,
	&dev_attr_ssr.attr,
	&dev_attr_date.attr,
	&dev_attr_erase_size.attr,
	&dev_attr_preferred_erase_size.attr,
	&dev_attr_fwrev.attr,
	&dev_attr_hwrev.attr,
	&dev_attr_manfid.attr,
	&dev_attr_name.attr,
	&dev_attr_oemid.attr,
	&dev_attr_serial.attr,
	&dev_attr_ocr.attr,
	&dev_attr_rca.attr,
	&dev_attr_dsr.attr,
	NULL,
};

static umode_t sd_std_is_visible(struct kobject *kobj, struct attribute *attr,
				 int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct mmc_card *card = mmc_dev_to_card(dev);

	 
	if ((attr == &dev_attr_vendor.attr ||
	     attr == &dev_attr_device.attr ||
	     attr == &dev_attr_revision.attr ||
	     attr == &dev_attr_info1.attr ||
	     attr == &dev_attr_info2.attr ||
	     attr == &dev_attr_info3.attr ||
	     attr == &dev_attr_info4.attr
	    ) &&!mmc_card_sd_combo(card))
		return 0;

	return attr->mode;
}

static const struct attribute_group sd_std_group = {
	.attrs = sd_std_attrs,
	.is_visible = sd_std_is_visible,
};
__ATTRIBUTE_GROUPS(sd_std);

struct device_type sd_type = {
	.groups = sd_std_groups,
};

 
int mmc_sd_get_cid(struct mmc_host *host, u32 ocr, u32 *cid, u32 *rocr)
{
	int err;
	u32 max_current;
	int retries = 10;
	u32 pocr = ocr;

try_again:
	if (!retries) {
		ocr &= ~SD_OCR_S18R;
		pr_warn("%s: Skipping voltage switch\n", mmc_hostname(host));
	}

	 
	mmc_go_idle(host);

	 
	err = mmc_send_if_cond(host, ocr);
	if (!err)
		ocr |= SD_OCR_CCS;

	 
	if (retries && mmc_host_uhs(host))
		ocr |= SD_OCR_S18R;

	 
	max_current = sd_get_host_max_current(host);
	if (max_current > 150)
		ocr |= SD_OCR_XPC;

	err = mmc_send_app_op_cond(host, ocr, rocr);
	if (err)
		return err;

	 
	if (!mmc_host_is_spi(host) && (ocr & SD_OCR_S18R) &&
	    rocr && (*rocr & SD_ROCR_S18A)) {
		err = mmc_set_uhs_voltage(host, pocr);
		if (err == -EAGAIN) {
			retries--;
			goto try_again;
		} else if (err) {
			retries = 0;
			goto try_again;
		}
	}

	err = mmc_send_cid(host, cid);
	return err;
}

int mmc_sd_get_csd(struct mmc_card *card)
{
	int err;

	 
	err = mmc_send_csd(card, card->raw_csd);
	if (err)
		return err;

	err = mmc_decode_csd(card);
	if (err)
		return err;

	return 0;
}

static int mmc_sd_get_ro(struct mmc_host *host)
{
	int ro;

	 
	if (host->caps2 & MMC_CAP2_NO_WRITE_PROTECT)
		return 0;

	if (!host->ops->get_ro)
		return -1;

	ro = host->ops->get_ro(host);

	return ro;
}

int mmc_sd_setup_card(struct mmc_host *host, struct mmc_card *card,
	bool reinit)
{
	int err;

	if (!reinit) {
		 
		err = mmc_app_send_scr(card);
		if (err)
			return err;

		err = mmc_decode_scr(card);
		if (err)
			return err;

		 
		err = mmc_read_ssr(card);
		if (err)
			return err;

		 
		mmc_init_erase(card);
	}

	 
	err = mmc_read_switch(card);
	if (err)
		return err;

	 
	if (mmc_host_is_spi(host)) {
		err = mmc_spi_set_crc(host, use_spi_crc);
		if (err)
			return err;
	}

	 
	if (!reinit) {
		int ro = mmc_sd_get_ro(host);

		if (ro < 0) {
			pr_warn("%s: host does not support reading read-only switch, assuming write-enable\n",
				mmc_hostname(host));
		} else if (ro > 0) {
			mmc_card_set_readonly(card);
		}
	}

	return 0;
}

unsigned mmc_sd_get_max_clock(struct mmc_card *card)
{
	unsigned max_dtr = (unsigned int)-1;

	if (mmc_card_hs(card)) {
		if (max_dtr > card->sw_caps.hs_max_dtr)
			max_dtr = card->sw_caps.hs_max_dtr;
	} else if (max_dtr > card->csd.max_dtr) {
		max_dtr = card->csd.max_dtr;
	}

	return max_dtr;
}

static bool mmc_sd_card_using_v18(struct mmc_card *card)
{
	 
	return card->sw_caps.sd3_bus_mode &
	       (SD_MODE_UHS_SDR50 | SD_MODE_UHS_SDR104 | SD_MODE_UHS_DDR50);
}

static int sd_write_ext_reg(struct mmc_card *card, u8 fno, u8 page, u16 offset,
			    u8 reg_data)
{
	struct mmc_host *host = card->host;
	struct mmc_request mrq = {};
	struct mmc_command cmd = {};
	struct mmc_data data = {};
	struct scatterlist sg;
	u8 *reg_buf;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	mrq.cmd = &cmd;
	mrq.data = &data;

	 
	cmd.arg = fno << 27 | page << 18 | offset << 9;

	 
	reg_buf[0] = reg_data;

	data.flags = MMC_DATA_WRITE;
	data.blksz = 512;
	data.blocks = 1;
	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, reg_buf, 512);

	cmd.opcode = SD_WRITE_EXTR_SINGLE;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	mmc_set_data_timeout(&data, card);
	mmc_wait_for_req(host, &mrq);

	kfree(reg_buf);

	 

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}

static int sd_read_ext_reg(struct mmc_card *card, u8 fno, u8 page,
			   u16 offset, u16 len, u8 *reg_buf)
{
	u32 cmd_args;

	 
	cmd_args = fno << 27 | page << 18 | offset << 9 | (len -1);

	return mmc_send_adtc_data(card, card->host, SD_READ_EXTR_SINGLE,
				  cmd_args, reg_buf, 512);
}

static int sd_parse_ext_reg_power(struct mmc_card *card, u8 fno, u8 page,
				  u16 offset)
{
	int err;
	u8 *reg_buf;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	 
	err = sd_read_ext_reg(card, fno, page, offset, 512, reg_buf);
	if (err) {
		pr_warn("%s: error %d reading PM func of ext reg\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	 
	card->ext_power.rev = reg_buf[0] & 0xf;

	 
	if (reg_buf[1] & BIT(4))
		card->ext_power.feature_support |= SD_EXT_POWER_OFF_NOTIFY;

	 
	if (reg_buf[1] & BIT(5))
		card->ext_power.feature_support |= SD_EXT_POWER_SUSTENANCE;

	 
	if (reg_buf[1] & BIT(6))
		card->ext_power.feature_support |= SD_EXT_POWER_DOWN_MODE;

	card->ext_power.fno = fno;
	card->ext_power.page = page;
	card->ext_power.offset = offset;

out:
	kfree(reg_buf);
	return err;
}

static int sd_parse_ext_reg_perf(struct mmc_card *card, u8 fno, u8 page,
				 u16 offset)
{
	int err;
	u8 *reg_buf;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	err = sd_read_ext_reg(card, fno, page, offset, 512, reg_buf);
	if (err) {
		pr_warn("%s: error %d reading PERF func of ext reg\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	 
	card->ext_perf.rev = reg_buf[0];

	 
	if (reg_buf[1] & BIT(0))
		card->ext_perf.feature_support |= SD_EXT_PERF_FX_EVENT;

	 
	if (reg_buf[2] & BIT(0))
		card->ext_perf.feature_support |= SD_EXT_PERF_CARD_MAINT;

	 
	if (reg_buf[2] & BIT(1))
		card->ext_perf.feature_support |= SD_EXT_PERF_HOST_MAINT;

	 
	if ((reg_buf[4] & BIT(0)) && !mmc_card_broken_sd_cache(card))
		card->ext_perf.feature_support |= SD_EXT_PERF_CACHE;

	 
	if (reg_buf[6] & 0x1f)
		card->ext_perf.feature_support |= SD_EXT_PERF_CMD_QUEUE;

	card->ext_perf.fno = fno;
	card->ext_perf.page = page;
	card->ext_perf.offset = offset;

out:
	kfree(reg_buf);
	return err;
}

static int sd_parse_ext_reg(struct mmc_card *card, u8 *gen_info_buf,
			    u16 *next_ext_addr)
{
	u8 num_regs, fno, page;
	u16 sfc, offset, ext = *next_ext_addr;
	u32 reg_addr;

	 
	if (ext + 48 > 512)
		return -EFAULT;

	 
	memcpy(&sfc, &gen_info_buf[ext], 2);

	 
	memcpy(next_ext_addr, &gen_info_buf[ext + 40], 2);

	 
	num_regs = gen_info_buf[ext + 42];

	 
	if (num_regs != 1)
		return 0;

	 
	memcpy(&reg_addr, &gen_info_buf[ext + 44], 4);

	 
	offset = reg_addr & 0x1ff;

	 
	page = reg_addr >> 9 & 0xff ;

	 
	fno = reg_addr >> 18 & 0xf;

	 
	if (sfc == 0x1)
		return sd_parse_ext_reg_power(card, fno, page, offset);

	 
	if (sfc == 0x2)
		return sd_parse_ext_reg_perf(card, fno, page, offset);

	return 0;
}

static int sd_read_ext_regs(struct mmc_card *card)
{
	int err, i;
	u8 num_ext, *gen_info_buf;
	u16 rev, len, next_ext_addr;

	if (mmc_host_is_spi(card->host))
		return 0;

	if (!(card->scr.cmds & SD_SCR_CMD48_SUPPORT))
		return 0;

	gen_info_buf = kzalloc(512, GFP_KERNEL);
	if (!gen_info_buf)
		return -ENOMEM;

	 
	err = sd_read_ext_reg(card, 0, 0, 0, 512, gen_info_buf);
	if (err) {
		pr_err("%s: error %d reading general info of SD ext reg\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	 
	memcpy(&rev, &gen_info_buf[0], 2);

	 
	memcpy(&len, &gen_info_buf[2], 2);

	 
	num_ext = gen_info_buf[4];

	 
	if (rev != 0 || len > 512) {
		pr_warn("%s: non-supported SD ext reg layout\n",
			mmc_hostname(card->host));
		goto out;
	}

	 
	next_ext_addr = 16;
	for (i = 0; i < num_ext; i++) {
		err = sd_parse_ext_reg(card, gen_info_buf, &next_ext_addr);
		if (err) {
			pr_err("%s: error %d parsing SD ext reg\n",
				mmc_hostname(card->host), err);
			goto out;
		}
	}

out:
	kfree(gen_info_buf);
	return err;
}

static bool sd_cache_enabled(struct mmc_host *host)
{
	return host->card->ext_perf.feature_enabled & SD_EXT_PERF_CACHE;
}

static int sd_flush_cache(struct mmc_host *host)
{
	struct mmc_card *card = host->card;
	u8 *reg_buf, fno, page;
	u16 offset;
	int err;

	if (!sd_cache_enabled(host))
		return 0;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	 
	fno = card->ext_perf.fno;
	page = card->ext_perf.page;
	offset = card->ext_perf.offset + 261;

	err = sd_write_ext_reg(card, fno, page, offset, BIT(0));
	if (err) {
		pr_warn("%s: error %d writing Cache Flush bit\n",
			mmc_hostname(host), err);
		goto out;
	}

	err = mmc_poll_for_busy(card, SD_WRITE_EXTR_SINGLE_TIMEOUT_MS, false,
				MMC_BUSY_EXTR_SINGLE);
	if (err)
		goto out;

	 
	err = sd_read_ext_reg(card, fno, page, offset, 1, reg_buf);
	if (err) {
		pr_warn("%s: error %d reading Cache Flush bit\n",
			mmc_hostname(host), err);
		goto out;
	}

	if (reg_buf[0] & BIT(0))
		err = -ETIMEDOUT;
out:
	kfree(reg_buf);
	return err;
}

static int sd_enable_cache(struct mmc_card *card)
{
	u8 *reg_buf;
	int err;

	card->ext_perf.feature_enabled &= ~SD_EXT_PERF_CACHE;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	 
	err = sd_write_ext_reg(card, card->ext_perf.fno, card->ext_perf.page,
			       card->ext_perf.offset + 260, BIT(0));
	if (err) {
		pr_warn("%s: error %d writing Cache Enable bit\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	err = mmc_poll_for_busy(card, SD_WRITE_EXTR_SINGLE_TIMEOUT_MS, false,
				MMC_BUSY_EXTR_SINGLE);
	if (!err)
		card->ext_perf.feature_enabled |= SD_EXT_PERF_CACHE;

out:
	kfree(reg_buf);
	return err;
}

 
static int mmc_sd_init_card(struct mmc_host *host, u32 ocr,
	struct mmc_card *oldcard)
{
	struct mmc_card *card;
	int err;
	u32 cid[4];
	u32 rocr = 0;
	bool v18_fixup_failed = false;

	WARN_ON(!host->claimed);
retry:
	err = mmc_sd_get_cid(host, ocr, cid, &rocr);
	if (err)
		return err;

	if (oldcard) {
		if (memcmp(cid, oldcard->raw_cid, sizeof(cid)) != 0) {
			pr_debug("%s: Perhaps the card was replaced\n",
				mmc_hostname(host));
			return -ENOENT;
		}

		card = oldcard;
	} else {
		 
		card = mmc_alloc_card(host, &sd_type);
		if (IS_ERR(card))
			return PTR_ERR(card);

		card->ocr = ocr;
		card->type = MMC_TYPE_SD;
		memcpy(card->raw_cid, cid, sizeof(card->raw_cid));
	}

	 
	if (host->ops->init_card)
		host->ops->init_card(host, card);

	 
	if (!mmc_host_is_spi(host)) {
		err = mmc_send_relative_addr(host, &card->rca);
		if (err)
			goto free_card;
	}

	if (!oldcard) {
		err = mmc_sd_get_csd(card);
		if (err)
			goto free_card;

		mmc_decode_cid(card);
	}

	 
	if (card->csd.dsr_imp && host->dsr_req)
		mmc_set_dsr(host);

	 
	if (!mmc_host_is_spi(host)) {
		err = mmc_select_card(card);
		if (err)
			goto free_card;
	}

	err = mmc_sd_setup_card(host, card, oldcard != NULL);
	if (err)
		goto free_card;

	 
	if (!v18_fixup_failed && !mmc_host_is_spi(host) && mmc_host_uhs(host) &&
	    mmc_sd_card_using_v18(card) &&
	    host->ios.signal_voltage != MMC_SIGNAL_VOLTAGE_180) {
		if (mmc_host_set_uhs_voltage(host) ||
		    mmc_sd_init_uhs_card(card)) {
			v18_fixup_failed = true;
			mmc_power_cycle(host, ocr);
			if (!oldcard)
				mmc_remove_card(card);
			goto retry;
		}
		goto cont;
	}

	 
	if (rocr & SD_ROCR_S18A && mmc_host_uhs(host)) {
		err = mmc_sd_init_uhs_card(card);
		if (err)
			goto free_card;
	} else {
		 
		err = mmc_sd_switch_hs(card);
		if (err > 0)
			mmc_set_timing(card->host, MMC_TIMING_SD_HS);
		else if (err)
			goto free_card;

		 
		mmc_set_clock(host, mmc_sd_get_max_clock(card));

		if (host->ios.timing == MMC_TIMING_SD_HS &&
			host->ops->prepare_sd_hs_tuning) {
			err = host->ops->prepare_sd_hs_tuning(host, card);
			if (err)
				goto free_card;
		}

		 
		if ((host->caps & MMC_CAP_4_BIT_DATA) &&
			(card->scr.bus_widths & SD_SCR_BUS_WIDTH_4)) {
			err = mmc_app_set_bus_width(card, MMC_BUS_WIDTH_4);
			if (err)
				goto free_card;

			mmc_set_bus_width(host, MMC_BUS_WIDTH_4);
		}

		if (host->ios.timing == MMC_TIMING_SD_HS &&
			host->ops->execute_sd_hs_tuning) {
			err = host->ops->execute_sd_hs_tuning(host, card);
			if (err)
				goto free_card;
		}
	}
cont:
	if (!oldcard) {
		 
		err = sd_read_ext_regs(card);
		if (err)
			goto free_card;
	}

	 
	if (card->ext_perf.feature_support & SD_EXT_PERF_CACHE) {
		err = sd_enable_cache(card);
		if (err)
			goto free_card;
	}

	if (host->cqe_ops && !host->cqe_enabled) {
		err = host->cqe_ops->cqe_enable(host, card);
		if (!err) {
			host->cqe_enabled = true;
			host->hsq_enabled = true;
			pr_info("%s: Host Software Queue enabled\n",
				mmc_hostname(host));
		}
	}

	if (host->caps2 & MMC_CAP2_AVOID_3_3V &&
	    host->ios.signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		pr_err("%s: Host failed to negotiate down from 3.3V\n",
			mmc_hostname(host));
		err = -EINVAL;
		goto free_card;
	}

	host->card = card;
	return 0;

free_card:
	if (!oldcard)
		mmc_remove_card(card);

	return err;
}

 
static void mmc_sd_remove(struct mmc_host *host)
{
	mmc_remove_card(host->card);
	host->card = NULL;
}

 
static int mmc_sd_alive(struct mmc_host *host)
{
	return mmc_send_status(host->card, NULL);
}

 
static void mmc_sd_detect(struct mmc_host *host)
{
	int err;

	mmc_get_card(host->card, NULL);

	 
	err = _mmc_detect_card_removed(host);

	mmc_put_card(host->card, NULL);

	if (err) {
		mmc_sd_remove(host);

		mmc_claim_host(host);
		mmc_detach_bus(host);
		mmc_power_off(host);
		mmc_release_host(host);
	}
}

static int sd_can_poweroff_notify(struct mmc_card *card)
{
	return card->ext_power.feature_support & SD_EXT_POWER_OFF_NOTIFY;
}

static int sd_busy_poweroff_notify_cb(void *cb_data, bool *busy)
{
	struct sd_busy_data *data = cb_data;
	struct mmc_card *card = data->card;
	int err;

	 
	err = sd_read_ext_reg(card, card->ext_power.fno, card->ext_power.page,
			      card->ext_power.offset + 1, 1, data->reg_buf);
	if (err) {
		pr_warn("%s: error %d reading status reg of PM func\n",
			mmc_hostname(card->host), err);
		return err;
	}

	*busy = !(data->reg_buf[0] & BIT(0));
	return 0;
}

static int sd_poweroff_notify(struct mmc_card *card)
{
	struct sd_busy_data cb_data;
	u8 *reg_buf;
	int err;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	 
	err = sd_write_ext_reg(card, card->ext_power.fno, card->ext_power.page,
			       card->ext_power.offset + 2, BIT(0));
	if (err) {
		pr_warn("%s: error %d writing Power Off Notify bit\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	 
	err = mmc_poll_for_busy(card, SD_WRITE_EXTR_SINGLE_TIMEOUT_MS, false,
				MMC_BUSY_EXTR_SINGLE);
	if (err)
		goto out;

	cb_data.card = card;
	cb_data.reg_buf = reg_buf;
	err = __mmc_poll_for_busy(card->host, 0, SD_POWEROFF_NOTIFY_TIMEOUT_MS,
				  &sd_busy_poweroff_notify_cb, &cb_data);

out:
	kfree(reg_buf);
	return err;
}

static int _mmc_sd_suspend(struct mmc_host *host)
{
	struct mmc_card *card = host->card;
	int err = 0;

	mmc_claim_host(host);

	if (mmc_card_suspended(card))
		goto out;

	if (sd_can_poweroff_notify(card))
		err = sd_poweroff_notify(card);
	else if (!mmc_host_is_spi(host))
		err = mmc_deselect_cards(host);

	if (!err) {
		mmc_power_off(host);
		mmc_card_set_suspended(card);
	}

out:
	mmc_release_host(host);
	return err;
}

 
static int mmc_sd_suspend(struct mmc_host *host)
{
	int err;

	err = _mmc_sd_suspend(host);
	if (!err) {
		pm_runtime_disable(&host->card->dev);
		pm_runtime_set_suspended(&host->card->dev);
	}

	return err;
}

 
static int _mmc_sd_resume(struct mmc_host *host)
{
	int err = 0;

	mmc_claim_host(host);

	if (!mmc_card_suspended(host->card))
		goto out;

	mmc_power_up(host, host->card->ocr);
	err = mmc_sd_init_card(host, host->card->ocr, host->card);
	mmc_card_clr_suspended(host->card);

out:
	mmc_release_host(host);
	return err;
}

 
static int mmc_sd_resume(struct mmc_host *host)
{
	pm_runtime_enable(&host->card->dev);
	return 0;
}

 
static int mmc_sd_runtime_suspend(struct mmc_host *host)
{
	int err;

	if (!(host->caps & MMC_CAP_AGGRESSIVE_PM))
		return 0;

	err = _mmc_sd_suspend(host);
	if (err)
		pr_err("%s: error %d doing aggressive suspend\n",
			mmc_hostname(host), err);

	return err;
}

 
static int mmc_sd_runtime_resume(struct mmc_host *host)
{
	int err;

	err = _mmc_sd_resume(host);
	if (err && err != -ENOMEDIUM)
		pr_err("%s: error %d doing runtime resume\n",
			mmc_hostname(host), err);

	return 0;
}

static int mmc_sd_hw_reset(struct mmc_host *host)
{
	mmc_power_cycle(host, host->card->ocr);
	return mmc_sd_init_card(host, host->card->ocr, host->card);
}

static const struct mmc_bus_ops mmc_sd_ops = {
	.remove = mmc_sd_remove,
	.detect = mmc_sd_detect,
	.runtime_suspend = mmc_sd_runtime_suspend,
	.runtime_resume = mmc_sd_runtime_resume,
	.suspend = mmc_sd_suspend,
	.resume = mmc_sd_resume,
	.alive = mmc_sd_alive,
	.shutdown = mmc_sd_suspend,
	.hw_reset = mmc_sd_hw_reset,
	.cache_enabled = sd_cache_enabled,
	.flush_cache = sd_flush_cache,
};

 
int mmc_attach_sd(struct mmc_host *host)
{
	int err;
	u32 ocr, rocr;

	WARN_ON(!host->claimed);

	err = mmc_send_app_op_cond(host, 0, &ocr);
	if (err)
		return err;

	mmc_attach_bus(host, &mmc_sd_ops);
	if (host->ocr_avail_sd)
		host->ocr_avail = host->ocr_avail_sd;

	 
	if (mmc_host_is_spi(host)) {
		mmc_go_idle(host);

		err = mmc_spi_read_ocr(host, 0, &ocr);
		if (err)
			goto err;
	}

	 
	ocr &= ~0x7FFF;

	rocr = mmc_select_voltage(host, ocr);

	 
	if (!rocr) {
		err = -EINVAL;
		goto err;
	}

	 
	err = mmc_sd_init_card(host, rocr, NULL);
	if (err)
		goto err;

	mmc_release_host(host);
	err = mmc_add_card(host->card);
	if (err)
		goto remove_card;

	mmc_claim_host(host);
	return 0;

remove_card:
	mmc_remove_card(host->card);
	host->card = NULL;
	mmc_claim_host(host);
err:
	mmc_detach_bus(host);

	pr_err("%s: error %d whilst initialising SD card\n",
		mmc_hostname(host), err);

	return err;
}
