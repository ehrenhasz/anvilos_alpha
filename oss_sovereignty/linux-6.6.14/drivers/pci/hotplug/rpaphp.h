 
 

#ifndef _PPC64PHP_H
#define _PPC64PHP_H

#include <linux/pci.h>
#include <linux/pci_hotplug.h>

#define DR_INDICATOR 9002
#define DR_ENTITY_SENSE 9003

#define POWER_ON	100
#define POWER_OFF	0

#define LED_OFF		0
#define LED_ON		1	 
#define LED_ID		2	 
#define LED_ACTION	3	 

 
#define EMPTY           0	 
#define PRESENT         1	 

#define MY_NAME "rpaphp"
extern bool rpaphp_debug;
#define dbg(format, arg...)					\
	do {							\
		if (rpaphp_debug)				\
			printk(KERN_DEBUG "%s: " format,	\
				MY_NAME, ## arg);		\
	} while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format, MY_NAME, ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format, MY_NAME, ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format, MY_NAME, ## arg)

 

#define	NOT_VALID	3
#define	NOT_CONFIGURED	2
#define	CONFIGURED	1
#define	EMPTY		0

 

#define MAX_DRC_NAME_LEN 64

 
struct slot {
	struct list_head rpaphp_slot_list;
	int state;
	u32 index;
	u32 type;
	u32 power_domain;
	u8 attention_status;
	char *name;
	struct device_node *dn;
	struct pci_bus *bus;
	struct list_head *pci_devs;
	struct hotplug_slot hotplug_slot;
};

extern const struct hotplug_slot_ops rpaphp_hotplug_slot_ops;
extern struct list_head rpaphp_slot_head;

static inline struct slot *to_slot(struct hotplug_slot *hotplug_slot)
{
	return container_of(hotplug_slot, struct slot, hotplug_slot);
}

 

 
int rpaphp_enable_slot(struct slot *slot);
int rpaphp_get_sensor_state(struct slot *slot, int *state);

 
int rpaphp_add_slot(struct device_node *dn);
int rpaphp_check_drc_props(struct device_node *dn, char *drc_name,
		char *drc_type);

 
void dealloc_slot_struct(struct slot *slot);
struct slot *alloc_slot_struct(struct device_node *dn, int drc_index, char *drc_name, int power_domain);
int rpaphp_register_slot(struct slot *slot);
int rpaphp_deregister_slot(struct slot *slot);

#endif				 
