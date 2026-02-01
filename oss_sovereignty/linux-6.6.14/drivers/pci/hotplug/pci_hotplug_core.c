
 

#include <linux/module.h>	 
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/uaccess.h>
#include "../pci.h"
#include "cpci_hotplug.h"

#define MY_NAME	"pci_hotplug"

#define dbg(fmt, arg...) do { if (debug) printk(KERN_DEBUG "%s: %s: " fmt, MY_NAME, __func__, ## arg); } while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format, MY_NAME, ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format, MY_NAME, ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format, MY_NAME, ## arg)

 
static bool debug;

static LIST_HEAD(pci_hotplug_slot_list);
static DEFINE_MUTEX(pci_hp_mutex);

 
#define GET_STATUS(name, type)	\
static int get_##name(struct hotplug_slot *slot, type *value)		\
{									\
	const struct hotplug_slot_ops *ops = slot->ops;			\
	int retval = 0;							\
	if (!try_module_get(slot->owner))				\
		return -ENODEV;						\
	if (ops->get_##name)						\
		retval = ops->get_##name(slot, value);			\
	module_put(slot->owner);					\
	return retval;							\
}

GET_STATUS(power_status, u8)
GET_STATUS(attention_status, u8)
GET_STATUS(latch_status, u8)
GET_STATUS(adapter_status, u8)

static ssize_t power_read_file(struct pci_slot *pci_slot, char *buf)
{
	int retval;
	u8 value;

	retval = get_power_status(pci_slot->hotplug, &value);
	if (retval)
		return retval;

	return sysfs_emit(buf, "%d\n", value);
}

static ssize_t power_write_file(struct pci_slot *pci_slot, const char *buf,
				size_t count)
{
	struct hotplug_slot *slot = pci_slot->hotplug;
	unsigned long lpower;
	u8 power;
	int retval = 0;

	lpower = simple_strtoul(buf, NULL, 10);
	power = (u8)(lpower & 0xff);
	dbg("power = %d\n", power);

	if (!try_module_get(slot->owner)) {
		retval = -ENODEV;
		goto exit;
	}
	switch (power) {
	case 0:
		if (slot->ops->disable_slot)
			retval = slot->ops->disable_slot(slot);
		break;

	case 1:
		if (slot->ops->enable_slot)
			retval = slot->ops->enable_slot(slot);
		break;

	default:
		err("Illegal value specified for power\n");
		retval = -EINVAL;
	}
	module_put(slot->owner);

exit:
	if (retval)
		return retval;
	return count;
}

static struct pci_slot_attribute hotplug_slot_attr_power = {
	.attr = {.name = "power", .mode = S_IFREG | S_IRUGO | S_IWUSR},
	.show = power_read_file,
	.store = power_write_file
};

static ssize_t attention_read_file(struct pci_slot *pci_slot, char *buf)
{
	int retval;
	u8 value;

	retval = get_attention_status(pci_slot->hotplug, &value);
	if (retval)
		return retval;

	return sysfs_emit(buf, "%d\n", value);
}

static ssize_t attention_write_file(struct pci_slot *pci_slot, const char *buf,
				    size_t count)
{
	struct hotplug_slot *slot = pci_slot->hotplug;
	const struct hotplug_slot_ops *ops = slot->ops;
	unsigned long lattention;
	u8 attention;
	int retval = 0;

	lattention = simple_strtoul(buf, NULL, 10);
	attention = (u8)(lattention & 0xff);
	dbg(" - attention = %d\n", attention);

	if (!try_module_get(slot->owner)) {
		retval = -ENODEV;
		goto exit;
	}
	if (ops->set_attention_status)
		retval = ops->set_attention_status(slot, attention);
	module_put(slot->owner);

exit:
	if (retval)
		return retval;
	return count;
}

static struct pci_slot_attribute hotplug_slot_attr_attention = {
	.attr = {.name = "attention", .mode = S_IFREG | S_IRUGO | S_IWUSR},
	.show = attention_read_file,
	.store = attention_write_file
};

static ssize_t latch_read_file(struct pci_slot *pci_slot, char *buf)
{
	int retval;
	u8 value;

	retval = get_latch_status(pci_slot->hotplug, &value);
	if (retval)
		return retval;

	return sysfs_emit(buf, "%d\n", value);
}

static struct pci_slot_attribute hotplug_slot_attr_latch = {
	.attr = {.name = "latch", .mode = S_IFREG | S_IRUGO},
	.show = latch_read_file,
};

static ssize_t presence_read_file(struct pci_slot *pci_slot, char *buf)
{
	int retval;
	u8 value;

	retval = get_adapter_status(pci_slot->hotplug, &value);
	if (retval)
		return retval;

	return sysfs_emit(buf, "%d\n", value);
}

static struct pci_slot_attribute hotplug_slot_attr_presence = {
	.attr = {.name = "adapter", .mode = S_IFREG | S_IRUGO},
	.show = presence_read_file,
};

static ssize_t test_write_file(struct pci_slot *pci_slot, const char *buf,
			       size_t count)
{
	struct hotplug_slot *slot = pci_slot->hotplug;
	unsigned long ltest;
	u32 test;
	int retval = 0;

	ltest = simple_strtoul(buf, NULL, 10);
	test = (u32)(ltest & 0xffffffff);
	dbg("test = %d\n", test);

	if (!try_module_get(slot->owner)) {
		retval = -ENODEV;
		goto exit;
	}
	if (slot->ops->hardware_test)
		retval = slot->ops->hardware_test(slot, test);
	module_put(slot->owner);

exit:
	if (retval)
		return retval;
	return count;
}

static struct pci_slot_attribute hotplug_slot_attr_test = {
	.attr = {.name = "test", .mode = S_IFREG | S_IRUGO | S_IWUSR},
	.store = test_write_file
};

static bool has_power_file(struct pci_slot *pci_slot)
{
	struct hotplug_slot *slot = pci_slot->hotplug;

	if ((!slot) || (!slot->ops))
		return false;
	if ((slot->ops->enable_slot) ||
	    (slot->ops->disable_slot) ||
	    (slot->ops->get_power_status))
		return true;
	return false;
}

static bool has_attention_file(struct pci_slot *pci_slot)
{
	struct hotplug_slot *slot = pci_slot->hotplug;

	if ((!slot) || (!slot->ops))
		return false;
	if ((slot->ops->set_attention_status) ||
	    (slot->ops->get_attention_status))
		return true;
	return false;
}

static bool has_latch_file(struct pci_slot *pci_slot)
{
	struct hotplug_slot *slot = pci_slot->hotplug;

	if ((!slot) || (!slot->ops))
		return false;
	if (slot->ops->get_latch_status)
		return true;
	return false;
}

static bool has_adapter_file(struct pci_slot *pci_slot)
{
	struct hotplug_slot *slot = pci_slot->hotplug;

	if ((!slot) || (!slot->ops))
		return false;
	if (slot->ops->get_adapter_status)
		return true;
	return false;
}

static bool has_test_file(struct pci_slot *pci_slot)
{
	struct hotplug_slot *slot = pci_slot->hotplug;

	if ((!slot) || (!slot->ops))
		return false;
	if (slot->ops->hardware_test)
		return true;
	return false;
}

static int fs_add_slot(struct pci_slot *pci_slot)
{
	int retval = 0;

	 
	pci_hp_create_module_link(pci_slot);

	if (has_power_file(pci_slot)) {
		retval = sysfs_create_file(&pci_slot->kobj,
					   &hotplug_slot_attr_power.attr);
		if (retval)
			goto exit_power;
	}

	if (has_attention_file(pci_slot)) {
		retval = sysfs_create_file(&pci_slot->kobj,
					   &hotplug_slot_attr_attention.attr);
		if (retval)
			goto exit_attention;
	}

	if (has_latch_file(pci_slot)) {
		retval = sysfs_create_file(&pci_slot->kobj,
					   &hotplug_slot_attr_latch.attr);
		if (retval)
			goto exit_latch;
	}

	if (has_adapter_file(pci_slot)) {
		retval = sysfs_create_file(&pci_slot->kobj,
					   &hotplug_slot_attr_presence.attr);
		if (retval)
			goto exit_adapter;
	}

	if (has_test_file(pci_slot)) {
		retval = sysfs_create_file(&pci_slot->kobj,
					   &hotplug_slot_attr_test.attr);
		if (retval)
			goto exit_test;
	}

	goto exit;

exit_test:
	if (has_adapter_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj,
				  &hotplug_slot_attr_presence.attr);
exit_adapter:
	if (has_latch_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj, &hotplug_slot_attr_latch.attr);
exit_latch:
	if (has_attention_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj,
				  &hotplug_slot_attr_attention.attr);
exit_attention:
	if (has_power_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj, &hotplug_slot_attr_power.attr);
exit_power:
	pci_hp_remove_module_link(pci_slot);
exit:
	return retval;
}

static void fs_remove_slot(struct pci_slot *pci_slot)
{
	if (has_power_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj, &hotplug_slot_attr_power.attr);

	if (has_attention_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj,
				  &hotplug_slot_attr_attention.attr);

	if (has_latch_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj, &hotplug_slot_attr_latch.attr);

	if (has_adapter_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj,
				  &hotplug_slot_attr_presence.attr);

	if (has_test_file(pci_slot))
		sysfs_remove_file(&pci_slot->kobj, &hotplug_slot_attr_test.attr);

	pci_hp_remove_module_link(pci_slot);
}

static struct hotplug_slot *get_slot_from_name(const char *name)
{
	struct hotplug_slot *slot;

	list_for_each_entry(slot, &pci_hotplug_slot_list, slot_list) {
		if (strcmp(hotplug_slot_name(slot), name) == 0)
			return slot;
	}
	return NULL;
}

 
int __pci_hp_register(struct hotplug_slot *slot, struct pci_bus *bus,
		      int devnr, const char *name,
		      struct module *owner, const char *mod_name)
{
	int result;

	result = __pci_hp_initialize(slot, bus, devnr, name, owner, mod_name);
	if (result)
		return result;

	result = pci_hp_add(slot);
	if (result)
		pci_hp_destroy(slot);

	return result;
}
EXPORT_SYMBOL_GPL(__pci_hp_register);

 
int __pci_hp_initialize(struct hotplug_slot *slot, struct pci_bus *bus,
			int devnr, const char *name, struct module *owner,
			const char *mod_name)
{
	struct pci_slot *pci_slot;

	if (slot == NULL)
		return -ENODEV;
	if (slot->ops == NULL)
		return -EINVAL;

	slot->owner = owner;
	slot->mod_name = mod_name;

	 
	pci_slot = pci_create_slot(bus, devnr, name, slot);
	if (IS_ERR(pci_slot))
		return PTR_ERR(pci_slot);

	slot->pci_slot = pci_slot;
	pci_slot->hotplug = slot;
	return 0;
}
EXPORT_SYMBOL_GPL(__pci_hp_initialize);

 
int pci_hp_add(struct hotplug_slot *slot)
{
	struct pci_slot *pci_slot = slot->pci_slot;
	int result;

	result = fs_add_slot(pci_slot);
	if (result)
		return result;

	kobject_uevent(&pci_slot->kobj, KOBJ_ADD);
	mutex_lock(&pci_hp_mutex);
	list_add(&slot->slot_list, &pci_hotplug_slot_list);
	mutex_unlock(&pci_hp_mutex);
	dbg("Added slot %s to the list\n", hotplug_slot_name(slot));
	return 0;
}
EXPORT_SYMBOL_GPL(pci_hp_add);

 
void pci_hp_deregister(struct hotplug_slot *slot)
{
	pci_hp_del(slot);
	pci_hp_destroy(slot);
}
EXPORT_SYMBOL_GPL(pci_hp_deregister);

 
void pci_hp_del(struct hotplug_slot *slot)
{
	struct hotplug_slot *temp;

	if (WARN_ON(!slot))
		return;

	mutex_lock(&pci_hp_mutex);
	temp = get_slot_from_name(hotplug_slot_name(slot));
	if (WARN_ON(temp != slot)) {
		mutex_unlock(&pci_hp_mutex);
		return;
	}

	list_del(&slot->slot_list);
	mutex_unlock(&pci_hp_mutex);
	dbg("Removed slot %s from the list\n", hotplug_slot_name(slot));
	fs_remove_slot(slot->pci_slot);
}
EXPORT_SYMBOL_GPL(pci_hp_del);

 
void pci_hp_destroy(struct hotplug_slot *slot)
{
	struct pci_slot *pci_slot = slot->pci_slot;

	slot->pci_slot = NULL;
	pci_slot->hotplug = NULL;
	pci_destroy_slot(pci_slot);
}
EXPORT_SYMBOL_GPL(pci_hp_destroy);

static int __init pci_hotplug_init(void)
{
	int result;

	result = cpci_hotplug_init(debug);
	if (result) {
		err("cpci_hotplug_init with error %d\n", result);
		return result;
	}

	return result;
}
device_initcall(pci_hotplug_init);

 
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debugging mode enabled or not");
