
 

#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

struct max8903_data {
	struct device *dev;
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	 
	struct gpio_desc *cen;  
	struct gpio_desc *dok;  
	struct gpio_desc *uok;  
	struct gpio_desc *chg;  
	struct gpio_desc *flt;  
	struct gpio_desc *dcm;  
	struct gpio_desc *usus;  
	bool fault;
	bool usb_in;
	bool ta_in;
};

static enum power_supply_property max8903_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,  
	POWER_SUPPLY_PROP_ONLINE,  
	POWER_SUPPLY_PROP_HEALTH,  
};

static int max8903_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max8903_data *data = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		if (data->chg) {
			if (gpiod_get_value(data->chg))
				 
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else if (data->usb_in || data->ta_in)
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		if (data->usb_in || data->ta_in)
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		if (data->fault)
			val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t max8903_dcin(int irq, void *_data)
{
	struct max8903_data *data = _data;
	bool ta_in;
	enum power_supply_type old_type;

	 
	ta_in = gpiod_get_value(data->dok);

	if (ta_in == data->ta_in)
		return IRQ_HANDLED;

	data->ta_in = ta_in;

	 
	if (data->dcm)
		gpiod_set_value(data->dcm, ta_in);

	 
	if (data->cen) {
		int val;

		if (ta_in)
			 
			val = 1;
		else if (data->usb_in)
			 
			val = 1;
		else
			 
			val = 0;

		gpiod_set_value(data->cen, val);
	}

	dev_dbg(data->dev, "TA(DC-IN) Charger %s.\n", ta_in ?
			"Connected" : "Disconnected");

	old_type = data->psy_desc.type;

	if (data->ta_in)
		data->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
	else if (data->usb_in)
		data->psy_desc.type = POWER_SUPPLY_TYPE_USB;
	else
		data->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;

	if (old_type != data->psy_desc.type)
		power_supply_changed(data->psy);

	return IRQ_HANDLED;
}

static irqreturn_t max8903_usbin(int irq, void *_data)
{
	struct max8903_data *data = _data;
	bool usb_in;
	enum power_supply_type old_type;

	 
	usb_in = gpiod_get_value(data->uok);

	if (usb_in == data->usb_in)
		return IRQ_HANDLED;

	data->usb_in = usb_in;

	 

	 
	if (data->cen) {
		int val;

		if (usb_in)
			 
			val = 1;
		else if (data->ta_in)
			 
			val = 1;
		else
			 
			val = 0;

		gpiod_set_value(data->cen, val);
	}

	dev_dbg(data->dev, "USB Charger %s.\n", usb_in ?
			"Connected" : "Disconnected");

	old_type = data->psy_desc.type;

	if (data->ta_in)
		data->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
	else if (data->usb_in)
		data->psy_desc.type = POWER_SUPPLY_TYPE_USB;
	else
		data->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;

	if (old_type != data->psy_desc.type)
		power_supply_changed(data->psy);

	return IRQ_HANDLED;
}

static irqreturn_t max8903_fault(int irq, void *_data)
{
	struct max8903_data *data = _data;
	bool fault;

	 
	fault = gpiod_get_value(data->flt);

	if (fault == data->fault)
		return IRQ_HANDLED;

	data->fault = fault;

	if (fault)
		dev_err(data->dev, "Charger suffers a fault and stops.\n");
	else
		dev_err(data->dev, "Charger recovered from a fault.\n");

	return IRQ_HANDLED;
}

static int max8903_setup_gpios(struct platform_device *pdev)
{
	struct max8903_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	bool ta_in = false;
	bool usb_in = false;
	enum gpiod_flags flags;

	data->dok = devm_gpiod_get_optional(dev, "dok", GPIOD_IN);
	if (IS_ERR(data->dok))
		return dev_err_probe(dev, PTR_ERR(data->dok),
				     "failed to get DOK GPIO");
	if (data->dok) {
		gpiod_set_consumer_name(data->dok, data->psy_desc.name);
		 
		ta_in = gpiod_get_value(data->dok);
	}

	data->uok = devm_gpiod_get_optional(dev, "uok", GPIOD_IN);
	if (IS_ERR(data->uok))
		return dev_err_probe(dev, PTR_ERR(data->uok),
				     "failed to get UOK GPIO");
	if (data->uok) {
		gpiod_set_consumer_name(data->uok, data->psy_desc.name);
		 
		usb_in = gpiod_get_value(data->uok);
	}

	 
	if (!data->dok && !data->uok) {
		dev_err(dev, "no valid power source\n");
		return -EINVAL;
	}

	 
	flags = (ta_in || usb_in) ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW;
	 
	data->cen = devm_gpiod_get(dev, "cen", flags);
	if (IS_ERR(data->cen))
		return dev_err_probe(dev, PTR_ERR(data->cen),
				     "failed to get CEN GPIO");
	gpiod_set_consumer_name(data->cen, data->psy_desc.name);

	 
	flags = ta_in ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW;
	data->dcm = devm_gpiod_get_optional(dev, "dcm", flags);
	if (IS_ERR(data->dcm))
		return dev_err_probe(dev, PTR_ERR(data->dcm),
				     "failed to get DCM GPIO");
	gpiod_set_consumer_name(data->dcm, data->psy_desc.name);

	data->chg = devm_gpiod_get_optional(dev, "chg", GPIOD_IN);
	if (IS_ERR(data->chg))
		return dev_err_probe(dev, PTR_ERR(data->chg),
				     "failed to get CHG GPIO");
	gpiod_set_consumer_name(data->chg, data->psy_desc.name);

	data->flt = devm_gpiod_get_optional(dev, "flt", GPIOD_IN);
	if (IS_ERR(data->flt))
		return dev_err_probe(dev, PTR_ERR(data->flt),
				     "failed to get FLT GPIO");
	gpiod_set_consumer_name(data->flt, data->psy_desc.name);

	data->usus = devm_gpiod_get_optional(dev, "usus", GPIOD_IN);
	if (IS_ERR(data->usus))
		return dev_err_probe(dev, PTR_ERR(data->usus),
				     "failed to get USUS GPIO");
	gpiod_set_consumer_name(data->usus, data->psy_desc.name);

	data->fault = false;
	data->ta_in = ta_in;
	data->usb_in = usb_in;

	return 0;
}

static int max8903_probe(struct platform_device *pdev)
{
	struct max8903_data *data;
	struct device *dev = &pdev->dev;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	data = devm_kzalloc(dev, sizeof(struct max8903_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	platform_set_drvdata(pdev, data);

	ret = max8903_setup_gpios(pdev);
	if (ret)
		return ret;

	data->psy_desc.name = "max8903_charger";
	data->psy_desc.type = (data->ta_in) ? POWER_SUPPLY_TYPE_MAINS :
			((data->usb_in) ? POWER_SUPPLY_TYPE_USB :
			 POWER_SUPPLY_TYPE_BATTERY);
	data->psy_desc.get_property = max8903_get_property;
	data->psy_desc.properties = max8903_charger_props;
	data->psy_desc.num_properties = ARRAY_SIZE(max8903_charger_props);

	psy_cfg.of_node = dev->of_node;
	psy_cfg.drv_data = data;

	data->psy = devm_power_supply_register(dev, &data->psy_desc, &psy_cfg);
	if (IS_ERR(data->psy)) {
		dev_err(dev, "failed: power supply register.\n");
		return PTR_ERR(data->psy);
	}

	if (data->dok) {
		ret = devm_request_threaded_irq(dev, gpiod_to_irq(data->dok),
					NULL, max8903_dcin,
					IRQF_TRIGGER_FALLING |
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"MAX8903 DC IN", data);
		if (ret) {
			dev_err(dev, "Cannot request irq %d for DC (%d)\n",
					gpiod_to_irq(data->dok), ret);
			return ret;
		}
	}

	if (data->uok) {
		ret = devm_request_threaded_irq(dev, gpiod_to_irq(data->uok),
					NULL, max8903_usbin,
					IRQF_TRIGGER_FALLING |
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"MAX8903 USB IN", data);
		if (ret) {
			dev_err(dev, "Cannot request irq %d for USB (%d)\n",
					gpiod_to_irq(data->uok), ret);
			return ret;
		}
	}

	if (data->flt) {
		ret = devm_request_threaded_irq(dev, gpiod_to_irq(data->flt),
					NULL, max8903_fault,
					IRQF_TRIGGER_FALLING |
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"MAX8903 Fault", data);
		if (ret) {
			dev_err(dev, "Cannot request irq %d for Fault (%d)\n",
					gpiod_to_irq(data->flt), ret);
			return ret;
		}
	}

	return 0;
}

static const struct of_device_id max8903_match_ids[] = {
	{ .compatible = "maxim,max8903", },
	{   }
};
MODULE_DEVICE_TABLE(of, max8903_match_ids);

static struct platform_driver max8903_driver = {
	.probe	= max8903_probe,
	.driver = {
		.name	= "max8903-charger",
		.of_match_table = max8903_match_ids
	},
};

module_platform_driver(max8903_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX8903 Charger Driver");
MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_ALIAS("platform:max8903-charger");
