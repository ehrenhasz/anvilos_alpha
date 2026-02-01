
 

#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/sysfs.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>

#include "core.h"
#include "card.h"
#include "host.h"
#include "bus.h"
#include "quirks.h"
#include "sd.h"
#include "sdio_bus.h"
#include "mmc_ops.h"
#include "sd_ops.h"
#include "sdio_ops.h"
#include "sdio_cis.h"

MMC_DEV_ATTR(vendor, "0x%04x\n", card->cis.vendor);
MMC_DEV_ATTR(device, "0x%04x\n", card->cis.device);
MMC_DEV_ATTR(revision, "%u.%u\n", card->major_rev, card->minor_rev);
MMC_DEV_ATTR(ocr, "0x%08x\n", card->ocr);
MMC_DEV_ATTR(rca, "0x%04x\n", card->rca);

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

static struct attribute *sdio_std_attrs[] = {
	&dev_attr_vendor.attr,
	&dev_attr_device.attr,
	&dev_attr_revision.attr,
	&dev_attr_info1.attr,
	&dev_attr_info2.attr,
	&dev_attr_info3.attr,
	&dev_attr_info4.attr,
	&dev_attr_ocr.attr,
	&dev_attr_rca.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sdio_std);

static struct device_type sdio_type = {
	.groups = sdio_std_groups,
};

static int sdio_read_fbr(struct sdio_func *func)
{
	int ret;
	unsigned char data;

	if (mmc_card_nonstd_func_interface(func->card)) {
		func->class = SDIO_CLASS_NONE;
		return 0;
	}

	ret = mmc_io_rw_direct(func->card, 0, 0,
		SDIO_FBR_BASE(func->num) + SDIO_FBR_STD_IF, 0, &data);
	if (ret)
		goto out;

	data &= 0x0f;

	if (data == 0x0f) {
		ret = mmc_io_rw_direct(func->card, 0, 0,
			SDIO_FBR_BASE(func->num) + SDIO_FBR_STD_IF_EXT, 0, &data);
		if (ret)
			goto out;
	}

	func->class = data;

out:
	return ret;
}

static int sdio_init_func(struct mmc_card *card, unsigned int fn)
{
	int ret;
	struct sdio_func *func;

	if (WARN_ON(fn > SDIO_MAX_FUNCS))
		return -EINVAL;

	func = sdio_alloc_func(card);
	if (IS_ERR(func))
		return PTR_ERR(func);

	func->num = fn;

	if (!(card->quirks & MMC_QUIRK_NONSTD_SDIO)) {
		ret = sdio_read_fbr(func);
		if (ret)
			goto fail;

		ret = sdio_read_func_cis(func);
		if (ret)
			goto fail;
	} else {
		func->vendor = func->card->cis.vendor;
		func->device = func->card->cis.device;
		func->max_blksize = func->card->cis.blksize;
	}

	card->sdio_func[fn - 1] = func;

	return 0;

fail:
	 
	sdio_remove_func(func);
	return ret;
}

static int sdio_read_cccr(struct mmc_card *card, u32 ocr)
{
	int ret;
	int cccr_vsn;
	int uhs = ocr & R4_18V_PRESENT;
	unsigned char data;
	unsigned char speed;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_CCCR, 0, &data);
	if (ret)
		goto out;

	cccr_vsn = data & 0x0f;

	if (cccr_vsn > SDIO_CCCR_REV_3_00) {
		pr_err("%s: unrecognised CCCR structure version %d\n",
			mmc_hostname(card->host), cccr_vsn);
		return -EINVAL;
	}

	card->cccr.sdio_vsn = (data & 0xf0) >> 4;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_CAPS, 0, &data);
	if (ret)
		goto out;

	if (data & SDIO_CCCR_CAP_SMB)
		card->cccr.multi_block = 1;
	if (data & SDIO_CCCR_CAP_LSC)
		card->cccr.low_speed = 1;
	if (data & SDIO_CCCR_CAP_4BLS)
		card->cccr.wide_bus = 1;

	if (cccr_vsn >= SDIO_CCCR_REV_1_10) {
		ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_POWER, 0, &data);
		if (ret)
			goto out;

		if (data & SDIO_POWER_SMPC)
			card->cccr.high_power = 1;
	}

	if (cccr_vsn >= SDIO_CCCR_REV_1_20) {
		ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
		if (ret)
			goto out;

		card->scr.sda_spec3 = 0;
		card->sw_caps.sd3_bus_mode = 0;
		card->sw_caps.sd3_drv_type = 0;
		if (cccr_vsn >= SDIO_CCCR_REV_3_00 && uhs) {
			card->scr.sda_spec3 = 1;
			ret = mmc_io_rw_direct(card, 0, 0,
				SDIO_CCCR_UHS, 0, &data);
			if (ret)
				goto out;

			if (mmc_host_uhs(card->host)) {
				if (data & SDIO_UHS_DDR50)
					card->sw_caps.sd3_bus_mode
						|= SD_MODE_UHS_DDR50 | SD_MODE_UHS_SDR50
							| SD_MODE_UHS_SDR25 | SD_MODE_UHS_SDR12;

				if (data & SDIO_UHS_SDR50)
					card->sw_caps.sd3_bus_mode
						|= SD_MODE_UHS_SDR50 | SD_MODE_UHS_SDR25
							| SD_MODE_UHS_SDR12;

				if (data & SDIO_UHS_SDR104)
					card->sw_caps.sd3_bus_mode
						|= SD_MODE_UHS_SDR104 | SD_MODE_UHS_SDR50
							| SD_MODE_UHS_SDR25 | SD_MODE_UHS_SDR12;
			}

			ret = mmc_io_rw_direct(card, 0, 0,
				SDIO_CCCR_DRIVE_STRENGTH, 0, &data);
			if (ret)
				goto out;

			if (data & SDIO_DRIVE_SDTA)
				card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_A;
			if (data & SDIO_DRIVE_SDTC)
				card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_C;
			if (data & SDIO_DRIVE_SDTD)
				card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_D;

			ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_INTERRUPT_EXT, 0, &data);
			if (ret)
				goto out;

			if (data & SDIO_INTERRUPT_EXT_SAI) {
				data |= SDIO_INTERRUPT_EXT_EAI;
				ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_INTERRUPT_EXT,
						       data, NULL);
				if (ret)
					goto out;

				card->cccr.enable_async_irq = 1;
			}
		}

		 
		if (!card->sw_caps.sd3_bus_mode) {
			if (speed & SDIO_SPEED_SHS) {
				card->cccr.high_speed = 1;
				card->sw_caps.hs_max_dtr = 50000000;
			} else {
				card->cccr.high_speed = 0;
				card->sw_caps.hs_max_dtr = 25000000;
			}
		}
	}

out:
	return ret;
}

static int sdio_enable_wide(struct mmc_card *card)
{
	int ret;
	u8 ctrl;

	if (!(card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (card->cccr.low_speed && !card->cccr.wide_bus)
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
	if (ret)
		return ret;

	if ((ctrl & SDIO_BUS_WIDTH_MASK) == SDIO_BUS_WIDTH_RESERVED)
		pr_warn("%s: SDIO_CCCR_IF is invalid: 0x%02x\n",
			mmc_hostname(card->host), ctrl);

	 
	ctrl &= ~SDIO_BUS_WIDTH_MASK;
	ctrl |= SDIO_BUS_WIDTH_4BIT;

	ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
	if (ret)
		return ret;

	return 1;
}

 
static int sdio_disable_cd(struct mmc_card *card)
{
	int ret;
	u8 ctrl;

	if (!mmc_card_disable_cd(card))
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
	if (ret)
		return ret;

	ctrl |= SDIO_BUS_CD_DISABLE;

	return mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
}

 
static int sdio_disable_wide(struct mmc_card *card)
{
	int ret;
	u8 ctrl;

	if (!(card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (card->cccr.low_speed && !card->cccr.wide_bus)
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
	if (ret)
		return ret;

	if (!(ctrl & SDIO_BUS_WIDTH_4BIT))
		return 0;

	ctrl &= ~SDIO_BUS_WIDTH_4BIT;
	ctrl |= SDIO_BUS_ASYNC_INT;

	ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
	if (ret)
		return ret;

	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_1);

	return 0;
}

static int sdio_disable_4bit_bus(struct mmc_card *card)
{
	int err;

	if (mmc_card_sdio(card))
		goto out;

	if (!(card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (!(card->scr.bus_widths & SD_SCR_BUS_WIDTH_4))
		return 0;

	err = mmc_app_set_bus_width(card, MMC_BUS_WIDTH_1);
	if (err)
		return err;

out:
	return sdio_disable_wide(card);
}


static int sdio_enable_4bit_bus(struct mmc_card *card)
{
	int err;

	err = sdio_enable_wide(card);
	if (err <= 0)
		return err;
	if (mmc_card_sdio(card))
		goto out;

	if (card->scr.bus_widths & SD_SCR_BUS_WIDTH_4) {
		err = mmc_app_set_bus_width(card, MMC_BUS_WIDTH_4);
		if (err) {
			sdio_disable_wide(card);
			return err;
		}
	}
out:
	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_4);

	return 0;
}


 
static int mmc_sdio_switch_hs(struct mmc_card *card, int enable)
{
	int ret;
	u8 speed;

	if (!(card->host->caps & MMC_CAP_SD_HIGHSPEED))
		return 0;

	if (!card->cccr.high_speed)
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
	if (ret)
		return ret;

	if (enable)
		speed |= SDIO_SPEED_EHS;
	else
		speed &= ~SDIO_SPEED_EHS;

	ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_SPEED, speed, NULL);
	if (ret)
		return ret;

	return 1;
}

 
static int sdio_enable_hs(struct mmc_card *card)
{
	int ret;

	ret = mmc_sdio_switch_hs(card, true);
	if (ret <= 0 || mmc_card_sdio(card))
		return ret;

	ret = mmc_sd_switch_hs(card);
	if (ret <= 0)
		mmc_sdio_switch_hs(card, false);

	return ret;
}

static unsigned mmc_sdio_get_max_clock(struct mmc_card *card)
{
	unsigned max_dtr;

	if (mmc_card_hs(card)) {
		 
		max_dtr = 50000000;
	} else {
		max_dtr = card->cis.max_dtr;
	}

	if (mmc_card_sd_combo(card))
		max_dtr = min(max_dtr, mmc_sd_get_max_clock(card));

	return max_dtr;
}

static unsigned char host_drive_to_sdio_drive(int host_strength)
{
	switch (host_strength) {
	case MMC_SET_DRIVER_TYPE_A:
		return SDIO_DTSx_SET_TYPE_A;
	case MMC_SET_DRIVER_TYPE_B:
		return SDIO_DTSx_SET_TYPE_B;
	case MMC_SET_DRIVER_TYPE_C:
		return SDIO_DTSx_SET_TYPE_C;
	case MMC_SET_DRIVER_TYPE_D:
		return SDIO_DTSx_SET_TYPE_D;
	default:
		return SDIO_DTSx_SET_TYPE_B;
	}
}

static void sdio_select_driver_type(struct mmc_card *card)
{
	int card_drv_type, drive_strength, drv_type;
	unsigned char card_strength;
	int err;

	card->drive_strength = 0;

	card_drv_type = card->sw_caps.sd3_drv_type | SD_DRIVER_TYPE_B;

	drive_strength = mmc_select_drive_strength(card,
						   card->sw_caps.uhs_max_dtr,
						   card_drv_type, &drv_type);

	if (drive_strength) {
		 
		err = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_DRIVE_STRENGTH, 0,
				       &card_strength);
		if (err)
			return;

		card_strength &= ~(SDIO_DRIVE_DTSx_MASK<<SDIO_DRIVE_DTSx_SHIFT);
		card_strength |= host_drive_to_sdio_drive(drive_strength);

		 
		err = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_DRIVE_STRENGTH,
				       card_strength, NULL);
		if (err)
			return;
		card->drive_strength = drive_strength;
	}

	if (drv_type)
		mmc_set_driver_type(card->host, drv_type);
}


static int sdio_set_bus_speed_mode(struct mmc_card *card)
{
	unsigned int bus_speed, timing;
	int err;
	unsigned char speed;
	unsigned int max_rate;

	 
	if (!mmc_host_uhs(card->host))
		return 0;

	bus_speed = SDIO_SPEED_SDR12;
	timing = MMC_TIMING_UHS_SDR12;
	if ((card->host->caps & MMC_CAP_UHS_SDR104) &&
	    (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR104)) {
			bus_speed = SDIO_SPEED_SDR104;
			timing = MMC_TIMING_UHS_SDR104;
			card->sw_caps.uhs_max_dtr = UHS_SDR104_MAX_DTR;
			card->sd_bus_speed = UHS_SDR104_BUS_SPEED;
	} else if ((card->host->caps & MMC_CAP_UHS_DDR50) &&
		   (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_DDR50)) {
			bus_speed = SDIO_SPEED_DDR50;
			timing = MMC_TIMING_UHS_DDR50;
			card->sw_caps.uhs_max_dtr = UHS_DDR50_MAX_DTR;
			card->sd_bus_speed = UHS_DDR50_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50)) && (card->sw_caps.sd3_bus_mode &
		    SD_MODE_UHS_SDR50)) {
			bus_speed = SDIO_SPEED_SDR50;
			timing = MMC_TIMING_UHS_SDR50;
			card->sw_caps.uhs_max_dtr = UHS_SDR50_MAX_DTR;
			card->sd_bus_speed = UHS_SDR50_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR25)) &&
		   (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR25)) {
			bus_speed = SDIO_SPEED_SDR25;
			timing = MMC_TIMING_UHS_SDR25;
			card->sw_caps.uhs_max_dtr = UHS_SDR25_MAX_DTR;
			card->sd_bus_speed = UHS_SDR25_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR25 |
		    MMC_CAP_UHS_SDR12)) && (card->sw_caps.sd3_bus_mode &
		    SD_MODE_UHS_SDR12)) {
			bus_speed = SDIO_SPEED_SDR12;
			timing = MMC_TIMING_UHS_SDR12;
			card->sw_caps.uhs_max_dtr = UHS_SDR12_MAX_DTR;
			card->sd_bus_speed = UHS_SDR12_BUS_SPEED;
	}

	err = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
	if (err)
		return err;

	speed &= ~SDIO_SPEED_BSS_MASK;
	speed |= bus_speed;
	err = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_SPEED, speed, NULL);
	if (err)
		return err;

	max_rate = min_not_zero(card->quirk_max_rate,
				card->sw_caps.uhs_max_dtr);

	mmc_set_timing(card->host, timing);
	mmc_set_clock(card->host, max_rate);

	return 0;
}

 
static int mmc_sdio_init_uhs_card(struct mmc_card *card)
{
	int err;

	if (!card->scr.sda_spec3)
		return 0;

	 
	err = sdio_enable_4bit_bus(card);
	if (err)
		goto out;

	 
	sdio_select_driver_type(card);

	 
	err = sdio_set_bus_speed_mode(card);
	if (err)
		goto out;

	 
	if (!mmc_host_is_spi(card->host) &&
	    ((card->host->ios.timing == MMC_TIMING_UHS_SDR50) ||
	      (card->host->ios.timing == MMC_TIMING_UHS_SDR104)))
		err = mmc_execute_tuning(card);
out:
	return err;
}

static int mmc_sdio_pre_init(struct mmc_host *host, u32 ocr,
			     struct mmc_card *card)
{
	if (card)
		mmc_remove_card(card);

	 

	sdio_reset(host);
	mmc_go_idle(host);
	mmc_send_if_cond(host, ocr);
	return mmc_send_io_op_cond(host, 0, NULL);
}

 
static int mmc_sdio_init_card(struct mmc_host *host, u32 ocr,
			      struct mmc_card *oldcard)
{
	struct mmc_card *card;
	int err;
	int retries = 10;
	u32 rocr = 0;
	u32 ocr_card = ocr;

	WARN_ON(!host->claimed);

	 
	if (mmc_host_uhs(host))
		ocr |= R4_18V_PRESENT;

try_again:
	if (!retries) {
		pr_warn("%s: Skipping voltage switch\n", mmc_hostname(host));
		ocr &= ~R4_18V_PRESENT;
	}

	 
	err = mmc_send_io_op_cond(host, ocr, &rocr);
	if (err)
		return err;

	 
	if (mmc_host_is_spi(host)) {
		err = mmc_spi_set_crc(host, use_spi_crc);
		if (err)
			return err;
	}

	 
	card = mmc_alloc_card(host, &sdio_type);
	if (IS_ERR(card))
		return PTR_ERR(card);

	if ((rocr & R4_MEMORY_PRESENT) &&
	    mmc_sd_get_cid(host, ocr & rocr, card->raw_cid, NULL) == 0) {
		card->type = MMC_TYPE_SD_COMBO;

		if (oldcard && (!mmc_card_sd_combo(oldcard) ||
		    memcmp(card->raw_cid, oldcard->raw_cid, sizeof(card->raw_cid)) != 0)) {
			err = -ENOENT;
			goto mismatch;
		}
	} else {
		card->type = MMC_TYPE_SDIO;

		if (oldcard && !mmc_card_sdio(oldcard)) {
			err = -ENOENT;
			goto mismatch;
		}
	}

	 
	if (host->ops->init_card)
		host->ops->init_card(host, card);
	mmc_fixup_device(card, sdio_card_init_methods);

	card->ocr = ocr_card;

	 
	if (rocr & ocr & R4_18V_PRESENT) {
		err = mmc_set_uhs_voltage(host, ocr_card);
		if (err == -EAGAIN) {
			mmc_sdio_pre_init(host, ocr_card, card);
			retries--;
			goto try_again;
		} else if (err) {
			ocr &= ~R4_18V_PRESENT;
		}
	}

	 
	if (!mmc_host_is_spi(host)) {
		err = mmc_send_relative_addr(host, &card->rca);
		if (err)
			goto remove;

		 
		if (oldcard)
			oldcard->rca = card->rca;
	}

	 
	if (!oldcard && mmc_card_sd_combo(card)) {
		err = mmc_sd_get_csd(card);
		if (err)
			goto remove;

		mmc_decode_cid(card);
	}

	 
	if (!mmc_host_is_spi(host)) {
		err = mmc_select_card(card);
		if (err)
			goto remove;
	}

	if (card->quirks & MMC_QUIRK_NONSTD_SDIO) {
		 
		mmc_set_clock(host, card->cis.max_dtr);

		if (card->cccr.high_speed) {
			mmc_set_timing(card->host, MMC_TIMING_SD_HS);
		}

		if (oldcard)
			mmc_remove_card(card);
		else
			host->card = card;

		return 0;
	}

	 
	err = sdio_read_cccr(card, ocr);
	if (err) {
		mmc_sdio_pre_init(host, ocr_card, card);
		if (ocr & R4_18V_PRESENT) {
			 
			retries = 0;
			goto try_again;
		}
		return err;
	}

	 
	err = sdio_read_common_cis(card);
	if (err)
		goto remove;

	if (oldcard) {
		if (card->cis.vendor == oldcard->cis.vendor &&
		    card->cis.device == oldcard->cis.device) {
			mmc_remove_card(card);
			card = oldcard;
		} else {
			err = -ENOENT;
			goto mismatch;
		}
	}

	mmc_fixup_device(card, sdio_fixup_methods);

	if (mmc_card_sd_combo(card)) {
		err = mmc_sd_setup_card(host, card, oldcard != NULL);
		 
		if (err) {
			mmc_go_idle(host);
			if (mmc_host_is_spi(host))
				 
				mmc_spi_set_crc(host, use_spi_crc);
			card->type = MMC_TYPE_SDIO;
		} else
			card->dev.type = &sd_type;
	}

	 
	err = sdio_disable_cd(card);
	if (err)
		goto remove;

	 
	 
	if ((ocr & R4_18V_PRESENT) && card->sw_caps.sd3_bus_mode) {
		err = mmc_sdio_init_uhs_card(card);
		if (err)
			goto remove;
	} else {
		 
		err = sdio_enable_hs(card);
		if (err > 0)
			mmc_set_timing(card->host, MMC_TIMING_SD_HS);
		else if (err)
			goto remove;

		 
		mmc_set_clock(host, mmc_sdio_get_max_clock(card));

		 
		err = sdio_enable_4bit_bus(card);
		if (err)
			goto remove;
	}

	if (host->caps2 & MMC_CAP2_AVOID_3_3V &&
	    host->ios.signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		pr_err("%s: Host failed to negotiate down from 3.3V\n",
			mmc_hostname(host));
		err = -EINVAL;
		goto remove;
	}

	host->card = card;
	return 0;

mismatch:
	pr_debug("%s: Perhaps the card was replaced\n", mmc_hostname(host));
remove:
	if (oldcard != card)
		mmc_remove_card(card);
	return err;
}

static int mmc_sdio_reinit_card(struct mmc_host *host)
{
	int ret;

	ret = mmc_sdio_pre_init(host, host->card->ocr, NULL);
	if (ret)
		return ret;

	return mmc_sdio_init_card(host, host->card->ocr, host->card);
}

 
static void mmc_sdio_remove(struct mmc_host *host)
{
	int i;

	for (i = 0;i < host->card->sdio_funcs;i++) {
		if (host->card->sdio_func[i]) {
			sdio_remove_func(host->card->sdio_func[i]);
			host->card->sdio_func[i] = NULL;
		}
	}

	mmc_remove_card(host->card);
	host->card = NULL;
}

 
static int mmc_sdio_alive(struct mmc_host *host)
{
	return mmc_select_card(host->card);
}

 
static void mmc_sdio_detect(struct mmc_host *host)
{
	int err;

	 
	if (host->caps & MMC_CAP_POWER_OFF_CARD) {
		err = pm_runtime_resume_and_get(&host->card->dev);
		if (err < 0)
			goto out;
	}

	mmc_claim_host(host);

	 
	err = _mmc_detect_card_removed(host);

	mmc_release_host(host);

	 
	if (host->caps & MMC_CAP_POWER_OFF_CARD)
		pm_runtime_put_sync(&host->card->dev);

out:
	if (err) {
		mmc_sdio_remove(host);

		mmc_claim_host(host);
		mmc_detach_bus(host);
		mmc_power_off(host);
		mmc_release_host(host);
	}
}

 
static int mmc_sdio_pre_suspend(struct mmc_host *host)
{
	int i;

	for (i = 0; i < host->card->sdio_funcs; i++) {
		struct sdio_func *func = host->card->sdio_func[i];
		if (func && sdio_func_present(func) && func->dev.driver) {
			const struct dev_pm_ops *pmops = func->dev.driver->pm;
			if (!pmops || !pmops->suspend || !pmops->resume)
				 
				goto remove;
		}
	}

	return 0;

remove:
	if (!mmc_card_is_removable(host)) {
		dev_warn(mmc_dev(host),
			 "missing suspend/resume ops for non-removable SDIO card\n");
		 
		return 0;
	}

	 
	mmc_sdio_remove(host);
	mmc_claim_host(host);
	mmc_detach_bus(host);
	mmc_power_off(host);
	mmc_release_host(host);
	host->pm_flags = 0;

	return 0;
}

 
static int mmc_sdio_suspend(struct mmc_host *host)
{
	WARN_ON(host->sdio_irqs && !mmc_card_keep_power(host));

	 
	mmc_card_set_suspended(host->card);
	cancel_work_sync(&host->sdio_irq_work);

	mmc_claim_host(host);

	if (mmc_card_keep_power(host) && mmc_card_wake_sdio_irq(host))
		sdio_disable_4bit_bus(host->card);

	if (!mmc_card_keep_power(host)) {
		mmc_power_off(host);
	} else if (host->retune_period) {
		mmc_retune_timer_stop(host);
		mmc_retune_needed(host);
	}

	mmc_release_host(host);

	return 0;
}

static int mmc_sdio_resume(struct mmc_host *host)
{
	int err = 0;

	 
	mmc_claim_host(host);

	 
	if (!mmc_card_keep_power(host)) {
		mmc_power_up(host, host->card->ocr);
		 
		if (host->caps & MMC_CAP_POWER_OFF_CARD) {
			pm_runtime_disable(&host->card->dev);
			pm_runtime_set_active(&host->card->dev);
			pm_runtime_enable(&host->card->dev);
		}
		err = mmc_sdio_reinit_card(host);
	} else if (mmc_card_wake_sdio_irq(host)) {
		 
		mmc_retune_hold_now(host);
		err = sdio_enable_4bit_bus(host->card);
		mmc_retune_release(host);
	}

	if (err)
		goto out;

	 
	mmc_card_clr_suspended(host->card);

	if (host->sdio_irqs) {
		if (!(host->caps2 & MMC_CAP2_SDIO_IRQ_NOTHREAD))
			wake_up_process(host->sdio_irq_thread);
		else if (host->caps & MMC_CAP_SDIO_IRQ)
			schedule_work(&host->sdio_irq_work);
	}

out:
	mmc_release_host(host);

	host->pm_flags &= ~MMC_PM_KEEP_POWER;
	return err;
}

static int mmc_sdio_runtime_suspend(struct mmc_host *host)
{
	 
	mmc_claim_host(host);
	mmc_power_off(host);
	mmc_release_host(host);

	return 0;
}

static int mmc_sdio_runtime_resume(struct mmc_host *host)
{
	int ret;

	 
	mmc_claim_host(host);
	mmc_power_up(host, host->card->ocr);
	ret = mmc_sdio_reinit_card(host);
	mmc_release_host(host);

	return ret;
}

 
static int mmc_sdio_hw_reset(struct mmc_host *host)
{
	struct mmc_card *card = host->card;

	 
	if (atomic_read(&card->sdio_funcs_probed) > 1) {
		if (mmc_card_removed(card))
			return 1;
		host->rescan_entered = 0;
		mmc_card_set_removed(card);
		_mmc_detect_change(host, 0, false);
		return 1;
	}

	 
	mmc_power_cycle(host, card->ocr);
	return mmc_sdio_reinit_card(host);
}

static int mmc_sdio_sw_reset(struct mmc_host *host)
{
	mmc_set_clock(host, host->f_init);
	sdio_reset(host);
	mmc_go_idle(host);

	mmc_set_initial_state(host);
	mmc_set_initial_signal_voltage(host);

	return mmc_sdio_reinit_card(host);
}

static const struct mmc_bus_ops mmc_sdio_ops = {
	.remove = mmc_sdio_remove,
	.detect = mmc_sdio_detect,
	.pre_suspend = mmc_sdio_pre_suspend,
	.suspend = mmc_sdio_suspend,
	.resume = mmc_sdio_resume,
	.runtime_suspend = mmc_sdio_runtime_suspend,
	.runtime_resume = mmc_sdio_runtime_resume,
	.alive = mmc_sdio_alive,
	.hw_reset = mmc_sdio_hw_reset,
	.sw_reset = mmc_sdio_sw_reset,
};


 
int mmc_attach_sdio(struct mmc_host *host)
{
	int err, i, funcs;
	u32 ocr, rocr;
	struct mmc_card *card;

	WARN_ON(!host->claimed);

	err = mmc_send_io_op_cond(host, 0, &ocr);
	if (err)
		return err;

	mmc_attach_bus(host, &mmc_sdio_ops);
	if (host->ocr_avail_sdio)
		host->ocr_avail = host->ocr_avail_sdio;


	rocr = mmc_select_voltage(host, ocr);

	 
	if (!rocr) {
		err = -EINVAL;
		goto err;
	}

	 
	err = mmc_sdio_init_card(host, rocr, NULL);
	if (err)
		goto err;

	card = host->card;

	 
	if (host->caps & MMC_CAP_POWER_OFF_CARD) {
		 
		pm_runtime_get_noresume(&card->dev);

		 
		err = pm_runtime_set_active(&card->dev);
		if (err)
			goto remove;

		 
		pm_runtime_enable(&card->dev);
	}

	 
	funcs = (ocr & 0x70000000) >> 28;
	card->sdio_funcs = 0;

	 
	for (i = 0; i < funcs; i++, card->sdio_funcs++) {
		err = sdio_init_func(host->card, i + 1);
		if (err)
			goto remove;

		 
		if (host->caps & MMC_CAP_POWER_OFF_CARD)
			pm_runtime_enable(&card->sdio_func[i]->dev);
	}

	 
	mmc_release_host(host);
	err = mmc_add_card(host->card);
	if (err)
		goto remove_added;

	 
	for (i = 0;i < funcs;i++) {
		err = sdio_add_func(host->card->sdio_func[i]);
		if (err)
			goto remove_added;
	}

	if (host->caps & MMC_CAP_POWER_OFF_CARD)
		pm_runtime_put(&card->dev);

	mmc_claim_host(host);
	return 0;


remove:
	mmc_release_host(host);
remove_added:
	 
	mmc_sdio_remove(host);
	mmc_claim_host(host);
err:
	mmc_detach_bus(host);

	pr_err("%s: error %d whilst initialising SDIO card\n",
		mmc_hostname(host), err);

	return err;
}

