 

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>

#define MAX_GPIO_PER_REG		32
#define PIN_OFFSET(pin)			(pin % MAX_GPIO_PER_REG)
#define REG_OFFSET(base, reg, pin)	(base + reg + (pin / MAX_GPIO_PER_REG) \
							* sizeof(int *))

 
#define PTO_ENB_REG		0x001
#define PTO_WDATA_REG		0x002
#define PTO_DIR_REG		0x004
#define PTO_IE_REG		0x008
#define PTO_RDATA_REG		0x010
#define PTO_MIS_REG		0x020

struct plgpio_regs {
	u32 enb;		 
	u32 wdata;		 
	u32 dir;		 
	u32 rdata;		 
	u32 ie;			 
	u32 mis;		 
	u32 eit;		 
};

 
struct plgpio {
	spinlock_t		lock;
	struct regmap		*regmap;
	struct clk		*clk;
	struct gpio_chip	chip;
	int			(*p2o)(int pin);	 
	int			(*o2p)(int offset);	 
	u32			p2o_regs;
	struct plgpio_regs	regs;
#ifdef CONFIG_PM_SLEEP
	struct plgpio_regs	*csave_regs;
#endif
};

 
static inline u32 is_plgpio_set(struct regmap *regmap, u32 pin, u32 reg)
{
	u32 offset = PIN_OFFSET(pin);
	u32 reg_off = REG_OFFSET(0, reg, pin);
	u32 val;

	regmap_read(regmap, reg_off, &val);

	return !!(val & (1 << offset));
}

static inline void plgpio_reg_set(struct regmap *regmap, u32 pin, u32 reg)
{
	u32 offset = PIN_OFFSET(pin);
	u32 reg_off = REG_OFFSET(0, reg, pin);
	u32 mask;

	mask = 1 << offset;
	regmap_update_bits(regmap, reg_off, mask, mask);
}

static inline void plgpio_reg_reset(struct regmap *regmap, u32 pin, u32 reg)
{
	u32 offset = PIN_OFFSET(pin);
	u32 reg_off = REG_OFFSET(0, reg, pin);
	u32 mask;

	mask = 1 << offset;
	regmap_update_bits(regmap, reg_off, mask, 0);
}


 
static int plgpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct plgpio *plgpio = gpiochip_get_data(chip);
	unsigned long flags;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & PTO_DIR_REG)) {
		offset = plgpio->p2o(offset);
		if (offset == -1)
			return -EINVAL;
	}

	spin_lock_irqsave(&plgpio->lock, flags);
	plgpio_reg_set(plgpio->regmap, offset, plgpio->regs.dir);
	spin_unlock_irqrestore(&plgpio->lock, flags);

	return 0;
}

static int plgpio_direction_output(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct plgpio *plgpio = gpiochip_get_data(chip);
	unsigned long flags;
	unsigned dir_offset = offset, wdata_offset = offset, tmp;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & (PTO_DIR_REG | PTO_WDATA_REG))) {
		tmp = plgpio->p2o(offset);
		if (tmp == -1)
			return -EINVAL;

		if (plgpio->p2o_regs & PTO_DIR_REG)
			dir_offset = tmp;
		if (plgpio->p2o_regs & PTO_WDATA_REG)
			wdata_offset = tmp;
	}

	spin_lock_irqsave(&plgpio->lock, flags);
	if (value)
		plgpio_reg_set(plgpio->regmap, wdata_offset,
				plgpio->regs.wdata);
	else
		plgpio_reg_reset(plgpio->regmap, wdata_offset,
				plgpio->regs.wdata);

	plgpio_reg_reset(plgpio->regmap, dir_offset, plgpio->regs.dir);
	spin_unlock_irqrestore(&plgpio->lock, flags);

	return 0;
}

static int plgpio_get_value(struct gpio_chip *chip, unsigned offset)
{
	struct plgpio *plgpio = gpiochip_get_data(chip);

	if (offset >= chip->ngpio)
		return -EINVAL;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & PTO_RDATA_REG)) {
		offset = plgpio->p2o(offset);
		if (offset == -1)
			return -EINVAL;
	}

	return is_plgpio_set(plgpio->regmap, offset, plgpio->regs.rdata);
}

static void plgpio_set_value(struct gpio_chip *chip, unsigned offset, int value)
{
	struct plgpio *plgpio = gpiochip_get_data(chip);

	if (offset >= chip->ngpio)
		return;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & PTO_WDATA_REG)) {
		offset = plgpio->p2o(offset);
		if (offset == -1)
			return;
	}

	if (value)
		plgpio_reg_set(plgpio->regmap, offset, plgpio->regs.wdata);
	else
		plgpio_reg_reset(plgpio->regmap, offset, plgpio->regs.wdata);
}

static int plgpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct plgpio *plgpio = gpiochip_get_data(chip);
	int gpio = chip->base + offset;
	unsigned long flags;
	int ret = 0;

	if (offset >= chip->ngpio)
		return -EINVAL;

	ret = pinctrl_gpio_request(gpio);
	if (ret)
		return ret;

	if (!IS_ERR(plgpio->clk)) {
		ret = clk_enable(plgpio->clk);
		if (ret)
			goto err0;
	}

	if (plgpio->regs.enb == -1)
		return 0;

	 
	ret = plgpio_direction_input(chip, offset);
	if (ret)
		goto err1;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & PTO_ENB_REG)) {
		offset = plgpio->p2o(offset);
		if (offset == -1) {
			ret = -EINVAL;
			goto err1;
		}
	}

	spin_lock_irqsave(&plgpio->lock, flags);
	plgpio_reg_set(plgpio->regmap, offset, plgpio->regs.enb);
	spin_unlock_irqrestore(&plgpio->lock, flags);
	return 0;

err1:
	if (!IS_ERR(plgpio->clk))
		clk_disable(plgpio->clk);
err0:
	pinctrl_gpio_free(gpio);
	return ret;
}

static void plgpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct plgpio *plgpio = gpiochip_get_data(chip);
	int gpio = chip->base + offset;
	unsigned long flags;

	if (offset >= chip->ngpio)
		return;

	if (plgpio->regs.enb == -1)
		goto disable_clk;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & PTO_ENB_REG)) {
		offset = plgpio->p2o(offset);
		if (offset == -1)
			return;
	}

	spin_lock_irqsave(&plgpio->lock, flags);
	plgpio_reg_reset(plgpio->regmap, offset, plgpio->regs.enb);
	spin_unlock_irqrestore(&plgpio->lock, flags);

disable_clk:
	if (!IS_ERR(plgpio->clk))
		clk_disable(plgpio->clk);

	pinctrl_gpio_free(gpio);
}

 
static void plgpio_irq_disable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct plgpio *plgpio = gpiochip_get_data(gc);
	int offset = d->hwirq;
	unsigned long flags;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & PTO_IE_REG)) {
		offset = plgpio->p2o(offset);
		if (offset == -1)
			return;
	}

	spin_lock_irqsave(&plgpio->lock, flags);
	plgpio_reg_set(plgpio->regmap, offset, plgpio->regs.ie);
	spin_unlock_irqrestore(&plgpio->lock, flags);
	gpiochip_disable_irq(gc, irqd_to_hwirq(d));
}

static void plgpio_irq_enable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct plgpio *plgpio = gpiochip_get_data(gc);
	int offset = d->hwirq;
	unsigned long flags;

	 
	if (plgpio->p2o && (plgpio->p2o_regs & PTO_IE_REG)) {
		offset = plgpio->p2o(offset);
		if (offset == -1)
			return;
	}

	gpiochip_enable_irq(gc, irqd_to_hwirq(d));
	spin_lock_irqsave(&plgpio->lock, flags);
	plgpio_reg_reset(plgpio->regmap, offset, plgpio->regs.ie);
	spin_unlock_irqrestore(&plgpio->lock, flags);
}

static int plgpio_irq_set_type(struct irq_data *d, unsigned trigger)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct plgpio *plgpio = gpiochip_get_data(gc);
	int offset = d->hwirq;
	u32 reg_off;
	unsigned int supported_type = 0, val;

	if (offset >= plgpio->chip.ngpio)
		return -EINVAL;

	if (plgpio->regs.eit == -1)
		supported_type = IRQ_TYPE_LEVEL_HIGH;
	else
		supported_type = IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING;

	if (!(trigger & supported_type))
		return -EINVAL;

	if (plgpio->regs.eit == -1)
		return 0;

	reg_off = REG_OFFSET(0, plgpio->regs.eit, offset);
	regmap_read(plgpio->regmap, reg_off, &val);

	offset = PIN_OFFSET(offset);
	if (trigger & IRQ_TYPE_EDGE_RISING)
		regmap_write(plgpio->regmap, reg_off, val | (1 << offset));
	else
		regmap_write(plgpio->regmap, reg_off, val & ~(1 << offset));

	return 0;
}

static const struct irq_chip plgpio_irqchip = {
	.name		= "PLGPIO",
	.irq_enable	= plgpio_irq_enable,
	.irq_disable	= plgpio_irq_disable,
	.irq_set_type	= plgpio_irq_set_type,
	.flags		= IRQCHIP_IMMUTABLE,
	GPIOCHIP_IRQ_RESOURCE_HELPERS,
};

static void plgpio_irq_handler(struct irq_desc *desc)
{
	struct gpio_chip *gc = irq_desc_get_handler_data(desc);
	struct plgpio *plgpio = gpiochip_get_data(gc);
	struct irq_chip *irqchip = irq_desc_get_chip(desc);
	int regs_count, count, pin, offset, i = 0;
	u32 pending;
	unsigned long pendingl;

	count = plgpio->chip.ngpio;
	regs_count = DIV_ROUND_UP(count, MAX_GPIO_PER_REG);

	chained_irq_enter(irqchip, desc);
	 
	for (; i < regs_count; i++) {
		regmap_read(plgpio->regmap, plgpio->regs.mis +
			i * sizeof(int *), &pending);
		if (!pending)
			continue;

		 
		regmap_write(plgpio->regmap, plgpio->regs.mis +
			i * sizeof(int *), ~pending);
		 
		count = count - i * MAX_GPIO_PER_REG;
		if (count < MAX_GPIO_PER_REG)
			pending &= (1 << count) - 1;

		pendingl = pending;
		for_each_set_bit(offset, &pendingl, MAX_GPIO_PER_REG) {
			 
			if (plgpio->o2p && (plgpio->p2o_regs & PTO_MIS_REG)) {
				pin = plgpio->o2p(offset);
				if (pin == -1)
					continue;
			} else
				pin = offset;

			 
			pin = i * MAX_GPIO_PER_REG + pin;
			generic_handle_domain_irq(gc->irq.domain, pin);
		}
	}
	chained_irq_exit(irqchip, desc);
}

 
static int spear310_p2o(int pin)
{
	int offset = pin;

	if (pin <= 27)
		offset += 4;
	else if (pin <= 33)
		offset = -1;
	else if (pin <= 97)
		offset -= 2;
	else if (pin <= 101)
		offset = 101 - pin;
	else
		offset = -1;

	return offset;
}

static int spear310_o2p(int offset)
{
	if (offset <= 3)
		return 101 - offset;
	else if (offset <= 31)
		return offset - 4;
	else
		return offset + 2;
}

static int plgpio_probe_dt(struct platform_device *pdev, struct plgpio *plgpio)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = -EINVAL;
	u32 val;

	if (of_machine_is_compatible("st,spear310")) {
		plgpio->p2o = spear310_p2o;
		plgpio->o2p = spear310_o2p;
		plgpio->p2o_regs = PTO_WDATA_REG | PTO_DIR_REG | PTO_IE_REG |
			PTO_RDATA_REG | PTO_MIS_REG;
	}

	if (!of_property_read_u32(np, "st-plgpio,ngpio", &val)) {
		plgpio->chip.ngpio = val;
	} else {
		dev_err(&pdev->dev, "DT: Invalid ngpio field\n");
		goto end;
	}

	if (!of_property_read_u32(np, "st-plgpio,enb-reg", &val))
		plgpio->regs.enb = val;
	else
		plgpio->regs.enb = -1;

	if (!of_property_read_u32(np, "st-plgpio,wdata-reg", &val)) {
		plgpio->regs.wdata = val;
	} else {
		dev_err(&pdev->dev, "DT: Invalid wdata reg\n");
		goto end;
	}

	if (!of_property_read_u32(np, "st-plgpio,dir-reg", &val)) {
		plgpio->regs.dir = val;
	} else {
		dev_err(&pdev->dev, "DT: Invalid dir reg\n");
		goto end;
	}

	if (!of_property_read_u32(np, "st-plgpio,ie-reg", &val)) {
		plgpio->regs.ie = val;
	} else {
		dev_err(&pdev->dev, "DT: Invalid ie reg\n");
		goto end;
	}

	if (!of_property_read_u32(np, "st-plgpio,rdata-reg", &val)) {
		plgpio->regs.rdata = val;
	} else {
		dev_err(&pdev->dev, "DT: Invalid rdata reg\n");
		goto end;
	}

	if (!of_property_read_u32(np, "st-plgpio,mis-reg", &val)) {
		plgpio->regs.mis = val;
	} else {
		dev_err(&pdev->dev, "DT: Invalid mis reg\n");
		goto end;
	}

	if (!of_property_read_u32(np, "st-plgpio,eit-reg", &val))
		plgpio->regs.eit = val;
	else
		plgpio->regs.eit = -1;

	return 0;

end:
	return ret;
}

static int plgpio_probe(struct platform_device *pdev)
{
	struct device_node *regmap_np;
	struct plgpio *plgpio;
	int ret, irq;

	plgpio = devm_kzalloc(&pdev->dev, sizeof(*plgpio), GFP_KERNEL);
	if (!plgpio)
		return -ENOMEM;

	regmap_np = of_parse_phandle(pdev->dev.of_node, "regmap", 0);
	if (regmap_np) {
		plgpio->regmap = device_node_to_regmap(regmap_np);
		of_node_put(regmap_np);
		if (IS_ERR(plgpio->regmap)) {
			dev_err(&pdev->dev, "Retrieve regmap failed (%pe)\n",
				plgpio->regmap);
			return PTR_ERR(plgpio->regmap);
		}
	} else {
		plgpio->regmap = device_node_to_regmap(pdev->dev.of_node);
		if (IS_ERR(plgpio->regmap)) {
			dev_err(&pdev->dev, "Init regmap failed (%pe)\n",
				plgpio->regmap);
			return PTR_ERR(plgpio->regmap);
		}
	}

	ret = plgpio_probe_dt(pdev, plgpio);
	if (ret) {
		dev_err(&pdev->dev, "DT probe failed\n");
		return ret;
	}

	plgpio->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(plgpio->clk))
		dev_warn(&pdev->dev, "clk_get() failed, work without it\n");

#ifdef CONFIG_PM_SLEEP
	plgpio->csave_regs = devm_kcalloc(&pdev->dev,
			DIV_ROUND_UP(plgpio->chip.ngpio, MAX_GPIO_PER_REG),
			sizeof(*plgpio->csave_regs),
			GFP_KERNEL);
	if (!plgpio->csave_regs)
		return -ENOMEM;
#endif

	platform_set_drvdata(pdev, plgpio);
	spin_lock_init(&plgpio->lock);

	plgpio->chip.base = -1;
	plgpio->chip.request = plgpio_request;
	plgpio->chip.free = plgpio_free;
	plgpio->chip.direction_input = plgpio_direction_input;
	plgpio->chip.direction_output = plgpio_direction_output;
	plgpio->chip.get = plgpio_get_value;
	plgpio->chip.set = plgpio_set_value;
	plgpio->chip.label = dev_name(&pdev->dev);
	plgpio->chip.parent = &pdev->dev;
	plgpio->chip.owner = THIS_MODULE;

	if (!IS_ERR(plgpio->clk)) {
		ret = clk_prepare(plgpio->clk);
		if (ret) {
			dev_err(&pdev->dev, "clk prepare failed\n");
			return ret;
		}
	}

	irq = platform_get_irq(pdev, 0);
	if (irq > 0) {
		struct gpio_irq_chip *girq;

		girq = &plgpio->chip.irq;
		gpio_irq_chip_set_chip(girq, &plgpio_irqchip);
		girq->parent_handler = plgpio_irq_handler;
		girq->num_parents = 1;
		girq->parents = devm_kcalloc(&pdev->dev, 1,
					     sizeof(*girq->parents),
					     GFP_KERNEL);
		if (!girq->parents)
			return -ENOMEM;
		girq->parents[0] = irq;
		girq->default_type = IRQ_TYPE_NONE;
		girq->handler = handle_simple_irq;
		dev_info(&pdev->dev, "PLGPIO registering with IRQs\n");
	} else {
		dev_info(&pdev->dev, "PLGPIO registering without IRQs\n");
	}

	ret = gpiochip_add_data(&plgpio->chip, plgpio);
	if (ret) {
		dev_err(&pdev->dev, "unable to add gpio chip\n");
		goto unprepare_clk;
	}

	return 0;

unprepare_clk:
	if (!IS_ERR(plgpio->clk))
		clk_unprepare(plgpio->clk);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int plgpio_suspend(struct device *dev)
{
	struct plgpio *plgpio = dev_get_drvdata(dev);
	int i, reg_count = DIV_ROUND_UP(plgpio->chip.ngpio, MAX_GPIO_PER_REG);
	u32 off;

	for (i = 0; i < reg_count; i++) {
		off = i * sizeof(int *);

		if (plgpio->regs.enb != -1)
			regmap_read(plgpio->regmap, plgpio->regs.enb + off,
				&plgpio->csave_regs[i].enb);
		if (plgpio->regs.eit != -1)
			regmap_read(plgpio->regmap, plgpio->regs.eit + off,
				&plgpio->csave_regs[i].eit);
		regmap_read(plgpio->regmap, plgpio->regs.wdata + off,
				&plgpio->csave_regs[i].wdata);
		regmap_read(plgpio->regmap, plgpio->regs.dir + off,
				&plgpio->csave_regs[i].dir);
		regmap_read(plgpio->regmap, plgpio->regs.ie + off,
				&plgpio->csave_regs[i].ie);
	}

	return 0;
}

 
#define plgpio_prepare_reg(__reg, _off, _mask, _tmp)		\
{								\
	regmap_read(plgpio->regmap, plgpio->regs.__reg + _off, &_tmp); \
	_tmp &= ~_mask;						\
	plgpio->csave_regs[i].__reg =				\
		_tmp | (plgpio->csave_regs[i].__reg & _mask);	\
}

static int plgpio_resume(struct device *dev)
{
	struct plgpio *plgpio = dev_get_drvdata(dev);
	int i, reg_count = DIV_ROUND_UP(plgpio->chip.ngpio, MAX_GPIO_PER_REG);
	u32 off;
	u32 mask, tmp;

	for (i = 0; i < reg_count; i++) {
		off = i * sizeof(int *);

		if (i == reg_count - 1) {
			mask = (1 << (plgpio->chip.ngpio - i *
						MAX_GPIO_PER_REG)) - 1;

			if (plgpio->regs.enb != -1)
				plgpio_prepare_reg(enb, off, mask, tmp);

			if (plgpio->regs.eit != -1)
				plgpio_prepare_reg(eit, off, mask, tmp);

			plgpio_prepare_reg(wdata, off, mask, tmp);
			plgpio_prepare_reg(dir, off, mask, tmp);
			plgpio_prepare_reg(ie, off, mask, tmp);
		}

		regmap_write(plgpio->regmap, plgpio->regs.wdata + off,
			plgpio->csave_regs[i].wdata);

		regmap_write(plgpio->regmap, plgpio->regs.dir + off,
			plgpio->csave_regs[i].dir);

		if (plgpio->regs.eit != -1)
			regmap_write(plgpio->regmap, plgpio->regs.eit + off,
				plgpio->csave_regs[i].eit);

		regmap_write(plgpio->regmap, plgpio->regs.ie + off,
			plgpio->csave_regs[i].ie);

		if (plgpio->regs.enb != -1)
			regmap_write(plgpio->regmap, plgpio->regs.enb + off,
				plgpio->csave_regs[i].enb);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(plgpio_dev_pm_ops, plgpio_suspend, plgpio_resume);

static const struct of_device_id plgpio_of_match[] = {
	{ .compatible = "st,spear-plgpio" },
	{}
};

static struct platform_driver plgpio_driver = {
	.probe = plgpio_probe,
	.driver = {
		.name = "spear-plgpio",
		.pm = &plgpio_dev_pm_ops,
		.of_match_table = plgpio_of_match,
	},
};

static int __init plgpio_init(void)
{
	return platform_driver_register(&plgpio_driver);
}
subsys_initcall(plgpio_init);
