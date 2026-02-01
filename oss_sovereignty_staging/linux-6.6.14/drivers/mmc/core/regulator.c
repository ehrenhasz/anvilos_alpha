
 

#include <linux/device.h>
#include <linux/err.h>
#include <linux/log2.h>
#include <linux/regulator/consumer.h>

#include <linux/mmc/host.h>

#include "core.h"
#include "host.h"

#ifdef CONFIG_REGULATOR

 
static int mmc_ocrbitnum_to_vdd(int vdd_bit, int *min_uV, int *max_uV)
{
	int		tmp;

	if (!vdd_bit)
		return -EINVAL;

	 
	tmp = vdd_bit - ilog2(MMC_VDD_165_195);
	if (tmp == 0) {
		*min_uV = 1650 * 1000;
		*max_uV = 1950 * 1000;
	} else {
		*min_uV = 1900 * 1000 + tmp * 100 * 1000;
		*max_uV = *min_uV + 100 * 1000;
	}

	return 0;
}

 
static int mmc_regulator_get_ocrmask(struct regulator *supply)
{
	int			result = 0;
	int			count;
	int			i;
	int			vdd_uV;
	int			vdd_mV;

	count = regulator_count_voltages(supply);
	if (count < 0)
		return count;

	for (i = 0; i < count; i++) {
		vdd_uV = regulator_list_voltage(supply, i);
		if (vdd_uV <= 0)
			continue;

		vdd_mV = vdd_uV / 1000;
		result |= mmc_vddrange_to_ocrmask(vdd_mV, vdd_mV);
	}

	if (!result) {
		vdd_uV = regulator_get_voltage(supply);
		if (vdd_uV <= 0)
			return vdd_uV;

		vdd_mV = vdd_uV / 1000;
		result = mmc_vddrange_to_ocrmask(vdd_mV, vdd_mV);
	}

	return result;
}

 
int mmc_regulator_set_ocr(struct mmc_host *mmc,
			struct regulator *supply,
			unsigned short vdd_bit)
{
	int			result = 0;
	int			min_uV, max_uV;

	if (IS_ERR(supply))
		return 0;

	if (vdd_bit) {
		mmc_ocrbitnum_to_vdd(vdd_bit, &min_uV, &max_uV);

		result = regulator_set_voltage(supply, min_uV, max_uV);
		if (result == 0 && !mmc->regulator_enabled) {
			result = regulator_enable(supply);
			if (!result)
				mmc->regulator_enabled = true;
		}
	} else if (mmc->regulator_enabled) {
		result = regulator_disable(supply);
		if (result == 0)
			mmc->regulator_enabled = false;
	}

	if (result)
		dev_err(mmc_dev(mmc),
			"could not set regulator OCR (%d)\n", result);
	return result;
}
EXPORT_SYMBOL_GPL(mmc_regulator_set_ocr);

static int mmc_regulator_set_voltage_if_supported(struct regulator *regulator,
						  int min_uV, int target_uV,
						  int max_uV)
{
	int current_uV;

	 
	if (!regulator_is_supported_voltage(regulator, min_uV, max_uV))
		return -EINVAL;

	 
	current_uV = regulator_get_voltage(regulator);
	if (current_uV == target_uV)
		return 1;

	return regulator_set_voltage_triplet(regulator, min_uV, target_uV,
					     max_uV);
}

 
int mmc_regulator_set_vqmmc(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct device *dev = mmc_dev(mmc);
	int ret, volt, min_uV, max_uV;

	 
	if (IS_ERR(mmc->supply.vqmmc))
		return -EINVAL;

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_120:
		return mmc_regulator_set_voltage_if_supported(mmc->supply.vqmmc,
						1100000, 1200000, 1300000);
	case MMC_SIGNAL_VOLTAGE_180:
		return mmc_regulator_set_voltage_if_supported(mmc->supply.vqmmc,
						1700000, 1800000, 1950000);
	case MMC_SIGNAL_VOLTAGE_330:
		ret = mmc_ocrbitnum_to_vdd(mmc->ios.vdd, &volt, &max_uV);
		if (ret < 0)
			return ret;

		dev_dbg(dev, "%s: found vmmc voltage range of %d-%duV\n",
			__func__, volt, max_uV);

		min_uV = max(volt - 300000, 2700000);
		max_uV = min(max_uV + 200000, 3600000);

		 
		ret = mmc_regulator_set_voltage_if_supported(mmc->supply.vqmmc,
							min_uV, volt, max_uV);
		if (ret >= 0)
			return ret;

		return mmc_regulator_set_voltage_if_supported(mmc->supply.vqmmc,
						2700000, volt, 3600000);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(mmc_regulator_set_vqmmc);

#else

static inline int mmc_regulator_get_ocrmask(struct regulator *supply)
{
	return 0;
}

#endif  

 
int mmc_regulator_get_supply(struct mmc_host *mmc)
{
	struct device *dev = mmc_dev(mmc);
	int ret;

	mmc->supply.vmmc = devm_regulator_get_optional(dev, "vmmc");
	mmc->supply.vqmmc = devm_regulator_get_optional(dev, "vqmmc");

	if (IS_ERR(mmc->supply.vmmc)) {
		if (PTR_ERR(mmc->supply.vmmc) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_dbg(dev, "No vmmc regulator found\n");
	} else {
		ret = mmc_regulator_get_ocrmask(mmc->supply.vmmc);
		if (ret > 0)
			mmc->ocr_avail = ret;
		else
			dev_warn(dev, "Failed getting OCR mask: %d\n", ret);
	}

	if (IS_ERR(mmc->supply.vqmmc)) {
		if (PTR_ERR(mmc->supply.vqmmc) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_dbg(dev, "No vqmmc regulator found\n");
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mmc_regulator_get_supply);

 
int mmc_regulator_enable_vqmmc(struct mmc_host *mmc)
{
	int ret = 0;

	if (!IS_ERR(mmc->supply.vqmmc) && !mmc->vqmmc_enabled) {
		ret = regulator_enable(mmc->supply.vqmmc);
		if (ret < 0)
			dev_err(mmc_dev(mmc), "enabling vqmmc regulator failed\n");
		else
			mmc->vqmmc_enabled = true;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mmc_regulator_enable_vqmmc);

 
void mmc_regulator_disable_vqmmc(struct mmc_host *mmc)
{
	if (!IS_ERR(mmc->supply.vqmmc) && mmc->vqmmc_enabled) {
		regulator_disable(mmc->supply.vqmmc);
		mmc->vqmmc_enabled = false;
	}
}
EXPORT_SYMBOL_GPL(mmc_regulator_disable_vqmmc);
