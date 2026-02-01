


#include <linux/acpi.h>
#include <linux/soundwire/sdw_registers.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_intel.h>
#include "cadence_master.h"
#include "bus.h"
#include "intel.h"

int intel_start_bus(struct sdw_intel *sdw)
{
	struct device *dev = sdw->cdns.dev;
	struct sdw_cdns *cdns = &sdw->cdns;
	struct sdw_bus *bus = &cdns->bus;
	int ret;

	 
	if (bus->multi_link)
		sdw_intel_sync_arm(sdw);

	ret = sdw_cdns_init(cdns);
	if (ret < 0) {
		dev_err(dev, "%s: unable to initialize Cadence IP: %d\n", __func__, ret);
		return ret;
	}

	sdw_cdns_config_update(cdns);

	if (bus->multi_link) {
		ret = sdw_intel_sync_go(sdw);
		if (ret < 0) {
			dev_err(dev, "%s: sync go failed: %d\n", __func__, ret);
			return ret;
		}
	}

	ret = sdw_cdns_config_update_set_wait(cdns);
	if (ret < 0) {
		dev_err(dev, "%s: CONFIG_UPDATE BIT still set\n", __func__);
		return ret;
	}

	ret = sdw_cdns_exit_reset(cdns);
	if (ret < 0) {
		dev_err(dev, "%s: unable to exit bus reset sequence: %d\n", __func__, ret);
		return ret;
	}

	ret = sdw_cdns_enable_interrupt(cdns, true);
	if (ret < 0) {
		dev_err(dev, "%s: cannot enable interrupts: %d\n", __func__, ret);
		return ret;
	}

	sdw_cdns_check_self_clearing_bits(cdns, __func__,
					  true, INTEL_MASTER_RESET_ITERATIONS);

	return 0;
}

int intel_start_bus_after_reset(struct sdw_intel *sdw)
{
	struct device *dev = sdw->cdns.dev;
	struct sdw_cdns *cdns = &sdw->cdns;
	struct sdw_bus *bus = &cdns->bus;
	bool clock_stop0;
	int status;
	int ret;

	 
	clock_stop0 = sdw_cdns_is_clock_stop(&sdw->cdns);

	if (!clock_stop0) {

		 

		status = SDW_UNATTACH_REQUEST_MASTER_RESET;
		sdw_clear_slave_status(bus, status);

		 
		if (bus->multi_link)
			sdw_intel_sync_arm(sdw);

		 
		sdw_cdns_init(&sdw->cdns);

	} else {
		ret = sdw_cdns_enable_interrupt(cdns, true);
		if (ret < 0) {
			dev_err(dev, "cannot enable interrupts during resume\n");
			return ret;
		}
	}

	ret = sdw_cdns_clock_restart(cdns, !clock_stop0);
	if (ret < 0) {
		dev_err(dev, "unable to restart clock during resume\n");
		if (!clock_stop0)
			sdw_cdns_enable_interrupt(cdns, false);
		return ret;
	}

	if (!clock_stop0) {
		sdw_cdns_config_update(cdns);

		if (bus->multi_link) {
			ret = sdw_intel_sync_go(sdw);
			if (ret < 0) {
				dev_err(sdw->cdns.dev, "sync go failed during resume\n");
				return ret;
			}
		}

		ret = sdw_cdns_config_update_set_wait(cdns);
		if (ret < 0) {
			dev_err(dev, "%s: CONFIG_UPDATE BIT still set\n", __func__);
			return ret;
		}

		ret = sdw_cdns_exit_reset(cdns);
		if (ret < 0) {
			dev_err(dev, "unable to exit bus reset sequence during resume\n");
			return ret;
		}

		ret = sdw_cdns_enable_interrupt(cdns, true);
		if (ret < 0) {
			dev_err(dev, "cannot enable interrupts during resume\n");
			return ret;
		}

	}
	sdw_cdns_check_self_clearing_bits(cdns, __func__, true, INTEL_MASTER_RESET_ITERATIONS);

	return 0;
}

void intel_check_clock_stop(struct sdw_intel *sdw)
{
	struct device *dev = sdw->cdns.dev;
	bool clock_stop0;

	clock_stop0 = sdw_cdns_is_clock_stop(&sdw->cdns);
	if (!clock_stop0)
		dev_err(dev, "%s: invalid configuration, clock was not stopped\n", __func__);
}

int intel_start_bus_after_clock_stop(struct sdw_intel *sdw)
{
	struct device *dev = sdw->cdns.dev;
	struct sdw_cdns *cdns = &sdw->cdns;
	int ret;

	ret = sdw_cdns_clock_restart(cdns, false);
	if (ret < 0) {
		dev_err(dev, "%s: unable to restart clock: %d\n", __func__, ret);
		return ret;
	}

	ret = sdw_cdns_enable_interrupt(cdns, true);
	if (ret < 0) {
		dev_err(dev, "%s: cannot enable interrupts: %d\n", __func__, ret);
		return ret;
	}

	sdw_cdns_check_self_clearing_bits(cdns, __func__, true, INTEL_MASTER_RESET_ITERATIONS);

	return 0;
}

int intel_stop_bus(struct sdw_intel *sdw, bool clock_stop)
{
	struct device *dev = sdw->cdns.dev;
	struct sdw_cdns *cdns = &sdw->cdns;
	bool wake_enable = false;
	int ret;

	if (clock_stop) {
		ret = sdw_cdns_clock_stop(cdns, true);
		if (ret < 0)
			dev_err(dev, "%s: cannot stop clock: %d\n", __func__, ret);
		else
			wake_enable = true;
	}

	ret = sdw_cdns_enable_interrupt(cdns, false);
	if (ret < 0) {
		dev_err(dev, "%s: cannot disable interrupts: %d\n", __func__, ret);
		return ret;
	}

	ret = sdw_intel_link_power_down(sdw);
	if (ret) {
		dev_err(dev, "%s: Link power down failed: %d\n", __func__, ret);
		return ret;
	}

	sdw_intel_shim_wake(sdw, wake_enable);

	return 0;
}

 

int intel_pre_bank_switch(struct sdw_intel *sdw)
{
	struct sdw_cdns *cdns = &sdw->cdns;
	struct sdw_bus *bus = &cdns->bus;

	 
	if (!bus->multi_link)
		return 0;

	sdw_intel_sync_arm(sdw);

	return 0;
}

int intel_post_bank_switch(struct sdw_intel *sdw)
{
	struct sdw_cdns *cdns = &sdw->cdns;
	struct sdw_bus *bus = &cdns->bus;
	int ret = 0;

	 
	if (!bus->multi_link)
		return 0;

	mutex_lock(sdw->link_res->shim_lock);

	 
	if (sdw_intel_sync_check_cmdsync_unlocked(sdw))
		ret = sdw_intel_sync_go_unlocked(sdw);

	mutex_unlock(sdw->link_res->shim_lock);

	if (ret < 0)
		dev_err(sdw->cdns.dev, "Post bank switch failed: %d\n", ret);

	return ret;
}
