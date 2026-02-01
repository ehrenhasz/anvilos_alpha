
 

#define pr_fmt(fmt) "pciehp: " fmt
#define dev_fmt pr_fmt

#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pci.h>
#include "pciehp.h"

#include "../pci.h"

 
bool pciehp_poll_mode;
int pciehp_poll_time;

 
module_param(pciehp_poll_mode, bool, 0644);
module_param(pciehp_poll_time, int, 0644);
MODULE_PARM_DESC(pciehp_poll_mode, "Using polling mechanism for hot-plug events or not");
MODULE_PARM_DESC(pciehp_poll_time, "Polling mechanism frequency, in seconds");

static int set_attention_status(struct hotplug_slot *slot, u8 value);
static int get_power_status(struct hotplug_slot *slot, u8 *value);
static int get_latch_status(struct hotplug_slot *slot, u8 *value);
static int get_adapter_status(struct hotplug_slot *slot, u8 *value);

static int init_slot(struct controller *ctrl)
{
	struct hotplug_slot_ops *ops;
	char name[SLOT_NAME_SIZE];
	int retval;

	 
	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return -ENOMEM;

	ops->enable_slot = pciehp_sysfs_enable_slot;
	ops->disable_slot = pciehp_sysfs_disable_slot;
	ops->get_power_status = get_power_status;
	ops->get_adapter_status = get_adapter_status;
	ops->reset_slot = pciehp_reset_slot;
	if (MRL_SENS(ctrl))
		ops->get_latch_status = get_latch_status;
	if (ATTN_LED(ctrl)) {
		ops->get_attention_status = pciehp_get_attention_status;
		ops->set_attention_status = set_attention_status;
	} else if (ctrl->pcie->port->hotplug_user_indicators) {
		ops->get_attention_status = pciehp_get_raw_indicator_status;
		ops->set_attention_status = pciehp_set_raw_indicator_status;
	}

	 
	ctrl->hotplug_slot.ops = ops;
	snprintf(name, SLOT_NAME_SIZE, "%u", PSN(ctrl));

	retval = pci_hp_initialize(&ctrl->hotplug_slot,
				   ctrl->pcie->port->subordinate, 0, name);
	if (retval) {
		ctrl_err(ctrl, "pci_hp_initialize failed: error %d\n", retval);
		kfree(ops);
	}
	return retval;
}

static void cleanup_slot(struct controller *ctrl)
{
	struct hotplug_slot *hotplug_slot = &ctrl->hotplug_slot;

	pci_hp_destroy(hotplug_slot);
	kfree(hotplug_slot->ops);
}

 
static int set_attention_status(struct hotplug_slot *hotplug_slot, u8 status)
{
	struct controller *ctrl = to_ctrl(hotplug_slot);
	struct pci_dev *pdev = ctrl->pcie->port;

	if (status)
		status <<= PCI_EXP_SLTCTL_ATTN_IND_SHIFT;
	else
		status = PCI_EXP_SLTCTL_ATTN_IND_OFF;

	pci_config_pm_runtime_get(pdev);
	pciehp_set_indicators(ctrl, INDICATOR_NOOP, status);
	pci_config_pm_runtime_put(pdev);
	return 0;
}

static int get_power_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct controller *ctrl = to_ctrl(hotplug_slot);
	struct pci_dev *pdev = ctrl->pcie->port;

	pci_config_pm_runtime_get(pdev);
	pciehp_get_power_status(ctrl, value);
	pci_config_pm_runtime_put(pdev);
	return 0;
}

static int get_latch_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct controller *ctrl = to_ctrl(hotplug_slot);
	struct pci_dev *pdev = ctrl->pcie->port;

	pci_config_pm_runtime_get(pdev);
	pciehp_get_latch_status(ctrl, value);
	pci_config_pm_runtime_put(pdev);
	return 0;
}

static int get_adapter_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct controller *ctrl = to_ctrl(hotplug_slot);
	struct pci_dev *pdev = ctrl->pcie->port;
	int ret;

	pci_config_pm_runtime_get(pdev);
	ret = pciehp_card_present_or_link_active(ctrl);
	pci_config_pm_runtime_put(pdev);
	if (ret < 0)
		return ret;

	*value = ret;
	return 0;
}

 
static void pciehp_check_presence(struct controller *ctrl)
{
	int occupied;

	down_read_nested(&ctrl->reset_lock, ctrl->depth);
	mutex_lock(&ctrl->state_lock);

	occupied = pciehp_card_present_or_link_active(ctrl);
	if ((occupied > 0 && (ctrl->state == OFF_STATE ||
			  ctrl->state == BLINKINGON_STATE)) ||
	    (!occupied && (ctrl->state == ON_STATE ||
			   ctrl->state == BLINKINGOFF_STATE)))
		pciehp_request(ctrl, PCI_EXP_SLTSTA_PDC);

	mutex_unlock(&ctrl->state_lock);
	up_read(&ctrl->reset_lock);
}

static int pciehp_probe(struct pcie_device *dev)
{
	int rc;
	struct controller *ctrl;

	 
	if (dev->service != PCIE_PORT_SERVICE_HP)
		return -ENODEV;

	if (!dev->port->subordinate) {
		 
		pci_err(dev->port,
			"Hotplug bridge without secondary bus, ignoring\n");
		return -ENODEV;
	}

	ctrl = pcie_init(dev);
	if (!ctrl) {
		pci_err(dev->port, "Controller initialization failed\n");
		return -ENODEV;
	}
	set_service_data(dev, ctrl);

	 
	rc = init_slot(ctrl);
	if (rc) {
		if (rc == -EBUSY)
			ctrl_warn(ctrl, "Slot already registered by another hotplug driver\n");
		else
			ctrl_err(ctrl, "Slot initialization failed (%d)\n", rc);
		goto err_out_release_ctlr;
	}

	 
	rc = pcie_init_notification(ctrl);
	if (rc) {
		ctrl_err(ctrl, "Notification initialization failed (%d)\n", rc);
		goto err_out_free_ctrl_slot;
	}

	 
	rc = pci_hp_add(&ctrl->hotplug_slot);
	if (rc) {
		ctrl_err(ctrl, "Publication to user space failed (%d)\n", rc);
		goto err_out_shutdown_notification;
	}

	pciehp_check_presence(ctrl);

	return 0;

err_out_shutdown_notification:
	pcie_shutdown_notification(ctrl);
err_out_free_ctrl_slot:
	cleanup_slot(ctrl);
err_out_release_ctlr:
	pciehp_release_ctrl(ctrl);
	return -ENODEV;
}

static void pciehp_remove(struct pcie_device *dev)
{
	struct controller *ctrl = get_service_data(dev);

	pci_hp_del(&ctrl->hotplug_slot);
	pcie_shutdown_notification(ctrl);
	cleanup_slot(ctrl);
	pciehp_release_ctrl(ctrl);
}

#ifdef CONFIG_PM
static bool pme_is_native(struct pcie_device *dev)
{
	const struct pci_host_bridge *host;

	host = pci_find_host_bridge(dev->port->bus);
	return pcie_ports_native || host->native_pme;
}

static void pciehp_disable_interrupt(struct pcie_device *dev)
{
	 
	if (pme_is_native(dev))
		pcie_disable_interrupt(get_service_data(dev));
}

#ifdef CONFIG_PM_SLEEP
static int pciehp_suspend(struct pcie_device *dev)
{
	 
	if (dev_pm_skip_suspend(&dev->port->dev))
		return 0;

	pciehp_disable_interrupt(dev);
	return 0;
}

static int pciehp_resume_noirq(struct pcie_device *dev)
{
	struct controller *ctrl = get_service_data(dev);

	 
	ctrl->cmd_started = jiffies;
	ctrl->cmd_busy = true;

	 
	if (ctrl->state == ON_STATE || ctrl->state == BLINKINGOFF_STATE)
		pcie_clear_hotplug_events(ctrl);

	return 0;
}
#endif

static int pciehp_resume(struct pcie_device *dev)
{
	struct controller *ctrl = get_service_data(dev);

	if (pme_is_native(dev))
		pcie_enable_interrupt(ctrl);

	pciehp_check_presence(ctrl);

	return 0;
}

static int pciehp_runtime_suspend(struct pcie_device *dev)
{
	pciehp_disable_interrupt(dev);
	return 0;
}

static int pciehp_runtime_resume(struct pcie_device *dev)
{
	struct controller *ctrl = get_service_data(dev);

	 
	ctrl->cmd_started = jiffies;
	ctrl->cmd_busy = true;

	 
	if ((ctrl->state == ON_STATE || ctrl->state == BLINKINGOFF_STATE) &&
	     pme_is_native(dev))
		pcie_clear_hotplug_events(ctrl);

	return pciehp_resume(dev);
}
#endif  

static struct pcie_port_service_driver hpdriver_portdrv = {
	.name		= "pciehp",
	.port_type	= PCIE_ANY_PORT,
	.service	= PCIE_PORT_SERVICE_HP,

	.probe		= pciehp_probe,
	.remove		= pciehp_remove,

#ifdef	CONFIG_PM
#ifdef	CONFIG_PM_SLEEP
	.suspend	= pciehp_suspend,
	.resume_noirq	= pciehp_resume_noirq,
	.resume		= pciehp_resume,
#endif
	.runtime_suspend = pciehp_runtime_suspend,
	.runtime_resume	= pciehp_runtime_resume,
#endif	 

	.slot_reset	= pciehp_slot_reset,
};

int __init pcie_hp_init(void)
{
	int retval = 0;

	retval = pcie_port_service_register(&hpdriver_portdrv);
	pr_debug("pcie_port_service_register = %d\n", retval);
	if (retval)
		pr_debug("Failure to register service\n");

	return retval;
}
