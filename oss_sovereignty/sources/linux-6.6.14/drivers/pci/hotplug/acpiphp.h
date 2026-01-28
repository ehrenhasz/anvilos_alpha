


#ifndef _ACPIPHP_H
#define _ACPIPHP_H

#include <linux/acpi.h>
#include <linux/mutex.h>
#include <linux/pci_hotplug.h>

struct acpiphp_context;
struct acpiphp_bridge;
struct acpiphp_slot;


struct slot {
	struct hotplug_slot	hotplug_slot;
	struct acpiphp_slot	*acpi_slot;
	unsigned int sun;	
};

static inline const char *slot_name(struct slot *slot)
{
	return hotplug_slot_name(&slot->hotplug_slot);
}

static inline struct slot *to_slot(struct hotplug_slot *hotplug_slot)
{
	return container_of(hotplug_slot, struct slot, hotplug_slot);
}


struct acpiphp_bridge {
	struct list_head list;
	struct list_head slots;
	struct kref ref;

	struct acpiphp_context *context;

	int nr_slots;

	
	struct pci_bus *pci_bus;

	
	struct pci_dev *pci_dev;

	bool is_going_away;
};



struct acpiphp_slot {
	struct list_head node;
	struct pci_bus *bus;
	struct list_head funcs;		
	struct slot *slot;

	u8		device;		
	u32		flags;		
};



struct acpiphp_func {
	struct acpiphp_bridge *parent;
	struct acpiphp_slot *slot;

	struct list_head sibling;

	u8		function;	
	u32		flags;		
};

struct acpiphp_context {
	struct acpi_hotplug_context hp;
	struct acpiphp_func func;
	struct acpiphp_bridge *bridge;
	unsigned int refcount;
};

static inline struct acpiphp_context *to_acpiphp_context(struct acpi_hotplug_context *hp)
{
	return container_of(hp, struct acpiphp_context, hp);
}

static inline struct acpiphp_context *func_to_context(struct acpiphp_func *func)
{
	return container_of(func, struct acpiphp_context, func);
}

static inline struct acpi_device *func_to_acpi_device(struct acpiphp_func *func)
{
	return func_to_context(func)->hp.self;
}

static inline acpi_handle func_to_handle(struct acpiphp_func *func)
{
	return func_to_acpi_device(func)->handle;
}

struct acpiphp_root_context {
	struct acpi_hotplug_context hp;
	struct acpiphp_bridge *root_bridge;
};

static inline struct acpiphp_root_context *to_acpiphp_root_context(struct acpi_hotplug_context *hp)
{
	return container_of(hp, struct acpiphp_root_context, hp);
}


struct acpiphp_attention_info {
	int (*set_attn)(struct hotplug_slot *slot, u8 status);
	int (*get_attn)(struct hotplug_slot *slot, u8 *status);
	struct module *owner;
};


#define ACPI_STA_ALL			(0x0000000f)



#define SLOT_ENABLED		(0x00000001)
#define SLOT_IS_GOING_AWAY	(0x00000002)



#define FUNC_HAS_STA		(0x00000001)
#define FUNC_HAS_EJ0		(0x00000002)




int acpiphp_register_attention(struct acpiphp_attention_info *info);
int acpiphp_unregister_attention(struct acpiphp_attention_info *info);
int acpiphp_register_hotplug_slot(struct acpiphp_slot *slot, unsigned int sun);
void acpiphp_unregister_hotplug_slot(struct acpiphp_slot *slot);

int acpiphp_enable_slot(struct acpiphp_slot *slot);
int acpiphp_disable_slot(struct acpiphp_slot *slot);
u8 acpiphp_get_power_status(struct acpiphp_slot *slot);
u8 acpiphp_get_latch_status(struct acpiphp_slot *slot);
u8 acpiphp_get_adapter_status(struct acpiphp_slot *slot);


extern bool acpiphp_disabled;

#endif 
