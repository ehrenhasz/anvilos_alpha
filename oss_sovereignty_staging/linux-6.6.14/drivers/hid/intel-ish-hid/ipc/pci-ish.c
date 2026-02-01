
 

#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#define CREATE_TRACE_POINTS
#include <trace/events/intel_ish.h>
#include "ishtp-dev.h"
#include "hw-ish.h"

static const struct pci_device_id ish_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, CHV_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, BXT_Ax_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, BXT_Bx_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, APL_Ax_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, SPT_Ax_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, CNL_Ax_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, GLK_Ax_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, CNL_H_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, ICL_MOBILE_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, SPT_H_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, CML_LP_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, CMP_H_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, EHL_Ax_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, TGL_LP_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, TGL_H_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, ADL_S_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, ADL_P_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, ADL_N_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, RPL_S_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, MTL_P_DEVICE_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, ARL_H_DEVICE_ID)},
	{0, }
};
MODULE_DEVICE_TABLE(pci, ish_pci_tbl);

 
static __printf(2, 3)
void ish_event_tracer(struct ishtp_device *dev, const char *format, ...)
{
	if (trace_ishtp_dump_enabled()) {
		va_list args;
		char tmp_buf[100];

		va_start(args, format);
		vsnprintf(tmp_buf, sizeof(tmp_buf), format, args);
		va_end(args);

		trace_ishtp_dump(tmp_buf);
	}
}

 
static int ish_init(struct ishtp_device *dev)
{
	int ret;

	 
	ret = ish_hw_start(dev);
	if (ret) {
		dev_err(dev->devc, "ISH: hw start failed.\n");
		return ret;
	}

	 
	ret = ishtp_start(dev);
	if (ret) {
		dev_err(dev->devc, "ISHTP: Protocol init failed.\n");
		return ret;
	}

	return 0;
}

static const struct pci_device_id ish_invalid_pci_ids[] = {
	 
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA309)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xA30A)},
	{}
};

static inline bool ish_should_enter_d0i3(struct pci_dev *pdev)
{
	return !pm_suspend_via_firmware() || pdev->device == CHV_DEVICE_ID;
}

static inline bool ish_should_leave_d0i3(struct pci_dev *pdev)
{
	return !pm_resume_via_firmware() || pdev->device == CHV_DEVICE_ID;
}

static int enable_gpe(struct device *dev)
{
#ifdef CONFIG_ACPI
	acpi_status acpi_sts;
	struct acpi_device *adev;
	struct acpi_device_wakeup *wakeup;

	adev = ACPI_COMPANION(dev);
	if (!adev) {
		dev_err(dev, "get acpi handle failed\n");
		return -ENODEV;
	}
	wakeup = &adev->wakeup;

	 
	acpi_disable_gpe(wakeup->gpe_device, wakeup->gpe_number);

	acpi_sts = acpi_enable_gpe(wakeup->gpe_device, wakeup->gpe_number);
	if (ACPI_FAILURE(acpi_sts)) {
		dev_err(dev, "enable ose_gpe failed\n");
		return -EIO;
	}

	return 0;
#else
	return -ENODEV;
#endif
}

static void enable_pme_wake(struct pci_dev *pdev)
{
	if ((pci_pme_capable(pdev, PCI_D0) ||
	     pci_pme_capable(pdev, PCI_D3hot) ||
	     pci_pme_capable(pdev, PCI_D3cold)) && !enable_gpe(&pdev->dev)) {
		pci_pme_active(pdev, true);
		dev_dbg(&pdev->dev, "ish ipc driver pme wake enabled\n");
	}
}

 
static int ish_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int ret;
	struct ish_hw *hw;
	unsigned long irq_flag = 0;
	struct ishtp_device *ishtp;
	struct device *dev = &pdev->dev;

	 
	if (pci_dev_present(ish_invalid_pci_ids))
		return -ENODEV;

	 
	ret = pcim_enable_device(pdev);
	if (ret) {
		dev_err(dev, "ISH: Failed to enable PCI device\n");
		return ret;
	}

	 
	pci_set_master(pdev);

	 
	ret = pcim_iomap_regions(pdev, 1 << 0, KBUILD_MODNAME);
	if (ret) {
		dev_err(dev, "ISH: Failed to get PCI regions\n");
		return ret;
	}

	 
	ishtp = ish_dev_init(pdev);
	if (!ishtp) {
		ret = -ENOMEM;
		return ret;
	}
	hw = to_ish_hw(ishtp);
	ishtp->print_log = ish_event_tracer;

	 
	hw->mem_addr = pcim_iomap_table(pdev)[0];
	ishtp->pdev = pdev;

	 
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_ALL_TYPES);
	if (!pdev->msi_enabled && !pdev->msix_enabled)
		irq_flag = IRQF_SHARED;

	ret = devm_request_irq(dev, pdev->irq, ish_irq_handler,
			       irq_flag, KBUILD_MODNAME, ishtp);
	if (ret) {
		dev_err(dev, "ISH: request IRQ %d failed\n", pdev->irq);
		return ret;
	}

	dev_set_drvdata(ishtp->devc, ishtp);

	init_waitqueue_head(&ishtp->suspend_wait);
	init_waitqueue_head(&ishtp->resume_wait);

	 
	if (pdev->device == EHL_Ax_DEVICE_ID)
		enable_pme_wake(pdev);

	ret = ish_init(ishtp);
	if (ret)
		return ret;

	return 0;
}

 
static void ish_remove(struct pci_dev *pdev)
{
	struct ishtp_device *ishtp_dev = pci_get_drvdata(pdev);

	ishtp_bus_remove_all_clients(ishtp_dev, false);
	ish_device_disable(ishtp_dev);
}

static struct device __maybe_unused *ish_resume_device;

 
#define WAIT_FOR_RESUME_ACK_MS		50

 
static void __maybe_unused ish_resume_handler(struct work_struct *work)
{
	struct pci_dev *pdev = to_pci_dev(ish_resume_device);
	struct ishtp_device *dev = pci_get_drvdata(pdev);
	uint32_t fwsts = dev->ops->get_fw_status(dev);

	if (ish_should_leave_d0i3(pdev) && !dev->suspend_flag
			&& IPC_IS_ISH_ILUP(fwsts)) {
		if (device_may_wakeup(&pdev->dev))
			disable_irq_wake(pdev->irq);

		ish_set_host_ready(dev);

		ishtp_send_resume(dev);

		 
		if (dev->resume_flag)
			wait_event_interruptible_timeout(dev->resume_wait,
				!dev->resume_flag,
				msecs_to_jiffies(WAIT_FOR_RESUME_ACK_MS));

		 
		if (dev->resume_flag)
			ish_init(dev);
	} else {
		 
		ish_init(dev);
	}
}

 
static int __maybe_unused ish_suspend(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct ishtp_device *dev = pci_get_drvdata(pdev);

	if (ish_should_enter_d0i3(pdev)) {
		 
		if (dev->suspend_flag)
			return	0;

		dev->resume_flag = 0;
		dev->suspend_flag = 1;
		ishtp_send_suspend(dev);

		 
		if (dev->suspend_flag)
			wait_event_interruptible_timeout(dev->suspend_wait,
					!dev->suspend_flag,
					msecs_to_jiffies(25));

		if (dev->suspend_flag) {
			 
			ish_disable_dma(dev);
		} else {
			 
			pci_save_state(pdev);

			if (device_may_wakeup(&pdev->dev))
				enable_irq_wake(pdev->irq);
		}
	} else {
		 
		ish_disable_dma(dev);
	}

	return 0;
}

static __maybe_unused DECLARE_WORK(resume_work, ish_resume_handler);
 
static int __maybe_unused ish_resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct ishtp_device *dev = pci_get_drvdata(pdev);

	 
	if (dev->pdev->device == EHL_Ax_DEVICE_ID) {
		pci_set_power_state(pdev, PCI_D0);
		enable_pme_wake(pdev);
		dev_dbg(dev->devc, "set power state to D0 for ehl\n");
	}

	ish_resume_device = device;
	dev->resume_flag = 1;

	schedule_work(&resume_work);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ish_pm_ops, ish_suspend, ish_resume);

static struct pci_driver ish_driver = {
	.name = KBUILD_MODNAME,
	.id_table = ish_pci_tbl,
	.probe = ish_probe,
	.remove = ish_remove,
	.driver.pm = &ish_pm_ops,
};

module_pci_driver(ish_driver);

 
MODULE_AUTHOR("Daniel Drubin <daniel.drubin@intel.com>");
 
MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com>");

MODULE_DESCRIPTION("Intel(R) Integrated Sensor Hub PCI Device Driver");
MODULE_LICENSE("GPL");
