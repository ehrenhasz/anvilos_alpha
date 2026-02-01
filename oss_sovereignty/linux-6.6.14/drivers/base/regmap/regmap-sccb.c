


#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "internal.h"

 
static bool sccb_is_available(struct i2c_adapter *adap)
{
	u32 needed_funcs = I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA;

	 

	return (i2c_get_functionality(adap) & needed_funcs) == needed_funcs;
}

 
static int regmap_sccb_read(void *context, unsigned int reg, unsigned int *val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	int ret;
	union i2c_smbus_data data;

	i2c_lock_bus(i2c->adapter, I2C_LOCK_SEGMENT);

	ret = __i2c_smbus_xfer(i2c->adapter, i2c->addr, i2c->flags,
			       I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE, NULL);
	if (ret < 0)
		goto out;

	ret = __i2c_smbus_xfer(i2c->adapter, i2c->addr, i2c->flags,
			       I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
	if (ret < 0)
		goto out;

	*val = data.byte;
out:
	i2c_unlock_bus(i2c->adapter, I2C_LOCK_SEGMENT);

	return ret;
}

 
static int regmap_sccb_write(void *context, unsigned int reg, unsigned int val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);

	return i2c_smbus_write_byte_data(i2c, reg, val);
}

static const struct regmap_bus regmap_sccb_bus = {
	.reg_write = regmap_sccb_write,
	.reg_read = regmap_sccb_read,
};

static const struct regmap_bus *regmap_get_sccb_bus(struct i2c_client *i2c,
					const struct regmap_config *config)
{
	if (config->val_bits == 8 && config->reg_bits == 8 &&
			sccb_is_available(i2c->adapter))
		return &regmap_sccb_bus;

	return ERR_PTR(-ENOTSUPP);
}

struct regmap *__regmap_init_sccb(struct i2c_client *i2c,
				  const struct regmap_config *config,
				  struct lock_class_key *lock_key,
				  const char *lock_name)
{
	const struct regmap_bus *bus = regmap_get_sccb_bus(i2c, config);

	if (IS_ERR(bus))
		return ERR_CAST(bus);

	return __regmap_init(&i2c->dev, bus, &i2c->dev, config,
			     lock_key, lock_name);
}
EXPORT_SYMBOL_GPL(__regmap_init_sccb);

struct regmap *__devm_regmap_init_sccb(struct i2c_client *i2c,
				       const struct regmap_config *config,
				       struct lock_class_key *lock_key,
				       const char *lock_name)
{
	const struct regmap_bus *bus = regmap_get_sccb_bus(i2c, config);

	if (IS_ERR(bus))
		return ERR_CAST(bus);

	return __devm_regmap_init(&i2c->dev, bus, &i2c->dev, config,
				  lock_key, lock_name);
}
EXPORT_SYMBOL_GPL(__devm_regmap_init_sccb);

MODULE_LICENSE("GPL v2");
