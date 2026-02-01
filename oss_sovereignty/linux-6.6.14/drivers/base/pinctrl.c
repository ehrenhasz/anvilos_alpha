
 

#include <linux/device.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>

 
int pinctrl_bind_pins(struct device *dev)
{
	int ret;

	if (dev->of_node_reused)
		return 0;

	dev->pins = devm_kzalloc(dev, sizeof(*(dev->pins)), GFP_KERNEL);
	if (!dev->pins)
		return -ENOMEM;

	dev->pins->p = devm_pinctrl_get(dev);
	if (IS_ERR(dev->pins->p)) {
		dev_dbg(dev, "no pinctrl handle\n");
		ret = PTR_ERR(dev->pins->p);
		goto cleanup_alloc;
	}

	dev->pins->default_state = pinctrl_lookup_state(dev->pins->p,
					PINCTRL_STATE_DEFAULT);
	if (IS_ERR(dev->pins->default_state)) {
		dev_dbg(dev, "no default pinctrl state\n");
		ret = 0;
		goto cleanup_get;
	}

	dev->pins->init_state = pinctrl_lookup_state(dev->pins->p,
					PINCTRL_STATE_INIT);
	if (IS_ERR(dev->pins->init_state)) {
		 
		dev_dbg(dev, "no init pinctrl state\n");

		ret = pinctrl_select_state(dev->pins->p,
					   dev->pins->default_state);
	} else {
		ret = pinctrl_select_state(dev->pins->p, dev->pins->init_state);
	}

	if (ret) {
		dev_dbg(dev, "failed to activate initial pinctrl state\n");
		goto cleanup_get;
	}

#ifdef CONFIG_PM
	 
	dev->pins->sleep_state = pinctrl_lookup_state(dev->pins->p,
					PINCTRL_STATE_SLEEP);
	if (IS_ERR(dev->pins->sleep_state))
		 
		dev_dbg(dev, "no sleep pinctrl state\n");

	dev->pins->idle_state = pinctrl_lookup_state(dev->pins->p,
					PINCTRL_STATE_IDLE);
	if (IS_ERR(dev->pins->idle_state))
		 
		dev_dbg(dev, "no idle pinctrl state\n");
#endif

	return 0;

	 
cleanup_get:
	devm_pinctrl_put(dev->pins->p);
cleanup_alloc:
	devm_kfree(dev, dev->pins);
	dev->pins = NULL;

	 
	if (ret == -EPROBE_DEFER)
		return ret;
	 
	if (ret == -EINVAL)
		return ret;
	 

	return 0;
}
