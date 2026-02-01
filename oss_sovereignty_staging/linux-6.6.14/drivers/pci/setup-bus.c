
 

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/cache.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include "pci.h"

unsigned int pci_flags;
EXPORT_SYMBOL_GPL(pci_flags);

struct pci_dev_resource {
	struct list_head list;
	struct resource *res;
	struct pci_dev *dev;
	resource_size_t start;
	resource_size_t end;
	resource_size_t add_size;
	resource_size_t min_align;
	unsigned long flags;
};

static void free_list(struct list_head *head)
{
	struct pci_dev_resource *dev_res, *tmp;

	list_for_each_entry_safe(dev_res, tmp, head, list) {
		list_del(&dev_res->list);
		kfree(dev_res);
	}
}

 
static int add_to_list(struct list_head *head, struct pci_dev *dev,
		       struct resource *res, resource_size_t add_size,
		       resource_size_t min_align)
{
	struct pci_dev_resource *tmp;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	tmp->res = res;
	tmp->dev = dev;
	tmp->start = res->start;
	tmp->end = res->end;
	tmp->flags = res->flags;
	tmp->add_size = add_size;
	tmp->min_align = min_align;

	list_add(&tmp->list, head);

	return 0;
}

static void remove_from_list(struct list_head *head, struct resource *res)
{
	struct pci_dev_resource *dev_res, *tmp;

	list_for_each_entry_safe(dev_res, tmp, head, list) {
		if (dev_res->res == res) {
			list_del(&dev_res->list);
			kfree(dev_res);
			break;
		}
	}
}

static struct pci_dev_resource *res_to_dev_res(struct list_head *head,
					       struct resource *res)
{
	struct pci_dev_resource *dev_res;

	list_for_each_entry(dev_res, head, list) {
		if (dev_res->res == res)
			return dev_res;
	}

	return NULL;
}

static resource_size_t get_res_add_size(struct list_head *head,
					struct resource *res)
{
	struct pci_dev_resource *dev_res;

	dev_res = res_to_dev_res(head, res);
	return dev_res ? dev_res->add_size : 0;
}

static resource_size_t get_res_add_align(struct list_head *head,
					 struct resource *res)
{
	struct pci_dev_resource *dev_res;

	dev_res = res_to_dev_res(head, res);
	return dev_res ? dev_res->min_align : 0;
}

 
static void pdev_sort_resources(struct pci_dev *dev, struct list_head *head)
{
	struct resource *r;
	int i;

	pci_dev_for_each_resource(dev, r, i) {
		struct pci_dev_resource *dev_res, *tmp;
		resource_size_t r_align;
		struct list_head *n;

		if (r->flags & IORESOURCE_PCI_FIXED)
			continue;

		if (!(r->flags) || r->parent)
			continue;

		r_align = pci_resource_alignment(dev, r);
		if (!r_align) {
			pci_warn(dev, "BAR %d: %pR has bogus alignment\n",
				 i, r);
			continue;
		}

		tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
		if (!tmp)
			panic("%s: kzalloc() failed!\n", __func__);
		tmp->res = r;
		tmp->dev = dev;

		 
		n = head;
		list_for_each_entry(dev_res, head, list) {
			resource_size_t align;

			align = pci_resource_alignment(dev_res->dev,
							 dev_res->res);

			if (r_align > align) {
				n = &dev_res->list;
				break;
			}
		}
		 
		list_add_tail(&tmp->list, n);
	}
}

static void __dev_sort_resources(struct pci_dev *dev, struct list_head *head)
{
	u16 class = dev->class >> 8;

	 
	if (class == PCI_CLASS_NOT_DEFINED || class == PCI_CLASS_BRIDGE_HOST)
		return;

	 
	if (class == PCI_CLASS_SYSTEM_PIC) {
		u16 command;
		pci_read_config_word(dev, PCI_COMMAND, &command);
		if (command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY))
			return;
	}

	pdev_sort_resources(dev, head);
}

static inline void reset_resource(struct resource *res)
{
	res->start = 0;
	res->end = 0;
	res->flags = 0;
}

 
static void reassign_resources_sorted(struct list_head *realloc_head,
				      struct list_head *head)
{
	struct resource *res;
	struct pci_dev_resource *add_res, *tmp;
	struct pci_dev_resource *dev_res;
	resource_size_t add_size, align;
	int idx;

	list_for_each_entry_safe(add_res, tmp, realloc_head, list) {
		bool found_match = false;

		res = add_res->res;
		 
		if (!res->flags)
			goto out;

		 
		list_for_each_entry(dev_res, head, list) {
			if (dev_res->res == res) {
				found_match = true;
				break;
			}
		}
		if (!found_match)  
			continue;

		idx = res - &add_res->dev->resource[0];
		add_size = add_res->add_size;
		align = add_res->min_align;
		if (!resource_size(res)) {
			res->start = align;
			res->end = res->start + add_size - 1;
			if (pci_assign_resource(add_res->dev, idx))
				reset_resource(res);
		} else {
			res->flags |= add_res->flags &
				 (IORESOURCE_STARTALIGN|IORESOURCE_SIZEALIGN);
			if (pci_reassign_resource(add_res->dev, idx,
						  add_size, align))
				pci_info(add_res->dev, "failed to add %llx res[%d]=%pR\n",
					 (unsigned long long) add_size, idx,
					 res);
		}
out:
		list_del(&add_res->list);
		kfree(add_res);
	}
}

 
static void assign_requested_resources_sorted(struct list_head *head,
				 struct list_head *fail_head)
{
	struct resource *res;
	struct pci_dev_resource *dev_res;
	int idx;

	list_for_each_entry(dev_res, head, list) {
		res = dev_res->res;
		idx = res - &dev_res->dev->resource[0];
		if (resource_size(res) &&
		    pci_assign_resource(dev_res->dev, idx)) {
			if (fail_head) {
				 
				if (!((idx == PCI_ROM_RESOURCE) &&
				      (!(res->flags & IORESOURCE_ROM_ENABLE))))
					add_to_list(fail_head,
						    dev_res->dev, res,
						    0  ,
						    0  );
			}
			reset_resource(res);
		}
	}
}

static unsigned long pci_fail_res_type_mask(struct list_head *fail_head)
{
	struct pci_dev_resource *fail_res;
	unsigned long mask = 0;

	 
	list_for_each_entry(fail_res, fail_head, list)
		mask |= fail_res->flags;

	 
	return mask & (IORESOURCE_IO | IORESOURCE_MEM | IORESOURCE_PREFETCH);
}

static bool pci_need_to_release(unsigned long mask, struct resource *res)
{
	if (res->flags & IORESOURCE_IO)
		return !!(mask & IORESOURCE_IO);

	 
	if (res->flags & IORESOURCE_PREFETCH) {
		if (mask & IORESOURCE_PREFETCH)
			return true;
		 
		else if ((mask & IORESOURCE_MEM) &&
			 !(res->parent->flags & IORESOURCE_PREFETCH))
			return true;
		else
			return false;
	}

	if (res->flags & IORESOURCE_MEM)
		return !!(mask & IORESOURCE_MEM);

	return false;	 
}

static void __assign_resources_sorted(struct list_head *head,
				      struct list_head *realloc_head,
				      struct list_head *fail_head)
{
	 
	LIST_HEAD(save_head);
	LIST_HEAD(local_fail_head);
	struct pci_dev_resource *save_res;
	struct pci_dev_resource *dev_res, *tmp_res, *dev_res2;
	unsigned long fail_type;
	resource_size_t add_align, align;

	 
	if (!realloc_head || list_empty(realloc_head))
		goto requested_and_reassign;

	 
	list_for_each_entry(dev_res, head, list) {
		if (add_to_list(&save_head, dev_res->dev, dev_res->res, 0, 0)) {
			free_list(&save_head);
			goto requested_and_reassign;
		}
	}

	 
	list_for_each_entry_safe(dev_res, tmp_res, head, list) {
		dev_res->res->end += get_res_add_size(realloc_head,
							dev_res->res);

		 
		if (!(dev_res->res->flags & IORESOURCE_STARTALIGN))
			continue;

		add_align = get_res_add_align(realloc_head, dev_res->res);

		 
		if (add_align > dev_res->res->start) {
			resource_size_t r_size = resource_size(dev_res->res);

			dev_res->res->start = add_align;
			dev_res->res->end = add_align + r_size - 1;

			list_for_each_entry(dev_res2, head, list) {
				align = pci_resource_alignment(dev_res2->dev,
							       dev_res2->res);
				if (add_align > align) {
					list_move_tail(&dev_res->list,
						       &dev_res2->list);
					break;
				}
			}
		}

	}

	 
	assign_requested_resources_sorted(head, &local_fail_head);

	 
	if (list_empty(&local_fail_head)) {
		 
		list_for_each_entry(dev_res, head, list)
			remove_from_list(realloc_head, dev_res->res);
		free_list(&save_head);
		free_list(head);
		return;
	}

	 
	fail_type = pci_fail_res_type_mask(&local_fail_head);
	 
	list_for_each_entry_safe(dev_res, tmp_res, head, list)
		if (dev_res->res->parent &&
		    !pci_need_to_release(fail_type, dev_res->res)) {
			 
			remove_from_list(realloc_head, dev_res->res);
			remove_from_list(&save_head, dev_res->res);
			list_del(&dev_res->list);
			kfree(dev_res);
		}

	free_list(&local_fail_head);
	 
	list_for_each_entry(dev_res, head, list)
		if (dev_res->res->parent)
			release_resource(dev_res->res);
	 
	list_for_each_entry(save_res, &save_head, list) {
		struct resource *res = save_res->res;

		res->start = save_res->start;
		res->end = save_res->end;
		res->flags = save_res->flags;
	}
	free_list(&save_head);

requested_and_reassign:
	 
	assign_requested_resources_sorted(head, fail_head);

	 
	if (realloc_head)
		reassign_resources_sorted(realloc_head, head);
	free_list(head);
}

static void pdev_assign_resources_sorted(struct pci_dev *dev,
					 struct list_head *add_head,
					 struct list_head *fail_head)
{
	LIST_HEAD(head);

	__dev_sort_resources(dev, &head);
	__assign_resources_sorted(&head, add_head, fail_head);

}

static void pbus_assign_resources_sorted(const struct pci_bus *bus,
					 struct list_head *realloc_head,
					 struct list_head *fail_head)
{
	struct pci_dev *dev;
	LIST_HEAD(head);

	list_for_each_entry(dev, &bus->devices, bus_list)
		__dev_sort_resources(dev, &head);

	__assign_resources_sorted(&head, realloc_head, fail_head);
}

void pci_setup_cardbus(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	struct resource *res;
	struct pci_bus_region region;

	pci_info(bridge, "CardBus bridge to %pR\n",
		 &bus->busn_res);

	res = bus->resource[0];
	pcibios_resource_to_bus(bridge->bus, &region, res);
	if (res->flags & IORESOURCE_IO) {
		 
		pci_info(bridge, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_IO_BASE_0,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_IO_LIMIT_0,
					region.end);
	}

	res = bus->resource[1];
	pcibios_resource_to_bus(bridge->bus, &region, res);
	if (res->flags & IORESOURCE_IO) {
		pci_info(bridge, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_IO_BASE_1,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_IO_LIMIT_1,
					region.end);
	}

	res = bus->resource[2];
	pcibios_resource_to_bus(bridge->bus, &region, res);
	if (res->flags & IORESOURCE_MEM) {
		pci_info(bridge, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_BASE_0,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_LIMIT_0,
					region.end);
	}

	res = bus->resource[3];
	pcibios_resource_to_bus(bridge->bus, &region, res);
	if (res->flags & IORESOURCE_MEM) {
		pci_info(bridge, "  bridge window %pR\n", res);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_BASE_1,
					region.start);
		pci_write_config_dword(bridge, PCI_CB_MEMORY_LIMIT_1,
					region.end);
	}
}
EXPORT_SYMBOL(pci_setup_cardbus);

 
static void pci_setup_bridge_io(struct pci_dev *bridge)
{
	struct resource *res;
	struct pci_bus_region region;
	unsigned long io_mask;
	u8 io_base_lo, io_limit_lo;
	u16 l;
	u32 io_upper16;

	io_mask = PCI_IO_RANGE_MASK;
	if (bridge->io_window_1k)
		io_mask = PCI_IO_1K_RANGE_MASK;

	 
	res = &bridge->resource[PCI_BRIDGE_IO_WINDOW];
	pcibios_resource_to_bus(bridge->bus, &region, res);
	if (res->flags & IORESOURCE_IO) {
		pci_read_config_word(bridge, PCI_IO_BASE, &l);
		io_base_lo = (region.start >> 8) & io_mask;
		io_limit_lo = (region.end >> 8) & io_mask;
		l = ((u16) io_limit_lo << 8) | io_base_lo;
		 
		io_upper16 = (region.end & 0xffff0000) | (region.start >> 16);
		pci_info(bridge, "  bridge window %pR\n", res);
	} else {
		 
		io_upper16 = 0;
		l = 0x00f0;
	}
	 
	pci_write_config_dword(bridge, PCI_IO_BASE_UPPER16, 0x0000ffff);
	 
	pci_write_config_word(bridge, PCI_IO_BASE, l);
	 
	pci_write_config_dword(bridge, PCI_IO_BASE_UPPER16, io_upper16);
}

static void pci_setup_bridge_mmio(struct pci_dev *bridge)
{
	struct resource *res;
	struct pci_bus_region region;
	u32 l;

	 
	res = &bridge->resource[PCI_BRIDGE_MEM_WINDOW];
	pcibios_resource_to_bus(bridge->bus, &region, res);
	if (res->flags & IORESOURCE_MEM) {
		l = (region.start >> 16) & 0xfff0;
		l |= region.end & 0xfff00000;
		pci_info(bridge, "  bridge window %pR\n", res);
	} else {
		l = 0x0000fff0;
	}
	pci_write_config_dword(bridge, PCI_MEMORY_BASE, l);
}

static void pci_setup_bridge_mmio_pref(struct pci_dev *bridge)
{
	struct resource *res;
	struct pci_bus_region region;
	u32 l, bu, lu;

	 
	pci_write_config_dword(bridge, PCI_PREF_LIMIT_UPPER32, 0);

	 
	bu = lu = 0;
	res = &bridge->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
	pcibios_resource_to_bus(bridge->bus, &region, res);
	if (res->flags & IORESOURCE_PREFETCH) {
		l = (region.start >> 16) & 0xfff0;
		l |= region.end & 0xfff00000;
		if (res->flags & IORESOURCE_MEM_64) {
			bu = upper_32_bits(region.start);
			lu = upper_32_bits(region.end);
		}
		pci_info(bridge, "  bridge window %pR\n", res);
	} else {
		l = 0x0000fff0;
	}
	pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE, l);

	 
	pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32, bu);
	pci_write_config_dword(bridge, PCI_PREF_LIMIT_UPPER32, lu);
}

static void __pci_setup_bridge(struct pci_bus *bus, unsigned long type)
{
	struct pci_dev *bridge = bus->self;

	pci_info(bridge, "PCI bridge to %pR\n",
		 &bus->busn_res);

	if (type & IORESOURCE_IO)
		pci_setup_bridge_io(bridge);

	if (type & IORESOURCE_MEM)
		pci_setup_bridge_mmio(bridge);

	if (type & IORESOURCE_PREFETCH)
		pci_setup_bridge_mmio_pref(bridge);

	pci_write_config_word(bridge, PCI_BRIDGE_CONTROL, bus->bridge_ctl);
}

void __weak pcibios_setup_bridge(struct pci_bus *bus, unsigned long type)
{
}

void pci_setup_bridge(struct pci_bus *bus)
{
	unsigned long type = IORESOURCE_IO | IORESOURCE_MEM |
				  IORESOURCE_PREFETCH;

	pcibios_setup_bridge(bus, type);
	__pci_setup_bridge(bus, type);
}


int pci_claim_bridge_resource(struct pci_dev *bridge, int i)
{
	if (i < PCI_BRIDGE_RESOURCES || i > PCI_BRIDGE_RESOURCE_END)
		return 0;

	if (pci_claim_resource(bridge, i) == 0)
		return 0;	 

	if ((bridge->class >> 8) != PCI_CLASS_BRIDGE_PCI)
		return 0;

	if (!pci_bus_clip_resource(bridge, i))
		return -EINVAL;	 

	switch (i) {
	case PCI_BRIDGE_IO_WINDOW:
		pci_setup_bridge_io(bridge);
		break;
	case PCI_BRIDGE_MEM_WINDOW:
		pci_setup_bridge_mmio(bridge);
		break;
	case PCI_BRIDGE_PREF_MEM_WINDOW:
		pci_setup_bridge_mmio_pref(bridge);
		break;
	default:
		return -EINVAL;
	}

	if (pci_claim_resource(bridge, i) == 0)
		return 0;	 

	return -EINVAL;
}

 
static void pci_bridge_check_ranges(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	struct resource *b_res;

	b_res = &bridge->resource[PCI_BRIDGE_MEM_WINDOW];
	b_res->flags |= IORESOURCE_MEM;

	if (bridge->io_window) {
		b_res = &bridge->resource[PCI_BRIDGE_IO_WINDOW];
		b_res->flags |= IORESOURCE_IO;
	}

	if (bridge->pref_window) {
		b_res = &bridge->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
		b_res->flags |= IORESOURCE_MEM | IORESOURCE_PREFETCH;
		if (bridge->pref_64_window) {
			b_res->flags |= IORESOURCE_MEM_64 |
					PCI_PREF_RANGE_TYPE_64;
		}
	}
}

 
static struct resource *find_bus_resource_of_type(struct pci_bus *bus,
						  unsigned long type_mask,
						  unsigned long type)
{
	struct resource *r, *r_assigned = NULL;

	pci_bus_for_each_resource(bus, r) {
		if (r == &ioport_resource || r == &iomem_resource)
			continue;
		if (r && (r->flags & type_mask) == type && !r->parent)
			return r;
		if (r && (r->flags & type_mask) == type && !r_assigned)
			r_assigned = r;
	}
	return r_assigned;
}

static resource_size_t calculate_iosize(resource_size_t size,
					resource_size_t min_size,
					resource_size_t size1,
					resource_size_t add_size,
					resource_size_t children_add_size,
					resource_size_t old_size,
					resource_size_t align)
{
	if (size < min_size)
		size = min_size;
	if (old_size == 1)
		old_size = 0;
	 
#if defined(CONFIG_ISA) || defined(CONFIG_EISA)
	size = (size & 0xff) + ((size & ~0xffUL) << 2);
#endif
	size = size + size1;
	if (size < old_size)
		size = old_size;

	size = ALIGN(max(size, add_size) + children_add_size, align);
	return size;
}

static resource_size_t calculate_memsize(resource_size_t size,
					 resource_size_t min_size,
					 resource_size_t add_size,
					 resource_size_t children_add_size,
					 resource_size_t old_size,
					 resource_size_t align)
{
	if (size < min_size)
		size = min_size;
	if (old_size == 1)
		old_size = 0;
	if (size < old_size)
		size = old_size;

	size = ALIGN(max(size, add_size) + children_add_size, align);
	return size;
}

resource_size_t __weak pcibios_window_alignment(struct pci_bus *bus,
						unsigned long type)
{
	return 1;
}

#define PCI_P2P_DEFAULT_MEM_ALIGN	0x100000	 
#define PCI_P2P_DEFAULT_IO_ALIGN	0x1000		 
#define PCI_P2P_DEFAULT_IO_ALIGN_1K	0x400		 

static resource_size_t window_alignment(struct pci_bus *bus, unsigned long type)
{
	resource_size_t align = 1, arch_align;

	if (type & IORESOURCE_MEM)
		align = PCI_P2P_DEFAULT_MEM_ALIGN;
	else if (type & IORESOURCE_IO) {
		 
		if (bus->self && bus->self->io_window_1k)
			align = PCI_P2P_DEFAULT_IO_ALIGN_1K;
		else
			align = PCI_P2P_DEFAULT_IO_ALIGN;
	}

	arch_align = pcibios_window_alignment(bus, type);
	return max(align, arch_align);
}

 
static void pbus_size_io(struct pci_bus *bus, resource_size_t min_size,
			 resource_size_t add_size,
			 struct list_head *realloc_head)
{
	struct pci_dev *dev;
	struct resource *b_res = find_bus_resource_of_type(bus, IORESOURCE_IO,
							   IORESOURCE_IO);
	resource_size_t size = 0, size0 = 0, size1 = 0;
	resource_size_t children_add_size = 0;
	resource_size_t min_align, align;

	if (!b_res)
		return;

	 
	if (b_res->parent)
		return;

	min_align = window_alignment(bus, IORESOURCE_IO);
	list_for_each_entry(dev, &bus->devices, bus_list) {
		struct resource *r;

		pci_dev_for_each_resource(dev, r) {
			unsigned long r_size;

			if (r->parent || !(r->flags & IORESOURCE_IO))
				continue;
			r_size = resource_size(r);

			if (r_size < 0x400)
				 
				size += r_size;
			else
				size1 += r_size;

			align = pci_resource_alignment(dev, r);
			if (align > min_align)
				min_align = align;

			if (realloc_head)
				children_add_size += get_res_add_size(realloc_head, r);
		}
	}

	size0 = calculate_iosize(size, min_size, size1, 0, 0,
			resource_size(b_res), min_align);
	size1 = (!realloc_head || (realloc_head && !add_size && !children_add_size)) ? size0 :
		calculate_iosize(size, min_size, size1, add_size, children_add_size,
			resource_size(b_res), min_align);
	if (!size0 && !size1) {
		if (bus->self && (b_res->start || b_res->end))
			pci_info(bus->self, "disabling bridge window %pR to %pR (unused)\n",
				 b_res, &bus->busn_res);
		b_res->flags = 0;
		return;
	}

	b_res->start = min_align;
	b_res->end = b_res->start + size0 - 1;
	b_res->flags |= IORESOURCE_STARTALIGN;
	if (bus->self && size1 > size0 && realloc_head) {
		add_to_list(realloc_head, bus->self, b_res, size1-size0,
			    min_align);
		pci_info(bus->self, "bridge window %pR to %pR add_size %llx\n",
			 b_res, &bus->busn_res,
			 (unsigned long long) size1 - size0);
	}
}

static inline resource_size_t calculate_mem_align(resource_size_t *aligns,
						  int max_order)
{
	resource_size_t align = 0;
	resource_size_t min_align = 0;
	int order;

	for (order = 0; order <= max_order; order++) {
		resource_size_t align1 = 1;

		align1 <<= (order + 20);

		if (!align)
			min_align = align1;
		else if (ALIGN(align + min_align, min_align) < align1)
			min_align = align1 >> 1;
		align += aligns[order];
	}

	return min_align;
}

 
static int pbus_size_mem(struct pci_bus *bus, unsigned long mask,
			 unsigned long type, unsigned long type2,
			 unsigned long type3, resource_size_t min_size,
			 resource_size_t add_size,
			 struct list_head *realloc_head)
{
	struct pci_dev *dev;
	resource_size_t min_align, align, size, size0, size1;
	resource_size_t aligns[24];  
	int order, max_order;
	struct resource *b_res = find_bus_resource_of_type(bus,
					mask | IORESOURCE_PREFETCH, type);
	resource_size_t children_add_size = 0;
	resource_size_t children_add_align = 0;
	resource_size_t add_align = 0;

	if (!b_res)
		return -ENOSPC;

	 
	if (b_res->parent)
		return 0;

	memset(aligns, 0, sizeof(aligns));
	max_order = 0;
	size = 0;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		struct resource *r;
		int i;

		pci_dev_for_each_resource(dev, r, i) {
			resource_size_t r_size;

			if (r->parent || (r->flags & IORESOURCE_PCI_FIXED) ||
			    ((r->flags & mask) != type &&
			     (r->flags & mask) != type2 &&
			     (r->flags & mask) != type3))
				continue;
			r_size = resource_size(r);
#ifdef CONFIG_PCI_IOV
			 
			if (realloc_head && i >= PCI_IOV_RESOURCES &&
					i <= PCI_IOV_RESOURCE_END) {
				add_align = max(pci_resource_alignment(dev, r), add_align);
				r->end = r->start - 1;
				add_to_list(realloc_head, dev, r, r_size, 0  );
				children_add_size += r_size;
				continue;
			}
#endif
			 
			align = pci_resource_alignment(dev, r);
			order = __ffs(align) - 20;
			if (order < 0)
				order = 0;
			if (order >= ARRAY_SIZE(aligns)) {
				pci_warn(dev, "disabling BAR %d: %pR (bad alignment %#llx)\n",
					 i, r, (unsigned long long) align);
				r->flags = 0;
				continue;
			}
			size += max(r_size, align);
			 
			if (r_size <= align)
				aligns[order] += align;
			if (order > max_order)
				max_order = order;

			if (realloc_head) {
				children_add_size += get_res_add_size(realloc_head, r);
				children_add_align = get_res_add_align(realloc_head, r);
				add_align = max(add_align, children_add_align);
			}
		}
	}

	min_align = calculate_mem_align(aligns, max_order);
	min_align = max(min_align, window_alignment(bus, b_res->flags));
	size0 = calculate_memsize(size, min_size, 0, 0, resource_size(b_res), min_align);
	add_align = max(min_align, add_align);
	size1 = (!realloc_head || (realloc_head && !add_size && !children_add_size)) ? size0 :
		calculate_memsize(size, min_size, add_size, children_add_size,
				resource_size(b_res), add_align);
	if (!size0 && !size1) {
		if (bus->self && (b_res->start || b_res->end))
			pci_info(bus->self, "disabling bridge window %pR to %pR (unused)\n",
				 b_res, &bus->busn_res);
		b_res->flags = 0;
		return 0;
	}
	b_res->start = min_align;
	b_res->end = size0 + min_align - 1;
	b_res->flags |= IORESOURCE_STARTALIGN;
	if (bus->self && size1 > size0 && realloc_head) {
		add_to_list(realloc_head, bus->self, b_res, size1-size0, add_align);
		pci_info(bus->self, "bridge window %pR to %pR add_size %llx add_align %llx\n",
			   b_res, &bus->busn_res,
			   (unsigned long long) (size1 - size0),
			   (unsigned long long) add_align);
	}
	return 0;
}

unsigned long pci_cardbus_resource_alignment(struct resource *res)
{
	if (res->flags & IORESOURCE_IO)
		return pci_cardbus_io_size;
	if (res->flags & IORESOURCE_MEM)
		return pci_cardbus_mem_size;
	return 0;
}

static void pci_bus_size_cardbus(struct pci_bus *bus,
				 struct list_head *realloc_head)
{
	struct pci_dev *bridge = bus->self;
	struct resource *b_res;
	resource_size_t b_res_3_size = pci_cardbus_mem_size * 2;
	u16 ctrl;

	b_res = &bridge->resource[PCI_CB_BRIDGE_IO_0_WINDOW];
	if (b_res->parent)
		goto handle_b_res_1;
	 
	b_res->start = pci_cardbus_io_size;
	b_res->end = b_res->start + pci_cardbus_io_size - 1;
	b_res->flags |= IORESOURCE_IO | IORESOURCE_STARTALIGN;
	if (realloc_head) {
		b_res->end -= pci_cardbus_io_size;
		add_to_list(realloc_head, bridge, b_res, pci_cardbus_io_size,
			    pci_cardbus_io_size);
	}

handle_b_res_1:
	b_res = &bridge->resource[PCI_CB_BRIDGE_IO_1_WINDOW];
	if (b_res->parent)
		goto handle_b_res_2;
	b_res->start = pci_cardbus_io_size;
	b_res->end = b_res->start + pci_cardbus_io_size - 1;
	b_res->flags |= IORESOURCE_IO | IORESOURCE_STARTALIGN;
	if (realloc_head) {
		b_res->end -= pci_cardbus_io_size;
		add_to_list(realloc_head, bridge, b_res, pci_cardbus_io_size,
			    pci_cardbus_io_size);
	}

handle_b_res_2:
	 
	pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	if (ctrl & PCI_CB_BRIDGE_CTL_PREFETCH_MEM1) {
		ctrl &= ~PCI_CB_BRIDGE_CTL_PREFETCH_MEM1;
		pci_write_config_word(bridge, PCI_CB_BRIDGE_CONTROL, ctrl);
		pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	}

	 
	pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	if (!(ctrl & PCI_CB_BRIDGE_CTL_PREFETCH_MEM0)) {
		ctrl |= PCI_CB_BRIDGE_CTL_PREFETCH_MEM0;
		pci_write_config_word(bridge, PCI_CB_BRIDGE_CONTROL, ctrl);
		pci_read_config_word(bridge, PCI_CB_BRIDGE_CONTROL, &ctrl);
	}

	b_res = &bridge->resource[PCI_CB_BRIDGE_MEM_0_WINDOW];
	if (b_res->parent)
		goto handle_b_res_3;
	 
	if (ctrl & PCI_CB_BRIDGE_CTL_PREFETCH_MEM0) {
		b_res->start = pci_cardbus_mem_size;
		b_res->end = b_res->start + pci_cardbus_mem_size - 1;
		b_res->flags |= IORESOURCE_MEM | IORESOURCE_PREFETCH |
				    IORESOURCE_STARTALIGN;
		if (realloc_head) {
			b_res->end -= pci_cardbus_mem_size;
			add_to_list(realloc_head, bridge, b_res,
				    pci_cardbus_mem_size, pci_cardbus_mem_size);
		}

		 
		b_res_3_size = pci_cardbus_mem_size;
	}

handle_b_res_3:
	b_res = &bridge->resource[PCI_CB_BRIDGE_MEM_1_WINDOW];
	if (b_res->parent)
		goto handle_done;
	b_res->start = pci_cardbus_mem_size;
	b_res->end = b_res->start + b_res_3_size - 1;
	b_res->flags |= IORESOURCE_MEM | IORESOURCE_STARTALIGN;
	if (realloc_head) {
		b_res->end -= b_res_3_size;
		add_to_list(realloc_head, bridge, b_res, b_res_3_size,
			    pci_cardbus_mem_size);
	}

handle_done:
	;
}

void __pci_bus_size_bridges(struct pci_bus *bus, struct list_head *realloc_head)
{
	struct pci_dev *dev;
	unsigned long mask, prefmask, type2 = 0, type3 = 0;
	resource_size_t additional_io_size = 0, additional_mmio_size = 0,
			additional_mmio_pref_size = 0;
	struct resource *pref;
	struct pci_host_bridge *host;
	int hdr_type, ret;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		struct pci_bus *b = dev->subordinate;
		if (!b)
			continue;

		switch (dev->hdr_type) {
		case PCI_HEADER_TYPE_CARDBUS:
			pci_bus_size_cardbus(b, realloc_head);
			break;

		case PCI_HEADER_TYPE_BRIDGE:
		default:
			__pci_bus_size_bridges(b, realloc_head);
			break;
		}
	}

	 
	if (pci_is_root_bus(bus)) {
		host = to_pci_host_bridge(bus->bridge);
		if (!host->size_windows)
			return;
		pci_bus_for_each_resource(bus, pref)
			if (pref && (pref->flags & IORESOURCE_PREFETCH))
				break;
		hdr_type = -1;	 
	} else {
		pref = &bus->self->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
		hdr_type = bus->self->hdr_type;
	}

	switch (hdr_type) {
	case PCI_HEADER_TYPE_CARDBUS:
		 
		break;

	case PCI_HEADER_TYPE_BRIDGE:
		pci_bridge_check_ranges(bus);
		if (bus->self->is_hotplug_bridge) {
			additional_io_size  = pci_hotplug_io_size;
			additional_mmio_size = pci_hotplug_mmio_size;
			additional_mmio_pref_size = pci_hotplug_mmio_pref_size;
		}
		fallthrough;
	default:
		pbus_size_io(bus, realloc_head ? 0 : additional_io_size,
			     additional_io_size, realloc_head);

		 
		mask = IORESOURCE_MEM;
		prefmask = IORESOURCE_MEM | IORESOURCE_PREFETCH;
		if (pref && (pref->flags & IORESOURCE_MEM_64)) {
			prefmask |= IORESOURCE_MEM_64;
			ret = pbus_size_mem(bus, prefmask, prefmask,
				prefmask, prefmask,
				realloc_head ? 0 : additional_mmio_pref_size,
				additional_mmio_pref_size, realloc_head);

			 
			if (ret == 0) {
				mask = prefmask;
				type2 = prefmask & ~IORESOURCE_MEM_64;
				type3 = prefmask & ~IORESOURCE_PREFETCH;
			}
		}

		 
		if (!type2) {
			prefmask &= ~IORESOURCE_MEM_64;
			ret = pbus_size_mem(bus, prefmask, prefmask,
				prefmask, prefmask,
				realloc_head ? 0 : additional_mmio_pref_size,
				additional_mmio_pref_size, realloc_head);

			 
			if (ret == 0)
				mask = prefmask;
			else
				additional_mmio_size += additional_mmio_pref_size;

			type2 = type3 = IORESOURCE_MEM;
		}

		 
		pbus_size_mem(bus, mask, IORESOURCE_MEM, type2, type3,
			      realloc_head ? 0 : additional_mmio_size,
			      additional_mmio_size, realloc_head);
		break;
	}
}

void pci_bus_size_bridges(struct pci_bus *bus)
{
	__pci_bus_size_bridges(bus, NULL);
}
EXPORT_SYMBOL(pci_bus_size_bridges);

static void assign_fixed_resource_on_bus(struct pci_bus *b, struct resource *r)
{
	struct resource *parent_r;
	unsigned long mask = IORESOURCE_IO | IORESOURCE_MEM |
			     IORESOURCE_PREFETCH;

	pci_bus_for_each_resource(b, parent_r) {
		if (!parent_r)
			continue;

		if ((r->flags & mask) == (parent_r->flags & mask) &&
		    resource_contains(parent_r, r))
			request_resource(parent_r, r);
	}
}

 
static void pdev_assign_fixed_resources(struct pci_dev *dev)
{
	struct resource *r;

	pci_dev_for_each_resource(dev, r) {
		struct pci_bus *b;

		if (r->parent || !(r->flags & IORESOURCE_PCI_FIXED) ||
		    !(r->flags & (IORESOURCE_IO | IORESOURCE_MEM)))
			continue;

		b = dev->bus;
		while (b && !r->parent) {
			assign_fixed_resource_on_bus(b, r);
			b = b->parent;
		}
	}
}

void __pci_bus_assign_resources(const struct pci_bus *bus,
				struct list_head *realloc_head,
				struct list_head *fail_head)
{
	struct pci_bus *b;
	struct pci_dev *dev;

	pbus_assign_resources_sorted(bus, realloc_head, fail_head);

	list_for_each_entry(dev, &bus->devices, bus_list) {
		pdev_assign_fixed_resources(dev);

		b = dev->subordinate;
		if (!b)
			continue;

		__pci_bus_assign_resources(b, realloc_head, fail_head);

		switch (dev->hdr_type) {
		case PCI_HEADER_TYPE_BRIDGE:
			if (!pci_is_enabled(dev))
				pci_setup_bridge(b);
			break;

		case PCI_HEADER_TYPE_CARDBUS:
			pci_setup_cardbus(b);
			break;

		default:
			pci_info(dev, "not setting up bridge for bus %04x:%02x\n",
				 pci_domain_nr(b), b->number);
			break;
		}
	}
}

void pci_bus_assign_resources(const struct pci_bus *bus)
{
	__pci_bus_assign_resources(bus, NULL, NULL);
}
EXPORT_SYMBOL(pci_bus_assign_resources);

static void pci_claim_device_resources(struct pci_dev *dev)
{
	int i;

	for (i = 0; i < PCI_BRIDGE_RESOURCES; i++) {
		struct resource *r = &dev->resource[i];

		if (!r->flags || r->parent)
			continue;

		pci_claim_resource(dev, i);
	}
}

static void pci_claim_bridge_resources(struct pci_dev *dev)
{
	int i;

	for (i = PCI_BRIDGE_RESOURCES; i < PCI_NUM_RESOURCES; i++) {
		struct resource *r = &dev->resource[i];

		if (!r->flags || r->parent)
			continue;

		pci_claim_bridge_resource(dev, i);
	}
}

static void pci_bus_allocate_dev_resources(struct pci_bus *b)
{
	struct pci_dev *dev;
	struct pci_bus *child;

	list_for_each_entry(dev, &b->devices, bus_list) {
		pci_claim_device_resources(dev);

		child = dev->subordinate;
		if (child)
			pci_bus_allocate_dev_resources(child);
	}
}

static void pci_bus_allocate_resources(struct pci_bus *b)
{
	struct pci_bus *child;

	 
	if (b->self) {
		pci_read_bridge_bases(b);
		pci_claim_bridge_resources(b->self);
	}

	list_for_each_entry(child, &b->children, node)
		pci_bus_allocate_resources(child);
}

void pci_bus_claim_resources(struct pci_bus *b)
{
	pci_bus_allocate_resources(b);
	pci_bus_allocate_dev_resources(b);
}
EXPORT_SYMBOL(pci_bus_claim_resources);

static void __pci_bridge_assign_resources(const struct pci_dev *bridge,
					  struct list_head *add_head,
					  struct list_head *fail_head)
{
	struct pci_bus *b;

	pdev_assign_resources_sorted((struct pci_dev *)bridge,
					 add_head, fail_head);

	b = bridge->subordinate;
	if (!b)
		return;

	__pci_bus_assign_resources(b, add_head, fail_head);

	switch (bridge->class >> 8) {
	case PCI_CLASS_BRIDGE_PCI:
		pci_setup_bridge(b);
		break;

	case PCI_CLASS_BRIDGE_CARDBUS:
		pci_setup_cardbus(b);
		break;

	default:
		pci_info(bridge, "not setting up bridge for bus %04x:%02x\n",
			 pci_domain_nr(b), b->number);
		break;
	}
}

#define PCI_RES_TYPE_MASK \
	(IORESOURCE_IO | IORESOURCE_MEM | IORESOURCE_PREFETCH |\
	 IORESOURCE_MEM_64)

static void pci_bridge_release_resources(struct pci_bus *bus,
					 unsigned long type)
{
	struct pci_dev *dev = bus->self;
	struct resource *r;
	unsigned int old_flags;
	struct resource *b_res;
	int idx = 1;

	b_res = &dev->resource[PCI_BRIDGE_RESOURCES];

	 
	if (type & IORESOURCE_IO)
		idx = 0;
	else if (!(type & IORESOURCE_PREFETCH))
		idx = 1;
	else if ((type & IORESOURCE_MEM_64) &&
		 (b_res[2].flags & IORESOURCE_MEM_64))
		idx = 2;
	else if (!(b_res[2].flags & IORESOURCE_MEM_64) &&
		 (b_res[2].flags & IORESOURCE_PREFETCH))
		idx = 2;
	else
		idx = 1;

	r = &b_res[idx];

	if (!r->parent)
		return;

	 
	release_child_resources(r);
	if (!release_resource(r)) {
		type = old_flags = r->flags & PCI_RES_TYPE_MASK;
		pci_info(dev, "resource %d %pR released\n",
			 PCI_BRIDGE_RESOURCES + idx, r);
		 
		r->end = resource_size(r) - 1;
		r->start = 0;
		r->flags = 0;

		 
		if (type & IORESOURCE_PREFETCH)
			type = IORESOURCE_PREFETCH;
		__pci_setup_bridge(bus, type);
		 
		r->flags = old_flags;
	}
}

enum release_type {
	leaf_only,
	whole_subtree,
};

 
static void pci_bus_release_bridge_resources(struct pci_bus *bus,
					     unsigned long type,
					     enum release_type rel_type)
{
	struct pci_dev *dev;
	bool is_leaf_bridge = true;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		struct pci_bus *b = dev->subordinate;
		if (!b)
			continue;

		is_leaf_bridge = false;

		if ((dev->class >> 8) != PCI_CLASS_BRIDGE_PCI)
			continue;

		if (rel_type == whole_subtree)
			pci_bus_release_bridge_resources(b, type,
						 whole_subtree);
	}

	if (pci_is_root_bus(bus))
		return;

	if ((bus->self->class >> 8) != PCI_CLASS_BRIDGE_PCI)
		return;

	if ((rel_type == whole_subtree) || is_leaf_bridge)
		pci_bridge_release_resources(bus, type);
}

static void pci_bus_dump_res(struct pci_bus *bus)
{
	struct resource *res;
	int i;

	pci_bus_for_each_resource(bus, res, i) {
		if (!res || !res->end || !res->flags)
			continue;

		dev_info(&bus->dev, "resource %d %pR\n", i, res);
	}
}

static void pci_bus_dump_resources(struct pci_bus *bus)
{
	struct pci_bus *b;
	struct pci_dev *dev;


	pci_bus_dump_res(bus);

	list_for_each_entry(dev, &bus->devices, bus_list) {
		b = dev->subordinate;
		if (!b)
			continue;

		pci_bus_dump_resources(b);
	}
}

static int pci_bus_get_depth(struct pci_bus *bus)
{
	int depth = 0;
	struct pci_bus *child_bus;

	list_for_each_entry(child_bus, &bus->children, node) {
		int ret;

		ret = pci_bus_get_depth(child_bus);
		if (ret + 1 > depth)
			depth = ret + 1;
	}

	return depth;
}

 
enum enable_type {
	undefined = -1,
	user_disabled,
	auto_disabled,
	user_enabled,
	auto_enabled,
};

static enum enable_type pci_realloc_enable = undefined;
void __init pci_realloc_get_opt(char *str)
{
	if (!strncmp(str, "off", 3))
		pci_realloc_enable = user_disabled;
	else if (!strncmp(str, "on", 2))
		pci_realloc_enable = user_enabled;
}
static bool pci_realloc_enabled(enum enable_type enable)
{
	return enable >= user_enabled;
}

#if defined(CONFIG_PCI_IOV) && defined(CONFIG_PCI_REALLOC_ENABLE_AUTO)
static int iov_resources_unassigned(struct pci_dev *dev, void *data)
{
	int i;
	bool *unassigned = data;

	for (i = 0; i < PCI_SRIOV_NUM_BARS; i++) {
		struct resource *r = &dev->resource[i + PCI_IOV_RESOURCES];
		struct pci_bus_region region;

		 
		if (!r->flags)
			continue;

		pcibios_resource_to_bus(dev->bus, &region, r);
		if (!region.start) {
			*unassigned = true;
			return 1;  
		}
	}

	return 0;
}

static enum enable_type pci_realloc_detect(struct pci_bus *bus,
					   enum enable_type enable_local)
{
	bool unassigned = false;
	struct pci_host_bridge *host;

	if (enable_local != undefined)
		return enable_local;

	host = pci_find_host_bridge(bus);
	if (host->preserve_config)
		return auto_disabled;

	pci_walk_bus(bus, iov_resources_unassigned, &unassigned);
	if (unassigned)
		return auto_enabled;

	return enable_local;
}
#else
static enum enable_type pci_realloc_detect(struct pci_bus *bus,
					   enum enable_type enable_local)
{
	return enable_local;
}
#endif

static void adjust_bridge_window(struct pci_dev *bridge, struct resource *res,
				 struct list_head *add_list,
				 resource_size_t new_size)
{
	resource_size_t add_size, size = resource_size(res);

	if (res->parent)
		return;

	if (!new_size)
		return;

	if (new_size > size) {
		add_size = new_size - size;
		pci_dbg(bridge, "bridge window %pR extended by %pa\n", res,
			&add_size);
	} else if (new_size < size) {
		add_size = size - new_size;
		pci_dbg(bridge, "bridge window %pR shrunken by %pa\n", res,
			&add_size);
	} else {
		return;
	}

	res->end = res->start + new_size - 1;

	 
	if (add_list)
		remove_from_list(add_list, res);
}

static void remove_dev_resource(struct resource *avail, struct pci_dev *dev,
				struct resource *res)
{
	resource_size_t size, align, tmp;

	size = resource_size(res);
	if (!size)
		return;

	align = pci_resource_alignment(dev, res);
	align = align ? ALIGN(avail->start, align) - avail->start : 0;
	tmp = align + size;
	avail->start = min(avail->start + tmp, avail->end + 1);
}

static void remove_dev_resources(struct pci_dev *dev, struct resource *io,
				 struct resource *mmio,
				 struct resource *mmio_pref)
{
	struct resource *res;

	pci_dev_for_each_resource(dev, res) {
		if (resource_type(res) == IORESOURCE_IO) {
			remove_dev_resource(io, dev, res);
		} else if (resource_type(res) == IORESOURCE_MEM) {

			 
			if ((res->flags & IORESOURCE_PREFETCH) &&
			    ((res->flags & IORESOURCE_MEM_64) ==
			     (mmio_pref->flags & IORESOURCE_MEM_64)))
				remove_dev_resource(mmio_pref, dev, res);
			else
				remove_dev_resource(mmio, dev, res);
		}
	}
}

 
static void pci_bus_distribute_available_resources(struct pci_bus *bus,
					    struct list_head *add_list,
					    struct resource io,
					    struct resource mmio,
					    struct resource mmio_pref)
{
	unsigned int normal_bridges = 0, hotplug_bridges = 0;
	struct resource *io_res, *mmio_res, *mmio_pref_res;
	struct pci_dev *dev, *bridge = bus->self;
	resource_size_t io_per_b, mmio_per_b, mmio_pref_per_b, align;

	io_res = &bridge->resource[PCI_BRIDGE_IO_WINDOW];
	mmio_res = &bridge->resource[PCI_BRIDGE_MEM_WINDOW];
	mmio_pref_res = &bridge->resource[PCI_BRIDGE_PREF_MEM_WINDOW];

	 
	align = pci_resource_alignment(bridge, io_res);
	if (!io_res->parent && align)
		io.start = min(ALIGN(io.start, align), io.end + 1);

	align = pci_resource_alignment(bridge, mmio_res);
	if (!mmio_res->parent && align)
		mmio.start = min(ALIGN(mmio.start, align), mmio.end + 1);

	align = pci_resource_alignment(bridge, mmio_pref_res);
	if (!mmio_pref_res->parent && align)
		mmio_pref.start = min(ALIGN(mmio_pref.start, align),
			mmio_pref.end + 1);

	 
	adjust_bridge_window(bridge, io_res, add_list, resource_size(&io));
	adjust_bridge_window(bridge, mmio_res, add_list, resource_size(&mmio));
	adjust_bridge_window(bridge, mmio_pref_res, add_list,
			     resource_size(&mmio_pref));

	 
	for_each_pci_bridge(dev, bus) {
		if (dev->is_hotplug_bridge)
			hotplug_bridges++;
		else
			normal_bridges++;
	}

	if (!(hotplug_bridges + normal_bridges))
		return;

	 
	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (!dev->is_virtfn)
			remove_dev_resources(dev, &io, &mmio, &mmio_pref);
	}

	 
	if (hotplug_bridges) {
		io_per_b = div64_ul(resource_size(&io), hotplug_bridges);
		mmio_per_b = div64_ul(resource_size(&mmio), hotplug_bridges);
		mmio_pref_per_b = div64_ul(resource_size(&mmio_pref),
					   hotplug_bridges);
	} else {
		io_per_b = div64_ul(resource_size(&io), normal_bridges);
		mmio_per_b = div64_ul(resource_size(&mmio), normal_bridges);
		mmio_pref_per_b = div64_ul(resource_size(&mmio_pref),
					   normal_bridges);
	}

	for_each_pci_bridge(dev, bus) {
		struct resource *res;
		struct pci_bus *b;

		b = dev->subordinate;
		if (!b)
			continue;
		if (hotplug_bridges && !dev->is_hotplug_bridge)
			continue;

		res = &dev->resource[PCI_BRIDGE_IO_WINDOW];

		 
		align = pci_resource_alignment(dev, res);
		io.end = align ? io.start + ALIGN_DOWN(io_per_b, align) - 1
			       : io.start + io_per_b - 1;

		 
		io.start -= resource_size(res);

		res = &dev->resource[PCI_BRIDGE_MEM_WINDOW];
		align = pci_resource_alignment(dev, res);
		mmio.end = align ? mmio.start + ALIGN_DOWN(mmio_per_b, align) - 1
				 : mmio.start + mmio_per_b - 1;
		mmio.start -= resource_size(res);

		res = &dev->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
		align = pci_resource_alignment(dev, res);
		mmio_pref.end = align ? mmio_pref.start +
					ALIGN_DOWN(mmio_pref_per_b, align) - 1
				      : mmio_pref.start + mmio_pref_per_b - 1;
		mmio_pref.start -= resource_size(res);

		pci_bus_distribute_available_resources(b, add_list, io, mmio,
						       mmio_pref);

		io.start += io.end + 1;
		mmio.start += mmio.end + 1;
		mmio_pref.start += mmio_pref.end + 1;
	}
}

static void pci_bridge_distribute_available_resources(struct pci_dev *bridge,
						      struct list_head *add_list)
{
	struct resource available_io, available_mmio, available_mmio_pref;

	if (!bridge->is_hotplug_bridge)
		return;

	pci_dbg(bridge, "distributing available resources\n");

	 
	available_io = bridge->resource[PCI_BRIDGE_IO_WINDOW];
	available_mmio = bridge->resource[PCI_BRIDGE_MEM_WINDOW];
	available_mmio_pref = bridge->resource[PCI_BRIDGE_PREF_MEM_WINDOW];

	pci_bus_distribute_available_resources(bridge->subordinate,
					       add_list, available_io,
					       available_mmio,
					       available_mmio_pref);
}

static bool pci_bridge_resources_not_assigned(struct pci_dev *dev)
{
	const struct resource *r;

	 
	r = &dev->resource[PCI_BRIDGE_IO_WINDOW];
	if (r->flags && !(r->flags & IORESOURCE_STARTALIGN))
		return false;
	r = &dev->resource[PCI_BRIDGE_MEM_WINDOW];
	if (r->flags && !(r->flags & IORESOURCE_STARTALIGN))
		return false;
	r = &dev->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
	if (r->flags && !(r->flags & IORESOURCE_STARTALIGN))
		return false;

	return true;
}

static void
pci_root_bus_distribute_available_resources(struct pci_bus *bus,
					    struct list_head *add_list)
{
	struct pci_dev *dev, *bridge = bus->self;

	for_each_pci_bridge(dev, bus) {
		struct pci_bus *b;

		b = dev->subordinate;
		if (!b)
			continue;

		 
		if (bridge && pci_bridge_resources_not_assigned(dev))
			pci_bridge_distribute_available_resources(bridge,
								  add_list);
		else
			pci_root_bus_distribute_available_resources(b, add_list);
	}
}

 
void pci_assign_unassigned_root_bus_resources(struct pci_bus *bus)
{
	LIST_HEAD(realloc_head);
	 
	struct list_head *add_list = NULL;
	int tried_times = 0;
	enum release_type rel_type = leaf_only;
	LIST_HEAD(fail_head);
	struct pci_dev_resource *fail_res;
	int pci_try_num = 1;
	enum enable_type enable_local;

	 
	enable_local = pci_realloc_detect(bus, pci_realloc_enable);
	if (pci_realloc_enabled(enable_local)) {
		int max_depth = pci_bus_get_depth(bus);

		pci_try_num = max_depth + 1;
		dev_info(&bus->dev, "max bus depth: %d pci_try_num: %d\n",
			 max_depth, pci_try_num);
	}

again:
	 
	if (tried_times + 1 == pci_try_num)
		add_list = &realloc_head;
	 
	__pci_bus_size_bridges(bus, add_list);

	pci_root_bus_distribute_available_resources(bus, add_list);

	 
	__pci_bus_assign_resources(bus, add_list, &fail_head);
	if (add_list)
		BUG_ON(!list_empty(add_list));
	tried_times++;

	 
	if (list_empty(&fail_head))
		goto dump;

	if (tried_times >= pci_try_num) {
		if (enable_local == undefined)
			dev_info(&bus->dev, "Some PCI device resources are unassigned, try booting with pci=realloc\n");
		else if (enable_local == auto_enabled)
			dev_info(&bus->dev, "Automatically enabled pci realloc, if you have problem, try booting with pci=realloc=off\n");

		free_list(&fail_head);
		goto dump;
	}

	dev_info(&bus->dev, "No. %d try to assign unassigned res\n",
		 tried_times + 1);

	 
	if ((tried_times + 1) > 2)
		rel_type = whole_subtree;

	 
	list_for_each_entry(fail_res, &fail_head, list)
		pci_bus_release_bridge_resources(fail_res->dev->bus,
						 fail_res->flags & PCI_RES_TYPE_MASK,
						 rel_type);

	 
	list_for_each_entry(fail_res, &fail_head, list) {
		struct resource *res = fail_res->res;
		int idx;

		res->start = fail_res->start;
		res->end = fail_res->end;
		res->flags = fail_res->flags;

		if (pci_is_bridge(fail_res->dev)) {
			idx = res - &fail_res->dev->resource[0];
			if (idx >= PCI_BRIDGE_RESOURCES &&
			    idx <= PCI_BRIDGE_RESOURCE_END)
				res->flags = 0;
		}
	}
	free_list(&fail_head);

	goto again;

dump:
	 
	pci_bus_dump_resources(bus);
}

void __init pci_assign_unassigned_resources(void)
{
	struct pci_bus *root_bus;

	list_for_each_entry(root_bus, &pci_root_buses, node) {
		pci_assign_unassigned_root_bus_resources(root_bus);

		 
		if (ACPI_HANDLE(root_bus->bridge))
			acpi_ioapic_add(ACPI_HANDLE(root_bus->bridge));
	}
}

void pci_assign_unassigned_bridge_resources(struct pci_dev *bridge)
{
	struct pci_bus *parent = bridge->subordinate;
	 
	LIST_HEAD(add_list);

	int tried_times = 0;
	LIST_HEAD(fail_head);
	struct pci_dev_resource *fail_res;
	int retval;

again:
	__pci_bus_size_bridges(parent, &add_list);

	 
	pci_bridge_distribute_available_resources(bridge, &add_list);

	__pci_bridge_assign_resources(bridge, &add_list, &fail_head);
	BUG_ON(!list_empty(&add_list));
	tried_times++;

	if (list_empty(&fail_head))
		goto enable_all;

	if (tried_times >= 2) {
		 
		free_list(&fail_head);
		goto enable_all;
	}

	printk(KERN_DEBUG "PCI: No. %d try to assign unassigned res\n",
			 tried_times + 1);

	 
	list_for_each_entry(fail_res, &fail_head, list)
		pci_bus_release_bridge_resources(fail_res->dev->bus,
						 fail_res->flags & PCI_RES_TYPE_MASK,
						 whole_subtree);

	 
	list_for_each_entry(fail_res, &fail_head, list) {
		struct resource *res = fail_res->res;
		int idx;

		res->start = fail_res->start;
		res->end = fail_res->end;
		res->flags = fail_res->flags;

		if (pci_is_bridge(fail_res->dev)) {
			idx = res - &fail_res->dev->resource[0];
			if (idx >= PCI_BRIDGE_RESOURCES &&
			    idx <= PCI_BRIDGE_RESOURCE_END)
				res->flags = 0;
		}
	}
	free_list(&fail_head);

	goto again;

enable_all:
	retval = pci_reenable_device(bridge);
	if (retval)
		pci_err(bridge, "Error reenabling bridge (%d)\n", retval);
	pci_set_master(bridge);
}
EXPORT_SYMBOL_GPL(pci_assign_unassigned_bridge_resources);

int pci_reassign_bridge_resources(struct pci_dev *bridge, unsigned long type)
{
	struct pci_dev_resource *dev_res;
	struct pci_dev *next;
	LIST_HEAD(saved);
	LIST_HEAD(added);
	LIST_HEAD(failed);
	unsigned int i;
	int ret;

	down_read(&pci_bus_sem);

	 
	next = bridge;
	do {
		bridge = next;
		for (i = PCI_BRIDGE_RESOURCES; i < PCI_BRIDGE_RESOURCE_END;
		     i++) {
			struct resource *res = &bridge->resource[i];

			if ((res->flags ^ type) & PCI_RES_TYPE_MASK)
				continue;

			 
			if (res->child)
				continue;

			ret = add_to_list(&saved, bridge, res, 0, 0);
			if (ret)
				goto cleanup;

			pci_info(bridge, "BAR %d: releasing %pR\n",
				 i, res);

			if (res->parent)
				release_resource(res);
			res->start = 0;
			res->end = 0;
			break;
		}
		if (i == PCI_BRIDGE_RESOURCE_END)
			break;

		next = bridge->bus ? bridge->bus->self : NULL;
	} while (next);

	if (list_empty(&saved)) {
		up_read(&pci_bus_sem);
		return -ENOENT;
	}

	__pci_bus_size_bridges(bridge->subordinate, &added);
	__pci_bridge_assign_resources(bridge, &added, &failed);
	BUG_ON(!list_empty(&added));

	if (!list_empty(&failed)) {
		ret = -ENOSPC;
		goto cleanup;
	}

	list_for_each_entry(dev_res, &saved, list) {
		 
		if (bridge == dev_res->dev)
			continue;

		bridge = dev_res->dev;
		pci_setup_bridge(bridge->subordinate);
	}

	free_list(&saved);
	up_read(&pci_bus_sem);
	return 0;

cleanup:
	 
	list_for_each_entry(dev_res, &failed, list) {
		struct resource *res = dev_res->res;

		res->start = dev_res->start;
		res->end = dev_res->end;
		res->flags = dev_res->flags;
	}
	free_list(&failed);

	 
	list_for_each_entry(dev_res, &saved, list) {
		struct resource *res = dev_res->res;

		bridge = dev_res->dev;
		i = res - bridge->resource;

		res->start = dev_res->start;
		res->end = dev_res->end;
		res->flags = dev_res->flags;

		pci_claim_resource(bridge, i);
		pci_setup_bridge(bridge->subordinate);
	}
	free_list(&saved);
	up_read(&pci_bus_sem);

	return ret;
}

void pci_assign_unassigned_bus_resources(struct pci_bus *bus)
{
	struct pci_dev *dev;
	 
	LIST_HEAD(add_list);

	down_read(&pci_bus_sem);
	for_each_pci_bridge(dev, bus)
		if (pci_has_subordinate(dev))
			__pci_bus_size_bridges(dev->subordinate, &add_list);
	up_read(&pci_bus_sem);
	__pci_bus_assign_resources(bus, &add_list, NULL);
	BUG_ON(!list_empty(&add_list));
}
EXPORT_SYMBOL_GPL(pci_assign_unassigned_bus_resources);
