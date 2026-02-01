
 

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>


 

struct i2c_arbitrator_data {
	struct gpio_desc *our_gpio;
	struct gpio_desc *their_gpio;
	unsigned int slew_delay_us;
	unsigned int wait_retry_us;
	unsigned int wait_free_us;
};


 
static int i2c_arbitrator_select(struct i2c_mux_core *muxc, u32 chan)
{
	const struct i2c_arbitrator_data *arb = i2c_mux_priv(muxc);
	unsigned long stop_retry, stop_time;

	 
	stop_time = jiffies + usecs_to_jiffies(arb->wait_free_us) + 1;
	do {
		 
		gpiod_set_value(arb->our_gpio, 1);
		udelay(arb->slew_delay_us);

		 
		stop_retry = jiffies + usecs_to_jiffies(arb->wait_retry_us) + 1;
		while (time_before(jiffies, stop_retry)) {
			int gpio_val = gpiod_get_value(arb->their_gpio);

			if (!gpio_val) {
				 
				return 0;
			}

			usleep_range(50, 200);
		}

		 
		gpiod_set_value(arb->our_gpio, 0);

		usleep_range(arb->wait_retry_us, arb->wait_retry_us * 2);
	} while (time_before(jiffies, stop_time));

	 
	gpiod_set_value(arb->our_gpio, 0);
	udelay(arb->slew_delay_us);
	dev_err(muxc->dev, "Could not claim bus, timeout\n");
	return -EBUSY;
}

 
static int i2c_arbitrator_deselect(struct i2c_mux_core *muxc, u32 chan)
{
	const struct i2c_arbitrator_data *arb = i2c_mux_priv(muxc);

	 
	gpiod_set_value(arb->our_gpio, 0);
	udelay(arb->slew_delay_us);

	return 0;
}

static int i2c_arbitrator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *parent_np;
	struct i2c_mux_core *muxc;
	struct i2c_arbitrator_data *arb;
	struct gpio_desc *dummy;
	int ret;

	 
	if (!np) {
		dev_err(dev, "Cannot find device tree node\n");
		return -ENODEV;
	}
	if (dev_get_platdata(dev)) {
		dev_err(dev, "Platform data is not supported\n");
		return -EINVAL;
	}

	muxc = i2c_mux_alloc(NULL, dev, 1, sizeof(*arb), I2C_MUX_ARBITRATOR,
			     i2c_arbitrator_select, i2c_arbitrator_deselect);
	if (!muxc)
		return -ENOMEM;
	arb = i2c_mux_priv(muxc);

	platform_set_drvdata(pdev, muxc);

	 
	arb->our_gpio = devm_gpiod_get(dev, "our-claim", GPIOD_OUT_LOW);
	if (IS_ERR(arb->our_gpio)) {
		dev_err(dev, "could not get \"our-claim\" GPIO (%ld)\n",
			PTR_ERR(arb->our_gpio));
		return PTR_ERR(arb->our_gpio);
	}

	arb->their_gpio = devm_gpiod_get(dev, "their-claim", GPIOD_IN);
	if (IS_ERR(arb->their_gpio)) {
		dev_err(dev, "could not get \"their-claim\" GPIO (%ld)\n",
			PTR_ERR(arb->their_gpio));
		return PTR_ERR(arb->their_gpio);
	}

	 
	dummy = devm_gpiod_get_index(dev, "their-claim", 1, GPIOD_IN);
	if (!IS_ERR(dummy)) {
		dev_err(dev, "Only one other master is supported\n");
		return -EINVAL;
	} else if (PTR_ERR(dummy) == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	}

	 
	if (of_property_read_u32(np, "slew-delay-us", &arb->slew_delay_us))
		arb->slew_delay_us = 10;
	if (of_property_read_u32(np, "wait-retry-us", &arb->wait_retry_us))
		arb->wait_retry_us = 3000;
	if (of_property_read_u32(np, "wait-free-us", &arb->wait_free_us))
		arb->wait_free_us = 50000;

	 
	parent_np = of_parse_phandle(np, "i2c-parent", 0);
	if (!parent_np) {
		dev_err(dev, "Cannot parse i2c-parent\n");
		return -EINVAL;
	}
	muxc->parent = of_get_i2c_adapter_by_node(parent_np);
	of_node_put(parent_np);
	if (!muxc->parent) {
		dev_err(dev, "Cannot find parent bus\n");
		return -EPROBE_DEFER;
	}

	 
	ret = i2c_mux_add_adapter(muxc, 0, 0, 0);
	if (ret)
		i2c_put_adapter(muxc->parent);

	return ret;
}

static void i2c_arbitrator_remove(struct platform_device *pdev)
{
	struct i2c_mux_core *muxc = platform_get_drvdata(pdev);

	i2c_mux_del_adapters(muxc);
	i2c_put_adapter(muxc->parent);
}

static const struct of_device_id i2c_arbitrator_of_match[] = {
	{ .compatible = "i2c-arb-gpio-challenge", },
	{},
};
MODULE_DEVICE_TABLE(of, i2c_arbitrator_of_match);

static struct platform_driver i2c_arbitrator_driver = {
	.probe	= i2c_arbitrator_probe,
	.remove_new = i2c_arbitrator_remove,
	.driver	= {
		.name	= "i2c-arb-gpio-challenge",
		.of_match_table = i2c_arbitrator_of_match,
	},
};

module_platform_driver(i2c_arbitrator_driver);

MODULE_DESCRIPTION("GPIO-based I2C Arbitration");
MODULE_AUTHOR("Doug Anderson <dianders@chromium.org>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:i2c-arb-gpio-challenge");
