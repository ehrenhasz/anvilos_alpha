
 

#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>

#include <linux/regulator/machine.h>

#include <linux/i2c.h>
#include <linux/mfd/twl.h>

 
#include <linux/mfd/twl4030-audio.h>

#include "twl-core.h"

 

#define DRIVER_NAME			"twl"

 

 

 
#define TWL4030_BASEADD_USB		0x0000

 
#define TWL4030_BASEADD_AUDIO_VOICE	0x0000
#define TWL4030_BASEADD_GPIO		0x0098
#define TWL4030_BASEADD_INTBR		0x0085
#define TWL4030_BASEADD_PIH		0x0080
#define TWL4030_BASEADD_TEST		0x004C

 
#define TWL4030_BASEADD_INTERRUPTS	0x00B9
#define TWL4030_BASEADD_LED		0x00EE
#define TWL4030_BASEADD_MADC		0x0000
#define TWL4030_BASEADD_MAIN_CHARGE	0x0074
#define TWL4030_BASEADD_PRECHARGE	0x00AA
#define TWL4030_BASEADD_PWM		0x00F8
#define TWL4030_BASEADD_KEYPAD		0x00D2

#define TWL5031_BASEADD_ACCESSORY	0x0074  
#define TWL5031_BASEADD_INTERRUPTS	0x00B9  

 
#define TWL4030_BASEADD_BACKUP		0x0014
#define TWL4030_BASEADD_INT		0x002E
#define TWL4030_BASEADD_PM_MASTER	0x0036

#define TWL4030_BASEADD_PM_RECEIVER	0x005B
#define TWL4030_DCDC_GLOBAL_CFG		0x06
#define SMARTREFLEX_ENABLE		BIT(3)

#define TWL4030_BASEADD_RTC		0x001C
#define TWL4030_BASEADD_SECURED_REG	0x0000

 


 
#define TWL6030_BASEADD_RTC		0x0000
#define TWL6030_BASEADD_SECURED_REG	0x0017
#define TWL6030_BASEADD_PM_MASTER	0x001F
#define TWL6030_BASEADD_PM_SLAVE_MISC	0x0030  
#define TWL6030_BASEADD_PM_MISC		0x00E2
#define TWL6030_BASEADD_PM_PUPD		0x00F0

 
#define TWL6030_BASEADD_USB		0x0000
#define TWL6030_BASEADD_GPADC_CTRL	0x002E
#define TWL6030_BASEADD_AUX		0x0090
#define TWL6030_BASEADD_PWM		0x00BA
#define TWL6030_BASEADD_GASGAUGE	0x00C0
#define TWL6030_BASEADD_PIH		0x00D0
#define TWL6032_BASEADD_CHARGER		0x00DA
#define TWL6030_BASEADD_CHARGER		0x00E0
#define TWL6030_BASEADD_LED		0x00F4

 
#define TWL6030_BASEADD_DIEID		0x00C0

 
#define TWL6030_BASEADD_AUDIO		0x0000
#define TWL6030_BASEADD_RSV		0x0000
#define TWL6030_BASEADD_ZERO		0x0000

 
#define R_CFG_BOOT			0x05

 
#define HFCLK_FREQ_19p2_MHZ		(1 << 0)
#define HFCLK_FREQ_26_MHZ		(2 << 0)
#define HFCLK_FREQ_38p4_MHZ		(3 << 0)
#define HIGH_PERF_SQ			(1 << 3)
#define CK32K_LOWPWR_EN			(1 << 7)

 

 
struct twl_client {
	struct i2c_client *client;
	struct regmap *regmap;
};

 
struct twl_mapping {
	unsigned char sid;	 
	unsigned char base;	 
};

struct twl_private {
	bool ready;  
	u32 twl_idcode;  
	unsigned int twl_id;

	struct twl_mapping *twl_map;
	struct twl_client *twl_modules;
};

static struct twl_private *twl_priv;

static struct twl_mapping twl4030_map[] = {
	 

	 
	{ 0, TWL4030_BASEADD_USB },
	{ 1, TWL4030_BASEADD_PIH },
	{ 2, TWL4030_BASEADD_MAIN_CHARGE },
	{ 3, TWL4030_BASEADD_PM_MASTER },
	{ 3, TWL4030_BASEADD_PM_RECEIVER },

	{ 3, TWL4030_BASEADD_RTC },
	{ 2, TWL4030_BASEADD_PWM },
	{ 2, TWL4030_BASEADD_LED },
	{ 3, TWL4030_BASEADD_SECURED_REG },

	 
	{ 1, TWL4030_BASEADD_AUDIO_VOICE },
	{ 1, TWL4030_BASEADD_GPIO },
	{ 1, TWL4030_BASEADD_INTBR },
	{ 1, TWL4030_BASEADD_TEST },
	{ 2, TWL4030_BASEADD_KEYPAD },

	{ 2, TWL4030_BASEADD_MADC },
	{ 2, TWL4030_BASEADD_INTERRUPTS },
	{ 2, TWL4030_BASEADD_PRECHARGE },
	{ 3, TWL4030_BASEADD_BACKUP },
	{ 3, TWL4030_BASEADD_INT },

	{ 2, TWL5031_BASEADD_ACCESSORY },
	{ 2, TWL5031_BASEADD_INTERRUPTS },
};

static const struct reg_default twl4030_49_defaults[] = {
	 
	{ 0x01, 0x00},  
	{ 0x02, 0x00},  
	 
	{ 0x04, 0x00},  
	{ 0x05, 0x00},  
	{ 0x06, 0x00},  
	{ 0x07, 0x00},  
	{ 0x08, 0x00},  
	{ 0x09, 0x00},  
	{ 0x0a, 0x0f},  
	{ 0x0b, 0x0f},  
	{ 0x0c, 0x0f},  
	{ 0x0d, 0x0f},  
	{ 0x0e, 0x00},  
	{ 0x0f, 0x00},  
	{ 0x10, 0x3f},  
	{ 0x11, 0x3f},  
	{ 0x12, 0x3f},  
	{ 0x13, 0x3f},  
	{ 0x14, 0x25},  
	{ 0x15, 0x00},  
	{ 0x16, 0x00},  
	{ 0x17, 0x00},  
	{ 0x18, 0x00},  
	{ 0x19, 0x32},  
	{ 0x1a, 0x32},  
	{ 0x1b, 0x32},  
	{ 0x1c, 0x32},  
	{ 0x1d, 0x00},  
	{ 0x1e, 0x00},  
	{ 0x1f, 0x55},  
	{ 0x20, 0x00},  
	{ 0x21, 0x00},  
	{ 0x22, 0x00},  
	{ 0x23, 0x00},  
	{ 0x24, 0x00},  
	{ 0x25, 0x00},  
	{ 0x26, 0x00},  
	{ 0x27, 0x00},  
	{ 0x28, 0x00},  
	{ 0x29, 0x00},  
	{ 0x2a, 0x00},  
	{ 0x2b, 0x05},  
	{ 0x2c, 0x00},  
	{ 0x2d, 0x00},  
	{ 0x2e, 0x00},  
	{ 0x2f, 0x00},  
	{ 0x30, 0x13},  
	{ 0x31, 0x00},  
	{ 0x32, 0x00},  
	{ 0x33, 0x00},  
	{ 0x34, 0x00},  
	{ 0x35, 0x79},  
	{ 0x36, 0x11},  
	{ 0x37, 0x00},  
	{ 0x38, 0x00},  
	{ 0x39, 0x00},  
	{ 0x3a, 0x06},  
	{ 0x3b, 0x00},  
	{ 0x3c, 0x44},  
	{ 0x3d, 0x69},  
	{ 0x3e, 0x00},  
	{ 0x3f, 0x00},  
	 
	{ 0x43, 0x00},  
	{ 0x44, 0x32},  
	{ 0x45, 0x00},  
	{ 0x46, 0x00},  
	{ 0x47, 0x00},  
	{ 0x48, 0x00},  
	{ 0x49, 0x00},  
	 
};

static bool twl4030_49_nop_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00:
	case 0x03:
	case 0x40:
	case 0x41:
	case 0x42:
		return false;
	default:
		return true;
	}
}

static const struct regmap_range twl4030_49_volatile_ranges[] = {
	regmap_reg_range(TWL4030_BASEADD_TEST, 0xff),
};

static const struct regmap_access_table twl4030_49_volatile_table = {
	.yes_ranges = twl4030_49_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(twl4030_49_volatile_ranges),
};

static const struct regmap_config twl4030_regmap_config[4] = {
	{
		 
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xff,
	},
	{
		 
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xff,

		.readable_reg = twl4030_49_nop_reg,
		.writeable_reg = twl4030_49_nop_reg,

		.volatile_table = &twl4030_49_volatile_table,

		.reg_defaults = twl4030_49_defaults,
		.num_reg_defaults = ARRAY_SIZE(twl4030_49_defaults),
		.cache_type = REGCACHE_RBTREE,
	},
	{
		 
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xff,
	},
	{
		 
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xff,
	},
};

static struct twl_mapping twl6030_map[] = {
	 

	 
	{ 1, TWL6030_BASEADD_USB },
	{ 1, TWL6030_BASEADD_PIH },
	{ 1, TWL6030_BASEADD_CHARGER },
	{ 0, TWL6030_BASEADD_PM_MASTER },
	{ 0, TWL6030_BASEADD_PM_SLAVE_MISC },

	{ 0, TWL6030_BASEADD_RTC },
	{ 1, TWL6030_BASEADD_PWM },
	{ 1, TWL6030_BASEADD_LED },
	{ 0, TWL6030_BASEADD_SECURED_REG },

	 
	{ 0, TWL6030_BASEADD_ZERO },
	{ 1, TWL6030_BASEADD_ZERO },
	{ 2, TWL6030_BASEADD_ZERO },
	{ 1, TWL6030_BASEADD_GPADC_CTRL },
	{ 1, TWL6030_BASEADD_GASGAUGE },

	 
	{ 1, TWL6032_BASEADD_CHARGER },
};

static const struct regmap_config twl6030_regmap_config[3] = {
	{
		 
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xff,
	},
	{
		 
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xff,
	},
	{
		 
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0xff,
	},
};

 

static inline int twl_get_num_slaves(void)
{
	if (twl_class_is_4030())
		return 4;  
	else
		return 3;  
}

static inline int twl_get_last_module(void)
{
	if (twl_class_is_4030())
		return TWL4030_MODULE_LAST;
	else
		return TWL6030_MODULE_LAST;
}

 

unsigned int twl_rev(void)
{
	return twl_priv ? twl_priv->twl_id : 0;
}
EXPORT_SYMBOL(twl_rev);

 
static struct regmap *twl_get_regmap(u8 mod_no)
{
	int sid;
	struct twl_client *twl;

	if (unlikely(!twl_priv || !twl_priv->ready)) {
		pr_err("%s: not initialized\n", DRIVER_NAME);
		return NULL;
	}
	if (unlikely(mod_no >= twl_get_last_module())) {
		pr_err("%s: invalid module number %d\n", DRIVER_NAME, mod_no);
		return NULL;
	}

	sid = twl_priv->twl_map[mod_no].sid;
	twl = &twl_priv->twl_modules[sid];

	return twl->regmap;
}

 
int twl_i2c_write(u8 mod_no, u8 *value, u8 reg, unsigned num_bytes)
{
	struct regmap *regmap = twl_get_regmap(mod_no);
	int ret;

	if (!regmap)
		return -EPERM;

	ret = regmap_bulk_write(regmap, twl_priv->twl_map[mod_no].base + reg,
				value, num_bytes);

	if (ret)
		pr_err("%s: Write failed (mod %d, reg 0x%02x count %d)\n",
		       DRIVER_NAME, mod_no, reg, num_bytes);

	return ret;
}
EXPORT_SYMBOL(twl_i2c_write);

 
int twl_i2c_read(u8 mod_no, u8 *value, u8 reg, unsigned num_bytes)
{
	struct regmap *regmap = twl_get_regmap(mod_no);
	int ret;

	if (!regmap)
		return -EPERM;

	ret = regmap_bulk_read(regmap, twl_priv->twl_map[mod_no].base + reg,
			       value, num_bytes);

	if (ret)
		pr_err("%s: Read failed (mod %d, reg 0x%02x count %d)\n",
		       DRIVER_NAME, mod_no, reg, num_bytes);

	return ret;
}
EXPORT_SYMBOL(twl_i2c_read);

 
int twl_set_regcache_bypass(u8 mod_no, bool enable)
{
	struct regmap *regmap = twl_get_regmap(mod_no);

	if (!regmap)
		return -EPERM;

	regcache_cache_bypass(regmap, enable);

	return 0;
}
EXPORT_SYMBOL(twl_set_regcache_bypass);

 

 
static int twl_read_idcode_register(void)
{
	int err;

	err = twl_i2c_write_u8(TWL4030_MODULE_INTBR, TWL_EEPROM_R_UNLOCK,
						REG_UNLOCK_TEST_REG);
	if (err) {
		pr_err("TWL4030 Unable to unlock IDCODE registers -%d\n", err);
		goto fail;
	}

	err = twl_i2c_read(TWL4030_MODULE_INTBR, (u8 *)(&twl_priv->twl_idcode),
						REG_IDCODE_7_0, 4);
	if (err) {
		pr_err("TWL4030: unable to read IDCODE -%d\n", err);
		goto fail;
	}

	err = twl_i2c_write_u8(TWL4030_MODULE_INTBR, 0x0, REG_UNLOCK_TEST_REG);
	if (err)
		pr_err("TWL4030 Unable to relock IDCODE registers -%d\n", err);
fail:
	return err;
}

 
int twl_get_type(void)
{
	return TWL_SIL_TYPE(twl_priv->twl_idcode);
}
EXPORT_SYMBOL_GPL(twl_get_type);

 
int twl_get_version(void)
{
	return TWL_SIL_REV(twl_priv->twl_idcode);
}
EXPORT_SYMBOL_GPL(twl_get_version);

 
int twl_get_hfclk_rate(void)
{
	u8 ctrl;
	int rate;

	twl_i2c_read_u8(TWL_MODULE_PM_MASTER, &ctrl, R_CFG_BOOT);

	switch (ctrl & 0x3) {
	case HFCLK_FREQ_19p2_MHZ:
		rate = 19200000;
		break;
	case HFCLK_FREQ_26_MHZ:
		rate = 26000000;
		break;
	case HFCLK_FREQ_38p4_MHZ:
		rate = 38400000;
		break;
	default:
		pr_err("TWL4030: HFCLK is not configured\n");
		rate = -EINVAL;
		break;
	}

	return rate;
}
EXPORT_SYMBOL_GPL(twl_get_hfclk_rate);

 

 
static inline int protect_pm_master(void)
{
	int e = 0;

	e = twl_i2c_write_u8(TWL_MODULE_PM_MASTER, 0,
			     TWL4030_PM_MASTER_PROTECT_KEY);
	return e;
}

static inline int unprotect_pm_master(void)
{
	int e = 0;

	e |= twl_i2c_write_u8(TWL_MODULE_PM_MASTER, TWL4030_PM_MASTER_KEY_CFG1,
			      TWL4030_PM_MASTER_PROTECT_KEY);
	e |= twl_i2c_write_u8(TWL_MODULE_PM_MASTER, TWL4030_PM_MASTER_KEY_CFG2,
			      TWL4030_PM_MASTER_PROTECT_KEY);

	return e;
}

static void clocks_init(struct device *dev)
{
	int e = 0;
	struct clk *osc;
	u32 rate;
	u8 ctrl = HFCLK_FREQ_26_MHZ;

	osc = clk_get(dev, "fck");
	if (IS_ERR(osc)) {
		printk(KERN_WARNING "Skipping twl internal clock init and "
				"using bootloader value (unknown osc rate)\n");
		return;
	}

	rate = clk_get_rate(osc);
	clk_put(osc);

	switch (rate) {
	case 19200000:
		ctrl = HFCLK_FREQ_19p2_MHZ;
		break;
	case 26000000:
		ctrl = HFCLK_FREQ_26_MHZ;
		break;
	case 38400000:
		ctrl = HFCLK_FREQ_38p4_MHZ;
		break;
	}

	ctrl |= HIGH_PERF_SQ;

	e |= unprotect_pm_master();
	 
	e |= twl_i2c_write_u8(TWL_MODULE_PM_MASTER, ctrl, R_CFG_BOOT);
	e |= protect_pm_master();

	if (e < 0)
		pr_err("%s: clock init err [%d]\n", DRIVER_NAME, e);
}

 


static void twl_remove(struct i2c_client *client)
{
	unsigned i, num_slaves;

	if (twl_class_is_4030())
		twl4030_exit_irq();
	else
		twl6030_exit_irq();

	num_slaves = twl_get_num_slaves();
	for (i = 0; i < num_slaves; i++) {
		struct twl_client	*twl = &twl_priv->twl_modules[i];

		if (twl->client && twl->client != client)
			i2c_unregister_device(twl->client);
		twl->client = NULL;
	}
	twl_priv->ready = false;
}

static struct of_dev_auxdata twl_auxdata_lookup[] = {
	OF_DEV_AUXDATA("ti,twl4030-gpio", 0, "twl4030-gpio", NULL),
	{   },
};

 
static int
twl_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct device_node		*node = client->dev.of_node;
	struct platform_device		*pdev;
	const struct regmap_config	*twl_regmap_config;
	int				irq_base = 0;
	int				status;
	unsigned			i, num_slaves;

	if (!node) {
		dev_err(&client->dev, "no platform data\n");
		return -EINVAL;
	}

	if (twl_priv) {
		dev_dbg(&client->dev, "only one instance of %s allowed\n",
			DRIVER_NAME);
		return -EBUSY;
	}

	pdev = platform_device_alloc(DRIVER_NAME, -1);
	if (!pdev) {
		dev_err(&client->dev, "can't alloc pdev\n");
		return -ENOMEM;
	}

	status = platform_device_add(pdev);
	if (status) {
		platform_device_put(pdev);
		return status;
	}

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
		dev_dbg(&client->dev, "can't talk I2C?\n");
		status = -EIO;
		goto free;
	}

	twl_priv = devm_kzalloc(&client->dev, sizeof(struct twl_private),
				GFP_KERNEL);
	if (!twl_priv) {
		status = -ENOMEM;
		goto free;
	}

	if ((id->driver_data) & TWL6030_CLASS) {
		twl_priv->twl_id = TWL6030_CLASS_ID;
		twl_priv->twl_map = &twl6030_map[0];
		twl_regmap_config = twl6030_regmap_config;
	} else {
		twl_priv->twl_id = TWL4030_CLASS_ID;
		twl_priv->twl_map = &twl4030_map[0];
		twl_regmap_config = twl4030_regmap_config;
	}

	num_slaves = twl_get_num_slaves();
	twl_priv->twl_modules = devm_kcalloc(&client->dev,
					 num_slaves,
					 sizeof(struct twl_client),
					 GFP_KERNEL);
	if (!twl_priv->twl_modules) {
		status = -ENOMEM;
		goto free;
	}

	for (i = 0; i < num_slaves; i++) {
		struct twl_client *twl = &twl_priv->twl_modules[i];

		if (i == 0) {
			twl->client = client;
		} else {
			twl->client = i2c_new_dummy_device(client->adapter,
						    client->addr + i);
			if (IS_ERR(twl->client)) {
				dev_err(&client->dev,
					"can't attach client %d\n", i);
				status = PTR_ERR(twl->client);
				goto fail;
			}
		}

		twl->regmap = devm_regmap_init_i2c(twl->client,
						   &twl_regmap_config[i]);
		if (IS_ERR(twl->regmap)) {
			status = PTR_ERR(twl->regmap);
			dev_err(&client->dev,
				"Failed to allocate regmap %d, err: %d\n", i,
				status);
			goto fail;
		}
	}

	twl_priv->ready = true;

	 
	clocks_init(&client->dev);

	 
	if (twl_class_is_4030()) {
		status = twl_read_idcode_register();
		WARN(status < 0, "Error: reading twl_idcode register value\n");
	}

	 
	if (client->irq) {
		if (twl_class_is_4030()) {
			twl4030_init_chip_irq(id->name);
			irq_base = twl4030_init_irq(&client->dev, client->irq);
		} else {
			irq_base = twl6030_init_irq(&client->dev, client->irq);
		}

		if (irq_base < 0) {
			status = irq_base;
			goto fail;
		}
	}

	 
	if (twl_class_is_4030()) {
		u8 temp;

		twl_i2c_read_u8(TWL4030_MODULE_INTBR, &temp, REG_GPPUPDCTR1);
		temp &= ~(SR_I2C_SDA_CTRL_PU | SR_I2C_SCL_CTRL_PU | \
			I2C_SDA_CTRL_PU | I2C_SCL_CTRL_PU);
		twl_i2c_write_u8(TWL4030_MODULE_INTBR, temp, REG_GPPUPDCTR1);

		twl_i2c_read_u8(TWL_MODULE_PM_RECEIVER, &temp,
				TWL4030_DCDC_GLOBAL_CFG);
		temp |= SMARTREFLEX_ENABLE;
		twl_i2c_write_u8(TWL_MODULE_PM_RECEIVER, temp,
				 TWL4030_DCDC_GLOBAL_CFG);
	}

	status = of_platform_populate(node, NULL, twl_auxdata_lookup,
				      &client->dev);

fail:
	if (status < 0)
		twl_remove(client);
free:
	if (status < 0)
		platform_device_unregister(pdev);

	return status;
}

static int __maybe_unused twl_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (client->irq)
		disable_irq(client->irq);

	return 0;
}

static int __maybe_unused twl_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (client->irq)
		enable_irq(client->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(twl_dev_pm_ops, twl_suspend, twl_resume);

static const struct i2c_device_id twl_ids[] = {
	{ "twl4030", TWL4030_VAUX2 },	 
	{ "twl5030", 0 },		 
	{ "twl5031", TWL5031 },		 
	{ "tps65950", 0 },		 
	{ "tps65930", TPS_SUBSET },	 
	{ "tps65920", TPS_SUBSET },	 
	{ "tps65921", TPS_SUBSET },	 
	{ "twl6030", TWL6030_CLASS },	 
	{ "twl6032", TWL6030_CLASS | TWL6032_SUBCLASS },  
	{   },
};

 
static struct i2c_driver twl_driver = {
	.driver.name	= DRIVER_NAME,
	.driver.pm	= &twl_dev_pm_ops,
	.id_table	= twl_ids,
	.probe		= twl_probe,
	.remove		= twl_remove,
};
builtin_i2c_driver(twl_driver);
