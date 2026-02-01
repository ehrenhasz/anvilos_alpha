
 

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/thermal.h>
#include <dt-bindings/thermal/mediatek,lvts-thermal.h>

#include "../thermal_hwmon.h"

#define LVTS_MONCTL0(__base)	(__base + 0x0000)
#define LVTS_MONCTL1(__base)	(__base + 0x0004)
#define LVTS_MONCTL2(__base)	(__base + 0x0008)
#define LVTS_MONINT(__base)		(__base + 0x000C)
#define LVTS_MONINTSTS(__base)	(__base + 0x0010)
#define LVTS_MONIDET0(__base)	(__base + 0x0014)
#define LVTS_MONIDET1(__base)	(__base + 0x0018)
#define LVTS_MONIDET2(__base)	(__base + 0x001C)
#define LVTS_MONIDET3(__base)	(__base + 0x0020)
#define LVTS_H2NTHRE(__base)	(__base + 0x0024)
#define LVTS_HTHRE(__base)		(__base + 0x0028)
#define LVTS_OFFSETH(__base)	(__base + 0x0030)
#define LVTS_OFFSETL(__base)	(__base + 0x0034)
#define LVTS_MSRCTL0(__base)	(__base + 0x0038)
#define LVTS_MSRCTL1(__base)	(__base + 0x003C)
#define LVTS_TSSEL(__base)		(__base + 0x0040)
#define LVTS_CALSCALE(__base)	(__base + 0x0048)
#define LVTS_ID(__base)			(__base + 0x004C)
#define LVTS_CONFIG(__base)		(__base + 0x0050)
#define LVTS_EDATA00(__base)	(__base + 0x0054)
#define LVTS_EDATA01(__base)	(__base + 0x0058)
#define LVTS_EDATA02(__base)	(__base + 0x005C)
#define LVTS_EDATA03(__base)	(__base + 0x0060)
#define LVTS_MSR0(__base)		(__base + 0x0090)
#define LVTS_MSR1(__base)		(__base + 0x0094)
#define LVTS_MSR2(__base)		(__base + 0x0098)
#define LVTS_MSR3(__base)		(__base + 0x009C)
#define LVTS_IMMD0(__base)		(__base + 0x00A0)
#define LVTS_IMMD1(__base)		(__base + 0x00A4)
#define LVTS_IMMD2(__base)		(__base + 0x00A8)
#define LVTS_IMMD3(__base)		(__base + 0x00AC)
#define LVTS_PROTCTL(__base)	(__base + 0x00C0)
#define LVTS_PROTTA(__base)		(__base + 0x00C4)
#define LVTS_PROTTB(__base)		(__base + 0x00C8)
#define LVTS_PROTTC(__base)		(__base + 0x00CC)
#define LVTS_CLKEN(__base)		(__base + 0x00E4)

#define LVTS_PERIOD_UNIT			0
#define LVTS_GROUP_INTERVAL			0
#define LVTS_FILTER_INTERVAL		0
#define LVTS_SENSOR_INTERVAL		0
#define LVTS_HW_FILTER				0x0
#define LVTS_TSSEL_CONF				0x13121110
#define LVTS_CALSCALE_CONF			0x300
#define LVTS_MONINT_CONF			0x8300318C

#define LVTS_MONINT_OFFSET_SENSOR0		0xC
#define LVTS_MONINT_OFFSET_SENSOR1		0x180
#define LVTS_MONINT_OFFSET_SENSOR2		0x3000
#define LVTS_MONINT_OFFSET_SENSOR3		0x3000000

#define LVTS_INT_SENSOR0			0x0009001F
#define LVTS_INT_SENSOR1			0x001203E0
#define LVTS_INT_SENSOR2			0x00247C00
#define LVTS_INT_SENSOR3			0x1FC00000

#define LVTS_SENSOR_MAX				4
#define LVTS_GOLDEN_TEMP_MAX		62
#define LVTS_GOLDEN_TEMP_DEFAULT	50
#define LVTS_COEFF_A				-250460
#define LVTS_COEFF_B				250460

#define LVTS_MSR_IMMEDIATE_MODE		0
#define LVTS_MSR_FILTERED_MODE		1

#define LVTS_MSR_READ_TIMEOUT_US	400
#define LVTS_MSR_READ_WAIT_US		(LVTS_MSR_READ_TIMEOUT_US / 2)

#define LVTS_HW_SHUTDOWN_MT8195		105000

#define LVTS_MINIMUM_THRESHOLD		20000

static int golden_temp = LVTS_GOLDEN_TEMP_DEFAULT;
static int coeff_b = LVTS_COEFF_B;

struct lvts_sensor_data {
	int dt_id;
};

struct lvts_ctrl_data {
	struct lvts_sensor_data lvts_sensor[LVTS_SENSOR_MAX];
	int cal_offset[LVTS_SENSOR_MAX];
	int hw_tshut_temp;
	int num_lvts_sensor;
	int offset;
	int mode;
};

struct lvts_data {
	const struct lvts_ctrl_data *lvts_ctrl;
	int num_lvts_ctrl;
};

struct lvts_sensor {
	struct thermal_zone_device *tz;
	void __iomem *msr;
	void __iomem *base;
	int id;
	int dt_id;
	int low_thresh;
	int high_thresh;
};

struct lvts_ctrl {
	struct lvts_sensor sensors[LVTS_SENSOR_MAX];
	u32 calibration[LVTS_SENSOR_MAX];
	u32 hw_tshut_raw_temp;
	int num_lvts_sensor;
	int mode;
	void __iomem *base;
	int low_thresh;
	int high_thresh;
};

struct lvts_domain {
	struct lvts_ctrl *lvts_ctrl;
	struct reset_control *reset;
	struct clk *clk;
	int num_lvts_ctrl;
	void __iomem *base;
	size_t calib_len;
	u8 *calib;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dom_dentry;
#endif
};

#ifdef CONFIG_MTK_LVTS_THERMAL_DEBUGFS

#define LVTS_DEBUG_FS_REGS(__reg)		\
{						\
	.name = __stringify(__reg),		\
	.offset = __reg(0),			\
}

static const struct debugfs_reg32 lvts_regs[] = {
	LVTS_DEBUG_FS_REGS(LVTS_MONCTL0),
	LVTS_DEBUG_FS_REGS(LVTS_MONCTL1),
	LVTS_DEBUG_FS_REGS(LVTS_MONCTL2),
	LVTS_DEBUG_FS_REGS(LVTS_MONINT),
	LVTS_DEBUG_FS_REGS(LVTS_MONINTSTS),
	LVTS_DEBUG_FS_REGS(LVTS_MONIDET0),
	LVTS_DEBUG_FS_REGS(LVTS_MONIDET1),
	LVTS_DEBUG_FS_REGS(LVTS_MONIDET2),
	LVTS_DEBUG_FS_REGS(LVTS_MONIDET3),
	LVTS_DEBUG_FS_REGS(LVTS_H2NTHRE),
	LVTS_DEBUG_FS_REGS(LVTS_HTHRE),
	LVTS_DEBUG_FS_REGS(LVTS_OFFSETH),
	LVTS_DEBUG_FS_REGS(LVTS_OFFSETL),
	LVTS_DEBUG_FS_REGS(LVTS_MSRCTL0),
	LVTS_DEBUG_FS_REGS(LVTS_MSRCTL1),
	LVTS_DEBUG_FS_REGS(LVTS_TSSEL),
	LVTS_DEBUG_FS_REGS(LVTS_CALSCALE),
	LVTS_DEBUG_FS_REGS(LVTS_ID),
	LVTS_DEBUG_FS_REGS(LVTS_CONFIG),
	LVTS_DEBUG_FS_REGS(LVTS_EDATA00),
	LVTS_DEBUG_FS_REGS(LVTS_EDATA01),
	LVTS_DEBUG_FS_REGS(LVTS_EDATA02),
	LVTS_DEBUG_FS_REGS(LVTS_EDATA03),
	LVTS_DEBUG_FS_REGS(LVTS_MSR0),
	LVTS_DEBUG_FS_REGS(LVTS_MSR1),
	LVTS_DEBUG_FS_REGS(LVTS_MSR2),
	LVTS_DEBUG_FS_REGS(LVTS_MSR3),
	LVTS_DEBUG_FS_REGS(LVTS_IMMD0),
	LVTS_DEBUG_FS_REGS(LVTS_IMMD1),
	LVTS_DEBUG_FS_REGS(LVTS_IMMD2),
	LVTS_DEBUG_FS_REGS(LVTS_IMMD3),
	LVTS_DEBUG_FS_REGS(LVTS_PROTCTL),
	LVTS_DEBUG_FS_REGS(LVTS_PROTTA),
	LVTS_DEBUG_FS_REGS(LVTS_PROTTB),
	LVTS_DEBUG_FS_REGS(LVTS_PROTTC),
	LVTS_DEBUG_FS_REGS(LVTS_CLKEN),
};

static int lvts_debugfs_init(struct device *dev, struct lvts_domain *lvts_td)
{
	struct debugfs_regset32 *regset;
	struct lvts_ctrl *lvts_ctrl;
	struct dentry *dentry;
	char name[64];
	int i;

	lvts_td->dom_dentry = debugfs_create_dir(dev_name(dev), NULL);
	if (IS_ERR(lvts_td->dom_dentry))
		return 0;

	for (i = 0; i < lvts_td->num_lvts_ctrl; i++) {

		lvts_ctrl = &lvts_td->lvts_ctrl[i];

		sprintf(name, "controller%d", i);
		dentry = debugfs_create_dir(name, lvts_td->dom_dentry);
		if (!dentry)
			continue;

		regset = devm_kzalloc(dev, sizeof(*regset), GFP_KERNEL);
		if (!regset)
			continue;

		regset->base = lvts_ctrl->base;
		regset->regs = lvts_regs;
		regset->nregs = ARRAY_SIZE(lvts_regs);

		debugfs_create_regset32("registers", 0400, dentry, regset);
	}

	return 0;
}

static void lvts_debugfs_exit(struct lvts_domain *lvts_td)
{
	debugfs_remove_recursive(lvts_td->dom_dentry);
}

#else

static inline int lvts_debugfs_init(struct device *dev,
				    struct lvts_domain *lvts_td)
{
	return 0;
}

static void lvts_debugfs_exit(struct lvts_domain *lvts_td) { }

#endif

static int lvts_raw_to_temp(u32 raw_temp)
{
	int temperature;

	temperature = ((s64)(raw_temp & 0xFFFF) * LVTS_COEFF_A) >> 14;
	temperature += coeff_b;

	return temperature;
}

static u32 lvts_temp_to_raw(int temperature)
{
	u32 raw_temp = ((s64)(coeff_b - temperature)) << 14;

	raw_temp = div_s64(raw_temp, -LVTS_COEFF_A);

	return raw_temp;
}

static int lvts_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct lvts_sensor *lvts_sensor = thermal_zone_device_priv(tz);
	void __iomem *msr = lvts_sensor->msr;
	u32 value;
	int rc;

	 
	rc = readl_poll_timeout(msr, value, value & BIT(16),
				LVTS_MSR_READ_WAIT_US, LVTS_MSR_READ_TIMEOUT_US);

	 
	if (rc)
		return -EAGAIN;

	*temp = lvts_raw_to_temp(value & 0xFFFF);

	return 0;
}

static void lvts_update_irq_mask(struct lvts_ctrl *lvts_ctrl)
{
	u32 masks[] = {
		LVTS_MONINT_OFFSET_SENSOR0,
		LVTS_MONINT_OFFSET_SENSOR1,
		LVTS_MONINT_OFFSET_SENSOR2,
		LVTS_MONINT_OFFSET_SENSOR3,
	};
	u32 value = 0;
	int i;

	value = readl(LVTS_MONINT(lvts_ctrl->base));

	for (i = 0; i < ARRAY_SIZE(masks); i++) {
		if (lvts_ctrl->sensors[i].high_thresh == lvts_ctrl->high_thresh
		    && lvts_ctrl->sensors[i].low_thresh == lvts_ctrl->low_thresh)
			value |= masks[i];
		else
			value &= ~masks[i];
	}

	writel(value, LVTS_MONINT(lvts_ctrl->base));
}

static bool lvts_should_update_thresh(struct lvts_ctrl *lvts_ctrl, int high)
{
	int i;

	if (high > lvts_ctrl->high_thresh)
		return true;

	for (i = 0; i < lvts_ctrl->num_lvts_sensor; i++)
		if (lvts_ctrl->sensors[i].high_thresh == lvts_ctrl->high_thresh
		    && lvts_ctrl->sensors[i].low_thresh == lvts_ctrl->low_thresh)
			return false;

	return true;
}

static int lvts_set_trips(struct thermal_zone_device *tz, int low, int high)
{
	struct lvts_sensor *lvts_sensor = thermal_zone_device_priv(tz);
	struct lvts_ctrl *lvts_ctrl = container_of(lvts_sensor, struct lvts_ctrl, sensors[lvts_sensor->id]);
	void __iomem *base = lvts_sensor->base;
	u32 raw_low = lvts_temp_to_raw(low != -INT_MAX ? low : LVTS_MINIMUM_THRESHOLD);
	u32 raw_high = lvts_temp_to_raw(high);
	bool should_update_thresh;

	lvts_sensor->low_thresh = low;
	lvts_sensor->high_thresh = high;

	should_update_thresh = lvts_should_update_thresh(lvts_ctrl, high);
	if (should_update_thresh) {
		lvts_ctrl->high_thresh = high;
		lvts_ctrl->low_thresh = low;
	}
	lvts_update_irq_mask(lvts_ctrl);

	if (!should_update_thresh)
		return 0;

	 
	pr_debug("%s: Setting low limit temperature interrupt: %d\n",
		 thermal_zone_device_type(tz), low);
	writel(raw_low, LVTS_OFFSETL(base));

	 
	pr_debug("%s: Setting high limit temperature interrupt: %d\n",
		 thermal_zone_device_type(tz), high);
	writel(raw_high, LVTS_OFFSETH(base));

	return 0;
}

static irqreturn_t lvts_ctrl_irq_handler(struct lvts_ctrl *lvts_ctrl)
{
	irqreturn_t iret = IRQ_NONE;
	u32 value;
	u32 masks[] = {
		LVTS_INT_SENSOR0,
		LVTS_INT_SENSOR1,
		LVTS_INT_SENSOR2,
		LVTS_INT_SENSOR3
	};
	int i;

	 
	value = readl(LVTS_MONINTSTS(lvts_ctrl->base));

	 
	for (i = 0; i < ARRAY_SIZE(masks); i++) {

		if (!(value & masks[i]))
			continue;

		thermal_zone_device_update(lvts_ctrl->sensors[i].tz,
					   THERMAL_TRIP_VIOLATED);
		iret = IRQ_HANDLED;
	}

	 
	writel(value, LVTS_MONINTSTS(lvts_ctrl->base));

	return iret;
}

 
static irqreturn_t lvts_irq_handler(int irq, void *data)
{
	struct lvts_domain *lvts_td = data;
	irqreturn_t aux, iret = IRQ_NONE;
	int i;

	for (i = 0; i < lvts_td->num_lvts_ctrl; i++) {

		aux = lvts_ctrl_irq_handler(&lvts_td->lvts_ctrl[i]);
		if (aux != IRQ_HANDLED)
			continue;

		iret = IRQ_HANDLED;
	}

	return iret;
}

static struct thermal_zone_device_ops lvts_ops = {
	.get_temp = lvts_get_temp,
	.set_trips = lvts_set_trips,
};

static int lvts_sensor_init(struct device *dev, struct lvts_ctrl *lvts_ctrl,
					const struct lvts_ctrl_data *lvts_ctrl_data)
{
	struct lvts_sensor *lvts_sensor = lvts_ctrl->sensors;
	void __iomem *msr_regs[] = {
		LVTS_MSR0(lvts_ctrl->base),
		LVTS_MSR1(lvts_ctrl->base),
		LVTS_MSR2(lvts_ctrl->base),
		LVTS_MSR3(lvts_ctrl->base)
	};

	void __iomem *imm_regs[] = {
		LVTS_IMMD0(lvts_ctrl->base),
		LVTS_IMMD1(lvts_ctrl->base),
		LVTS_IMMD2(lvts_ctrl->base),
		LVTS_IMMD3(lvts_ctrl->base)
	};

	int i;

	for (i = 0; i < lvts_ctrl_data->num_lvts_sensor; i++) {

		int dt_id = lvts_ctrl_data->lvts_sensor[i].dt_id;

		 
		lvts_sensor[i].id = i;

		 
		lvts_sensor[i].dt_id = dt_id;

		 
		lvts_sensor[i].base = lvts_ctrl->base;

		 
		lvts_sensor[i].msr = lvts_ctrl_data->mode == LVTS_MSR_IMMEDIATE_MODE ?
			imm_regs[i] : msr_regs[i];

		lvts_sensor[i].low_thresh = INT_MIN;
		lvts_sensor[i].high_thresh = INT_MIN;
	};

	lvts_ctrl->num_lvts_sensor = lvts_ctrl_data->num_lvts_sensor;

	return 0;
}

 
static int lvts_calibration_init(struct device *dev, struct lvts_ctrl *lvts_ctrl,
					const struct lvts_ctrl_data *lvts_ctrl_data,
					u8 *efuse_calibration)
{
	int i;

	for (i = 0; i < lvts_ctrl_data->num_lvts_sensor; i++)
		memcpy(&lvts_ctrl->calibration[i],
		       efuse_calibration + lvts_ctrl_data->cal_offset[i], 2);

	return 0;
}

 
static int lvts_calibration_read(struct device *dev, struct lvts_domain *lvts_td,
					const struct lvts_data *lvts_data)
{
	struct device_node *np = dev_of_node(dev);
	struct nvmem_cell *cell;
	struct property *prop;
	const char *cell_name;

	of_property_for_each_string(np, "nvmem-cell-names", prop, cell_name) {
		size_t len;
		u8 *efuse;

		cell = of_nvmem_cell_get(np, cell_name);
		if (IS_ERR(cell)) {
			dev_err(dev, "Failed to get cell '%s'\n", cell_name);
			return PTR_ERR(cell);
		}

		efuse = nvmem_cell_read(cell, &len);

		nvmem_cell_put(cell);

		if (IS_ERR(efuse)) {
			dev_err(dev, "Failed to read cell '%s'\n", cell_name);
			return PTR_ERR(efuse);
		}

		lvts_td->calib = devm_krealloc(dev, lvts_td->calib,
					       lvts_td->calib_len + len, GFP_KERNEL);
		if (!lvts_td->calib)
			return -ENOMEM;

		memcpy(lvts_td->calib + lvts_td->calib_len, efuse, len);

		lvts_td->calib_len += len;

		kfree(efuse);
	}

	return 0;
}

static int lvts_golden_temp_init(struct device *dev, u32 *value)
{
	u32 gt;

	gt = (*value) >> 24;

	if (gt && gt < LVTS_GOLDEN_TEMP_MAX)
		golden_temp = gt;

	coeff_b = golden_temp * 500 + LVTS_COEFF_B;

	return 0;
}

static int lvts_ctrl_init(struct device *dev, struct lvts_domain *lvts_td,
					const struct lvts_data *lvts_data)
{
	size_t size = sizeof(*lvts_td->lvts_ctrl) * lvts_data->num_lvts_ctrl;
	struct lvts_ctrl *lvts_ctrl;
	int i, ret;

	 
	ret = lvts_calibration_read(dev, lvts_td, lvts_data);
	if (ret)
		return ret;

	 
	ret = lvts_golden_temp_init(dev, (u32 *)lvts_td->calib);
	if (ret)
		return ret;

	lvts_ctrl = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!lvts_ctrl)
		return -ENOMEM;

	for (i = 0; i < lvts_data->num_lvts_ctrl; i++) {

		lvts_ctrl[i].base = lvts_td->base + lvts_data->lvts_ctrl[i].offset;

		ret = lvts_sensor_init(dev, &lvts_ctrl[i],
				       &lvts_data->lvts_ctrl[i]);
		if (ret)
			return ret;

		ret = lvts_calibration_init(dev, &lvts_ctrl[i],
					    &lvts_data->lvts_ctrl[i],
					    lvts_td->calib);
		if (ret)
			return ret;

		 
		lvts_ctrl[i].mode = lvts_data->lvts_ctrl[i].mode;

		 
		lvts_ctrl[i].hw_tshut_raw_temp =
			lvts_temp_to_raw(lvts_data->lvts_ctrl[i].hw_tshut_temp);

		lvts_ctrl[i].low_thresh = INT_MIN;
		lvts_ctrl[i].high_thresh = INT_MIN;
	}

	 
	devm_kfree(dev, lvts_td->calib);

	lvts_td->lvts_ctrl = lvts_ctrl;
	lvts_td->num_lvts_ctrl = lvts_data->num_lvts_ctrl;

	return 0;
}

 
static void lvts_write_config(struct lvts_ctrl *lvts_ctrl, u32 *cmds, int nr_cmds)
{
	int i;

	 
	for (i = 0; i < nr_cmds; i++) {
		writel(cmds[i], LVTS_CONFIG(lvts_ctrl->base));
		usleep_range(2, 4);
	}
}

static int lvts_irq_init(struct lvts_ctrl *lvts_ctrl)
{
	 
	writel(BIT(16), LVTS_PROTCTL(lvts_ctrl->base));

	 
	writel(lvts_ctrl->hw_tshut_raw_temp, LVTS_PROTTC(lvts_ctrl->base));

	 
	writel(LVTS_MONINT_CONF, LVTS_MONINT(lvts_ctrl->base));

	return 0;
}

static int lvts_domain_reset(struct device *dev, struct reset_control *reset)
{
	int ret;

	ret = reset_control_assert(reset);
	if (ret)
		return ret;

	return reset_control_deassert(reset);
}

 
static int lvts_ctrl_set_enable(struct lvts_ctrl *lvts_ctrl, int enable)
{
	 
	writel(enable, LVTS_CLKEN(lvts_ctrl->base));

	return 0;
}

static int lvts_ctrl_connect(struct device *dev, struct lvts_ctrl *lvts_ctrl)
{
	u32 id, cmds[] = { 0xC103FFFF, 0xC502FF55 };

	lvts_write_config(lvts_ctrl, cmds, ARRAY_SIZE(cmds));

	 
	id = readl(LVTS_ID(lvts_ctrl->base));
	if (!(id & BIT(7)))
		return -EIO;

	return 0;
}

static int lvts_ctrl_initialize(struct device *dev, struct lvts_ctrl *lvts_ctrl)
{
	 
	u32 cmds[] = {
		0xC1030E01, 0xC1030CFC, 0xC1030A8C, 0xC103098D, 0xC10308F1,
		0xC10307A6, 0xC10306B8, 0xC1030500, 0xC1030420, 0xC1030300,
		0xC1030030, 0xC10300F6, 0xC1030050, 0xC1030060, 0xC10300AC,
		0xC10300FC, 0xC103009D, 0xC10300F1, 0xC10300E1
	};

	lvts_write_config(lvts_ctrl, cmds, ARRAY_SIZE(cmds));

	return 0;
}

static int lvts_ctrl_calibrate(struct device *dev, struct lvts_ctrl *lvts_ctrl)
{
	int i;
	void __iomem *lvts_edata[] = {
		LVTS_EDATA00(lvts_ctrl->base),
		LVTS_EDATA01(lvts_ctrl->base),
		LVTS_EDATA02(lvts_ctrl->base),
		LVTS_EDATA03(lvts_ctrl->base)
	};

	 
	for (i = 0; i < LVTS_SENSOR_MAX; i++)
		writel(lvts_ctrl->calibration[i], lvts_edata[i]);

	return 0;
}

static int lvts_ctrl_configure(struct device *dev, struct lvts_ctrl *lvts_ctrl)
{
	u32 value;

	 
	value = LVTS_TSSEL_CONF;
	writel(value, LVTS_TSSEL(lvts_ctrl->base));

	 
	value = 0x300;
	value = LVTS_CALSCALE_CONF;

	 
	value = LVTS_HW_FILTER << 9 |  LVTS_HW_FILTER << 6 |
			LVTS_HW_FILTER << 3 | LVTS_HW_FILTER;
	writel(value, LVTS_MSRCTL0(lvts_ctrl->base));

	 
	value = LVTS_GROUP_INTERVAL << 20 | LVTS_PERIOD_UNIT;
	writel(value, LVTS_MONCTL1(lvts_ctrl->base));

	 
	value = LVTS_FILTER_INTERVAL << 16 | LVTS_SENSOR_INTERVAL;
	writel(value, LVTS_MONCTL2(lvts_ctrl->base));

	return lvts_irq_init(lvts_ctrl);
}

static int lvts_ctrl_start(struct device *dev, struct lvts_ctrl *lvts_ctrl)
{
	struct lvts_sensor *lvts_sensors = lvts_ctrl->sensors;
	struct thermal_zone_device *tz;
	u32 sensor_map = 0;
	int i;
	 
	u32 sensor_imm_bitmap[] = { BIT(4), BIT(5), BIT(6), BIT(9) };
	u32 sensor_filt_bitmap[] = { BIT(0), BIT(1), BIT(2), BIT(3) };

	u32 *sensor_bitmap = lvts_ctrl->mode == LVTS_MSR_IMMEDIATE_MODE ?
			     sensor_imm_bitmap : sensor_filt_bitmap;

	for (i = 0; i < lvts_ctrl->num_lvts_sensor; i++) {

		int dt_id = lvts_sensors[i].dt_id;

		tz = devm_thermal_of_zone_register(dev, dt_id, &lvts_sensors[i],
						   &lvts_ops);
		if (IS_ERR(tz)) {
			 
			if (PTR_ERR(tz) == -ENODEV)
				continue;

			return PTR_ERR(tz);
		}

		devm_thermal_add_hwmon_sysfs(dev, tz);

		 
		lvts_sensors[i].tz = tz;

		 
		sensor_map |= sensor_bitmap[i];
	}

	 
	if (lvts_ctrl->mode == LVTS_MSR_IMMEDIATE_MODE) {
		 
		writel(sensor_map, LVTS_MSRCTL1(lvts_ctrl->base));
	} else {
		 
		writel(sensor_map | BIT(9), LVTS_MONCTL0(lvts_ctrl->base));
	}

	return 0;
}

static int lvts_domain_init(struct device *dev, struct lvts_domain *lvts_td,
					const struct lvts_data *lvts_data)
{
	struct lvts_ctrl *lvts_ctrl;
	int i, ret;

	ret = lvts_ctrl_init(dev, lvts_td, lvts_data);
	if (ret)
		return ret;

	ret = lvts_domain_reset(dev, lvts_td->reset);
	if (ret) {
		dev_dbg(dev, "Failed to reset domain");
		return ret;
	}

	for (i = 0; i < lvts_td->num_lvts_ctrl; i++) {

		lvts_ctrl = &lvts_td->lvts_ctrl[i];

		 
		ret = lvts_ctrl_set_enable(lvts_ctrl, true);
		if (ret) {
			dev_dbg(dev, "Failed to enable LVTS clock");
			return ret;
		}

		ret = lvts_ctrl_connect(dev, lvts_ctrl);
		if (ret) {
			dev_dbg(dev, "Failed to connect to LVTS controller");
			return ret;
		}

		ret = lvts_ctrl_initialize(dev, lvts_ctrl);
		if (ret) {
			dev_dbg(dev, "Failed to initialize controller");
			return ret;
		}

		ret = lvts_ctrl_calibrate(dev, lvts_ctrl);
		if (ret) {
			dev_dbg(dev, "Failed to calibrate controller");
			return ret;
		}

		ret = lvts_ctrl_configure(dev, lvts_ctrl);
		if (ret) {
			dev_dbg(dev, "Failed to configure controller");
			return ret;
		}

		ret = lvts_ctrl_start(dev, lvts_ctrl);
		if (ret) {
			dev_dbg(dev, "Failed to start controller");
			return ret;
		}
	}

	return lvts_debugfs_init(dev, lvts_td);
}

static int lvts_probe(struct platform_device *pdev)
{
	const struct lvts_data *lvts_data;
	struct lvts_domain *lvts_td;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, ret;

	lvts_td = devm_kzalloc(dev, sizeof(*lvts_td), GFP_KERNEL);
	if (!lvts_td)
		return -ENOMEM;

	lvts_data = of_device_get_match_data(dev);

	lvts_td->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(lvts_td->clk))
		return dev_err_probe(dev, PTR_ERR(lvts_td->clk), "Failed to retrieve clock\n");

	res = platform_get_mem_or_io(pdev, 0);
	if (!res)
		return dev_err_probe(dev, (-ENXIO), "No IO resource\n");

	lvts_td->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(lvts_td->base))
		return dev_err_probe(dev, PTR_ERR(lvts_td->base), "Failed to map io resource\n");

	lvts_td->reset = devm_reset_control_get_by_index(dev, 0);
	if (IS_ERR(lvts_td->reset))
		return dev_err_probe(dev, PTR_ERR(lvts_td->reset), "Failed to get reset control\n");

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = lvts_domain_init(dev, lvts_td, lvts_data);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to initialize the lvts domain\n");

	 
	ret = devm_request_threaded_irq(dev, irq, NULL, lvts_irq_handler,
					IRQF_ONESHOT, dev_name(dev), lvts_td);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to request interrupt\n");

	platform_set_drvdata(pdev, lvts_td);

	return 0;
}

static int lvts_remove(struct platform_device *pdev)
{
	struct lvts_domain *lvts_td;
	int i;

	lvts_td = platform_get_drvdata(pdev);

	for (i = 0; i < lvts_td->num_lvts_ctrl; i++)
		lvts_ctrl_set_enable(&lvts_td->lvts_ctrl[i], false);

	lvts_debugfs_exit(lvts_td);

	return 0;
}

static const struct lvts_ctrl_data mt8195_lvts_mcu_data_ctrl[] = {
	{
		.cal_offset = { 0x04, 0x07 },
		.lvts_sensor = {
			{ .dt_id = MT8195_MCU_BIG_CPU0 },
			{ .dt_id = MT8195_MCU_BIG_CPU1 }
		},
		.num_lvts_sensor = 2,
		.offset = 0x0,
		.hw_tshut_temp = LVTS_HW_SHUTDOWN_MT8195,
	},
	{
		.cal_offset = { 0x0d, 0x10 },
		.lvts_sensor = {
			{ .dt_id = MT8195_MCU_BIG_CPU2 },
			{ .dt_id = MT8195_MCU_BIG_CPU3 }
		},
		.num_lvts_sensor = 2,
		.offset = 0x100,
		.hw_tshut_temp = LVTS_HW_SHUTDOWN_MT8195,
	},
	{
		.cal_offset = { 0x16, 0x19, 0x1c, 0x1f },
		.lvts_sensor = {
			{ .dt_id = MT8195_MCU_LITTLE_CPU0 },
			{ .dt_id = MT8195_MCU_LITTLE_CPU1 },
			{ .dt_id = MT8195_MCU_LITTLE_CPU2 },
			{ .dt_id = MT8195_MCU_LITTLE_CPU3 }
		},
		.num_lvts_sensor = 4,
		.offset = 0x200,
		.hw_tshut_temp = LVTS_HW_SHUTDOWN_MT8195,
	}
};

static const struct lvts_ctrl_data mt8195_lvts_ap_data_ctrl[] = {
		{
		.cal_offset = { 0x25, 0x28 },
		.lvts_sensor = {
			{ .dt_id = MT8195_AP_VPU0 },
			{ .dt_id = MT8195_AP_VPU1 }
		},
		.num_lvts_sensor = 2,
		.offset = 0x0,
		.hw_tshut_temp = LVTS_HW_SHUTDOWN_MT8195,
	},
	{
		.cal_offset = { 0x2e, 0x31 },
		.lvts_sensor = {
			{ .dt_id = MT8195_AP_GPU0 },
			{ .dt_id = MT8195_AP_GPU1 }
		},
		.num_lvts_sensor = 2,
		.offset = 0x100,
		.hw_tshut_temp = LVTS_HW_SHUTDOWN_MT8195,
	},
	{
		.cal_offset = { 0x37, 0x3a, 0x3d },
		.lvts_sensor = {
			{ .dt_id = MT8195_AP_VDEC },
			{ .dt_id = MT8195_AP_IMG },
			{ .dt_id = MT8195_AP_INFRA },
		},
		.num_lvts_sensor = 3,
		.offset = 0x200,
		.hw_tshut_temp = LVTS_HW_SHUTDOWN_MT8195,
	},
	{
		.cal_offset = { 0x43, 0x46 },
		.lvts_sensor = {
			{ .dt_id = MT8195_AP_CAM0 },
			{ .dt_id = MT8195_AP_CAM1 }
		},
		.num_lvts_sensor = 2,
		.offset = 0x300,
		.hw_tshut_temp = LVTS_HW_SHUTDOWN_MT8195,
	}
};

static const struct lvts_data mt8195_lvts_mcu_data = {
	.lvts_ctrl	= mt8195_lvts_mcu_data_ctrl,
	.num_lvts_ctrl	= ARRAY_SIZE(mt8195_lvts_mcu_data_ctrl),
};

static const struct lvts_data mt8195_lvts_ap_data = {
	.lvts_ctrl	= mt8195_lvts_ap_data_ctrl,
	.num_lvts_ctrl	= ARRAY_SIZE(mt8195_lvts_ap_data_ctrl),
};

static const struct of_device_id lvts_of_match[] = {
	{ .compatible = "mediatek,mt8195-lvts-mcu", .data = &mt8195_lvts_mcu_data },
	{ .compatible = "mediatek,mt8195-lvts-ap", .data = &mt8195_lvts_ap_data },
	{},
};
MODULE_DEVICE_TABLE(of, lvts_of_match);

static struct platform_driver lvts_driver = {
	.probe = lvts_probe,
	.remove = lvts_remove,
	.driver = {
		.name = "mtk-lvts-thermal",
		.of_match_table = lvts_of_match,
	},
};
module_platform_driver(lvts_driver);

MODULE_AUTHOR("Balsam CHIHI <bchihi@baylibre.com>");
MODULE_DESCRIPTION("MediaTek LVTS Thermal Driver");
MODULE_LICENSE("GPL");
