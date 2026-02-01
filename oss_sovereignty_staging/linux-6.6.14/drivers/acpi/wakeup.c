
 

#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "internal.h"
#include "sleep.h"

struct acpi_wakeup_handler {
	struct list_head list_node;
	bool (*wakeup)(void *context);
	void *context;
};

static LIST_HEAD(acpi_wakeup_handler_head);
static DEFINE_MUTEX(acpi_wakeup_handler_mutex);

 

 
void acpi_enable_wakeup_devices(u8 sleep_state)
{
	struct acpi_device *dev, *tmp;

	list_for_each_entry_safe(dev, tmp, &acpi_wakeup_device_list,
				 wakeup_list) {
		if (!dev->wakeup.flags.valid
		    || sleep_state > (u32) dev->wakeup.sleep_state
		    || !(device_may_wakeup(&dev->dev)
			 || dev->wakeup.prepare_count))
			continue;

		if (device_may_wakeup(&dev->dev))
			acpi_enable_wakeup_device_power(dev, sleep_state);

		 
		acpi_set_gpe_wake_mask(dev->wakeup.gpe_device, dev->wakeup.gpe_number,
				ACPI_GPE_ENABLE);
	}
}

 
void acpi_disable_wakeup_devices(u8 sleep_state)
{
	struct acpi_device *dev, *tmp;

	list_for_each_entry_safe(dev, tmp, &acpi_wakeup_device_list,
				 wakeup_list) {
		if (!dev->wakeup.flags.valid
		    || sleep_state > (u32) dev->wakeup.sleep_state
		    || !(device_may_wakeup(&dev->dev)
			 || dev->wakeup.prepare_count))
			continue;

		acpi_set_gpe_wake_mask(dev->wakeup.gpe_device, dev->wakeup.gpe_number,
				ACPI_GPE_DISABLE);

		if (device_may_wakeup(&dev->dev))
			acpi_disable_wakeup_device_power(dev);
	}
}

int __init acpi_wakeup_device_init(void)
{
	struct acpi_device *dev, *tmp;

	mutex_lock(&acpi_device_lock);
	list_for_each_entry_safe(dev, tmp, &acpi_wakeup_device_list,
				 wakeup_list) {
		if (device_can_wakeup(&dev->dev)) {
			 
			acpi_enable_gpe(dev->wakeup.gpe_device,
					dev->wakeup.gpe_number);
			device_set_wakeup_enable(&dev->dev, true);
		}
	}
	mutex_unlock(&acpi_device_lock);
	return 0;
}

 
int acpi_register_wakeup_handler(int wake_irq, bool (*wakeup)(void *context),
				 void *context)
{
	struct acpi_wakeup_handler *handler;

	 
	if (!acpi_sci_irq_valid() || wake_irq != acpi_sci_irq)
		return 0;

	handler = kmalloc(sizeof(*handler), GFP_KERNEL);
	if (!handler)
		return -ENOMEM;

	handler->wakeup = wakeup;
	handler->context = context;

	mutex_lock(&acpi_wakeup_handler_mutex);
	list_add(&handler->list_node, &acpi_wakeup_handler_head);
	mutex_unlock(&acpi_wakeup_handler_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(acpi_register_wakeup_handler);

 
void acpi_unregister_wakeup_handler(bool (*wakeup)(void *context),
				    void *context)
{
	struct acpi_wakeup_handler *handler;

	mutex_lock(&acpi_wakeup_handler_mutex);
	list_for_each_entry(handler, &acpi_wakeup_handler_head, list_node) {
		if (handler->wakeup == wakeup && handler->context == context) {
			list_del(&handler->list_node);
			kfree(handler);
			break;
		}
	}
	mutex_unlock(&acpi_wakeup_handler_mutex);
}
EXPORT_SYMBOL_GPL(acpi_unregister_wakeup_handler);

bool acpi_check_wakeup_handlers(void)
{
	struct acpi_wakeup_handler *handler;

	 
	list_for_each_entry(handler, &acpi_wakeup_handler_head, list_node) {
		if (handler->wakeup(handler->context))
			return true;
	}

	return false;
}
