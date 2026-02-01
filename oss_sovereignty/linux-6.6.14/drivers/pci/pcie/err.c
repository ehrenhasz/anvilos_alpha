
 

#define dev_fmt(fmt) "AER: " fmt

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/aer.h>
#include "portdrv.h"
#include "../pci.h"

static pci_ers_result_t merge_result(enum pci_ers_result orig,
				  enum pci_ers_result new)
{
	if (new == PCI_ERS_RESULT_NO_AER_DRIVER)
		return PCI_ERS_RESULT_NO_AER_DRIVER;

	if (new == PCI_ERS_RESULT_NONE)
		return orig;

	switch (orig) {
	case PCI_ERS_RESULT_CAN_RECOVER:
	case PCI_ERS_RESULT_RECOVERED:
		orig = new;
		break;
	case PCI_ERS_RESULT_DISCONNECT:
		if (new == PCI_ERS_RESULT_NEED_RESET)
			orig = PCI_ERS_RESULT_NEED_RESET;
		break;
	default:
		break;
	}

	return orig;
}

static int report_error_detected(struct pci_dev *dev,
				 pci_channel_state_t state,
				 enum pci_ers_result *result)
{
	struct pci_driver *pdrv;
	pci_ers_result_t vote;
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	pdrv = dev->driver;
	if (pci_dev_is_disconnected(dev)) {
		vote = PCI_ERS_RESULT_DISCONNECT;
	} else if (!pci_dev_set_io_state(dev, state)) {
		pci_info(dev, "can't recover (state transition %u -> %u invalid)\n",
			dev->error_state, state);
		vote = PCI_ERS_RESULT_NONE;
	} else if (!pdrv || !pdrv->err_handler ||
		   !pdrv->err_handler->error_detected) {
		 
		if (dev->hdr_type != PCI_HEADER_TYPE_BRIDGE) {
			vote = PCI_ERS_RESULT_NO_AER_DRIVER;
			pci_info(dev, "can't recover (no error_detected callback)\n");
		} else {
			vote = PCI_ERS_RESULT_NONE;
		}
	} else {
		err_handler = pdrv->err_handler;
		vote = err_handler->error_detected(dev, state);
	}
	pci_uevent_ers(dev, vote);
	*result = merge_result(*result, vote);
	device_unlock(&dev->dev);
	return 0;
}

static int report_frozen_detected(struct pci_dev *dev, void *data)
{
	return report_error_detected(dev, pci_channel_io_frozen, data);
}

static int report_normal_detected(struct pci_dev *dev, void *data)
{
	return report_error_detected(dev, pci_channel_io_normal, data);
}

static int report_mmio_enabled(struct pci_dev *dev, void *data)
{
	struct pci_driver *pdrv;
	pci_ers_result_t vote, *result = data;
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	pdrv = dev->driver;
	if (!pdrv ||
		!pdrv->err_handler ||
		!pdrv->err_handler->mmio_enabled)
		goto out;

	err_handler = pdrv->err_handler;
	vote = err_handler->mmio_enabled(dev);
	*result = merge_result(*result, vote);
out:
	device_unlock(&dev->dev);
	return 0;
}

static int report_slot_reset(struct pci_dev *dev, void *data)
{
	struct pci_driver *pdrv;
	pci_ers_result_t vote, *result = data;
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	pdrv = dev->driver;
	if (!pdrv ||
		!pdrv->err_handler ||
		!pdrv->err_handler->slot_reset)
		goto out;

	err_handler = pdrv->err_handler;
	vote = err_handler->slot_reset(dev);
	*result = merge_result(*result, vote);
out:
	device_unlock(&dev->dev);
	return 0;
}

static int report_resume(struct pci_dev *dev, void *data)
{
	struct pci_driver *pdrv;
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	pdrv = dev->driver;
	if (!pci_dev_set_io_state(dev, pci_channel_io_normal) ||
		!pdrv ||
		!pdrv->err_handler ||
		!pdrv->err_handler->resume)
		goto out;

	err_handler = pdrv->err_handler;
	err_handler->resume(dev);
out:
	pci_uevent_ers(dev, PCI_ERS_RESULT_RECOVERED);
	device_unlock(&dev->dev);
	return 0;
}

 
static void pci_walk_bridge(struct pci_dev *bridge,
			    int (*cb)(struct pci_dev *, void *),
			    void *userdata)
{
	if (bridge->subordinate)
		pci_walk_bus(bridge->subordinate, cb, userdata);
	else
		cb(bridge, userdata);
}

pci_ers_result_t pcie_do_recovery(struct pci_dev *dev,
		pci_channel_state_t state,
		pci_ers_result_t (*reset_subordinates)(struct pci_dev *pdev))
{
	int type = pci_pcie_type(dev);
	struct pci_dev *bridge;
	pci_ers_result_t status = PCI_ERS_RESULT_CAN_RECOVER;
	struct pci_host_bridge *host = pci_find_host_bridge(dev->bus);

	 
	if (type == PCI_EXP_TYPE_ROOT_PORT ||
	    type == PCI_EXP_TYPE_DOWNSTREAM ||
	    type == PCI_EXP_TYPE_RC_EC ||
	    type == PCI_EXP_TYPE_RC_END)
		bridge = dev;
	else
		bridge = pci_upstream_bridge(dev);

	pci_dbg(bridge, "broadcast error_detected message\n");
	if (state == pci_channel_io_frozen) {
		pci_walk_bridge(bridge, report_frozen_detected, &status);
		if (reset_subordinates(bridge) != PCI_ERS_RESULT_RECOVERED) {
			pci_warn(bridge, "subordinate device reset failed\n");
			goto failed;
		}
	} else {
		pci_walk_bridge(bridge, report_normal_detected, &status);
	}

	if (status == PCI_ERS_RESULT_CAN_RECOVER) {
		status = PCI_ERS_RESULT_RECOVERED;
		pci_dbg(bridge, "broadcast mmio_enabled message\n");
		pci_walk_bridge(bridge, report_mmio_enabled, &status);
	}

	if (status == PCI_ERS_RESULT_NEED_RESET) {
		 
		status = PCI_ERS_RESULT_RECOVERED;
		pci_dbg(bridge, "broadcast slot_reset message\n");
		pci_walk_bridge(bridge, report_slot_reset, &status);
	}

	if (status != PCI_ERS_RESULT_RECOVERED)
		goto failed;

	pci_dbg(bridge, "broadcast resume message\n");
	pci_walk_bridge(bridge, report_resume, &status);

	 
	if (host->native_aer || pcie_ports_native) {
		pcie_clear_device_status(dev);
		pci_aer_clear_nonfatal_status(dev);
	}
	pci_info(bridge, "device recovery successful\n");
	return status;

failed:
	pci_uevent_ers(bridge, PCI_ERS_RESULT_DISCONNECT);

	 
	pci_info(bridge, "device recovery failed\n");

	return status;
}
