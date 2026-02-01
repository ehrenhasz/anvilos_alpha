
 

#include <linux/acpi.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/serdev.h>
#include <linux/slab.h>
#include <linux/platform_data/x86/apple.h>

static bool is_registered;
static DEFINE_IDA(ctrl_ida);

static ssize_t modalias_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int len;

	len = acpi_device_modalias(dev, buf, PAGE_SIZE - 1);
	if (len != -ENODEV)
		return len;

	return of_device_modalias(dev, buf, PAGE_SIZE);
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *serdev_device_attrs[] = {
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(serdev_device);

static int serdev_device_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	int rc;

	 

	rc = acpi_device_uevent_modalias(dev, env);
	if (rc != -ENODEV)
		return rc;

	return of_device_uevent_modalias(dev, env);
}

static void serdev_device_release(struct device *dev)
{
	struct serdev_device *serdev = to_serdev_device(dev);
	kfree(serdev);
}

static const struct device_type serdev_device_type = {
	.groups		= serdev_device_groups,
	.uevent		= serdev_device_uevent,
	.release	= serdev_device_release,
};

static bool is_serdev_device(const struct device *dev)
{
	return dev->type == &serdev_device_type;
}

static void serdev_ctrl_release(struct device *dev)
{
	struct serdev_controller *ctrl = to_serdev_controller(dev);
	ida_simple_remove(&ctrl_ida, ctrl->nr);
	kfree(ctrl);
}

static const struct device_type serdev_ctrl_type = {
	.release	= serdev_ctrl_release,
};

static int serdev_device_match(struct device *dev, struct device_driver *drv)
{
	if (!is_serdev_device(dev))
		return 0;

	 
	if (acpi_driver_match_device(dev, drv))
		return 1;

	return of_driver_match_device(dev, drv);
}

 
int serdev_device_add(struct serdev_device *serdev)
{
	struct serdev_controller *ctrl = serdev->ctrl;
	struct device *parent = serdev->dev.parent;
	int err;

	dev_set_name(&serdev->dev, "%s-%d", dev_name(parent), serdev->nr);

	 
	if (ctrl->serdev) {
		dev_err(&serdev->dev, "controller busy\n");
		return -EBUSY;
	}
	ctrl->serdev = serdev;

	err = device_add(&serdev->dev);
	if (err < 0) {
		dev_err(&serdev->dev, "Can't add %s, status %pe\n",
			dev_name(&serdev->dev), ERR_PTR(err));
		goto err_clear_serdev;
	}

	dev_dbg(&serdev->dev, "device %s registered\n", dev_name(&serdev->dev));

	return 0;

err_clear_serdev:
	ctrl->serdev = NULL;
	return err;
}
EXPORT_SYMBOL_GPL(serdev_device_add);

 
void serdev_device_remove(struct serdev_device *serdev)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	device_unregister(&serdev->dev);
	ctrl->serdev = NULL;
}
EXPORT_SYMBOL_GPL(serdev_device_remove);

int serdev_device_open(struct serdev_device *serdev)
{
	struct serdev_controller *ctrl = serdev->ctrl;
	int ret;

	if (!ctrl || !ctrl->ops->open)
		return -EINVAL;

	ret = ctrl->ops->open(ctrl);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(&ctrl->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(&ctrl->dev);
		goto err_close;
	}

	return 0;

err_close:
	if (ctrl->ops->close)
		ctrl->ops->close(ctrl);

	return ret;
}
EXPORT_SYMBOL_GPL(serdev_device_open);

void serdev_device_close(struct serdev_device *serdev)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->close)
		return;

	pm_runtime_put(&ctrl->dev);

	ctrl->ops->close(ctrl);
}
EXPORT_SYMBOL_GPL(serdev_device_close);

static void devm_serdev_device_release(struct device *dev, void *dr)
{
	serdev_device_close(*(struct serdev_device **)dr);
}

int devm_serdev_device_open(struct device *dev, struct serdev_device *serdev)
{
	struct serdev_device **dr;
	int ret;

	dr = devres_alloc(devm_serdev_device_release, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	ret = serdev_device_open(serdev);
	if (ret) {
		devres_free(dr);
		return ret;
	}

	*dr = serdev;
	devres_add(dev, dr);

	return 0;
}
EXPORT_SYMBOL_GPL(devm_serdev_device_open);

void serdev_device_write_wakeup(struct serdev_device *serdev)
{
	complete(&serdev->write_comp);
}
EXPORT_SYMBOL_GPL(serdev_device_write_wakeup);

 
int serdev_device_write_buf(struct serdev_device *serdev,
			    const unsigned char *buf, size_t count)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->write_buf)
		return -EINVAL;

	return ctrl->ops->write_buf(ctrl, buf, count);
}
EXPORT_SYMBOL_GPL(serdev_device_write_buf);

 
int serdev_device_write(struct serdev_device *serdev,
			const unsigned char *buf, size_t count,
			long timeout)
{
	struct serdev_controller *ctrl = serdev->ctrl;
	int written = 0;
	int ret;

	if (!ctrl || !ctrl->ops->write_buf || !serdev->ops->write_wakeup)
		return -EINVAL;

	if (timeout == 0)
		timeout = MAX_SCHEDULE_TIMEOUT;

	mutex_lock(&serdev->write_lock);
	do {
		reinit_completion(&serdev->write_comp);

		ret = ctrl->ops->write_buf(ctrl, buf, count);
		if (ret < 0)
			break;

		written += ret;
		buf += ret;
		count -= ret;

		if (count == 0)
			break;

		timeout = wait_for_completion_interruptible_timeout(&serdev->write_comp,
								    timeout);
	} while (timeout > 0);
	mutex_unlock(&serdev->write_lock);

	if (ret < 0)
		return ret;

	if (timeout <= 0 && written == 0) {
		if (timeout == -ERESTARTSYS)
			return -ERESTARTSYS;
		else
			return -ETIMEDOUT;
	}

	return written;
}
EXPORT_SYMBOL_GPL(serdev_device_write);

void serdev_device_write_flush(struct serdev_device *serdev)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->write_flush)
		return;

	ctrl->ops->write_flush(ctrl);
}
EXPORT_SYMBOL_GPL(serdev_device_write_flush);

int serdev_device_write_room(struct serdev_device *serdev)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->write_room)
		return 0;

	return serdev->ctrl->ops->write_room(ctrl);
}
EXPORT_SYMBOL_GPL(serdev_device_write_room);

unsigned int serdev_device_set_baudrate(struct serdev_device *serdev, unsigned int speed)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->set_baudrate)
		return 0;

	return ctrl->ops->set_baudrate(ctrl, speed);

}
EXPORT_SYMBOL_GPL(serdev_device_set_baudrate);

void serdev_device_set_flow_control(struct serdev_device *serdev, bool enable)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->set_flow_control)
		return;

	ctrl->ops->set_flow_control(ctrl, enable);
}
EXPORT_SYMBOL_GPL(serdev_device_set_flow_control);

int serdev_device_set_parity(struct serdev_device *serdev,
			     enum serdev_parity parity)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->set_parity)
		return -EOPNOTSUPP;

	return ctrl->ops->set_parity(ctrl, parity);
}
EXPORT_SYMBOL_GPL(serdev_device_set_parity);

void serdev_device_wait_until_sent(struct serdev_device *serdev, long timeout)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->wait_until_sent)
		return;

	ctrl->ops->wait_until_sent(ctrl, timeout);
}
EXPORT_SYMBOL_GPL(serdev_device_wait_until_sent);

int serdev_device_get_tiocm(struct serdev_device *serdev)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->get_tiocm)
		return -EOPNOTSUPP;

	return ctrl->ops->get_tiocm(ctrl);
}
EXPORT_SYMBOL_GPL(serdev_device_get_tiocm);

int serdev_device_set_tiocm(struct serdev_device *serdev, int set, int clear)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->set_tiocm)
		return -EOPNOTSUPP;

	return ctrl->ops->set_tiocm(ctrl, set, clear);
}
EXPORT_SYMBOL_GPL(serdev_device_set_tiocm);

int serdev_device_break_ctl(struct serdev_device *serdev, int break_state)
{
	struct serdev_controller *ctrl = serdev->ctrl;

	if (!ctrl || !ctrl->ops->break_ctl)
		return -EOPNOTSUPP;

	return ctrl->ops->break_ctl(ctrl, break_state);
}
EXPORT_SYMBOL_GPL(serdev_device_break_ctl);

static int serdev_drv_probe(struct device *dev)
{
	const struct serdev_device_driver *sdrv = to_serdev_device_driver(dev->driver);
	int ret;

	ret = dev_pm_domain_attach(dev, true);
	if (ret)
		return ret;

	ret = sdrv->probe(to_serdev_device(dev));
	if (ret)
		dev_pm_domain_detach(dev, true);

	return ret;
}

static void serdev_drv_remove(struct device *dev)
{
	const struct serdev_device_driver *sdrv = to_serdev_device_driver(dev->driver);
	if (sdrv->remove)
		sdrv->remove(to_serdev_device(dev));

	dev_pm_domain_detach(dev, true);
}

static struct bus_type serdev_bus_type = {
	.name		= "serial",
	.match		= serdev_device_match,
	.probe		= serdev_drv_probe,
	.remove		= serdev_drv_remove,
};

 
struct serdev_device *serdev_device_alloc(struct serdev_controller *ctrl)
{
	struct serdev_device *serdev;

	serdev = kzalloc(sizeof(*serdev), GFP_KERNEL);
	if (!serdev)
		return NULL;

	serdev->ctrl = ctrl;
	device_initialize(&serdev->dev);
	serdev->dev.parent = &ctrl->dev;
	serdev->dev.bus = &serdev_bus_type;
	serdev->dev.type = &serdev_device_type;
	init_completion(&serdev->write_comp);
	mutex_init(&serdev->write_lock);
	return serdev;
}
EXPORT_SYMBOL_GPL(serdev_device_alloc);

 
struct serdev_controller *serdev_controller_alloc(struct device *parent,
					      size_t size)
{
	struct serdev_controller *ctrl;
	int id;

	if (WARN_ON(!parent))
		return NULL;

	ctrl = kzalloc(sizeof(*ctrl) + size, GFP_KERNEL);
	if (!ctrl)
		return NULL;

	id = ida_simple_get(&ctrl_ida, 0, 0, GFP_KERNEL);
	if (id < 0) {
		dev_err(parent,
			"unable to allocate serdev controller identifier.\n");
		goto err_free;
	}

	ctrl->nr = id;

	device_initialize(&ctrl->dev);
	ctrl->dev.type = &serdev_ctrl_type;
	ctrl->dev.bus = &serdev_bus_type;
	ctrl->dev.parent = parent;
	ctrl->dev.of_node = parent->of_node;
	serdev_controller_set_drvdata(ctrl, &ctrl[1]);

	dev_set_name(&ctrl->dev, "serial%d", id);

	pm_runtime_no_callbacks(&ctrl->dev);
	pm_suspend_ignore_children(&ctrl->dev, true);

	dev_dbg(&ctrl->dev, "allocated controller 0x%p id %d\n", ctrl, id);
	return ctrl;

err_free:
	kfree(ctrl);

	return NULL;
}
EXPORT_SYMBOL_GPL(serdev_controller_alloc);

static int of_serdev_register_devices(struct serdev_controller *ctrl)
{
	struct device_node *node;
	struct serdev_device *serdev = NULL;
	int err;
	bool found = false;

	for_each_available_child_of_node(ctrl->dev.of_node, node) {
		if (!of_get_property(node, "compatible", NULL))
			continue;

		dev_dbg(&ctrl->dev, "adding child %pOF\n", node);

		serdev = serdev_device_alloc(ctrl);
		if (!serdev)
			continue;

		device_set_node(&serdev->dev, of_fwnode_handle(node));

		err = serdev_device_add(serdev);
		if (err) {
			dev_err(&serdev->dev,
				"failure adding device. status %pe\n",
				ERR_PTR(err));
			serdev_device_put(serdev);
		} else
			found = true;
	}
	if (!found)
		return -ENODEV;

	return 0;
}

#ifdef CONFIG_ACPI

#define SERDEV_ACPI_MAX_SCAN_DEPTH 32

struct acpi_serdev_lookup {
	acpi_handle device_handle;
	acpi_handle controller_handle;
	int n;
	int index;
};

 
bool serdev_acpi_get_uart_resource(struct acpi_resource *ares,
				   struct acpi_resource_uart_serialbus **uart)
{
	struct acpi_resource_uart_serialbus *sb;

	if (ares->type != ACPI_RESOURCE_TYPE_SERIAL_BUS)
		return false;

	sb = &ares->data.uart_serial_bus;
	if (sb->type != ACPI_RESOURCE_SERIAL_TYPE_UART)
		return false;

	*uart = sb;
	return true;
}
EXPORT_SYMBOL_GPL(serdev_acpi_get_uart_resource);

static int acpi_serdev_parse_resource(struct acpi_resource *ares, void *data)
{
	struct acpi_serdev_lookup *lookup = data;
	struct acpi_resource_uart_serialbus *sb;
	acpi_status status;

	if (!serdev_acpi_get_uart_resource(ares, &sb))
		return 1;

	if (lookup->index != -1 && lookup->n++ != lookup->index)
		return 1;

	status = acpi_get_handle(lookup->device_handle,
				 sb->resource_source.string_ptr,
				 &lookup->controller_handle);
	if (ACPI_FAILURE(status))
		return 1;

	 

	return 1;
}

static int acpi_serdev_do_lookup(struct acpi_device *adev,
                                 struct acpi_serdev_lookup *lookup)
{
	struct list_head resource_list;
	int ret;

	lookup->device_handle = acpi_device_handle(adev);
	lookup->controller_handle = NULL;
	lookup->n = 0;

	INIT_LIST_HEAD(&resource_list);
	ret = acpi_dev_get_resources(adev, &resource_list,
				     acpi_serdev_parse_resource, lookup);
	acpi_dev_free_resource_list(&resource_list);

	if (ret < 0)
		return -EINVAL;

	return 0;
}

static int acpi_serdev_check_resources(struct serdev_controller *ctrl,
				       struct acpi_device *adev)
{
	struct acpi_serdev_lookup lookup;
	int ret;

	if (acpi_bus_get_status(adev) || !adev->status.present)
		return -EINVAL;

	 
	lookup.index = -1;	

	ret = acpi_serdev_do_lookup(adev, &lookup);
	if (ret)
		return ret;

	 
	if (!lookup.controller_handle && x86_apple_machine &&
	    !acpi_dev_get_property(adev, "baud", ACPI_TYPE_BUFFER, NULL))
		acpi_get_parent(adev->handle, &lookup.controller_handle);

	 
	if (ACPI_HANDLE(ctrl->dev.parent) != lookup.controller_handle)
		return -ENODEV;

	return 0;
}

static acpi_status acpi_serdev_register_device(struct serdev_controller *ctrl,
					       struct acpi_device *adev)
{
	struct serdev_device *serdev;
	int err;

	serdev = serdev_device_alloc(ctrl);
	if (!serdev) {
		dev_err(&ctrl->dev, "failed to allocate serdev device for %s\n",
			dev_name(&adev->dev));
		return AE_NO_MEMORY;
	}

	ACPI_COMPANION_SET(&serdev->dev, adev);
	acpi_device_set_enumerated(adev);

	err = serdev_device_add(serdev);
	if (err) {
		dev_err(&serdev->dev,
			"failure adding ACPI serdev device. status %pe\n",
			ERR_PTR(err));
		serdev_device_put(serdev);
	}

	return AE_OK;
}

static const struct acpi_device_id serdev_acpi_devices_blacklist[] = {
	{ "INT3511", 0 },
	{ "INT3512", 0 },
	{ },
};

static acpi_status acpi_serdev_add_device(acpi_handle handle, u32 level,
					  void *data, void **return_value)
{
	struct acpi_device *adev = acpi_fetch_acpi_dev(handle);
	struct serdev_controller *ctrl = data;

	if (!adev || acpi_device_enumerated(adev))
		return AE_OK;

	 
	if (!acpi_match_device_ids(adev, serdev_acpi_devices_blacklist))
		return AE_OK;

	if (acpi_serdev_check_resources(ctrl, adev))
		return AE_OK;

	return acpi_serdev_register_device(ctrl, adev);
}


static int acpi_serdev_register_devices(struct serdev_controller *ctrl)
{
	acpi_status status;
	bool skip;
	int ret;

	if (!has_acpi_companion(ctrl->dev.parent))
		return -ENODEV;

	 
	ret = acpi_quirk_skip_serdev_enumeration(ctrl->dev.parent, &skip);
	if (ret)
		return ret;
	if (skip)
		return 0;

	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     SERDEV_ACPI_MAX_SCAN_DEPTH,
				     acpi_serdev_add_device, NULL, ctrl, NULL);
	if (ACPI_FAILURE(status))
		dev_warn(&ctrl->dev, "failed to enumerate serdev slaves\n");

	if (!ctrl->serdev)
		return -ENODEV;

	return 0;
}
#else
static inline int acpi_serdev_register_devices(struct serdev_controller *ctrl)
{
	return -ENODEV;
}
#endif  

 
int serdev_controller_add(struct serdev_controller *ctrl)
{
	int ret_of, ret_acpi, ret;

	 
	if (WARN_ON(!is_registered))
		return -EAGAIN;

	ret = device_add(&ctrl->dev);
	if (ret)
		return ret;

	pm_runtime_enable(&ctrl->dev);

	ret_of = of_serdev_register_devices(ctrl);
	ret_acpi = acpi_serdev_register_devices(ctrl);
	if (ret_of && ret_acpi) {
		dev_dbg(&ctrl->dev, "no devices registered: of:%pe acpi:%pe\n",
			ERR_PTR(ret_of), ERR_PTR(ret_acpi));
		ret = -ENODEV;
		goto err_rpm_disable;
	}

	dev_dbg(&ctrl->dev, "serdev%d registered: dev:%p\n",
		ctrl->nr, &ctrl->dev);
	return 0;

err_rpm_disable:
	pm_runtime_disable(&ctrl->dev);
	device_del(&ctrl->dev);
	return ret;
};
EXPORT_SYMBOL_GPL(serdev_controller_add);

 
static int serdev_remove_device(struct device *dev, void *data)
{
	struct serdev_device *serdev = to_serdev_device(dev);
	if (dev->type == &serdev_device_type)
		serdev_device_remove(serdev);
	return 0;
}

 
void serdev_controller_remove(struct serdev_controller *ctrl)
{
	if (!ctrl)
		return;

	device_for_each_child(&ctrl->dev, NULL, serdev_remove_device);
	pm_runtime_disable(&ctrl->dev);
	device_del(&ctrl->dev);
}
EXPORT_SYMBOL_GPL(serdev_controller_remove);

 
int __serdev_device_driver_register(struct serdev_device_driver *sdrv, struct module *owner)
{
	sdrv->driver.bus = &serdev_bus_type;
	sdrv->driver.owner = owner;

	 
        sdrv->driver.probe_type = PROBE_PREFER_ASYNCHRONOUS;

	return driver_register(&sdrv->driver);
}
EXPORT_SYMBOL_GPL(__serdev_device_driver_register);

static void __exit serdev_exit(void)
{
	bus_unregister(&serdev_bus_type);
	ida_destroy(&ctrl_ida);
}
module_exit(serdev_exit);

static int __init serdev_init(void)
{
	int ret;

	ret = bus_register(&serdev_bus_type);
	if (ret)
		return ret;

	is_registered = true;
	return 0;
}
 
postcore_initcall(serdev_init);

MODULE_AUTHOR("Rob Herring <robh@kernel.org>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Serial attached device bus");
