
 

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mempolicy.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/isolation.h>
#include <linux/cpu.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/kexec.h>
#include <linux/of_device.h>
#include <linux/acpi.h>
#include <linux/dma-map-ops.h>
#include <linux/iommu.h>
#include "pci.h"
#include "pcie/portdrv.h"

struct pci_dynid {
	struct list_head node;
	struct pci_device_id id;
};

 
int pci_add_dynid(struct pci_driver *drv,
		  unsigned int vendor, unsigned int device,
		  unsigned int subvendor, unsigned int subdevice,
		  unsigned int class, unsigned int class_mask,
		  unsigned long driver_data)
{
	struct pci_dynid *dynid;

	dynid = kzalloc(sizeof(*dynid), GFP_KERNEL);
	if (!dynid)
		return -ENOMEM;

	dynid->id.vendor = vendor;
	dynid->id.device = device;
	dynid->id.subvendor = subvendor;
	dynid->id.subdevice = subdevice;
	dynid->id.class = class;
	dynid->id.class_mask = class_mask;
	dynid->id.driver_data = driver_data;

	spin_lock(&drv->dynids.lock);
	list_add_tail(&dynid->node, &drv->dynids.list);
	spin_unlock(&drv->dynids.lock);

	return driver_attach(&drv->driver);
}
EXPORT_SYMBOL_GPL(pci_add_dynid);

static void pci_free_dynids(struct pci_driver *drv)
{
	struct pci_dynid *dynid, *n;

	spin_lock(&drv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &drv->dynids.list, node) {
		list_del(&dynid->node);
		kfree(dynid);
	}
	spin_unlock(&drv->dynids.lock);
}

 
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
					 struct pci_dev *dev)
{
	if (ids) {
		while (ids->vendor || ids->subvendor || ids->class_mask) {
			if (pci_match_one_device(ids, dev))
				return ids;
			ids++;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(pci_match_id);

static const struct pci_device_id pci_device_id_any = {
	.vendor = PCI_ANY_ID,
	.device = PCI_ANY_ID,
	.subvendor = PCI_ANY_ID,
	.subdevice = PCI_ANY_ID,
};

 
static const struct pci_device_id *pci_match_device(struct pci_driver *drv,
						    struct pci_dev *dev)
{
	struct pci_dynid *dynid;
	const struct pci_device_id *found_id = NULL, *ids;

	 
	if (dev->driver_override && strcmp(dev->driver_override, drv->name))
		return NULL;

	 
	spin_lock(&drv->dynids.lock);
	list_for_each_entry(dynid, &drv->dynids.list, node) {
		if (pci_match_one_device(&dynid->id, dev)) {
			found_id = &dynid->id;
			break;
		}
	}
	spin_unlock(&drv->dynids.lock);

	if (found_id)
		return found_id;

	for (ids = drv->id_table; (found_id = pci_match_id(ids, dev));
	     ids = found_id + 1) {
		 
		if (found_id->override_only) {
			if (dev->driver_override)
				return found_id;
		} else {
			return found_id;
		}
	}

	 
	if (dev->driver_override)
		return &pci_device_id_any;
	return NULL;
}

 
static ssize_t new_id_store(struct device_driver *driver, const char *buf,
			    size_t count)
{
	struct pci_driver *pdrv = to_pci_driver(driver);
	const struct pci_device_id *ids = pdrv->id_table;
	u32 vendor, device, subvendor = PCI_ANY_ID,
		subdevice = PCI_ANY_ID, class = 0, class_mask = 0;
	unsigned long driver_data = 0;
	int fields;
	int retval = 0;

	fields = sscanf(buf, "%x %x %x %x %x %x %lx",
			&vendor, &device, &subvendor, &subdevice,
			&class, &class_mask, &driver_data);
	if (fields < 2)
		return -EINVAL;

	if (fields != 7) {
		struct pci_dev *pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);
		if (!pdev)
			return -ENOMEM;

		pdev->vendor = vendor;
		pdev->device = device;
		pdev->subsystem_vendor = subvendor;
		pdev->subsystem_device = subdevice;
		pdev->class = class;

		if (pci_match_device(pdrv, pdev))
			retval = -EEXIST;

		kfree(pdev);

		if (retval)
			return retval;
	}

	 
	if (ids) {
		retval = -EINVAL;
		while (ids->vendor || ids->subvendor || ids->class_mask) {
			if (driver_data == ids->driver_data) {
				retval = 0;
				break;
			}
			ids++;
		}
		if (retval)	 
			return retval;
	}

	retval = pci_add_dynid(pdrv, vendor, device, subvendor, subdevice,
			       class, class_mask, driver_data);
	if (retval)
		return retval;
	return count;
}
static DRIVER_ATTR_WO(new_id);

 
static ssize_t remove_id_store(struct device_driver *driver, const char *buf,
			       size_t count)
{
	struct pci_dynid *dynid, *n;
	struct pci_driver *pdrv = to_pci_driver(driver);
	u32 vendor, device, subvendor = PCI_ANY_ID,
		subdevice = PCI_ANY_ID, class = 0, class_mask = 0;
	int fields;
	size_t retval = -ENODEV;

	fields = sscanf(buf, "%x %x %x %x %x %x",
			&vendor, &device, &subvendor, &subdevice,
			&class, &class_mask);
	if (fields < 2)
		return -EINVAL;

	spin_lock(&pdrv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &pdrv->dynids.list, node) {
		struct pci_device_id *id = &dynid->id;
		if ((id->vendor == vendor) &&
		    (id->device == device) &&
		    (subvendor == PCI_ANY_ID || id->subvendor == subvendor) &&
		    (subdevice == PCI_ANY_ID || id->subdevice == subdevice) &&
		    !((id->class ^ class) & class_mask)) {
			list_del(&dynid->node);
			kfree(dynid);
			retval = count;
			break;
		}
	}
	spin_unlock(&pdrv->dynids.lock);

	return retval;
}
static DRIVER_ATTR_WO(remove_id);

static struct attribute *pci_drv_attrs[] = {
	&driver_attr_new_id.attr,
	&driver_attr_remove_id.attr,
	NULL,
};
ATTRIBUTE_GROUPS(pci_drv);

struct drv_dev_and_id {
	struct pci_driver *drv;
	struct pci_dev *dev;
	const struct pci_device_id *id;
};

static long local_pci_probe(void *_ddi)
{
	struct drv_dev_and_id *ddi = _ddi;
	struct pci_dev *pci_dev = ddi->dev;
	struct pci_driver *pci_drv = ddi->drv;
	struct device *dev = &pci_dev->dev;
	int rc;

	 
	pm_runtime_get_sync(dev);
	pci_dev->driver = pci_drv;
	rc = pci_drv->probe(pci_dev, ddi->id);
	if (!rc)
		return rc;
	if (rc < 0) {
		pci_dev->driver = NULL;
		pm_runtime_put_sync(dev);
		return rc;
	}
	 
	pci_warn(pci_dev, "Driver probe function unexpectedly returned %d\n",
		 rc);
	return 0;
}

static bool pci_physfn_is_probed(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_IOV
	return dev->is_virtfn && dev->physfn->is_probed;
#else
	return false;
#endif
}

static int pci_call_probe(struct pci_driver *drv, struct pci_dev *dev,
			  const struct pci_device_id *id)
{
	int error, node, cpu;
	struct drv_dev_and_id ddi = { drv, dev, id };

	 
	node = dev_to_node(&dev->dev);
	dev->is_probed = 1;

	cpu_hotplug_disable();

	 
	if (node < 0 || node >= MAX_NUMNODES || !node_online(node) ||
	    pci_physfn_is_probed(dev)) {
		cpu = nr_cpu_ids;
	} else {
		cpumask_var_t wq_domain_mask;

		if (!zalloc_cpumask_var(&wq_domain_mask, GFP_KERNEL)) {
			error = -ENOMEM;
			goto out;
		}
		cpumask_and(wq_domain_mask,
			    housekeeping_cpumask(HK_TYPE_WQ),
			    housekeeping_cpumask(HK_TYPE_DOMAIN));

		cpu = cpumask_any_and(cpumask_of_node(node),
				      wq_domain_mask);
		free_cpumask_var(wq_domain_mask);
	}

	if (cpu < nr_cpu_ids)
		error = work_on_cpu(cpu, local_pci_probe, &ddi);
	else
		error = local_pci_probe(&ddi);
out:
	dev->is_probed = 0;
	cpu_hotplug_enable();
	return error;
}

 
static int __pci_device_probe(struct pci_driver *drv, struct pci_dev *pci_dev)
{
	const struct pci_device_id *id;
	int error = 0;

	if (drv->probe) {
		error = -ENODEV;

		id = pci_match_device(drv, pci_dev);
		if (id)
			error = pci_call_probe(drv, pci_dev, id);
	}
	return error;
}

int __weak pcibios_alloc_irq(struct pci_dev *dev)
{
	return 0;
}

void __weak pcibios_free_irq(struct pci_dev *dev)
{
}

#ifdef CONFIG_PCI_IOV
static inline bool pci_device_can_probe(struct pci_dev *pdev)
{
	return (!pdev->is_virtfn || pdev->physfn->sriov->drivers_autoprobe ||
		pdev->driver_override);
}
#else
static inline bool pci_device_can_probe(struct pci_dev *pdev)
{
	return true;
}
#endif

static int pci_device_probe(struct device *dev)
{
	int error;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = to_pci_driver(dev->driver);

	if (!pci_device_can_probe(pci_dev))
		return -ENODEV;

	pci_assign_irq(pci_dev);

	error = pcibios_alloc_irq(pci_dev);
	if (error < 0)
		return error;

	pci_dev_get(pci_dev);
	error = __pci_device_probe(drv, pci_dev);
	if (error) {
		pcibios_free_irq(pci_dev);
		pci_dev_put(pci_dev);
	}

	return error;
}

static void pci_device_remove(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;

	if (drv->remove) {
		pm_runtime_get_sync(dev);
		drv->remove(pci_dev);
		pm_runtime_put_noidle(dev);
	}
	pcibios_free_irq(pci_dev);
	pci_dev->driver = NULL;
	pci_iov_remove(pci_dev);

	 
	pm_runtime_put_sync(dev);

	 
	if (pci_dev->current_state == PCI_D0)
		pci_dev->current_state = PCI_UNKNOWN;

	 

	pci_dev_put(pci_dev);
}

static void pci_device_shutdown(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;

	pm_runtime_resume(dev);

	if (drv && drv->shutdown)
		drv->shutdown(pci_dev);

	 
	if (kexec_in_progress && (pci_dev->current_state <= PCI_D3hot))
		pci_clear_master(pci_dev);
}

#ifdef CONFIG_PM_SLEEP

 

 
static int pci_restore_standard_config(struct pci_dev *pci_dev)
{
	pci_update_current_state(pci_dev, PCI_UNKNOWN);

	if (pci_dev->current_state != PCI_D0) {
		int error = pci_set_power_state(pci_dev, PCI_D0);
		if (error)
			return error;
	}

	pci_restore_state(pci_dev);
	pci_pme_restore(pci_dev);
	return 0;
}
#endif  

#ifdef CONFIG_PM

 

static void pci_pm_default_resume(struct pci_dev *pci_dev)
{
	pci_fixup_device(pci_fixup_resume, pci_dev);
	pci_enable_wake(pci_dev, PCI_D0, false);
}

static void pci_pm_power_up_and_verify_state(struct pci_dev *pci_dev)
{
	pci_power_up(pci_dev);
	pci_update_current_state(pci_dev, PCI_D0);
}

static void pci_pm_default_resume_early(struct pci_dev *pci_dev)
{
	pci_pm_power_up_and_verify_state(pci_dev);
	pci_restore_state(pci_dev);
	pci_pme_restore(pci_dev);
}

static void pci_pm_bridge_power_up_actions(struct pci_dev *pci_dev)
{
	int ret;

	ret = pci_bridge_wait_for_secondary_bus(pci_dev, "resume");
	if (ret) {
		 
		pci_walk_bus(pci_dev->subordinate, pci_dev_set_disconnected,
			     NULL);
		return;
	}

	 
	pci_resume_bus(pci_dev->subordinate);
}

#endif  

#ifdef CONFIG_PM_SLEEP

 
static void pci_pm_set_unknown_state(struct pci_dev *pci_dev)
{
	 
	if (pci_dev->current_state == PCI_D0)
		pci_dev->current_state = PCI_UNKNOWN;
}

 
static int pci_pm_reenable_device(struct pci_dev *pci_dev)
{
	int retval;

	 
	retval = pci_reenable_device(pci_dev);
	 
	if (pci_dev->is_busmaster)
		pci_set_master(pci_dev);

	return retval;
}

static int pci_legacy_suspend(struct device *dev, pm_message_t state)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;

	if (drv && drv->suspend) {
		pci_power_t prev = pci_dev->current_state;
		int error;

		error = drv->suspend(pci_dev, state);
		suspend_report_result(dev, drv->suspend, error);
		if (error)
			return error;

		if (!pci_dev->state_saved && pci_dev->current_state != PCI_D0
		    && pci_dev->current_state != PCI_UNKNOWN) {
			pci_WARN_ONCE(pci_dev, pci_dev->current_state != prev,
				      "PCI PM: Device state not saved by %pS\n",
				      drv->suspend);
		}
	}

	pci_fixup_device(pci_fixup_suspend, pci_dev);

	return 0;
}

static int pci_legacy_suspend_late(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);

	if (!pci_dev->state_saved)
		pci_save_state(pci_dev);

	pci_pm_set_unknown_state(pci_dev);

	pci_fixup_device(pci_fixup_suspend_late, pci_dev);

	return 0;
}

static int pci_legacy_resume(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *drv = pci_dev->driver;

	pci_fixup_device(pci_fixup_resume, pci_dev);

	return drv && drv->resume ?
			drv->resume(pci_dev) : pci_pm_reenable_device(pci_dev);
}

 

static void pci_pm_default_suspend(struct pci_dev *pci_dev)
{
	 
	if (!pci_has_subordinate(pci_dev))
		pci_disable_enabled_device(pci_dev);
}

static bool pci_has_legacy_pm_support(struct pci_dev *pci_dev)
{
	struct pci_driver *drv = pci_dev->driver;
	bool ret = drv && (drv->suspend || drv->resume);

	 
	pci_WARN(pci_dev, ret && drv->driver.pm, "device %04x:%04x\n",
		 pci_dev->vendor, pci_dev->device);

	return ret;
}

 

static int pci_pm_prepare(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm && pm->prepare) {
		int error = pm->prepare(dev);
		if (error < 0)
			return error;

		if (!error && dev_pm_test_driver_flags(dev, DPM_FLAG_SMART_PREPARE))
			return 0;
	}
	if (pci_dev_need_resume(pci_dev))
		return 0;

	 
	pci_dev_adjust_pme(pci_dev);
	return 1;
}

static void pci_pm_complete(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);

	pci_dev_complete_resume(pci_dev);
	pm_generic_complete(dev);

	 
	if (pm_runtime_suspended(dev) && pm_resume_via_firmware()) {
		pci_power_t pre_sleep_state = pci_dev->current_state;

		pci_refresh_power_state(pci_dev);
		 
		if (pci_dev->current_state < pre_sleep_state)
			pm_request_resume(dev);
	}
}

#else  

#define pci_pm_prepare	NULL
#define pci_pm_complete	NULL

#endif  

#ifdef CONFIG_SUSPEND
static void pcie_pme_root_status_cleanup(struct pci_dev *pci_dev)
{
	 
	if (pci_is_pcie(pci_dev) &&
	    (pci_pcie_type(pci_dev) == PCI_EXP_TYPE_ROOT_PORT ||
	     pci_pcie_type(pci_dev) == PCI_EXP_TYPE_RC_EC))
		pcie_clear_root_pme_status(pci_dev);
}

static int pci_pm_suspend(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	pci_dev->skip_bus_pm = false;

	 
	pci_suspend_ptm(pci_dev);

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_suspend(dev, PMSG_SUSPEND);

	if (!pm) {
		pci_pm_default_suspend(pci_dev);
		return 0;
	}

	 
	if (!dev_pm_test_driver_flags(dev, DPM_FLAG_SMART_SUSPEND) ||
	    pci_dev_need_resume(pci_dev)) {
		pm_runtime_resume(dev);
		pci_dev->state_saved = false;
	} else {
		pci_dev_adjust_pme(pci_dev);
	}

	if (pm->suspend) {
		pci_power_t prev = pci_dev->current_state;
		int error;

		error = pm->suspend(dev);
		suspend_report_result(dev, pm->suspend, error);
		if (error)
			return error;

		if (!pci_dev->state_saved && pci_dev->current_state != PCI_D0
		    && pci_dev->current_state != PCI_UNKNOWN) {
			pci_WARN_ONCE(pci_dev, pci_dev->current_state != prev,
				      "PCI PM: State of device not saved by %pS\n",
				      pm->suspend);
		}
	}

	return 0;
}

static int pci_pm_suspend_late(struct device *dev)
{
	if (dev_pm_skip_suspend(dev))
		return 0;

	pci_fixup_device(pci_fixup_suspend, to_pci_dev(dev));

	return pm_generic_suspend_late(dev);
}

static int pci_pm_suspend_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (dev_pm_skip_suspend(dev))
		return 0;

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_suspend_late(dev);

	if (!pm) {
		pci_save_state(pci_dev);
		goto Fixup;
	}

	if (pm->suspend_noirq) {
		pci_power_t prev = pci_dev->current_state;
		int error;

		error = pm->suspend_noirq(dev);
		suspend_report_result(dev, pm->suspend_noirq, error);
		if (error)
			return error;

		if (!pci_dev->state_saved && pci_dev->current_state != PCI_D0
		    && pci_dev->current_state != PCI_UNKNOWN) {
			pci_WARN_ONCE(pci_dev, pci_dev->current_state != prev,
				      "PCI PM: State of device not saved by %pS\n",
				      pm->suspend_noirq);
			goto Fixup;
		}
	}

	if (!pci_dev->state_saved) {
		pci_save_state(pci_dev);

		 
		if (!pci_dev->skip_bus_pm && pci_power_manageable(pci_dev))
			pci_prepare_to_sleep(pci_dev);
	}

	pci_dbg(pci_dev, "PCI PM: Suspend power state: %s\n",
		pci_power_name(pci_dev->current_state));

	if (pci_dev->current_state == PCI_D0) {
		pci_dev->skip_bus_pm = true;
		 
		if (pci_dev->bus->self)
			pci_dev->bus->self->skip_bus_pm = true;
	}

	if (pci_dev->skip_bus_pm && pm_suspend_no_platform()) {
		pci_dbg(pci_dev, "PCI PM: Skipped\n");
		goto Fixup;
	}

	pci_pm_set_unknown_state(pci_dev);

	 
	if (pci_dev->class == PCI_CLASS_SERIAL_USB_EHCI)
		pci_write_config_word(pci_dev, PCI_COMMAND, 0);

Fixup:
	pci_fixup_device(pci_fixup_suspend_late, pci_dev);

	 
	if (device_can_wakeup(dev) && !device_may_wakeup(dev))
		dev->power.may_skip_resume = false;

	return 0;
}

static int pci_pm_resume_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	pci_power_t prev_state = pci_dev->current_state;
	bool skip_bus_pm = pci_dev->skip_bus_pm;

	if (dev_pm_skip_resume(dev))
		return 0;

	 
	if (!(skip_bus_pm && pm_suspend_no_platform()))
		pci_pm_default_resume_early(pci_dev);

	pci_fixup_device(pci_fixup_resume_early, pci_dev);
	pcie_pme_root_status_cleanup(pci_dev);

	if (!skip_bus_pm && prev_state == PCI_D3cold)
		pci_pm_bridge_power_up_actions(pci_dev);

	if (pci_has_legacy_pm_support(pci_dev))
		return 0;

	if (pm && pm->resume_noirq)
		return pm->resume_noirq(dev);

	return 0;
}

static int pci_pm_resume_early(struct device *dev)
{
	if (dev_pm_skip_resume(dev))
		return 0;

	return pm_generic_resume_early(dev);
}

static int pci_pm_resume(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	 
	if (pci_dev->state_saved)
		pci_restore_standard_config(pci_dev);

	pci_resume_ptm(pci_dev);

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_resume(dev);

	pci_pm_default_resume(pci_dev);

	if (pm) {
		if (pm->resume)
			return pm->resume(dev);
	} else {
		pci_pm_reenable_device(pci_dev);
	}

	return 0;
}

#else  

#define pci_pm_suspend		NULL
#define pci_pm_suspend_late	NULL
#define pci_pm_suspend_noirq	NULL
#define pci_pm_resume		NULL
#define pci_pm_resume_early	NULL
#define pci_pm_resume_noirq	NULL

#endif  

#ifdef CONFIG_HIBERNATE_CALLBACKS

static int pci_pm_freeze(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_suspend(dev, PMSG_FREEZE);

	if (!pm) {
		pci_pm_default_suspend(pci_dev);
		return 0;
	}

	 
	pm_runtime_resume(dev);
	pci_dev->state_saved = false;

	if (pm->freeze) {
		int error;

		error = pm->freeze(dev);
		suspend_report_result(dev, pm->freeze, error);
		if (error)
			return error;
	}

	return 0;
}

static int pci_pm_freeze_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_suspend_late(dev);

	if (pm && pm->freeze_noirq) {
		int error;

		error = pm->freeze_noirq(dev);
		suspend_report_result(dev, pm->freeze_noirq, error);
		if (error)
			return error;
	}

	if (!pci_dev->state_saved)
		pci_save_state(pci_dev);

	pci_pm_set_unknown_state(pci_dev);

	return 0;
}

static int pci_pm_thaw_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	 
	pci_pm_power_up_and_verify_state(pci_dev);
	pci_restore_state(pci_dev);

	if (pci_has_legacy_pm_support(pci_dev))
		return 0;

	if (pm && pm->thaw_noirq)
		return pm->thaw_noirq(dev);

	return 0;
}

static int pci_pm_thaw(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	int error = 0;

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_resume(dev);

	if (pm) {
		if (pm->thaw)
			error = pm->thaw(dev);
	} else {
		pci_pm_reenable_device(pci_dev);
	}

	pci_dev->state_saved = false;

	return error;
}

static int pci_pm_poweroff(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_suspend(dev, PMSG_HIBERNATE);

	if (!pm) {
		pci_pm_default_suspend(pci_dev);
		return 0;
	}

	 
	if (!dev_pm_test_driver_flags(dev, DPM_FLAG_SMART_SUSPEND) ||
	    pci_dev_need_resume(pci_dev)) {
		pm_runtime_resume(dev);
		pci_dev->state_saved = false;
	} else {
		pci_dev_adjust_pme(pci_dev);
	}

	if (pm->poweroff) {
		int error;

		error = pm->poweroff(dev);
		suspend_report_result(dev, pm->poweroff, error);
		if (error)
			return error;
	}

	return 0;
}

static int pci_pm_poweroff_late(struct device *dev)
{
	if (dev_pm_skip_suspend(dev))
		return 0;

	pci_fixup_device(pci_fixup_suspend, to_pci_dev(dev));

	return pm_generic_poweroff_late(dev);
}

static int pci_pm_poweroff_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (dev_pm_skip_suspend(dev))
		return 0;

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_suspend_late(dev);

	if (!pm) {
		pci_fixup_device(pci_fixup_suspend_late, pci_dev);
		return 0;
	}

	if (pm->poweroff_noirq) {
		int error;

		error = pm->poweroff_noirq(dev);
		suspend_report_result(dev, pm->poweroff_noirq, error);
		if (error)
			return error;
	}

	if (!pci_dev->state_saved && !pci_has_subordinate(pci_dev))
		pci_prepare_to_sleep(pci_dev);

	 
	if (pci_dev->class == PCI_CLASS_SERIAL_USB_EHCI)
		pci_write_config_word(pci_dev, PCI_COMMAND, 0);

	pci_fixup_device(pci_fixup_suspend_late, pci_dev);

	return 0;
}

static int pci_pm_restore_noirq(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	pci_pm_default_resume_early(pci_dev);
	pci_fixup_device(pci_fixup_resume_early, pci_dev);

	if (pci_has_legacy_pm_support(pci_dev))
		return 0;

	if (pm && pm->restore_noirq)
		return pm->restore_noirq(dev);

	return 0;
}

static int pci_pm_restore(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	 
	if (pci_dev->state_saved)
		pci_restore_standard_config(pci_dev);

	if (pci_has_legacy_pm_support(pci_dev))
		return pci_legacy_resume(dev);

	pci_pm_default_resume(pci_dev);

	if (pm) {
		if (pm->restore)
			return pm->restore(dev);
	} else {
		pci_pm_reenable_device(pci_dev);
	}

	return 0;
}

#else  

#define pci_pm_freeze		NULL
#define pci_pm_freeze_noirq	NULL
#define pci_pm_thaw		NULL
#define pci_pm_thaw_noirq	NULL
#define pci_pm_poweroff		NULL
#define pci_pm_poweroff_late	NULL
#define pci_pm_poweroff_noirq	NULL
#define pci_pm_restore		NULL
#define pci_pm_restore_noirq	NULL

#endif  

#ifdef CONFIG_PM

static int pci_pm_runtime_suspend(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	pci_power_t prev = pci_dev->current_state;
	int error;

	pci_suspend_ptm(pci_dev);

	 
	if (!pci_dev->driver) {
		pci_save_state(pci_dev);
		return 0;
	}

	pci_dev->state_saved = false;
	if (pm && pm->runtime_suspend) {
		error = pm->runtime_suspend(dev);
		 
		if (error == -EBUSY || error == -EAGAIN) {
			pci_dbg(pci_dev, "can't suspend now (%ps returned %d)\n",
				pm->runtime_suspend, error);
			return error;
		} else if (error) {
			pci_err(pci_dev, "can't suspend (%ps returned %d)\n",
				pm->runtime_suspend, error);
			return error;
		}
	}

	pci_fixup_device(pci_fixup_suspend, pci_dev);

	if (pm && pm->runtime_suspend
	    && !pci_dev->state_saved && pci_dev->current_state != PCI_D0
	    && pci_dev->current_state != PCI_UNKNOWN) {
		pci_WARN_ONCE(pci_dev, pci_dev->current_state != prev,
			      "PCI PM: State of device not saved by %pS\n",
			      pm->runtime_suspend);
		return 0;
	}

	if (!pci_dev->state_saved) {
		pci_save_state(pci_dev);
		pci_finish_runtime_suspend(pci_dev);
	}

	return 0;
}

static int pci_pm_runtime_resume(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;
	pci_power_t prev_state = pci_dev->current_state;
	int error = 0;

	 
	pci_pm_default_resume_early(pci_dev);
	pci_resume_ptm(pci_dev);

	if (!pci_dev->driver)
		return 0;

	pci_fixup_device(pci_fixup_resume_early, pci_dev);
	pci_pm_default_resume(pci_dev);

	if (prev_state == PCI_D3cold)
		pci_pm_bridge_power_up_actions(pci_dev);

	if (pm && pm->runtime_resume)
		error = pm->runtime_resume(dev);

	return error;
}

static int pci_pm_runtime_idle(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	 
	if (!pci_dev->driver)
		return 0;

	if (!pm)
		return -ENOSYS;

	if (pm->runtime_idle)
		return pm->runtime_idle(dev);

	return 0;
}

static const struct dev_pm_ops pci_dev_pm_ops = {
	.prepare = pci_pm_prepare,
	.complete = pci_pm_complete,
	.suspend = pci_pm_suspend,
	.suspend_late = pci_pm_suspend_late,
	.resume = pci_pm_resume,
	.resume_early = pci_pm_resume_early,
	.freeze = pci_pm_freeze,
	.thaw = pci_pm_thaw,
	.poweroff = pci_pm_poweroff,
	.poweroff_late = pci_pm_poweroff_late,
	.restore = pci_pm_restore,
	.suspend_noirq = pci_pm_suspend_noirq,
	.resume_noirq = pci_pm_resume_noirq,
	.freeze_noirq = pci_pm_freeze_noirq,
	.thaw_noirq = pci_pm_thaw_noirq,
	.poweroff_noirq = pci_pm_poweroff_noirq,
	.restore_noirq = pci_pm_restore_noirq,
	.runtime_suspend = pci_pm_runtime_suspend,
	.runtime_resume = pci_pm_runtime_resume,
	.runtime_idle = pci_pm_runtime_idle,
};

#define PCI_PM_OPS_PTR	(&pci_dev_pm_ops)

#else  

#define pci_pm_runtime_suspend	NULL
#define pci_pm_runtime_resume	NULL
#define pci_pm_runtime_idle	NULL

#define PCI_PM_OPS_PTR	NULL

#endif  

 
int __pci_register_driver(struct pci_driver *drv, struct module *owner,
			  const char *mod_name)
{
	 
	drv->driver.name = drv->name;
	drv->driver.bus = &pci_bus_type;
	drv->driver.owner = owner;
	drv->driver.mod_name = mod_name;
	drv->driver.groups = drv->groups;
	drv->driver.dev_groups = drv->dev_groups;

	spin_lock_init(&drv->dynids.lock);
	INIT_LIST_HEAD(&drv->dynids.list);

	 
	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(__pci_register_driver);

 

void pci_unregister_driver(struct pci_driver *drv)
{
	driver_unregister(&drv->driver);
	pci_free_dynids(drv);
}
EXPORT_SYMBOL(pci_unregister_driver);

static struct pci_driver pci_compat_driver = {
	.name = "compat"
};

 
struct pci_driver *pci_dev_driver(const struct pci_dev *dev)
{
	int i;

	if (dev->driver)
		return dev->driver;

	for (i = 0; i <= PCI_ROM_RESOURCE; i++)
		if (dev->resource[i].flags & IORESOURCE_BUSY)
			return &pci_compat_driver;

	return NULL;
}
EXPORT_SYMBOL(pci_dev_driver);

 
static int pci_bus_match(struct device *dev, struct device_driver *drv)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct pci_driver *pci_drv;
	const struct pci_device_id *found_id;

	if (!pci_dev->match_driver)
		return 0;

	pci_drv = to_pci_driver(drv);
	found_id = pci_match_device(pci_drv, pci_dev);
	if (found_id)
		return 1;

	return 0;
}

 
struct pci_dev *pci_dev_get(struct pci_dev *dev)
{
	if (dev)
		get_device(&dev->dev);
	return dev;
}
EXPORT_SYMBOL(pci_dev_get);

 
void pci_dev_put(struct pci_dev *dev)
{
	if (dev)
		put_device(&dev->dev);
}
EXPORT_SYMBOL(pci_dev_put);

static int pci_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct pci_dev *pdev;

	if (!dev)
		return -ENODEV;

	pdev = to_pci_dev(dev);

	if (add_uevent_var(env, "PCI_CLASS=%04X", pdev->class))
		return -ENOMEM;

	if (add_uevent_var(env, "PCI_ID=%04X:%04X", pdev->vendor, pdev->device))
		return -ENOMEM;

	if (add_uevent_var(env, "PCI_SUBSYS_ID=%04X:%04X", pdev->subsystem_vendor,
			   pdev->subsystem_device))
		return -ENOMEM;

	if (add_uevent_var(env, "PCI_SLOT_NAME=%s", pci_name(pdev)))
		return -ENOMEM;

	if (add_uevent_var(env, "MODALIAS=pci:v%08Xd%08Xsv%08Xsd%08Xbc%02Xsc%02Xi%02X",
			   pdev->vendor, pdev->device,
			   pdev->subsystem_vendor, pdev->subsystem_device,
			   (u8)(pdev->class >> 16), (u8)(pdev->class >> 8),
			   (u8)(pdev->class)))
		return -ENOMEM;

	return 0;
}

#if defined(CONFIG_PCIEAER) || defined(CONFIG_EEH)
 
void pci_uevent_ers(struct pci_dev *pdev, enum pci_ers_result err_type)
{
	int idx = 0;
	char *envp[3];

	switch (err_type) {
	case PCI_ERS_RESULT_NONE:
	case PCI_ERS_RESULT_CAN_RECOVER:
		envp[idx++] = "ERROR_EVENT=BEGIN_RECOVERY";
		envp[idx++] = "DEVICE_ONLINE=0";
		break;
	case PCI_ERS_RESULT_RECOVERED:
		envp[idx++] = "ERROR_EVENT=SUCCESSFUL_RECOVERY";
		envp[idx++] = "DEVICE_ONLINE=1";
		break;
	case PCI_ERS_RESULT_DISCONNECT:
		envp[idx++] = "ERROR_EVENT=FAILED_RECOVERY";
		envp[idx++] = "DEVICE_ONLINE=0";
		break;
	default:
		break;
	}

	if (idx > 0) {
		envp[idx++] = NULL;
		kobject_uevent_env(&pdev->dev.kobj, KOBJ_CHANGE, envp);
	}
}
#endif

static int pci_bus_num_vf(struct device *dev)
{
	return pci_num_vf(to_pci_dev(dev));
}

 
static int pci_dma_configure(struct device *dev)
{
	struct pci_driver *driver = to_pci_driver(dev->driver);
	struct device *bridge;
	int ret = 0;

	bridge = pci_get_host_bridge_device(to_pci_dev(dev));

	if (IS_ENABLED(CONFIG_OF) && bridge->parent &&
	    bridge->parent->of_node) {
		ret = of_dma_configure(dev, bridge->parent->of_node, true);
	} else if (has_acpi_companion(bridge)) {
		struct acpi_device *adev = to_acpi_device_node(bridge->fwnode);

		ret = acpi_dma_configure(dev, acpi_get_dma_attr(adev));
	}

	pci_put_host_bridge_device(bridge);

	if (!ret && !driver->driver_managed_dma) {
		ret = iommu_device_use_default_domain(dev);
		if (ret)
			arch_teardown_dma_ops(dev);
	}

	return ret;
}

static void pci_dma_cleanup(struct device *dev)
{
	struct pci_driver *driver = to_pci_driver(dev->driver);

	if (!driver->driver_managed_dma)
		iommu_device_unuse_default_domain(dev);
}

struct bus_type pci_bus_type = {
	.name		= "pci",
	.match		= pci_bus_match,
	.uevent		= pci_uevent,
	.probe		= pci_device_probe,
	.remove		= pci_device_remove,
	.shutdown	= pci_device_shutdown,
	.dev_groups	= pci_dev_groups,
	.bus_groups	= pci_bus_groups,
	.drv_groups	= pci_drv_groups,
	.pm		= PCI_PM_OPS_PTR,
	.num_vf		= pci_bus_num_vf,
	.dma_configure	= pci_dma_configure,
	.dma_cleanup	= pci_dma_cleanup,
};
EXPORT_SYMBOL(pci_bus_type);

#ifdef CONFIG_PCIEPORTBUS
static int pcie_port_bus_match(struct device *dev, struct device_driver *drv)
{
	struct pcie_device *pciedev;
	struct pcie_port_service_driver *driver;

	if (drv->bus != &pcie_port_bus_type || dev->bus != &pcie_port_bus_type)
		return 0;

	pciedev = to_pcie_device(dev);
	driver = to_service_driver(drv);

	if (driver->service != pciedev->service)
		return 0;

	if (driver->port_type != PCIE_ANY_PORT &&
	    driver->port_type != pci_pcie_type(pciedev->port))
		return 0;

	return 1;
}

struct bus_type pcie_port_bus_type = {
	.name		= "pci_express",
	.match		= pcie_port_bus_match,
};
#endif

static int __init pci_driver_init(void)
{
	int ret;

	ret = bus_register(&pci_bus_type);
	if (ret)
		return ret;

#ifdef CONFIG_PCIEPORTBUS
	ret = bus_register(&pcie_port_bus_type);
	if (ret)
		return ret;
#endif
	dma_debug_add_bus(&pci_bus_type);
	return 0;
}
postcore_initcall(pci_driver_init);
