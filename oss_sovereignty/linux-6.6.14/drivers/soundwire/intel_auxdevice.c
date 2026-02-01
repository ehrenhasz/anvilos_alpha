


 

#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/auxiliary_bus.h>
#include <sound/pcm_params.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <linux/soundwire/sdw_registers.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_intel.h>
#include "cadence_master.h"
#include "bus.h"
#include "intel.h"
#include "intel_auxdevice.h"

#define INTEL_MASTER_SUSPEND_DELAY_MS	3000

 

#define SDW_INTEL_MASTER_DISABLE_PM_RUNTIME		BIT(0)
#define SDW_INTEL_MASTER_DISABLE_CLOCK_STOP		BIT(1)
#define SDW_INTEL_MASTER_DISABLE_PM_RUNTIME_IDLE	BIT(2)
#define SDW_INTEL_MASTER_DISABLE_MULTI_LINK		BIT(3)

static int md_flags;
module_param_named(sdw_md_flags, md_flags, int, 0444);
MODULE_PARM_DESC(sdw_md_flags, "SoundWire Intel Master device flags (0x0 all off)");

struct wake_capable_part {
	const u16 mfg_id;
	const u16 part_id;
};

static struct wake_capable_part wake_capable_list[] = {
	{0x025d, 0x5682},
	{0x025d, 0x700},
	{0x025d, 0x711},
	{0x025d, 0x1712},
	{0x025d, 0x1713},
	{0x025d, 0x1716},
	{0x025d, 0x1717},
	{0x025d, 0x712},
	{0x025d, 0x713},
	{0x025d, 0x714},
	{0x025d, 0x715},
	{0x025d, 0x716},
	{0x025d, 0x717},
	{0x025d, 0x722},
};

static bool is_wake_capable(struct sdw_slave *slave)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wake_capable_list); i++)
		if (slave->id.part_id == wake_capable_list[i].part_id &&
		    slave->id.mfg_id == wake_capable_list[i].mfg_id)
			return true;
	return false;
}

static int generic_pre_bank_switch(struct sdw_bus *bus)
{
	struct sdw_cdns *cdns = bus_to_cdns(bus);
	struct sdw_intel *sdw = cdns_to_intel(cdns);

	return sdw->link_res->hw_ops->pre_bank_switch(sdw);
}

static int generic_post_bank_switch(struct sdw_bus *bus)
{
	struct sdw_cdns *cdns = bus_to_cdns(bus);
	struct sdw_intel *sdw = cdns_to_intel(cdns);

	return sdw->link_res->hw_ops->post_bank_switch(sdw);
}

static void generic_new_peripheral_assigned(struct sdw_bus *bus,
					    struct sdw_slave *slave,
					    int dev_num)
{
	struct sdw_cdns *cdns = bus_to_cdns(bus);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	int dev_num_min;
	int dev_num_max;
	bool wake_capable = slave->prop.wake_capable || is_wake_capable(slave);

	if (wake_capable) {
		dev_num_min = SDW_INTEL_DEV_NUM_IDA_MIN;
		dev_num_max = SDW_MAX_DEVICES;
	} else {
		dev_num_min = 1;
		dev_num_max = SDW_INTEL_DEV_NUM_IDA_MIN - 1;
	}

	 
	if (dev_num < dev_num_min || dev_num > dev_num_max)  {
		dev_err(bus->dev, "%s: invalid dev_num %d, wake supported %d\n",
			__func__, dev_num, slave->prop.wake_capable);
		return;
	}

	if (sdw->link_res->hw_ops->program_sdi && wake_capable)
		sdw->link_res->hw_ops->program_sdi(sdw, dev_num);
}

static int sdw_master_read_intel_prop(struct sdw_bus *bus)
{
	struct sdw_master_prop *prop = &bus->prop;
	struct fwnode_handle *link;
	char name[32];
	u32 quirk_mask;

	 
	snprintf(name, sizeof(name),
		 "mipi-sdw-link-%d-subproperties", bus->link_id);

	link = device_get_named_child_node(bus->dev, name);
	if (!link) {
		dev_err(bus->dev, "Master node %s not found\n", name);
		return -EIO;
	}

	fwnode_property_read_u32(link,
				 "intel-sdw-ip-clock",
				 &prop->mclk_freq);

	 
	prop->mclk_freq /= 2;

	fwnode_property_read_u32(link,
				 "intel-quirk-mask",
				 &quirk_mask);

	if (quirk_mask & SDW_INTEL_QUIRK_MASK_BUS_DISABLE)
		prop->hw_disabled = true;

	prop->quirks = SDW_MASTER_QUIRKS_CLEAR_INITIAL_CLASH |
		SDW_MASTER_QUIRKS_CLEAR_INITIAL_PARITY;

	return 0;
}

static int intel_prop_read(struct sdw_bus *bus)
{
	 
	sdw_master_read_prop(bus);

	 
	sdw_master_read_intel_prop(bus);

	return 0;
}

static DEFINE_IDA(intel_peripheral_ida);

static int intel_get_device_num_ida(struct sdw_bus *bus, struct sdw_slave *slave)
{
	int bit;

	if (slave->prop.wake_capable || is_wake_capable(slave))
		return ida_alloc_range(&intel_peripheral_ida,
				       SDW_INTEL_DEV_NUM_IDA_MIN, SDW_MAX_DEVICES,
				       GFP_KERNEL);

	bit = find_first_zero_bit(slave->bus->assigned, SDW_MAX_DEVICES);
	if (bit == SDW_MAX_DEVICES)
		return -ENODEV;

	return bit;
}

static void intel_put_device_num_ida(struct sdw_bus *bus, struct sdw_slave *slave)
{
	if (slave->prop.wake_capable || is_wake_capable(slave))
		ida_free(&intel_peripheral_ida, slave->dev_num);
}

static struct sdw_master_ops sdw_intel_ops = {
	.read_prop = intel_prop_read,
	.override_adr = sdw_dmi_override_adr,
	.xfer_msg = cdns_xfer_msg,
	.xfer_msg_defer = cdns_xfer_msg_defer,
	.set_bus_conf = cdns_bus_conf,
	.pre_bank_switch = generic_pre_bank_switch,
	.post_bank_switch = generic_post_bank_switch,
	.read_ping_status = cdns_read_ping_status,
	.get_device_num =  intel_get_device_num_ida,
	.put_device_num = intel_put_device_num_ida,
	.new_peripheral_assigned = generic_new_peripheral_assigned,
};

 
static int intel_link_probe(struct auxiliary_device *auxdev,
			    const struct auxiliary_device_id *aux_dev_id)

{
	struct device *dev = &auxdev->dev;
	struct sdw_intel_link_dev *ldev = auxiliary_dev_to_sdw_intel_link_dev(auxdev);
	struct sdw_intel *sdw;
	struct sdw_cdns *cdns;
	struct sdw_bus *bus;
	int ret;

	sdw = devm_kzalloc(dev, sizeof(*sdw), GFP_KERNEL);
	if (!sdw)
		return -ENOMEM;

	cdns = &sdw->cdns;
	bus = &cdns->bus;

	sdw->instance = auxdev->id;
	sdw->link_res = &ldev->link_res;
	cdns->dev = dev;
	cdns->registers = sdw->link_res->registers;
	cdns->ip_offset = sdw->link_res->ip_offset;
	cdns->instance = sdw->instance;
	cdns->msg_count = 0;

	bus->link_id = auxdev->id;
	bus->clk_stop_timeout = 1;

	sdw_cdns_probe(cdns);

	 
	bus->ops = &sdw_intel_ops;

	 
	auxiliary_set_drvdata(auxdev, cdns);

	 
	sdw->cdns.bus.compute_params = sdw_compute_params;

	 
	dev_pm_set_driver_flags(dev, DPM_FLAG_SMART_SUSPEND);

	ret = sdw_bus_master_add(bus, dev, dev->fwnode);
	if (ret) {
		dev_err(dev, "sdw_bus_master_add fail: %d\n", ret);
		return ret;
	}

	if (bus->prop.hw_disabled)
		dev_info(dev,
			 "SoundWire master %d is disabled, will be ignored\n",
			 bus->link_id);
	 
	bus->prop.err_threshold = 0;

	return 0;
}

int intel_link_startup(struct auxiliary_device *auxdev)
{
	struct device *dev = &auxdev->dev;
	struct sdw_cdns *cdns = auxiliary_get_drvdata(auxdev);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	struct sdw_bus *bus = &cdns->bus;
	int link_flags;
	bool multi_link;
	u32 clock_stop_quirks;
	int ret;

	if (bus->prop.hw_disabled) {
		dev_info(dev,
			 "SoundWire master %d is disabled, ignoring\n",
			 sdw->instance);
		return 0;
	}

	link_flags = md_flags >> (bus->link_id * 8);
	multi_link = !(link_flags & SDW_INTEL_MASTER_DISABLE_MULTI_LINK);
	if (!multi_link) {
		dev_dbg(dev, "Multi-link is disabled\n");
	} else {
		 
		bus->hw_sync_min_links = 1;
	}
	bus->multi_link = multi_link;

	 
	ret = sdw_intel_link_power_up(sdw);
	if (ret)
		goto err_init;

	 
	ret = sdw_intel_register_dai(sdw);
	if (ret) {
		dev_err(dev, "DAI registration failed: %d\n", ret);
		goto err_power_up;
	}

	sdw_intel_debugfs_init(sdw);

	 
	if (!(link_flags & SDW_INTEL_MASTER_DISABLE_PM_RUNTIME)) {
		pm_runtime_set_autosuspend_delay(dev,
						 INTEL_MASTER_SUSPEND_DELAY_MS);
		pm_runtime_use_autosuspend(dev);
		pm_runtime_mark_last_busy(dev);

		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);

		pm_runtime_resume(bus->dev);
	}

	 
	ret = sdw_intel_start_bus(sdw);
	if (ret) {
		dev_err(dev, "bus start failed: %d\n", ret);
		goto err_pm_runtime;
	}

	clock_stop_quirks = sdw->link_res->clock_stop_quirks;
	if (clock_stop_quirks & SDW_INTEL_CLK_STOP_NOT_ALLOWED) {
		 
		pm_runtime_get_noresume(dev);
	}

	 
	if (!(link_flags & SDW_INTEL_MASTER_DISABLE_PM_RUNTIME_IDLE)) {
		pm_runtime_mark_last_busy(bus->dev);
		pm_runtime_mark_last_busy(dev);
		pm_runtime_idle(dev);
	}

	sdw->startup_done = true;
	return 0;

err_pm_runtime:
	if (!(link_flags & SDW_INTEL_MASTER_DISABLE_PM_RUNTIME))
		pm_runtime_disable(dev);
err_power_up:
	sdw_intel_link_power_down(sdw);
err_init:
	return ret;
}

static void intel_link_remove(struct auxiliary_device *auxdev)
{
	struct sdw_cdns *cdns = auxiliary_get_drvdata(auxdev);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	struct sdw_bus *bus = &cdns->bus;

	 
	if (!bus->prop.hw_disabled) {
		sdw_intel_debugfs_exit(sdw);
		sdw_cdns_enable_interrupt(cdns, false);
	}
	sdw_bus_master_delete(bus);
}

int intel_link_process_wakeen_event(struct auxiliary_device *auxdev)
{
	struct device *dev = &auxdev->dev;
	struct sdw_intel *sdw;
	struct sdw_bus *bus;

	sdw = auxiliary_get_drvdata(auxdev);
	bus = &sdw->cdns.bus;

	if (bus->prop.hw_disabled || !sdw->startup_done) {
		dev_dbg(dev, "SoundWire master %d is disabled or not-started, ignoring\n",
			bus->link_id);
		return 0;
	}

	if (!sdw_intel_shim_check_wake(sdw))
		return 0;

	 
	sdw_intel_shim_wake(sdw, false);

	 
	pm_request_resume(dev);

	return 0;
}

 

static int intel_resume_child_device(struct device *dev, void *data)
{
	int ret;
	struct sdw_slave *slave = dev_to_sdw_dev(dev);

	if (!slave->probed) {
		dev_dbg(dev, "skipping device, no probed driver\n");
		return 0;
	}
	if (!slave->dev_num_sticky) {
		dev_dbg(dev, "skipping device, never detected on bus\n");
		return 0;
	}

	ret = pm_request_resume(dev);
	if (ret < 0) {
		dev_err(dev, "%s: pm_request_resume failed: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int __maybe_unused intel_pm_prepare(struct device *dev)
{
	struct sdw_cdns *cdns = dev_get_drvdata(dev);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	struct sdw_bus *bus = &cdns->bus;
	u32 clock_stop_quirks;
	int ret;

	if (bus->prop.hw_disabled || !sdw->startup_done) {
		dev_dbg(dev, "SoundWire master %d is disabled or not-started, ignoring\n",
			bus->link_id);
		return 0;
	}

	clock_stop_quirks = sdw->link_res->clock_stop_quirks;

	if (pm_runtime_suspended(dev) &&
	    pm_runtime_suspended(dev->parent) &&
	    ((clock_stop_quirks & SDW_INTEL_CLK_STOP_BUS_RESET) ||
	     !clock_stop_quirks)) {
		 

		 

		 
		ret = pm_request_resume(dev);
		if (ret < 0) {
			dev_err(dev, "%s: pm_request_resume failed: %d\n", __func__, ret);
			return 0;
		}

		 
		ret = device_for_each_child(bus->dev, NULL, intel_resume_child_device);

		if (ret < 0)
			dev_err(dev, "%s: intel_resume_child_device failed: %d\n", __func__, ret);
	}

	return 0;
}

static int __maybe_unused intel_suspend(struct device *dev)
{
	struct sdw_cdns *cdns = dev_get_drvdata(dev);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	struct sdw_bus *bus = &cdns->bus;
	u32 clock_stop_quirks;
	int ret;

	if (bus->prop.hw_disabled || !sdw->startup_done) {
		dev_dbg(dev, "SoundWire master %d is disabled or not-started, ignoring\n",
			bus->link_id);
		return 0;
	}

	if (pm_runtime_suspended(dev)) {
		dev_dbg(dev, "pm_runtime status: suspended\n");

		clock_stop_quirks = sdw->link_res->clock_stop_quirks;

		if ((clock_stop_quirks & SDW_INTEL_CLK_STOP_BUS_RESET) ||
		    !clock_stop_quirks) {

			if (pm_runtime_suspended(dev->parent)) {
				 
				dev_err(dev, "%s: invalid config: parent is suspended\n", __func__);
			} else {
				sdw_intel_shim_wake(sdw, false);
			}
		}

		return 0;
	}

	ret = sdw_intel_stop_bus(sdw, false);
	if (ret < 0) {
		dev_err(dev, "%s: cannot stop bus: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int __maybe_unused intel_suspend_runtime(struct device *dev)
{
	struct sdw_cdns *cdns = dev_get_drvdata(dev);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	struct sdw_bus *bus = &cdns->bus;
	u32 clock_stop_quirks;
	int ret;

	if (bus->prop.hw_disabled || !sdw->startup_done) {
		dev_dbg(dev, "SoundWire master %d is disabled or not-started, ignoring\n",
			bus->link_id);
		return 0;
	}

	clock_stop_quirks = sdw->link_res->clock_stop_quirks;

	if (clock_stop_quirks & SDW_INTEL_CLK_STOP_TEARDOWN) {
		ret = sdw_intel_stop_bus(sdw, false);
		if (ret < 0) {
			dev_err(dev, "%s: cannot stop bus during teardown: %d\n",
				__func__, ret);
			return ret;
		}
	} else if (clock_stop_quirks & SDW_INTEL_CLK_STOP_BUS_RESET || !clock_stop_quirks) {
		ret = sdw_intel_stop_bus(sdw, true);
		if (ret < 0) {
			dev_err(dev, "%s: cannot stop bus during clock_stop: %d\n",
				__func__, ret);
			return ret;
		}
	} else {
		dev_err(dev, "%s clock_stop_quirks %x unsupported\n",
			__func__, clock_stop_quirks);
		ret = -EINVAL;
	}

	return ret;
}

static int __maybe_unused intel_resume(struct device *dev)
{
	struct sdw_cdns *cdns = dev_get_drvdata(dev);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	struct sdw_bus *bus = &cdns->bus;
	int link_flags;
	int ret;

	if (bus->prop.hw_disabled || !sdw->startup_done) {
		dev_dbg(dev, "SoundWire master %d is disabled or not-started, ignoring\n",
			bus->link_id);
		return 0;
	}

	link_flags = md_flags >> (bus->link_id * 8);

	if (pm_runtime_suspended(dev)) {
		dev_dbg(dev, "pm_runtime status was suspended, forcing active\n");

		 
		pm_runtime_disable(dev);
		pm_runtime_set_active(dev);
		pm_runtime_mark_last_busy(dev);
		pm_runtime_enable(dev);

		pm_runtime_resume(bus->dev);

		link_flags = md_flags >> (bus->link_id * 8);

		if (!(link_flags & SDW_INTEL_MASTER_DISABLE_PM_RUNTIME_IDLE))
			pm_runtime_idle(dev);
	}

	ret = sdw_intel_link_power_up(sdw);
	if (ret) {
		dev_err(dev, "%s failed: %d\n", __func__, ret);
		return ret;
	}

	 
	sdw_clear_slave_status(bus, SDW_UNATTACH_REQUEST_MASTER_RESET);

	ret = sdw_intel_start_bus(sdw);
	if (ret < 0) {
		dev_err(dev, "cannot start bus during resume\n");
		sdw_intel_link_power_down(sdw);
		return ret;
	}

	 
	pm_runtime_mark_last_busy(bus->dev);
	pm_runtime_mark_last_busy(dev);

	return 0;
}

static int __maybe_unused intel_resume_runtime(struct device *dev)
{
	struct sdw_cdns *cdns = dev_get_drvdata(dev);
	struct sdw_intel *sdw = cdns_to_intel(cdns);
	struct sdw_bus *bus = &cdns->bus;
	u32 clock_stop_quirks;
	int ret;

	if (bus->prop.hw_disabled || !sdw->startup_done) {
		dev_dbg(dev, "SoundWire master %d is disabled or not-started, ignoring\n",
			bus->link_id);
		return 0;
	}

	 
	sdw_intel_shim_wake(sdw, false);

	clock_stop_quirks = sdw->link_res->clock_stop_quirks;

	if (clock_stop_quirks & SDW_INTEL_CLK_STOP_TEARDOWN) {
		ret = sdw_intel_link_power_up(sdw);
		if (ret) {
			dev_err(dev, "%s: power_up failed after teardown: %d\n", __func__, ret);
			return ret;
		}

		 
		sdw_clear_slave_status(bus, SDW_UNATTACH_REQUEST_MASTER_RESET);

		ret = sdw_intel_start_bus(sdw);
		if (ret < 0) {
			dev_err(dev, "%s: cannot start bus after teardown: %d\n", __func__, ret);
			sdw_intel_link_power_down(sdw);
			return ret;
		}

	} else if (clock_stop_quirks & SDW_INTEL_CLK_STOP_BUS_RESET) {
		ret = sdw_intel_link_power_up(sdw);
		if (ret) {
			dev_err(dev, "%s: power_up failed after bus reset: %d\n", __func__, ret);
			return ret;
		}

		ret = sdw_intel_start_bus_after_reset(sdw);
		if (ret < 0) {
			dev_err(dev, "%s: cannot start bus after reset: %d\n", __func__, ret);
			sdw_intel_link_power_down(sdw);
			return ret;
		}
	} else if (!clock_stop_quirks) {

		sdw_intel_check_clock_stop(sdw);

		ret = sdw_intel_link_power_up(sdw);
		if (ret) {
			dev_err(dev, "%s: power_up failed: %d\n", __func__, ret);
			return ret;
		}

		ret = sdw_intel_start_bus_after_clock_stop(sdw);
		if (ret < 0) {
			dev_err(dev, "%s: cannot start bus after clock stop: %d\n", __func__, ret);
			sdw_intel_link_power_down(sdw);
			return ret;
		}
	} else {
		dev_err(dev, "%s: clock_stop_quirks %x unsupported\n",
			__func__, clock_stop_quirks);
		ret = -EINVAL;
	}

	return ret;
}

static const struct dev_pm_ops intel_pm = {
	.prepare = intel_pm_prepare,
	SET_SYSTEM_SLEEP_PM_OPS(intel_suspend, intel_resume)
	SET_RUNTIME_PM_OPS(intel_suspend_runtime, intel_resume_runtime, NULL)
};

static const struct auxiliary_device_id intel_link_id_table[] = {
	{ .name = "soundwire_intel.link" },
	{},
};
MODULE_DEVICE_TABLE(auxiliary, intel_link_id_table);

static struct auxiliary_driver sdw_intel_drv = {
	.probe = intel_link_probe,
	.remove = intel_link_remove,
	.driver = {
		 
		.pm = &intel_pm,
	},
	.id_table = intel_link_id_table
};
module_auxiliary_driver(sdw_intel_drv);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel Soundwire Link Driver");
