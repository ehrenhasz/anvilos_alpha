
 

#define pr_fmt(fmt) "ACPI: PCI: " fmt

#include <linux/syscore_ops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/pci.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/irq.h>

#include "internal.h"

#define ACPI_PCI_LINK_CLASS		"pci_irq_routing"
#define ACPI_PCI_LINK_DEVICE_NAME	"PCI Interrupt Link"
#define ACPI_PCI_LINK_MAX_POSSIBLE	16

static int acpi_pci_link_add(struct acpi_device *device,
			     const struct acpi_device_id *not_used);
static void acpi_pci_link_remove(struct acpi_device *device);

static const struct acpi_device_id link_device_ids[] = {
	{"PNP0C0F", 0},
	{"", 0},
};

static struct acpi_scan_handler pci_link_handler = {
	.ids = link_device_ids,
	.attach = acpi_pci_link_add,
	.detach = acpi_pci_link_remove,
};

 
struct acpi_pci_link_irq {
	u32 active;		 
	u8 triggering;		 
	u8 polarity;		 
	u8 resource_type;
	u8 possible_count;
	u32 possible[ACPI_PCI_LINK_MAX_POSSIBLE];
	u8 initialized:1;
	u8 reserved:7;
};

struct acpi_pci_link {
	struct list_head		list;
	struct acpi_device		*device;
	struct acpi_pci_link_irq	irq;
	int				refcnt;
};

static LIST_HEAD(acpi_link_list);
static DEFINE_MUTEX(acpi_link_lock);
static int sci_irq = -1, sci_penalty;

 

 
static acpi_status acpi_pci_link_check_possible(struct acpi_resource *resource,
						void *context)
{
	struct acpi_pci_link *link = context;
	acpi_handle handle = link->device->handle;
	u32 i;

	switch (resource->type) {
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
	case ACPI_RESOURCE_TYPE_END_TAG:
		return AE_OK;
	case ACPI_RESOURCE_TYPE_IRQ:
		{
			struct acpi_resource_irq *p = &resource->data.irq;
			if (!p->interrupt_count) {
				acpi_handle_debug(handle,
						  "Blank _PRS IRQ resource\n");
				return AE_OK;
			}
			for (i = 0;
			     (i < p->interrupt_count
			      && i < ACPI_PCI_LINK_MAX_POSSIBLE); i++) {
				if (!p->interrupts[i]) {
					acpi_handle_debug(handle,
							  "Invalid _PRS IRQ %d\n",
							  p->interrupts[i]);
					continue;
				}
				link->irq.possible[i] = p->interrupts[i];
				link->irq.possible_count++;
			}
			link->irq.triggering = p->triggering;
			link->irq.polarity = p->polarity;
			link->irq.resource_type = ACPI_RESOURCE_TYPE_IRQ;
			break;
		}
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		{
			struct acpi_resource_extended_irq *p =
			    &resource->data.extended_irq;
			if (!p->interrupt_count) {
				acpi_handle_debug(handle,
						  "Blank _PRS EXT IRQ resource\n");
				return AE_OK;
			}
			for (i = 0;
			     (i < p->interrupt_count
			      && i < ACPI_PCI_LINK_MAX_POSSIBLE); i++) {
				if (!p->interrupts[i]) {
					acpi_handle_debug(handle,
							  "Invalid _PRS IRQ %d\n",
							  p->interrupts[i]);
					continue;
				}
				link->irq.possible[i] = p->interrupts[i];
				link->irq.possible_count++;
			}
			link->irq.triggering = p->triggering;
			link->irq.polarity = p->polarity;
			link->irq.resource_type = ACPI_RESOURCE_TYPE_EXTENDED_IRQ;
			break;
		}
	default:
		acpi_handle_debug(handle, "_PRS resource type 0x%x is not IRQ\n",
				  resource->type);
		return AE_OK;
	}

	return AE_CTRL_TERMINATE;
}

static int acpi_pci_link_get_possible(struct acpi_pci_link *link)
{
	acpi_handle handle = link->device->handle;
	acpi_status status;

	status = acpi_walk_resources(handle, METHOD_NAME__PRS,
				     acpi_pci_link_check_possible, link);
	if (ACPI_FAILURE(status)) {
		acpi_handle_debug(handle, "_PRS not present or invalid");
		return 0;
	}

	acpi_handle_debug(handle, "Found %d possible IRQs\n",
			  link->irq.possible_count);

	return 0;
}

static acpi_status acpi_pci_link_check_current(struct acpi_resource *resource,
					       void *context)
{
	int *irq = context;

	switch (resource->type) {
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
	case ACPI_RESOURCE_TYPE_END_TAG:
		return AE_OK;
	case ACPI_RESOURCE_TYPE_IRQ:
		{
			struct acpi_resource_irq *p = &resource->data.irq;
			if (!p->interrupt_count) {
				 
				pr_debug("Blank _CRS IRQ resource\n");
				return AE_OK;
			}
			*irq = p->interrupts[0];
			break;
		}
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		{
			struct acpi_resource_extended_irq *p =
			    &resource->data.extended_irq;
			if (!p->interrupt_count) {
				 
				pr_debug("Blank _CRS EXT IRQ resource\n");
				return AE_OK;
			}
			*irq = p->interrupts[0];
			break;
		}
		break;
	default:
		pr_debug("_CRS resource type 0x%x is not IRQ\n",
			 resource->type);
		return AE_OK;
	}

	return AE_CTRL_TERMINATE;
}

 
static int acpi_pci_link_get_current(struct acpi_pci_link *link)
{
	acpi_handle handle = link->device->handle;
	acpi_status status;
	int result = 0;
	int irq = 0;

	link->irq.active = 0;

	 
	if (acpi_strict) {
		 
		result = acpi_bus_get_status(link->device);
		if (result) {
			acpi_handle_err(handle, "Unable to read status\n");
			goto end;
		}

		if (!link->device->status.enabled) {
			acpi_handle_debug(handle, "Link disabled\n");
			return 0;
		}
	}

	 

	status = acpi_walk_resources(handle, METHOD_NAME__CRS,
				     acpi_pci_link_check_current, &irq);
	if (ACPI_FAILURE(status)) {
		acpi_evaluation_failure_warn(handle, "_CRS", status);
		result = -ENODEV;
		goto end;
	}

	if (acpi_strict && !irq) {
		acpi_handle_err(handle, "_CRS returned 0\n");
		result = -ENODEV;
	}

	link->irq.active = irq;

	acpi_handle_debug(handle, "Link at IRQ %d \n", link->irq.active);

      end:
	return result;
}

static int acpi_pci_link_set(struct acpi_pci_link *link, int irq)
{
	struct {
		struct acpi_resource res;
		struct acpi_resource end;
	} *resource;
	struct acpi_buffer buffer = { 0, NULL };
	acpi_handle handle = link->device->handle;
	acpi_status status;
	int result;

	if (!irq)
		return -EINVAL;

	resource = kzalloc(sizeof(*resource) + 1, irqs_disabled() ? GFP_ATOMIC: GFP_KERNEL);
	if (!resource)
		return -ENOMEM;

	buffer.length = sizeof(*resource) + 1;
	buffer.pointer = resource;

	switch (link->irq.resource_type) {
	case ACPI_RESOURCE_TYPE_IRQ:
		resource->res.type = ACPI_RESOURCE_TYPE_IRQ;
		resource->res.length = sizeof(struct acpi_resource);
		resource->res.data.irq.triggering = link->irq.triggering;
		resource->res.data.irq.polarity =
		    link->irq.polarity;
		if (link->irq.triggering == ACPI_EDGE_SENSITIVE)
			resource->res.data.irq.shareable =
			    ACPI_EXCLUSIVE;
		else
			resource->res.data.irq.shareable = ACPI_SHARED;
		resource->res.data.irq.interrupt_count = 1;
		resource->res.data.irq.interrupts[0] = irq;
		break;

	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		resource->res.type = ACPI_RESOURCE_TYPE_EXTENDED_IRQ;
		resource->res.length = sizeof(struct acpi_resource);
		resource->res.data.extended_irq.producer_consumer =
		    ACPI_CONSUMER;
		resource->res.data.extended_irq.triggering =
		    link->irq.triggering;
		resource->res.data.extended_irq.polarity =
		    link->irq.polarity;
		if (link->irq.triggering == ACPI_EDGE_SENSITIVE)
			resource->res.data.extended_irq.shareable =
			    ACPI_EXCLUSIVE;
		else
			resource->res.data.extended_irq.shareable = ACPI_SHARED;
		resource->res.data.extended_irq.interrupt_count = 1;
		resource->res.data.extended_irq.interrupts[0] = irq;
		 
		break;
	default:
		acpi_handle_err(handle, "Invalid resource type %d\n",
				link->irq.resource_type);
		result = -EINVAL;
		goto end;

	}
	resource->end.type = ACPI_RESOURCE_TYPE_END_TAG;
	resource->end.length = sizeof(struct acpi_resource);

	 
	status = acpi_set_current_resources(link->device->handle, &buffer);

	 
	if (ACPI_FAILURE(status)) {
		acpi_evaluation_failure_warn(handle, "_SRS", status);
		result = -ENODEV;
		goto end;
	}

	 
	result = acpi_bus_get_status(link->device);
	if (result) {
		acpi_handle_err(handle, "Unable to read status\n");
		goto end;
	}
	if (!link->device->status.enabled)
		acpi_handle_warn(handle, "Disabled and referenced, BIOS bug\n");

	 
	result = acpi_pci_link_get_current(link);
	if (result) {
		goto end;
	}

	 
	if (link->irq.active != irq) {
		 
		acpi_handle_warn(handle, "BIOS reported IRQ %d, using IRQ %d\n",
				 link->irq.active, irq);
		link->irq.active = irq;
	}

	acpi_handle_debug(handle, "Set IRQ %d\n", link->irq.active);

      end:
	kfree(resource);
	return result;
}

 

 

#define ACPI_MAX_ISA_IRQS	16

#define PIRQ_PENALTY_PCI_POSSIBLE	(16*16)
#define PIRQ_PENALTY_PCI_USING		(16*16*16)
#define PIRQ_PENALTY_ISA_TYPICAL	(16*16*16*16)
#define PIRQ_PENALTY_ISA_USED		(16*16*16*16*16)
#define PIRQ_PENALTY_ISA_ALWAYS		(16*16*16*16*16*16)

static int acpi_isa_irq_penalty[ACPI_MAX_ISA_IRQS] = {
	PIRQ_PENALTY_ISA_ALWAYS,	 
	PIRQ_PENALTY_ISA_ALWAYS,	 
	PIRQ_PENALTY_ISA_ALWAYS,	 
	PIRQ_PENALTY_ISA_TYPICAL,	 
	PIRQ_PENALTY_ISA_TYPICAL,	 
	PIRQ_PENALTY_ISA_TYPICAL,	 
	PIRQ_PENALTY_ISA_TYPICAL,	 
	PIRQ_PENALTY_ISA_TYPICAL,	 
	PIRQ_PENALTY_ISA_TYPICAL,	 
	0,				 
	0,				 
	0,				 
	PIRQ_PENALTY_ISA_USED,		 
	PIRQ_PENALTY_ISA_USED,		 
	PIRQ_PENALTY_ISA_USED,		 
	PIRQ_PENALTY_ISA_USED,		 
	 
};

static int acpi_irq_pci_sharing_penalty(int irq)
{
	struct acpi_pci_link *link;
	int penalty = 0;
	int i;

	list_for_each_entry(link, &acpi_link_list, list) {
		 
		if (link->irq.active && link->irq.active == irq)
			penalty += PIRQ_PENALTY_PCI_USING;

		 
		for (i = 0; i < link->irq.possible_count; i++)
			if (link->irq.possible[i] == irq)
				penalty += PIRQ_PENALTY_PCI_POSSIBLE /
					link->irq.possible_count;
	}

	return penalty;
}

static int acpi_irq_get_penalty(int irq)
{
	int penalty = 0;

	if (irq == sci_irq)
		penalty += sci_penalty;

	if (irq < ACPI_MAX_ISA_IRQS)
		return penalty + acpi_isa_irq_penalty[irq];

	return penalty + acpi_irq_pci_sharing_penalty(irq);
}

int __init acpi_irq_penalty_init(void)
{
	struct acpi_pci_link *link;
	int i;

	 
	list_for_each_entry(link, &acpi_link_list, list) {

		 
		if (link->irq.possible_count) {
			int penalty =
			    PIRQ_PENALTY_PCI_POSSIBLE /
			    link->irq.possible_count;

			for (i = 0; i < link->irq.possible_count; i++) {
				if (link->irq.possible[i] < ACPI_MAX_ISA_IRQS)
					acpi_isa_irq_penalty[link->irq.
							 possible[i]] +=
					    penalty;
			}

		} else if (link->irq.active &&
				(link->irq.active < ACPI_MAX_ISA_IRQS)) {
			acpi_isa_irq_penalty[link->irq.active] +=
			    PIRQ_PENALTY_PCI_POSSIBLE;
		}
	}

	return 0;
}

static int acpi_irq_balance = -1;	 

static int acpi_pci_link_allocate(struct acpi_pci_link *link)
{
	acpi_handle handle = link->device->handle;
	int irq;
	int i;

	if (link->irq.initialized) {
		if (link->refcnt == 0)
			 
			acpi_pci_link_set(link, link->irq.active);
		return 0;
	}

	 
	for (i = 0; i < link->irq.possible_count; ++i) {
		if (link->irq.active == link->irq.possible[i])
			break;
	}
	 
	if (i == link->irq.possible_count) {
		if (acpi_strict)
			acpi_handle_warn(handle, "_CRS %d not found in _PRS\n",
					 link->irq.active);
		link->irq.active = 0;
	}

	 
	if (link->irq.active)
		irq = link->irq.active;
	else
		irq = link->irq.possible[link->irq.possible_count - 1];

	if (acpi_irq_balance || !link->irq.active) {
		 
		for (i = (link->irq.possible_count - 1); i >= 0; i--) {
			if (acpi_irq_get_penalty(irq) >
			    acpi_irq_get_penalty(link->irq.possible[i]))
				irq = link->irq.possible[i];
		}
	}
	if (acpi_irq_get_penalty(irq) >= PIRQ_PENALTY_ISA_ALWAYS) {
		acpi_handle_err(handle,
				"No IRQ available. Try pci=noacpi or acpi=off\n");
		return -ENODEV;
	}

	 
	if (acpi_pci_link_set(link, irq)) {
		acpi_handle_err(handle,
				"Unable to set IRQ. Try pci=noacpi or acpi=off\n");
		return -ENODEV;
	} else {
		if (link->irq.active < ACPI_MAX_ISA_IRQS)
			acpi_isa_irq_penalty[link->irq.active] +=
				PIRQ_PENALTY_PCI_USING;

		acpi_handle_info(handle, "Enabled at IRQ %d\n",
				 link->irq.active);
	}

	link->irq.initialized = 1;
	return 0;
}

 
int acpi_pci_link_allocate_irq(acpi_handle handle, int index, int *triggering,
			       int *polarity, char **name)
{
	struct acpi_device *device = acpi_fetch_acpi_dev(handle);
	struct acpi_pci_link *link;

	if (!device) {
		acpi_handle_err(handle, "Invalid link device\n");
		return -1;
	}

	link = acpi_driver_data(device);
	if (!link) {
		acpi_handle_err(handle, "Invalid link context\n");
		return -1;
	}

	 
	if (index) {
		acpi_handle_err(handle, "Invalid index %d\n", index);
		return -1;
	}

	mutex_lock(&acpi_link_lock);
	if (acpi_pci_link_allocate(link)) {
		mutex_unlock(&acpi_link_lock);
		return -1;
	}

	if (!link->irq.active) {
		mutex_unlock(&acpi_link_lock);
		acpi_handle_err(handle, "Link active IRQ is 0!\n");
		return -1;
	}
	link->refcnt++;
	mutex_unlock(&acpi_link_lock);

	if (triggering)
		*triggering = link->irq.triggering;
	if (polarity)
		*polarity = link->irq.polarity;
	if (name)
		*name = acpi_device_bid(link->device);
	acpi_handle_debug(handle, "Link is referenced\n");
	return link->irq.active;
}

 
int acpi_pci_link_free_irq(acpi_handle handle)
{
	struct acpi_device *device = acpi_fetch_acpi_dev(handle);
	struct acpi_pci_link *link;

	if (!device) {
		acpi_handle_err(handle, "Invalid link device\n");
		return -1;
	}

	link = acpi_driver_data(device);
	if (!link) {
		acpi_handle_err(handle, "Invalid link context\n");
		return -1;
	}

	mutex_lock(&acpi_link_lock);
	if (!link->irq.initialized) {
		mutex_unlock(&acpi_link_lock);
		acpi_handle_err(handle, "Link isn't initialized\n");
		return -1;
	}
#ifdef	FUTURE_USE
	 
	link->refcnt--;
#endif
	acpi_handle_debug(handle, "Link is dereferenced\n");

	if (link->refcnt == 0)
		acpi_evaluate_object(link->device->handle, "_DIS", NULL, NULL);

	mutex_unlock(&acpi_link_lock);
	return link->irq.active;
}

 

static int acpi_pci_link_add(struct acpi_device *device,
			     const struct acpi_device_id *not_used)
{
	acpi_handle handle = device->handle;
	struct acpi_pci_link *link;
	int result;
	int i;

	link = kzalloc(sizeof(struct acpi_pci_link), GFP_KERNEL);
	if (!link)
		return -ENOMEM;

	link->device = device;
	strcpy(acpi_device_name(device), ACPI_PCI_LINK_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_PCI_LINK_CLASS);
	device->driver_data = link;

	mutex_lock(&acpi_link_lock);
	result = acpi_pci_link_get_possible(link);
	if (result)
		goto end;

	 
	acpi_pci_link_get_current(link);

	pr_info("Interrupt link %s configured for IRQ %d\n",
		acpi_device_bid(device), link->irq.active);

	for (i = 0; i < link->irq.possible_count; i++) {
		if (link->irq.active != link->irq.possible[i])
			acpi_handle_debug(handle, "Possible IRQ %d\n",
					  link->irq.possible[i]);
	}

	if (!link->device->status.enabled)
		pr_info("Interrupt link %s disabled\n", acpi_device_bid(device));

	list_add_tail(&link->list, &acpi_link_list);

      end:
	 
	acpi_evaluate_object(handle, "_DIS", NULL, NULL);
	mutex_unlock(&acpi_link_lock);

	if (result)
		kfree(link);

	return result < 0 ? result : 1;
}

static int acpi_pci_link_resume(struct acpi_pci_link *link)
{
	if (link->refcnt && link->irq.active && link->irq.initialized)
		return (acpi_pci_link_set(link, link->irq.active));

	return 0;
}

static void irqrouter_resume(void)
{
	struct acpi_pci_link *link;

	list_for_each_entry(link, &acpi_link_list, list) {
		acpi_pci_link_resume(link);
	}
}

static void acpi_pci_link_remove(struct acpi_device *device)
{
	struct acpi_pci_link *link;

	link = acpi_driver_data(device);

	mutex_lock(&acpi_link_lock);
	list_del(&link->list);
	mutex_unlock(&acpi_link_lock);

	kfree(link);
}

 
static int __init acpi_irq_penalty_update(char *str, int used)
{
	int i;

	for (i = 0; i < 16; i++) {
		int retval;
		int irq;
		int new_penalty;

		retval = get_option(&str, &irq);

		if (!retval)
			break;	 

		 
		if ((irq < 0) || (irq >= ACPI_MAX_ISA_IRQS))
			continue;

		if (used)
			new_penalty = acpi_isa_irq_penalty[irq] +
					PIRQ_PENALTY_ISA_USED;
		else
			new_penalty = 0;

		acpi_isa_irq_penalty[irq] = new_penalty;
		if (retval != 2)	 
			break;
	}
	return 1;
}

 
void acpi_penalize_isa_irq(int irq, int active)
{
	if ((irq >= 0) && (irq < ARRAY_SIZE(acpi_isa_irq_penalty)))
		acpi_isa_irq_penalty[irq] +=
		  (active ? PIRQ_PENALTY_ISA_USED : PIRQ_PENALTY_PCI_USING);
}

bool acpi_isa_irq_available(int irq)
{
	return irq >= 0 && (irq >= ARRAY_SIZE(acpi_isa_irq_penalty) ||
		    acpi_irq_get_penalty(irq) < PIRQ_PENALTY_ISA_ALWAYS);
}

void acpi_penalize_sci_irq(int irq, int trigger, int polarity)
{
	sci_irq = irq;

	if (trigger == ACPI_MADT_TRIGGER_LEVEL &&
	    polarity == ACPI_MADT_POLARITY_ACTIVE_LOW)
		sci_penalty = PIRQ_PENALTY_PCI_USING;
	else
		sci_penalty = PIRQ_PENALTY_ISA_ALWAYS;
}

 
static int __init acpi_irq_isa(char *str)
{
	return acpi_irq_penalty_update(str, 1);
}

__setup("acpi_irq_isa=", acpi_irq_isa);

 
static int __init acpi_irq_pci(char *str)
{
	return acpi_irq_penalty_update(str, 0);
}

__setup("acpi_irq_pci=", acpi_irq_pci);

static int __init acpi_irq_nobalance_set(char *str)
{
	acpi_irq_balance = 0;
	return 1;
}

__setup("acpi_irq_nobalance", acpi_irq_nobalance_set);

static int __init acpi_irq_balance_set(char *str)
{
	acpi_irq_balance = 1;
	return 1;
}

__setup("acpi_irq_balance", acpi_irq_balance_set);

static struct syscore_ops irqrouter_syscore_ops = {
	.resume = irqrouter_resume,
};

void __init acpi_pci_link_init(void)
{
	if (acpi_noirq)
		return;

	if (acpi_irq_balance == -1) {
		 
		if (acpi_irq_model == ACPI_IRQ_MODEL_IOAPIC)
			acpi_irq_balance = 1;
		else
			acpi_irq_balance = 0;
	}
	register_syscore_ops(&irqrouter_syscore_ops);
	acpi_scan_add_handler(&pci_link_handler);
}
