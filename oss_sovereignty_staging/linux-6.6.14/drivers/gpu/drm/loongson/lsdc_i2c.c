
 

#include <drm/drm_managed.h>

#include "lsdc_drv.h"
#include "lsdc_output.h"

 
static void __lsdc_gpio_i2c_set(struct lsdc_i2c * const li2c, int mask, int state)
{
	struct lsdc_device *ldev = to_lsdc(li2c->ddev);
	unsigned long flags;
	u8 val;

	spin_lock_irqsave(&ldev->reglock, flags);

	if (state) {
		 
		val = readb(li2c->dir_reg);
		val |= mask;
		writeb(val, li2c->dir_reg);
	} else {
		 
		val = readb(li2c->dir_reg);
		val &= ~mask;
		writeb(val, li2c->dir_reg);

		 
		val = readb(li2c->dat_reg);
		val &= ~mask;
		writeb(val, li2c->dat_reg);
	}

	spin_unlock_irqrestore(&ldev->reglock, flags);
}

 
static int __lsdc_gpio_i2c_get(struct lsdc_i2c * const li2c, int mask)
{
	struct lsdc_device *ldev = to_lsdc(li2c->ddev);
	unsigned long flags;
	u8 val;

	spin_lock_irqsave(&ldev->reglock, flags);

	 
	val = readb(li2c->dir_reg);
	val |= mask;
	writeb(val, li2c->dir_reg);

	 
	val = readb(li2c->dat_reg);

	spin_unlock_irqrestore(&ldev->reglock, flags);

	return (val & mask) ? 1 : 0;
}

static void lsdc_gpio_i2c_set_sda(void *i2c, int state)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	 
	return __lsdc_gpio_i2c_set(li2c, li2c->sda, state);
}

static void lsdc_gpio_i2c_set_scl(void *i2c, int state)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	 
	return __lsdc_gpio_i2c_set(li2c, li2c->scl, state);
}

static int lsdc_gpio_i2c_get_sda(void *i2c)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	 
	return __lsdc_gpio_i2c_get(li2c, li2c->sda);
}

static int lsdc_gpio_i2c_get_scl(void *i2c)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	 
	return __lsdc_gpio_i2c_get(li2c, li2c->scl);
}

static void lsdc_destroy_i2c(struct drm_device *ddev, void *data)
{
	struct lsdc_i2c *li2c = (struct lsdc_i2c *)data;

	if (li2c) {
		i2c_del_adapter(&li2c->adapter);
		kfree(li2c);
	}
}

 
int lsdc_create_i2c_chan(struct drm_device *ddev,
			 struct lsdc_display_pipe *dispipe,
			 unsigned int index)
{
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct i2c_adapter *adapter;
	struct lsdc_i2c *li2c;
	int ret;

	li2c = kzalloc(sizeof(*li2c), GFP_KERNEL);
	if (!li2c)
		return -ENOMEM;

	dispipe->li2c = li2c;

	if (index == 0) {
		li2c->sda = 0x01;   
		li2c->scl = 0x02;   
	} else if (index == 1) {
		li2c->sda = 0x04;   
		li2c->scl = 0x08;   
	} else {
		return -ENOENT;
	}

	li2c->ddev = ddev;
	li2c->dir_reg = ldev->reg_base + LS7A_DC_GPIO_DIR_REG;
	li2c->dat_reg = ldev->reg_base + LS7A_DC_GPIO_DAT_REG;

	li2c->bit.setsda = lsdc_gpio_i2c_set_sda;
	li2c->bit.setscl = lsdc_gpio_i2c_set_scl;
	li2c->bit.getsda = lsdc_gpio_i2c_get_sda;
	li2c->bit.getscl = lsdc_gpio_i2c_get_scl;
	li2c->bit.udelay = 5;
	li2c->bit.timeout = usecs_to_jiffies(2200);
	li2c->bit.data = li2c;

	adapter = &li2c->adapter;
	adapter->algo_data = &li2c->bit;
	adapter->owner = THIS_MODULE;
	adapter->class = I2C_CLASS_DDC;
	adapter->dev.parent = ddev->dev;
	adapter->nr = -1;

	snprintf(adapter->name, sizeof(adapter->name), "lsdc-i2c%u", index);

	i2c_set_adapdata(adapter, li2c);

	ret = i2c_bit_add_bus(adapter);
	if (ret) {
		kfree(li2c);
		return ret;
	}

	ret = drmm_add_action_or_reset(ddev, lsdc_destroy_i2c, li2c);
	if (ret)
		return ret;

	drm_info(ddev, "%s(sda pin mask=%u, scl pin mask=%u) created\n",
		 adapter->name, li2c->sda, li2c->scl);

	return 0;
}
