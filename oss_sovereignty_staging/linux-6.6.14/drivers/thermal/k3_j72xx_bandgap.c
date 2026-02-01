
 

#include <linux/math.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define K3_VTM_DEVINFO_PWR0_OFFSET		0x4
#define K3_VTM_DEVINFO_PWR0_TEMPSENS_CT_MASK	0xf0
#define K3_VTM_TMPSENS0_CTRL_OFFSET		0x300
#define K3_VTM_MISC_CTRL_OFFSET			0xc
#define K3_VTM_TMPSENS_STAT_OFFSET		0x8
#define K3_VTM_ANYMAXT_OUTRG_ALERT_EN		0x1
#define K3_VTM_MISC_CTRL2_OFFSET		0x10
#define K3_VTM_TS_STAT_DTEMP_MASK		0x3ff
#define K3_VTM_MAX_NUM_TS			8
#define K3_VTM_TMPSENS_CTRL_SOC			BIT(5)
#define K3_VTM_TMPSENS_CTRL_CLRZ		BIT(6)
#define K3_VTM_TMPSENS_CTRL_CLKON_REQ		BIT(7)
#define K3_VTM_TMPSENS_CTRL_MAXT_OUTRG_EN	BIT(11)

#define K3_VTM_CORRECTION_TEMP_CNT		3

#define MINUS40CREF				5
#define PLUS30CREF				253
#define PLUS125CREF				730
#define PLUS150CREF				940

#define TABLE_SIZE				1024
#define MAX_TEMP				123000
#define COOL_DOWN_TEMP				105000

#define FACTORS_REDUCTION			13
static int *derived_table;

static int compute_value(int index, const s64 *factors, int nr_factors,
			 int reduction)
{
	s64 value = 0;
	int i;

	for (i = 0; i < nr_factors; i++)
		value += factors[i] * int_pow(index, i);

	return (int)div64_s64(value, int_pow(10, reduction));
}

static void init_table(int factors_size, int *table, const s64 *factors)
{
	int i;

	for (i = 0; i < TABLE_SIZE; i++)
		table[i] = compute_value(i, factors, factors_size,
					 FACTORS_REDUCTION);
}

 
struct err_values {
	int refs[4];
	int errs[4];
};

static void create_table_segments(struct err_values *err_vals, int seg,
				  int *ref_table)
{
	int m = 0, c, num, den, i, err, idx1, idx2, err1, err2, ref1, ref2;

	if (seg == 0)
		idx1 = 0;
	else
		idx1 = err_vals->refs[seg];

	idx2 = err_vals->refs[seg + 1];
	err1 = err_vals->errs[seg];
	err2 = err_vals->errs[seg + 1];
	ref1 = err_vals->refs[seg];
	ref2 = err_vals->refs[seg + 1];

	 
	num = ref2 - ref1;
	den = err2 - err1;
	if (den)
		m = num / den;
	c = ref2 - m * err2;

	 
	if (den != 0 && m != 0) {
		for (i = idx1; i <= idx2; i++) {
			err = (i - c) / m;
			if (((i + err) < 0) || ((i + err) >= TABLE_SIZE))
				continue;
			derived_table[i] = ref_table[i + err];
		}
	} else {  
		for (i = idx1; i <= idx2; i++) {
			if (((i + err1) < 0) || ((i + err1) >= TABLE_SIZE))
				continue;
			derived_table[i] = ref_table[i + err1];
		}
	}
}

static int prep_lookup_table(struct err_values *err_vals, int *ref_table)
{
	int inc, i, seg;

	 
	for (seg = 0; seg < 3; seg++)
		create_table_segments(err_vals, seg, ref_table);

	 
	i = 0;
	while (!derived_table[i])
		i++;

	 
	if (i) {
		 
		while (i--)
			derived_table[i] = derived_table[i + 1] - 300;
	}

	 
	i = TABLE_SIZE - 1;
	while (!derived_table[i])
		i--;

	i++;
	inc = 1;
	while (i < TABLE_SIZE) {
		derived_table[i] = derived_table[i - 1] + inc * 100;
		i++;
	}

	return 0;
}

struct k3_thermal_data;

struct k3_j72xx_bandgap {
	struct device *dev;
	void __iomem *base;
	void __iomem *cfg2_base;
	struct k3_thermal_data *ts_data[K3_VTM_MAX_NUM_TS];
};

 
struct k3_thermal_data {
	struct k3_j72xx_bandgap *bgp;
	u32 ctrl_offset;
	u32 stat_offset;
};

static int two_cmp(int tmp, int mask)
{
	tmp = ~(tmp);
	tmp &= mask;
	tmp += 1;

	 
	return (0 - tmp);
}

static unsigned int vtm_get_best_value(unsigned int s0, unsigned int s1,
				       unsigned int s2)
{
	int d01 = abs(s0 - s1);
	int d02 = abs(s0 - s2);
	int d12 = abs(s1 - s2);

	if (d01 <= d02 && d01 <= d12)
		return (s0 + s1) / 2;

	if (d02 <= d01 && d02 <= d12)
		return (s0 + s2) / 2;

	return (s1 + s2) / 2;
}

static inline int k3_bgp_read_temp(struct k3_thermal_data *devdata,
				   int *temp)
{
	struct k3_j72xx_bandgap *bgp;
	unsigned int dtemp, s0, s1, s2;

	bgp = devdata->bgp;
	 
	s0 = readl(bgp->base + devdata->stat_offset) &
		K3_VTM_TS_STAT_DTEMP_MASK;
	s1 = readl(bgp->base + devdata->stat_offset) &
		K3_VTM_TS_STAT_DTEMP_MASK;
	s2 = readl(bgp->base + devdata->stat_offset) &
		K3_VTM_TS_STAT_DTEMP_MASK;
	dtemp = vtm_get_best_value(s0, s1, s2);

	if (dtemp < 0 || dtemp >= TABLE_SIZE)
		return -EINVAL;

	*temp = derived_table[dtemp];

	return 0;
}

 
static int k3_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
	return k3_bgp_read_temp(thermal_zone_device_priv(tz), temp);
}

static const struct thermal_zone_device_ops k3_of_thermal_ops = {
	.get_temp = k3_thermal_get_temp,
};

static int k3_j72xx_bandgap_temp_to_adc_code(int temp)
{
	int low = 0, high = TABLE_SIZE - 1, mid;

	if (temp > 160000 || temp < -50000)
		return -EINVAL;

	 
	while (low < (high - 1)) {
		mid = (low + high) / 2;
		if (temp <= derived_table[mid])
			high = mid;
		else
			low = mid;
	}

	return mid;
}

static void get_efuse_values(int id, struct k3_thermal_data *data, int *err,
			     void __iomem *fuse_base)
{
	int i, tmp, pow;
	int ct_offsets[5][K3_VTM_CORRECTION_TEMP_CNT] = {
		{ 0x0, 0x8, 0x4 },
		{ 0x0, 0x8, 0x4 },
		{ 0x0, -1,  0x4 },
		{ 0x0, 0xC, -1 },
		{ 0x0, 0xc, 0x8 }
	};
	int ct_bm[5][K3_VTM_CORRECTION_TEMP_CNT] = {
		{ 0x3f, 0x1fe000, 0x1ff },
		{ 0xfc0, 0x1fe000, 0x3fe00 },
		{ 0x3f000, 0x7f800000, 0x7fc0000 },
		{ 0xfc0000, 0x1fe0, 0x1f800000 },
		{ 0x3f000000, 0x1fe000, 0x1ff0 }
	};

	for (i = 0; i < 3; i++) {
		 
		if (ct_offsets[id][i] == -1 && i == 1) {
			 
			tmp = (readl(fuse_base + 0x8) & 0xE0000000) >> (29);
			tmp |= ((readl(fuse_base + 0xC) & 0x1F) << 3);
			pow = tmp & 0x80;
		} else if (ct_offsets[id][i] == -1 && i == 2) {
			 
			tmp = (readl(fuse_base + 0x4) & 0xF8000000) >> (27);
			tmp |= ((readl(fuse_base + 0x8) & 0xF) << 5);
			pow = tmp & 0x100;
		} else {
			tmp = readl(fuse_base + ct_offsets[id][i]);
			tmp &= ct_bm[id][i];
			tmp = tmp >> __ffs(ct_bm[id][i]);

			 
			pow = ct_bm[id][i] >> __ffs(ct_bm[id][i]);
			pow += 1;
			pow /= 2;
		}

		 
		if (tmp & pow) {
			 
			tmp = two_cmp(tmp, ct_bm[id][i] >> __ffs(ct_bm[id][i]));
		}
		err[i] = tmp;
	}

	 
	err[i] = 0;
}

static void print_look_up_table(struct device *dev, int *ref_table)
{
	int i;

	dev_dbg(dev, "The contents of derived array\n");
	dev_dbg(dev, "Code   Temperature\n");
	for (i = 0; i < TABLE_SIZE; i++)
		dev_dbg(dev, "%d       %d %d\n", i, derived_table[i], ref_table[i]);
}

struct k3_j72xx_bandgap_data {
	const bool has_errata_i2128;
};

static int k3_j72xx_bandgap_probe(struct platform_device *pdev)
{
	int ret = 0, cnt, val, id;
	int high_max, low_temp;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct k3_j72xx_bandgap *bgp;
	struct k3_thermal_data *data;
	bool workaround_needed = false;
	const struct k3_j72xx_bandgap_data *driver_data;
	struct thermal_zone_device *ti_thermal;
	int *ref_table;
	struct err_values err_vals;
	void __iomem *fuse_base;

	const s64 golden_factors[] = {
		-490019999999999936,
		3251200000000000,
		-1705800000000,
		603730000,
		-92627,
	};

	const s64 pvt_wa_factors[] = {
		-415230000000000000,
		3126600000000000,
		-1157800000000,
	};

	bgp = devm_kzalloc(&pdev->dev, sizeof(*bgp), GFP_KERNEL);
	if (!bgp)
		return -ENOMEM;

	bgp->dev = dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	bgp->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(bgp->base))
		return PTR_ERR(bgp->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	bgp->cfg2_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(bgp->cfg2_base))
		return PTR_ERR(bgp->cfg2_base);

	driver_data = of_device_get_match_data(dev);
	if (driver_data)
		workaround_needed = driver_data->has_errata_i2128;

	 
	if (workaround_needed) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		fuse_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(fuse_base))
			return PTR_ERR(fuse_base);

		if ((readl(fuse_base) & 0xc0000000) == 0xc0000000)
			workaround_needed = false;
	}

	dev_dbg(bgp->dev, "Work around %sneeded\n",
		workaround_needed ? "" : "not ");

	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pm_runtime_put_noidle(dev);
		pm_runtime_disable(dev);
		return ret;
	}

	 
	val = readl(bgp->base + K3_VTM_DEVINFO_PWR0_OFFSET);
	cnt = val & K3_VTM_DEVINFO_PWR0_TEMPSENS_CT_MASK;
	cnt >>= __ffs(K3_VTM_DEVINFO_PWR0_TEMPSENS_CT_MASK);

	data = devm_kcalloc(bgp->dev, cnt, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	ref_table = kzalloc(sizeof(*ref_table) * TABLE_SIZE, GFP_KERNEL);
	if (!ref_table) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	derived_table = devm_kzalloc(bgp->dev, sizeof(*derived_table) * TABLE_SIZE,
				     GFP_KERNEL);
	if (!derived_table) {
		ret = -ENOMEM;
		goto err_free_ref_table;
	}

	if (!workaround_needed)
		init_table(5, ref_table, golden_factors);
	else
		init_table(3, ref_table, pvt_wa_factors);

	 
	for (id = 0; id < cnt; id++) {
		data[id].bgp = bgp;
		data[id].ctrl_offset = K3_VTM_TMPSENS0_CTRL_OFFSET + id * 0x20;
		data[id].stat_offset = data[id].ctrl_offset +
					K3_VTM_TMPSENS_STAT_OFFSET;

		if (workaround_needed) {
			 
			err_vals.refs[0] = MINUS40CREF;
			err_vals.refs[1] = PLUS30CREF;
			err_vals.refs[2] = PLUS125CREF;
			err_vals.refs[3] = PLUS150CREF;
			get_efuse_values(id, &data[id], err_vals.errs, fuse_base);
		}

		if (id == 0 && workaround_needed)
			prep_lookup_table(&err_vals, ref_table);
		else if (id == 0 && !workaround_needed)
			memcpy(derived_table, ref_table, TABLE_SIZE * 4);

		val = readl(data[id].bgp->cfg2_base + data[id].ctrl_offset);
		val |= (K3_VTM_TMPSENS_CTRL_MAXT_OUTRG_EN |
			K3_VTM_TMPSENS_CTRL_SOC |
			K3_VTM_TMPSENS_CTRL_CLRZ | BIT(4));
		writel(val, data[id].bgp->cfg2_base + data[id].ctrl_offset);

		bgp->ts_data[id] = &data[id];
		ti_thermal = devm_thermal_of_zone_register(bgp->dev, id, &data[id],
							   &k3_of_thermal_ops);
		if (IS_ERR(ti_thermal)) {
			dev_err(bgp->dev, "thermal zone device is NULL\n");
			ret = PTR_ERR(ti_thermal);
			goto err_free_ref_table;
		}
	}

	 
	high_max = k3_j72xx_bandgap_temp_to_adc_code(MAX_TEMP);
	low_temp = k3_j72xx_bandgap_temp_to_adc_code(COOL_DOWN_TEMP);

	writel((low_temp << 16) | high_max, data[0].bgp->cfg2_base +
	       K3_VTM_MISC_CTRL2_OFFSET);
	mdelay(100);
	writel(K3_VTM_ANYMAXT_OUTRG_ALERT_EN, data[0].bgp->cfg2_base +
	       K3_VTM_MISC_CTRL_OFFSET);

	print_look_up_table(dev, ref_table);
	 
	kfree(ref_table);

	return 0;

err_free_ref_table:
	kfree(ref_table);

err_alloc:
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int k3_j72xx_bandgap_remove(struct platform_device *pdev)
{
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct k3_j72xx_bandgap_data k3_j72xx_bandgap_j721e_data = {
	.has_errata_i2128 = true,
};

static const struct k3_j72xx_bandgap_data k3_j72xx_bandgap_j7200_data = {
	.has_errata_i2128 = false,
};

static const struct of_device_id of_k3_j72xx_bandgap_match[] = {
	{
		.compatible = "ti,j721e-vtm",
		.data = &k3_j72xx_bandgap_j721e_data,
	},
	{
		.compatible = "ti,j7200-vtm",
		.data = &k3_j72xx_bandgap_j7200_data,
	},
	{   },
};
MODULE_DEVICE_TABLE(of, of_k3_j72xx_bandgap_match);

static struct platform_driver k3_j72xx_bandgap_sensor_driver = {
	.probe = k3_j72xx_bandgap_probe,
	.remove = k3_j72xx_bandgap_remove,
	.driver = {
		.name = "k3-j72xx-soc-thermal",
		.of_match_table	= of_k3_j72xx_bandgap_match,
	},
};

module_platform_driver(k3_j72xx_bandgap_sensor_driver);

MODULE_DESCRIPTION("K3 bandgap temperature sensor driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("J Keerthy <j-keerthy@ti.com>");
