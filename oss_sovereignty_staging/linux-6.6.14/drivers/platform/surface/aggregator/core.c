
 

#include <linux/acpi.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/serdev.h>
#include <linux/sysfs.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/device.h>

#include "bus.h"
#include "controller.h"

#define CREATE_TRACE_POINTS
#include "trace.h"


 

 
static struct ssam_controller *__ssam_controller;
static DEFINE_SPINLOCK(__ssam_controller_lock);

 
struct ssam_controller *ssam_get_controller(void)
{
	struct ssam_controller *ctrl;

	spin_lock(&__ssam_controller_lock);

	ctrl = __ssam_controller;
	if (!ctrl)
		goto out;

	if (WARN_ON(!kref_get_unless_zero(&ctrl->kref)))
		ctrl = NULL;

out:
	spin_unlock(&__ssam_controller_lock);
	return ctrl;
}
EXPORT_SYMBOL_GPL(ssam_get_controller);

 
static int ssam_try_set_controller(struct ssam_controller *ctrl)
{
	int status = 0;

	spin_lock(&__ssam_controller_lock);
	if (!__ssam_controller)
		__ssam_controller = ctrl;
	else
		status = -EEXIST;
	spin_unlock(&__ssam_controller_lock);

	return status;
}

 
static void ssam_clear_controller(void)
{
	spin_lock(&__ssam_controller_lock);
	__ssam_controller = NULL;
	spin_unlock(&__ssam_controller_lock);
}

 
int ssam_client_link(struct ssam_controller *c, struct device *client)
{
	const u32 flags = DL_FLAG_PM_RUNTIME | DL_FLAG_AUTOREMOVE_CONSUMER;
	struct device_link *link;
	struct device *ctrldev;

	ssam_controller_statelock(c);

	if (c->state != SSAM_CONTROLLER_STARTED) {
		ssam_controller_stateunlock(c);
		return -ENODEV;
	}

	ctrldev = ssam_controller_device(c);
	if (!ctrldev) {
		ssam_controller_stateunlock(c);
		return -ENODEV;
	}

	link = device_link_add(client, ctrldev, flags);
	if (!link) {
		ssam_controller_stateunlock(c);
		return -ENOMEM;
	}

	 
	if (READ_ONCE(link->status) == DL_STATE_SUPPLIER_UNBIND) {
		ssam_controller_stateunlock(c);
		return -ENODEV;
	}

	ssam_controller_stateunlock(c);
	return 0;
}
EXPORT_SYMBOL_GPL(ssam_client_link);

 
struct ssam_controller *ssam_client_bind(struct device *client)
{
	struct ssam_controller *c;
	int status;

	c = ssam_get_controller();
	if (!c)
		return ERR_PTR(-ENODEV);

	status = ssam_client_link(c, client);

	 
	ssam_controller_put(c);

	return status >= 0 ? c : ERR_PTR(status);
}
EXPORT_SYMBOL_GPL(ssam_client_bind);


 

static int ssam_receive_buf(struct serdev_device *dev, const unsigned char *buf,
			    size_t n)
{
	struct ssam_controller *ctrl;
	int ret;

	ctrl = serdev_device_get_drvdata(dev);
	ret = ssam_controller_receive_buf(ctrl, buf, n);

	return ret < 0 ? 0 : ret;
}

static void ssam_write_wakeup(struct serdev_device *dev)
{
	ssam_controller_write_wakeup(serdev_device_get_drvdata(dev));
}

static const struct serdev_device_ops ssam_serdev_ops = {
	.receive_buf = ssam_receive_buf,
	.write_wakeup = ssam_write_wakeup,
};


 

static int ssam_log_firmware_version(struct ssam_controller *ctrl)
{
	u32 version, a, b, c;
	int status;

	status = ssam_get_firmware_version(ctrl, &version);
	if (status)
		return status;

	a = (version >> 24) & 0xff;
	b = ((version >> 8) & 0xffff);
	c = version & 0xff;

	ssam_info(ctrl, "SAM firmware version: %u.%u.%u\n", a, b, c);
	return 0;
}

static ssize_t firmware_version_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ssam_controller *ctrl = dev_get_drvdata(dev);
	u32 version, a, b, c;
	int status;

	status = ssam_get_firmware_version(ctrl, &version);
	if (status < 0)
		return status;

	a = (version >> 24) & 0xff;
	b = ((version >> 8) & 0xffff);
	c = version & 0xff;

	return sysfs_emit(buf, "%u.%u.%u\n", a, b, c);
}
static DEVICE_ATTR_RO(firmware_version);

static struct attribute *ssam_sam_attrs[] = {
	&dev_attr_firmware_version.attr,
	NULL
};

static const struct attribute_group ssam_sam_group = {
	.name = "sam",
	.attrs = ssam_sam_attrs,
};


 

static acpi_status ssam_serdev_setup_via_acpi_crs(struct acpi_resource *rsc,
						  void *ctx)
{
	struct serdev_device *serdev = ctx;
	struct acpi_resource_uart_serialbus *uart;
	bool flow_control;
	int status = 0;

	if (!serdev_acpi_get_uart_resource(rsc, &uart))
		return AE_OK;

	 
	serdev_device_set_baudrate(serdev, uart->default_baud_rate);

	 
	if (uart->flow_control & (~((u8)ACPI_UART_FLOW_CONTROL_HW))) {
		dev_warn(&serdev->dev, "setup: unsupported flow control (value: %#04x)\n",
			 uart->flow_control);
	}

	 
	flow_control = uart->flow_control & ACPI_UART_FLOW_CONTROL_HW;
	serdev_device_set_flow_control(serdev, flow_control);

	 
	switch (uart->parity) {
	case ACPI_UART_PARITY_NONE:
		status = serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
		break;
	case ACPI_UART_PARITY_EVEN:
		status = serdev_device_set_parity(serdev, SERDEV_PARITY_EVEN);
		break;
	case ACPI_UART_PARITY_ODD:
		status = serdev_device_set_parity(serdev, SERDEV_PARITY_ODD);
		break;
	default:
		dev_warn(&serdev->dev, "setup: unsupported parity (value: %#04x)\n",
			 uart->parity);
		break;
	}

	if (status) {
		dev_err(&serdev->dev, "setup: failed to set parity (value: %#04x, error: %d)\n",
			uart->parity, status);
		return AE_ERROR;
	}

	 
	return AE_CTRL_TERMINATE;
}

static acpi_status ssam_serdev_setup_via_acpi(acpi_handle handle,
					      struct serdev_device *serdev)
{
	return acpi_walk_resources(handle, METHOD_NAME__CRS,
				   ssam_serdev_setup_via_acpi_crs, serdev);
}


 

static void ssam_serial_hub_shutdown(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	 

	status = ssam_notifier_disable_registered(c);
	if (status) {
		ssam_err(c, "pm: failed to disable notifiers for shutdown: %d\n",
			 status);
	}

	status = ssam_ctrl_notif_display_off(c);
	if (status)
		ssam_err(c, "pm: display-off notification failed: %d\n", status);

	status = ssam_ctrl_notif_d0_exit(c);
	if (status)
		ssam_err(c, "pm: D0-exit notification failed: %d\n", status);
}

#ifdef CONFIG_PM_SLEEP

static int ssam_serial_hub_pm_prepare(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	 

	status = ssam_ctrl_notif_display_off(c);
	if (status)
		ssam_err(c, "pm: display-off notification failed: %d\n", status);

	return status;
}

static void ssam_serial_hub_pm_complete(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	 

	status = ssam_ctrl_notif_display_on(c);
	if (status)
		ssam_err(c, "pm: display-on notification failed: %d\n", status);
}

static int ssam_serial_hub_pm_suspend(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	 

	status = ssam_ctrl_notif_d0_exit(c);
	if (status) {
		ssam_err(c, "pm: D0-exit notification failed: %d\n", status);
		goto err_notif;
	}

	status = ssam_irq_arm_for_wakeup(c);
	if (status)
		goto err_irq;

	WARN_ON(ssam_controller_suspend(c));
	return 0;

err_irq:
	ssam_ctrl_notif_d0_entry(c);
err_notif:
	ssam_ctrl_notif_display_on(c);
	return status;
}

static int ssam_serial_hub_pm_resume(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	WARN_ON(ssam_controller_resume(c));

	 

	ssam_irq_disarm_wakeup(c);

	status = ssam_ctrl_notif_d0_entry(c);
	if (status)
		ssam_err(c, "pm: D0-entry notification failed: %d\n", status);

	return 0;
}

static int ssam_serial_hub_pm_freeze(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	 

	status = ssam_ctrl_notif_d0_exit(c);
	if (status) {
		ssam_err(c, "pm: D0-exit notification failed: %d\n", status);
		ssam_ctrl_notif_display_on(c);
		return status;
	}

	WARN_ON(ssam_controller_suspend(c));
	return 0;
}

static int ssam_serial_hub_pm_thaw(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	WARN_ON(ssam_controller_resume(c));

	status = ssam_ctrl_notif_d0_entry(c);
	if (status)
		ssam_err(c, "pm: D0-exit notification failed: %d\n", status);

	return status;
}

static int ssam_serial_hub_pm_poweroff(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	 

	status = ssam_notifier_disable_registered(c);
	if (status) {
		ssam_err(c, "pm: failed to disable notifiers for hibernation: %d\n",
			 status);
		return status;
	}

	status = ssam_ctrl_notif_d0_exit(c);
	if (status) {
		ssam_err(c, "pm: D0-exit notification failed: %d\n", status);
		ssam_notifier_restore_registered(c);
		return status;
	}

	WARN_ON(ssam_controller_suspend(c));
	return 0;
}

static int ssam_serial_hub_pm_restore(struct device *dev)
{
	struct ssam_controller *c = dev_get_drvdata(dev);
	int status;

	 

	WARN_ON(ssam_controller_resume(c));

	status = ssam_ctrl_notif_d0_entry(c);
	if (status)
		ssam_err(c, "pm: D0-entry notification failed: %d\n", status);

	ssam_notifier_restore_registered(c);
	return 0;
}

static const struct dev_pm_ops ssam_serial_hub_pm_ops = {
	.prepare  = ssam_serial_hub_pm_prepare,
	.complete = ssam_serial_hub_pm_complete,
	.suspend  = ssam_serial_hub_pm_suspend,
	.resume   = ssam_serial_hub_pm_resume,
	.freeze   = ssam_serial_hub_pm_freeze,
	.thaw     = ssam_serial_hub_pm_thaw,
	.poweroff = ssam_serial_hub_pm_poweroff,
	.restore  = ssam_serial_hub_pm_restore,
};

#else  

static const struct dev_pm_ops ssam_serial_hub_pm_ops = { };

#endif  


 

static const struct acpi_gpio_params gpio_ssam_wakeup_int = { 0, 0, false };
static const struct acpi_gpio_params gpio_ssam_wakeup     = { 1, 0, false };

static const struct acpi_gpio_mapping ssam_acpi_gpios[] = {
	{ "ssam_wakeup-int-gpio", &gpio_ssam_wakeup_int, 1 },
	{ "ssam_wakeup-gpio",     &gpio_ssam_wakeup,     1 },
	{ },
};

static int ssam_serial_hub_probe(struct serdev_device *serdev)
{
	struct acpi_device *ssh = ACPI_COMPANION(&serdev->dev);
	struct ssam_controller *ctrl;
	acpi_status astatus;
	int status;

	if (gpiod_count(&serdev->dev, NULL) < 0)
		return -ENODEV;

	status = devm_acpi_dev_add_driver_gpios(&serdev->dev, ssam_acpi_gpios);
	if (status)
		return status;

	 
	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	 
	status = ssam_controller_init(ctrl, serdev);
	if (status)
		goto err_ctrl_init;

	ssam_controller_lock(ctrl);

	 
	serdev_device_set_drvdata(serdev, ctrl);
	serdev_device_set_client_ops(serdev, &ssam_serdev_ops);
	status = serdev_device_open(serdev);
	if (status)
		goto err_devopen;

	astatus = ssam_serdev_setup_via_acpi(ssh->handle, serdev);
	if (ACPI_FAILURE(astatus)) {
		status = -ENXIO;
		goto err_devinit;
	}

	 
	status = ssam_controller_start(ctrl);
	if (status)
		goto err_devinit;

	ssam_controller_unlock(ctrl);

	 
	status = ssam_log_firmware_version(ctrl);
	if (status)
		goto err_initrq;

	status = ssam_ctrl_notif_d0_entry(ctrl);
	if (status)
		goto err_initrq;

	status = ssam_ctrl_notif_display_on(ctrl);
	if (status)
		goto err_initrq;

	status = sysfs_create_group(&serdev->dev.kobj, &ssam_sam_group);
	if (status)
		goto err_initrq;

	 
	status = ssam_irq_setup(ctrl);
	if (status)
		goto err_irq;

	 
	status = ssam_try_set_controller(ctrl);
	if (WARN_ON(status))	 
		goto err_mainref;

	 
	device_set_wakeup_capable(&serdev->dev, true);
	acpi_dev_clear_dependencies(ssh);

	return 0;

err_mainref:
	ssam_irq_free(ctrl);
err_irq:
	sysfs_remove_group(&serdev->dev.kobj, &ssam_sam_group);
err_initrq:
	ssam_controller_lock(ctrl);
	ssam_controller_shutdown(ctrl);
err_devinit:
	serdev_device_close(serdev);
err_devopen:
	ssam_controller_destroy(ctrl);
	ssam_controller_unlock(ctrl);
err_ctrl_init:
	kfree(ctrl);
	return status;
}

static void ssam_serial_hub_remove(struct serdev_device *serdev)
{
	struct ssam_controller *ctrl = serdev_device_get_drvdata(serdev);
	int status;

	 
	ssam_clear_controller();

	 
	ssam_irq_free(ctrl);

	sysfs_remove_group(&serdev->dev.kobj, &ssam_sam_group);
	ssam_controller_lock(ctrl);

	 
	ssam_remove_clients(&serdev->dev);

	 
	status = ssam_ctrl_notif_display_off(ctrl);
	if (status) {
		dev_err(&serdev->dev, "display-off notification failed: %d\n",
			status);
	}

	status = ssam_ctrl_notif_d0_exit(ctrl);
	if (status) {
		dev_err(&serdev->dev, "D0-exit notification failed: %d\n",
			status);
	}

	 
	ssam_controller_shutdown(ctrl);

	 
	serdev_device_wait_until_sent(serdev, 0);
	serdev_device_close(serdev);

	 
	ssam_controller_unlock(ctrl);
	ssam_controller_put(ctrl);

	device_set_wakeup_capable(&serdev->dev, false);
}

static const struct acpi_device_id ssam_serial_hub_match[] = {
	{ "MSHW0084", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, ssam_serial_hub_match);

static struct serdev_device_driver ssam_serial_hub = {
	.probe = ssam_serial_hub_probe,
	.remove = ssam_serial_hub_remove,
	.driver = {
		.name = "surface_serial_hub",
		.acpi_match_table = ssam_serial_hub_match,
		.pm = &ssam_serial_hub_pm_ops,
		.shutdown = ssam_serial_hub_shutdown,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};


 

static int __init ssam_core_init(void)
{
	int status;

	status = ssam_bus_register();
	if (status)
		goto err_bus;

	status = ssh_ctrl_packet_cache_init();
	if (status)
		goto err_cpkg;

	status = ssam_event_item_cache_init();
	if (status)
		goto err_evitem;

	status = serdev_device_driver_register(&ssam_serial_hub);
	if (status)
		goto err_register;

	return 0;

err_register:
	ssam_event_item_cache_destroy();
err_evitem:
	ssh_ctrl_packet_cache_destroy();
err_cpkg:
	ssam_bus_unregister();
err_bus:
	return status;
}
subsys_initcall(ssam_core_init);

static void __exit ssam_core_exit(void)
{
	serdev_device_driver_unregister(&ssam_serial_hub);
	ssam_event_item_cache_destroy();
	ssh_ctrl_packet_cache_destroy();
	ssam_bus_unregister();
}
module_exit(ssam_core_exit);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Subsystem and Surface Serial Hub driver for Surface System Aggregator Module");
MODULE_LICENSE("GPL");
