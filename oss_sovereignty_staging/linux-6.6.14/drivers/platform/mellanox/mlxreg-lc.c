
 

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_data/mlxcpld.h>
#include <linux/platform_data/mlxreg.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

 
#define MLXREG_LC_REG_CPLD1_VER_OFFSET		0x2500
#define MLXREG_LC_REG_FPGA1_VER_OFFSET		0x2501
#define MLXREG_LC_REG_CPLD1_PN_OFFSET		0x2504
#define MLXREG_LC_REG_FPGA1_PN_OFFSET		0x2506
#define MLXREG_LC_REG_RESET_CAUSE_OFFSET	0x251d
#define MLXREG_LC_REG_LED1_OFFSET		0x2520
#define MLXREG_LC_REG_GP0_OFFSET		0x252e
#define MLXREG_LC_REG_FIELD_UPGRADE		0x2534
#define MLXREG_LC_CHANNEL_I2C_REG		0x25dc
#define MLXREG_LC_REG_CPLD1_MVER_OFFSET		0x25de
#define MLXREG_LC_REG_FPGA1_MVER_OFFSET		0x25df
#define MLXREG_LC_REG_MAX_POWER_OFFSET		0x25f1
#define MLXREG_LC_REG_CONFIG_OFFSET		0x25fb
#define MLXREG_LC_REG_MAX			0x3fff

 
enum mlxreg_lc_type {
	MLXREG_LC_SN4800_C16 = 0x0000,
};

 
enum mlxreg_lc_state {
	MLXREG_LC_INITIALIZED = BIT(0),
	MLXREG_LC_POWERED = BIT(1),
	MLXREG_LC_SYNCED = BIT(2),
};

#define MLXREG_LC_CONFIGURED	(MLXREG_LC_INITIALIZED | MLXREG_LC_POWERED | MLXREG_LC_SYNCED)

 
struct mlxreg_lc {
	struct device *dev;
	struct mutex lock;  
	void *par_regmap;
	struct mlxreg_core_data *data;
	struct mlxreg_core_platform_data *io_data;
	struct mlxreg_core_platform_data *led_data;
	struct mlxcpld_mux_plat_data *mux_data;
	struct platform_device *led;
	struct platform_device *io_regs;
	struct i2c_board_info *mux_brdinfo;
	struct platform_device *mux;
	struct mlxreg_hotplug_device *aux_devs;
	int aux_devs_num;
	struct mlxreg_hotplug_device *main_devs;
	int main_devs_num;
	enum mlxreg_lc_state state;
};

static bool mlxreg_lc_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MLXREG_LC_REG_LED1_OFFSET:
	case MLXREG_LC_REG_GP0_OFFSET:
	case MLXREG_LC_REG_FIELD_UPGRADE:
	case MLXREG_LC_CHANNEL_I2C_REG:
		return true;
	}
	return false;
}

static bool mlxreg_lc_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MLXREG_LC_REG_CPLD1_VER_OFFSET:
	case MLXREG_LC_REG_FPGA1_VER_OFFSET:
	case MLXREG_LC_REG_CPLD1_PN_OFFSET:
	case MLXREG_LC_REG_FPGA1_PN_OFFSET:
	case MLXREG_LC_REG_RESET_CAUSE_OFFSET:
	case MLXREG_LC_REG_LED1_OFFSET:
	case MLXREG_LC_REG_GP0_OFFSET:
	case MLXREG_LC_REG_FIELD_UPGRADE:
	case MLXREG_LC_CHANNEL_I2C_REG:
	case MLXREG_LC_REG_CPLD1_MVER_OFFSET:
	case MLXREG_LC_REG_FPGA1_MVER_OFFSET:
	case MLXREG_LC_REG_MAX_POWER_OFFSET:
	case MLXREG_LC_REG_CONFIG_OFFSET:
		return true;
	}
	return false;
}

static bool mlxreg_lc_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MLXREG_LC_REG_CPLD1_VER_OFFSET:
	case MLXREG_LC_REG_FPGA1_VER_OFFSET:
	case MLXREG_LC_REG_CPLD1_PN_OFFSET:
	case MLXREG_LC_REG_FPGA1_PN_OFFSET:
	case MLXREG_LC_REG_RESET_CAUSE_OFFSET:
	case MLXREG_LC_REG_LED1_OFFSET:
	case MLXREG_LC_REG_GP0_OFFSET:
	case MLXREG_LC_REG_FIELD_UPGRADE:
	case MLXREG_LC_CHANNEL_I2C_REG:
	case MLXREG_LC_REG_CPLD1_MVER_OFFSET:
	case MLXREG_LC_REG_FPGA1_MVER_OFFSET:
	case MLXREG_LC_REG_MAX_POWER_OFFSET:
	case MLXREG_LC_REG_CONFIG_OFFSET:
		return true;
	}
	return false;
}

static const struct reg_default mlxreg_lc_regmap_default[] = {
	{ MLXREG_LC_CHANNEL_I2C_REG, 0x00 },
};

 
static const struct regmap_config mlxreg_lc_regmap_conf = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = MLXREG_LC_REG_MAX,
	.cache_type = REGCACHE_FLAT,
	.writeable_reg = mlxreg_lc_writeable_reg,
	.readable_reg = mlxreg_lc_readable_reg,
	.volatile_reg = mlxreg_lc_volatile_reg,
	.reg_defaults = mlxreg_lc_regmap_default,
	.num_reg_defaults = ARRAY_SIZE(mlxreg_lc_regmap_default),
};

 
static int mlxreg_lc_chan[] = {
	0x04, 0x05, 0x06, 0x07, 0x08, 0x10, 0x20, 0x21, 0x22, 0x23, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
	0x4e, 0x4f
};

 
static struct mlxcpld_mux_plat_data mlxreg_lc_mux_data[] = {
	{
		.chan_ids = mlxreg_lc_chan,
		.num_adaps = ARRAY_SIZE(mlxreg_lc_chan),
		.sel_reg_addr = MLXREG_LC_CHANNEL_I2C_REG,
		.reg_size = 2,
	},
};

 
static struct i2c_board_info mlxreg_lc_mux_brdinfo = {
	I2C_BOARD_INFO("i2c-mux-mlxcpld", 0x32),
};

 
static struct i2c_board_info mlxreg_lc_aux_pwr_devices[] = {
	{
		I2C_BOARD_INFO("24c32", 0x51),
	},
	{
		I2C_BOARD_INFO("24c32", 0x51),
	},
};

 
static struct mlxreg_hotplug_device mlxreg_lc_aux_pwr_brdinfo[] = {
	{
		.brdinfo = &mlxreg_lc_aux_pwr_devices[0],
		.nr = 3,
	},
	{
		.brdinfo = &mlxreg_lc_aux_pwr_devices[1],
		.nr = 4,
	},
};

 
static struct i2c_board_info mlxreg_lc_main_pwr_devices[] = {
	{
		I2C_BOARD_INFO("mp2975", 0x62),
	},
	{
		I2C_BOARD_INFO("mp2975", 0x64),
	},
	{
		I2C_BOARD_INFO("max11603", 0x6d),
	},
	{
		I2C_BOARD_INFO("lm25066", 0x15),
	},
};

 
static struct mlxreg_hotplug_device mlxreg_lc_main_pwr_brdinfo[] = {
	{
		.brdinfo = &mlxreg_lc_main_pwr_devices[0],
		.nr = 0,
	},
	{
		.brdinfo = &mlxreg_lc_main_pwr_devices[1],
		.nr = 0,
	},
	{
		.brdinfo = &mlxreg_lc_main_pwr_devices[2],
		.nr = 1,
	},
	{
		.brdinfo = &mlxreg_lc_main_pwr_devices[3],
		.nr = 2,
	},
};

 
static struct mlxreg_core_data mlxreg_lc_led_data[] = {
	{
		.label = "status:green",
		.reg = MLXREG_LC_REG_LED1_OFFSET,
		.mask = GENMASK(7, 4),
	},
	{
		.label = "status:orange",
		.reg = MLXREG_LC_REG_LED1_OFFSET,
		.mask = GENMASK(7, 4),
	},
};

static struct mlxreg_core_platform_data mlxreg_lc_led = {
	.identity = "pci",
	.data = mlxreg_lc_led_data,
	.counter = ARRAY_SIZE(mlxreg_lc_led_data),
};

 
static struct mlxreg_core_data mlxreg_lc_io_data[] = {
	{
		.label = "cpld1_version",
		.reg = MLXREG_LC_REG_CPLD1_VER_OFFSET,
		.bit = GENMASK(7, 0),
		.mode = 0444,
	},
	{
		.label = "fpga1_version",
		.reg = MLXREG_LC_REG_FPGA1_VER_OFFSET,
		.bit = GENMASK(7, 0),
		.mode = 0444,
	},
	{
		.label = "cpld1_pn",
		.reg = MLXREG_LC_REG_CPLD1_PN_OFFSET,
		.bit = GENMASK(15, 0),
		.mode = 0444,
		.regnum = 2,
	},
	{
		.label = "fpga1_pn",
		.reg = MLXREG_LC_REG_FPGA1_PN_OFFSET,
		.bit = GENMASK(15, 0),
		.mode = 0444,
		.regnum = 2,
	},
	{
		.label = "cpld1_version_min",
		.reg = MLXREG_LC_REG_CPLD1_MVER_OFFSET,
		.bit = GENMASK(7, 0),
		.mode = 0444,
	},
	{
		.label = "fpga1_version_min",
		.reg = MLXREG_LC_REG_FPGA1_MVER_OFFSET,
		.bit = GENMASK(7, 0),
		.mode = 0444,
	},
	{
		.label = "reset_fpga_not_done",
		.reg = MLXREG_LC_REG_RESET_CAUSE_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(1),
		.mode = 0444,
	},
	{
		.label = "reset_aux_pwr_or_ref",
		.reg = MLXREG_LC_REG_RESET_CAUSE_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(2),
		.mode = 0444,
	},
	{
		.label = "reset_dc_dc_pwr_fail",
		.reg = MLXREG_LC_REG_RESET_CAUSE_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(3),
		.mode = 0444,
	},
	{
		.label = "reset_from_chassis",
		.reg = MLXREG_LC_REG_RESET_CAUSE_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(4),
		.mode = 0444,
	},
	{
		.label = "reset_pwr_off_from_chassis",
		.reg = MLXREG_LC_REG_RESET_CAUSE_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(5),
		.mode = 0444,
	},
	{
		.label = "reset_line_card",
		.reg = MLXREG_LC_REG_RESET_CAUSE_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(6),
		.mode = 0444,
	},
	{
		.label = "reset_line_card_pwr_en",
		.reg = MLXREG_LC_REG_RESET_CAUSE_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(7),
		.mode = 0444,
	},
	{
		.label = "cpld_upgrade_en",
		.reg = MLXREG_LC_REG_FIELD_UPGRADE,
		.mask = GENMASK(7, 0) & ~BIT(0),
		.mode = 0644,
		.secured = 1,
	},
	{
		.label = "fpga_upgrade_en",
		.reg = MLXREG_LC_REG_FIELD_UPGRADE,
		.mask = GENMASK(7, 0) & ~BIT(1),
		.mode = 0644,
		.secured = 1,
	},
	{
		.label = "qsfp_pwr_en",
		.reg = MLXREG_LC_REG_GP0_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(0),
		.mode = 0644,
	},
	{
		.label = "vpd_wp",
		.reg = MLXREG_LC_REG_GP0_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(3),
		.mode = 0644,
		.secured = 1,
	},
	{
		.label = "agb_spi_burn_en",
		.reg = MLXREG_LC_REG_GP0_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(5),
		.mode = 0644,
		.secured = 1,
	},
	{
		.label = "fpga_spi_burn_en",
		.reg = MLXREG_LC_REG_GP0_OFFSET,
		.mask = GENMASK(7, 0) & ~BIT(6),
		.mode = 0644,
		.secured = 1,
	},
	{
		.label = "max_power",
		.reg = MLXREG_LC_REG_MAX_POWER_OFFSET,
		.bit = GENMASK(15, 0),
		.mode = 0444,
		.regnum = 2,
	},
	{
		.label = "config",
		.reg = MLXREG_LC_REG_CONFIG_OFFSET,
		.bit = GENMASK(15, 0),
		.mode = 0444,
		.regnum = 2,
	},
};

static struct mlxreg_core_platform_data mlxreg_lc_regs_io = {
	.data = mlxreg_lc_io_data,
	.counter = ARRAY_SIZE(mlxreg_lc_io_data),
};

static int
mlxreg_lc_create_static_devices(struct mlxreg_lc *mlxreg_lc, struct mlxreg_hotplug_device *devs,
				int size)
{
	struct mlxreg_hotplug_device *dev = devs;
	int i, ret;

	 
	for (i = 0; i < size; i++, dev++) {
		dev->client = i2c_new_client_device(dev->adapter, dev->brdinfo);
		if (IS_ERR(dev->client)) {
			dev_err(mlxreg_lc->dev, "Failed to create client %s at bus %d at addr 0x%02x\n",
				dev->brdinfo->type, dev->nr, dev->brdinfo->addr);

			dev->adapter = NULL;
			ret = PTR_ERR(dev->client);
			goto fail_create_static_devices;
		}
	}

	return 0;

fail_create_static_devices:
	while (--i >= 0) {
		dev = devs + i;
		i2c_unregister_device(dev->client);
		dev->client = NULL;
	}
	return ret;
}

static void
mlxreg_lc_destroy_static_devices(struct mlxreg_lc *mlxreg_lc, struct mlxreg_hotplug_device *devs,
				 int size)
{
	struct mlxreg_hotplug_device *dev = devs;
	int i;

	 
	for (i = 0; i < size; i++, dev++) {
		if (dev->client) {
			i2c_unregister_device(dev->client);
			dev->client = NULL;
		}
	}
}

static int mlxreg_lc_power_on_off(struct mlxreg_lc *mlxreg_lc, u8 action)
{
	u32 regval;
	int err;

	err = regmap_read(mlxreg_lc->par_regmap, mlxreg_lc->data->reg_pwr, &regval);
	if (err)
		goto regmap_read_fail;

	if (action)
		regval |= BIT(mlxreg_lc->data->slot - 1);
	else
		regval &= ~BIT(mlxreg_lc->data->slot - 1);

	err = regmap_write(mlxreg_lc->par_regmap, mlxreg_lc->data->reg_pwr, regval);

regmap_read_fail:
	return err;
}

static int mlxreg_lc_enable_disable(struct mlxreg_lc *mlxreg_lc, bool action)
{
	u32 regval;
	int err;

	 
	err = regmap_read(mlxreg_lc->par_regmap, mlxreg_lc->data->reg_ena, &regval);
	if (err)
		goto regmap_read_fail;

	if (action)
		regval |= BIT(mlxreg_lc->data->slot - 1);
	else
		regval &= ~BIT(mlxreg_lc->data->slot - 1);

	err = regmap_write(mlxreg_lc->par_regmap, mlxreg_lc->data->reg_ena, regval);

regmap_read_fail:
	return err;
}

static int
mlxreg_lc_sn4800_c16_config_init(struct mlxreg_lc *mlxreg_lc, void *regmap,
				 struct mlxreg_core_data *data)
{
	struct device *dev = &data->hpdev.client->dev;

	 
	mlxreg_lc->mux_data = mlxreg_lc_mux_data;
	mlxreg_lc->io_data = &mlxreg_lc_regs_io;
	mlxreg_lc->led_data = &mlxreg_lc_led;
	mlxreg_lc->mux_brdinfo = &mlxreg_lc_mux_brdinfo;

	mlxreg_lc->aux_devs = devm_kmemdup(dev, mlxreg_lc_aux_pwr_brdinfo,
					   sizeof(mlxreg_lc_aux_pwr_brdinfo), GFP_KERNEL);
	if (!mlxreg_lc->aux_devs)
		return -ENOMEM;
	mlxreg_lc->aux_devs_num = ARRAY_SIZE(mlxreg_lc_aux_pwr_brdinfo);
	mlxreg_lc->main_devs = devm_kmemdup(dev, mlxreg_lc_main_pwr_brdinfo,
					    sizeof(mlxreg_lc_main_pwr_brdinfo), GFP_KERNEL);
	if (!mlxreg_lc->main_devs)
		return -ENOMEM;
	mlxreg_lc->main_devs_num = ARRAY_SIZE(mlxreg_lc_main_pwr_brdinfo);

	return 0;
}

static void
mlxreg_lc_state_update(struct mlxreg_lc *mlxreg_lc, enum mlxreg_lc_state state, u8 action)
{
	if (action)
		mlxreg_lc->state |= state;
	else
		mlxreg_lc->state &= ~state;
}

static void
mlxreg_lc_state_update_locked(struct mlxreg_lc *mlxreg_lc, enum mlxreg_lc_state state, u8 action)
{
	mutex_lock(&mlxreg_lc->lock);

	if (action)
		mlxreg_lc->state |= state;
	else
		mlxreg_lc->state &= ~state;

	mutex_unlock(&mlxreg_lc->lock);
}

 
static int mlxreg_lc_event_handler(void *handle, enum mlxreg_hotplug_kind kind, u8 action)
{
	struct mlxreg_lc *mlxreg_lc = handle;
	int err = 0;

	dev_info(mlxreg_lc->dev, "linecard#%d state %d event kind %d action %d\n",
		 mlxreg_lc->data->slot, mlxreg_lc->state, kind, action);

	mutex_lock(&mlxreg_lc->lock);
	if (!(mlxreg_lc->state & MLXREG_LC_INITIALIZED))
		goto mlxreg_lc_non_initialzed_exit;

	switch (kind) {
	case MLXREG_HOTPLUG_LC_SYNCED:
		 
		mlxreg_lc_state_update(mlxreg_lc, MLXREG_LC_SYNCED, action);
		 
		if (!(mlxreg_lc->state & MLXREG_LC_POWERED) && action) {
			err = mlxreg_lc_power_on_off(mlxreg_lc, 1);
			if (err)
				goto mlxreg_lc_power_on_off_fail;
		}
		 
		if (mlxreg_lc->state & MLXREG_LC_CONFIGURED && action)
			err = mlxreg_lc_enable_disable(mlxreg_lc, 1);
		break;
	case MLXREG_HOTPLUG_LC_POWERED:
		 
		if (action) {
			 
			if (mlxreg_lc->state & MLXREG_LC_POWERED) {
				 
				if (mlxreg_lc->state & MLXREG_LC_CONFIGURED)
					err = mlxreg_lc_enable_disable(mlxreg_lc, 1);

				goto mlxreg_lc_enable_disable_exit;
			}
			err = mlxreg_lc_create_static_devices(mlxreg_lc, mlxreg_lc->main_devs,
							      mlxreg_lc->main_devs_num);
			if (err)
				goto mlxreg_lc_create_static_devices_fail;

			 
			if (mlxreg_lc->state & MLXREG_LC_CONFIGURED)
				err = mlxreg_lc_enable_disable(mlxreg_lc, 1);
		} else {
			mlxreg_lc_destroy_static_devices(mlxreg_lc, mlxreg_lc->main_devs,
							 mlxreg_lc->main_devs_num);
		}
		mlxreg_lc_state_update(mlxreg_lc, MLXREG_LC_POWERED, action);
		break;
	case MLXREG_HOTPLUG_LC_READY:
		 
		err = mlxreg_lc_enable_disable(mlxreg_lc, !!action);
		break;
	case MLXREG_HOTPLUG_LC_THERMAL:
		 
		if (action)
			err = mlxreg_lc_power_on_off(mlxreg_lc, 0);
		break;
	default:
		break;
	}

mlxreg_lc_enable_disable_exit:
mlxreg_lc_power_on_off_fail:
mlxreg_lc_create_static_devices_fail:
mlxreg_lc_non_initialzed_exit:
	mutex_unlock(&mlxreg_lc->lock);

	return err;
}

 
static int mlxreg_lc_completion_notify(void *handle, struct i2c_adapter *parent,
				       struct i2c_adapter *adapters[])
{
	struct mlxreg_hotplug_device *main_dev, *aux_dev;
	struct mlxreg_lc *mlxreg_lc = handle;
	u32 regval;
	int i, err;

	 
	aux_dev = mlxreg_lc->aux_devs;
	for (i = 0; i < mlxreg_lc->aux_devs_num; i++, aux_dev++) {
		aux_dev->adapter = adapters[aux_dev->nr];
		aux_dev->nr = adapters[aux_dev->nr]->nr;
	}

	err = mlxreg_lc_create_static_devices(mlxreg_lc, mlxreg_lc->aux_devs,
					      mlxreg_lc->aux_devs_num);
	if (err)
		return err;

	 
	main_dev = mlxreg_lc->main_devs;
	for (i = 0; i < mlxreg_lc->main_devs_num; i++, main_dev++) {
		main_dev->adapter = adapters[main_dev->nr];
		main_dev->nr = adapters[main_dev->nr]->nr;
	}

	 
	err = regmap_read(mlxreg_lc->par_regmap, mlxreg_lc->data->reg_pwr, &regval);
	if (err)
		goto mlxreg_lc_regmap_read_power_fail;

	if (regval & mlxreg_lc->data->mask) {
		err = mlxreg_lc_create_static_devices(mlxreg_lc, mlxreg_lc->main_devs,
						      mlxreg_lc->main_devs_num);
		if (err)
			goto mlxreg_lc_create_static_devices_failed;

		mlxreg_lc_state_update_locked(mlxreg_lc, MLXREG_LC_POWERED, 1);
	}

	 
	err = regmap_read(mlxreg_lc->par_regmap, mlxreg_lc->data->reg_sync, &regval);
	if (err)
		goto mlxreg_lc_regmap_read_sync_fail;

	 
	if (regval & mlxreg_lc->data->mask) {
		mlxreg_lc->state |= MLXREG_LC_SYNCED;
		mlxreg_lc_state_update_locked(mlxreg_lc, MLXREG_LC_SYNCED, 1);
		if (mlxreg_lc->state & ~MLXREG_LC_POWERED) {
			err = mlxreg_lc_power_on_off(mlxreg_lc, 1);
			if (err)
				goto mlxreg_lc_regmap_power_on_off_fail;
		}
	}

	mlxreg_lc_state_update_locked(mlxreg_lc, MLXREG_LC_INITIALIZED, 1);

	return 0;

mlxreg_lc_regmap_power_on_off_fail:
mlxreg_lc_regmap_read_sync_fail:
	if (mlxreg_lc->state & MLXREG_LC_POWERED)
		mlxreg_lc_destroy_static_devices(mlxreg_lc, mlxreg_lc->main_devs,
						 mlxreg_lc->main_devs_num);
mlxreg_lc_create_static_devices_failed:
	mlxreg_lc_destroy_static_devices(mlxreg_lc, mlxreg_lc->aux_devs, mlxreg_lc->aux_devs_num);
mlxreg_lc_regmap_read_power_fail:
	return err;
}

static int
mlxreg_lc_config_init(struct mlxreg_lc *mlxreg_lc, void *regmap,
		      struct mlxreg_core_data *data)
{
	struct device *dev = &data->hpdev.client->dev;
	int lsb, err;
	u32 regval;

	 
	err = regmap_read(regmap, MLXREG_LC_REG_CONFIG_OFFSET, &lsb);
	err = (!err) ? regmap_read(regmap, MLXREG_LC_REG_CONFIG_OFFSET, &regval) : err;
	if (err)
		return err;
	regval = (regval & GENMASK(7, 0)) << 8 | (lsb & GENMASK(7, 0));
	switch (regval) {
	case MLXREG_LC_SN4800_C16:
		err = mlxreg_lc_sn4800_c16_config_init(mlxreg_lc, regmap, data);
		if (err) {
			dev_err(dev, "Failed to config client %s at bus %d at addr 0x%02x\n",
				data->hpdev.brdinfo->type, data->hpdev.nr,
				data->hpdev.brdinfo->addr);
			return err;
		}
		break;
	default:
		return -ENODEV;
	}

	 
	mlxreg_lc->mux_data->handle = mlxreg_lc;
	mlxreg_lc->mux_data->completion_notify = mlxreg_lc_completion_notify;
	mlxreg_lc->mux_brdinfo->platform_data = mlxreg_lc->mux_data;
	mlxreg_lc->mux = platform_device_register_resndata(dev, "i2c-mux-mlxcpld", data->hpdev.nr,
							   NULL, 0, mlxreg_lc->mux_data,
							   sizeof(*mlxreg_lc->mux_data));
	if (IS_ERR(mlxreg_lc->mux)) {
		dev_err(dev, "Failed to create mux infra for client %s at bus %d at addr 0x%02x\n",
			data->hpdev.brdinfo->type, data->hpdev.nr, data->hpdev.brdinfo->addr);
		return PTR_ERR(mlxreg_lc->mux);
	}

	 
	if (mlxreg_lc->io_data) {
		mlxreg_lc->io_data->regmap = regmap;
		mlxreg_lc->io_regs =
		platform_device_register_resndata(dev, "mlxreg-io", data->hpdev.nr, NULL, 0,
						  mlxreg_lc->io_data, sizeof(*mlxreg_lc->io_data));
		if (IS_ERR(mlxreg_lc->io_regs)) {
			dev_err(dev, "Failed to create regio for client %s at bus %d at addr 0x%02x\n",
				data->hpdev.brdinfo->type, data->hpdev.nr,
				data->hpdev.brdinfo->addr);
			err = PTR_ERR(mlxreg_lc->io_regs);
			goto fail_register_io;
		}
	}

	 
	if (mlxreg_lc->led_data) {
		mlxreg_lc->led_data->regmap = regmap;
		mlxreg_lc->led =
		platform_device_register_resndata(dev, "leds-mlxreg", data->hpdev.nr, NULL, 0,
						  mlxreg_lc->led_data,
						  sizeof(*mlxreg_lc->led_data));
		if (IS_ERR(mlxreg_lc->led)) {
			dev_err(dev, "Failed to create LED objects for client %s at bus %d at addr 0x%02x\n",
				data->hpdev.brdinfo->type, data->hpdev.nr,
				data->hpdev.brdinfo->addr);
			err = PTR_ERR(mlxreg_lc->led);
			goto fail_register_led;
		}
	}

	return 0;

fail_register_led:
	if (mlxreg_lc->io_regs)
		platform_device_unregister(mlxreg_lc->io_regs);
fail_register_io:
	if (mlxreg_lc->mux)
		platform_device_unregister(mlxreg_lc->mux);

	return err;
}

static void mlxreg_lc_config_exit(struct mlxreg_lc *mlxreg_lc)
{
	 
	if (mlxreg_lc->led)
		platform_device_unregister(mlxreg_lc->led);
	 
	if (mlxreg_lc->io_regs)
		platform_device_unregister(mlxreg_lc->io_regs);
	 
	if (mlxreg_lc->mux)
		platform_device_unregister(mlxreg_lc->mux);
}

static int mlxreg_lc_probe(struct platform_device *pdev)
{
	struct mlxreg_core_hotplug_platform_data *par_pdata;
	struct mlxreg_core_data *data;
	struct mlxreg_lc *mlxreg_lc;
	void *regmap;
	int i, err;

	data = dev_get_platdata(&pdev->dev);
	if (!data)
		return -EINVAL;

	mlxreg_lc = devm_kzalloc(&pdev->dev, sizeof(*mlxreg_lc), GFP_KERNEL);
	if (!mlxreg_lc)
		return -ENOMEM;

	mutex_init(&mlxreg_lc->lock);
	 
	data->notifier->user_handler = mlxreg_lc_event_handler;
	data->notifier->handle = mlxreg_lc;

	data->hpdev.adapter = i2c_get_adapter(data->hpdev.nr);
	if (!data->hpdev.adapter) {
		dev_err(&pdev->dev, "Failed to get adapter for bus %d\n",
			data->hpdev.nr);
		err = -EFAULT;
		goto i2c_get_adapter_fail;
	}

	 
	data->hpdev.client = i2c_new_client_device(data->hpdev.adapter,
						   data->hpdev.brdinfo);
	if (IS_ERR(data->hpdev.client)) {
		dev_err(&pdev->dev, "Failed to create client %s at bus %d at addr 0x%02x\n",
			data->hpdev.brdinfo->type, data->hpdev.nr, data->hpdev.brdinfo->addr);
		err = PTR_ERR(data->hpdev.client);
		goto i2c_new_device_fail;
	}

	regmap = devm_regmap_init_i2c(data->hpdev.client,
				      &mlxreg_lc_regmap_conf);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "Failed to create regmap for client %s at bus %d at addr 0x%02x\n",
			data->hpdev.brdinfo->type, data->hpdev.nr, data->hpdev.brdinfo->addr);
		err = PTR_ERR(regmap);
		goto devm_regmap_init_i2c_fail;
	}

	 
	for (i = 0; i < mlxreg_lc_regmap_conf.num_reg_defaults; i++) {
		err = regmap_write(regmap, mlxreg_lc_regmap_default[i].reg,
				   mlxreg_lc_regmap_default[i].def);
		if (err) {
			dev_err(&pdev->dev, "Failed to set default regmap %d for client %s at bus %d at addr 0x%02x\n",
				i, data->hpdev.brdinfo->type, data->hpdev.nr,
				data->hpdev.brdinfo->addr);
			goto regmap_write_fail;
		}
	}

	 
	regcache_mark_dirty(regmap);
	err = regcache_sync(regmap);
	if (err) {
		dev_err(&pdev->dev, "Failed to sync regmap for client %s at bus %d at addr 0x%02x\n",
			data->hpdev.brdinfo->type, data->hpdev.nr, data->hpdev.brdinfo->addr);
		goto regcache_sync_fail;
	}

	par_pdata = data->hpdev.brdinfo->platform_data;
	mlxreg_lc->par_regmap = par_pdata->regmap;
	mlxreg_lc->data = data;
	mlxreg_lc->dev = &pdev->dev;
	platform_set_drvdata(pdev, mlxreg_lc);

	 
	err = mlxreg_lc_config_init(mlxreg_lc, regmap, data);
	if (err)
		goto mlxreg_lc_config_init_fail;

	return 0;

mlxreg_lc_config_init_fail:
regcache_sync_fail:
regmap_write_fail:
devm_regmap_init_i2c_fail:
	i2c_unregister_device(data->hpdev.client);
	data->hpdev.client = NULL;
i2c_new_device_fail:
	i2c_put_adapter(data->hpdev.adapter);
	data->hpdev.adapter = NULL;
i2c_get_adapter_fail:
	 
	if (data->notifier) {
		data->notifier->user_handler = NULL;
		data->notifier->handle = NULL;
	}
	return err;
}

static int mlxreg_lc_remove(struct platform_device *pdev)
{
	struct mlxreg_core_data *data = dev_get_platdata(&pdev->dev);
	struct mlxreg_lc *mlxreg_lc = platform_get_drvdata(pdev);

	mlxreg_lc_state_update_locked(mlxreg_lc, MLXREG_LC_INITIALIZED, 0);

	 
	if (!data->notifier || !data->notifier->handle)
		return 0;

	 
	data->notifier->user_handler = NULL;
	data->notifier->handle = NULL;

	 
	mlxreg_lc_destroy_static_devices(mlxreg_lc, mlxreg_lc->main_devs,
					 mlxreg_lc->main_devs_num);
	 
	mlxreg_lc_destroy_static_devices(mlxreg_lc, mlxreg_lc->aux_devs, mlxreg_lc->aux_devs_num);
	 
	mlxreg_lc_config_exit(mlxreg_lc);
	if (data->hpdev.client) {
		i2c_unregister_device(data->hpdev.client);
		data->hpdev.client = NULL;
		i2c_put_adapter(data->hpdev.adapter);
		data->hpdev.adapter = NULL;
	}

	return 0;
}

static struct platform_driver mlxreg_lc_driver = {
	.probe = mlxreg_lc_probe,
	.remove = mlxreg_lc_remove,
	.driver = {
		.name = "mlxreg-lc",
	},
};

module_platform_driver(mlxreg_lc_driver);

MODULE_AUTHOR("Vadim Pasternak <vadimp@nvidia.com>");
MODULE_DESCRIPTION("Nvidia line card platform driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:mlxreg-lc");
