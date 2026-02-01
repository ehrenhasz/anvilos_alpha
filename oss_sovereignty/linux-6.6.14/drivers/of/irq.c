
 

#define pr_fmt(fmt)	"OF: " fmt

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/slab.h>

 
unsigned int irq_of_parse_and_map(struct device_node *dev, int index)
{
	struct of_phandle_args oirq;

	if (of_irq_parse_one(dev, index, &oirq))
		return 0;

	return irq_create_of_mapping(&oirq);
}
EXPORT_SYMBOL_GPL(irq_of_parse_and_map);

 
struct device_node *of_irq_find_parent(struct device_node *child)
{
	struct device_node *p;
	phandle parent;

	if (!of_node_get(child))
		return NULL;

	do {
		if (of_property_read_u32(child, "interrupt-parent", &parent)) {
			p = of_get_parent(child);
		} else	{
			if (of_irq_workarounds & OF_IMAP_NO_PHANDLE)
				p = of_node_get(of_irq_dflt_pic);
			else
				p = of_find_node_by_phandle(parent);
		}
		of_node_put(child);
		child = p;
	} while (p && of_get_property(p, "#interrupt-cells", NULL) == NULL);

	return p;
}
EXPORT_SYMBOL_GPL(of_irq_find_parent);

 
static const char * const of_irq_imap_abusers[] = {
	"CBEA,platform-spider-pic",
	"sti,platform-spider-pic",
	"realtek,rtl-intc",
	"fsl,ls1021a-extirq",
	"fsl,ls1043a-extirq",
	"fsl,ls1088a-extirq",
	"renesas,rza1-irqc",
	NULL,
};

 
int of_irq_parse_raw(const __be32 *addr, struct of_phandle_args *out_irq)
{
	struct device_node *ipar, *tnode, *old = NULL, *newpar = NULL;
	__be32 initial_match_array[MAX_PHANDLE_ARGS];
	const __be32 *match_array = initial_match_array;
	const __be32 *tmp, *imap, *imask, dummy_imask[] = { [0 ... MAX_PHANDLE_ARGS] = cpu_to_be32(~0) };
	u32 intsize = 1, addrsize, newintsize = 0, newaddrsize = 0;
	int imaplen, match, i, rc = -EINVAL;

#ifdef DEBUG
	of_print_phandle_args("of_irq_parse_raw: ", out_irq);
#endif

	ipar = of_node_get(out_irq->np);

	 
	do {
		if (!of_property_read_u32(ipar, "#interrupt-cells", &intsize))
			break;
		tnode = ipar;
		ipar = of_irq_find_parent(ipar);
		of_node_put(tnode);
	} while (ipar);
	if (ipar == NULL) {
		pr_debug(" -> no parent found !\n");
		goto fail;
	}

	pr_debug("of_irq_parse_raw: ipar=%pOF, size=%d\n", ipar, intsize);

	if (out_irq->args_count != intsize)
		goto fail;

	 
	old = of_node_get(ipar);
	do {
		tmp = of_get_property(old, "#address-cells", NULL);
		tnode = of_get_parent(old);
		of_node_put(old);
		old = tnode;
	} while (old && tmp == NULL);
	of_node_put(old);
	old = NULL;
	addrsize = (tmp == NULL) ? 2 : be32_to_cpu(*tmp);

	pr_debug(" -> addrsize=%d\n", addrsize);

	 
	if (WARN_ON(addrsize + intsize > MAX_PHANDLE_ARGS)) {
		rc = -EFAULT;
		goto fail;
	}

	 
	for (i = 0; i < addrsize; i++)
		initial_match_array[i] = addr ? addr[i] : 0;
	for (i = 0; i < intsize; i++)
		initial_match_array[addrsize + i] = cpu_to_be32(out_irq->args[i]);

	 
	while (ipar != NULL) {
		 
		bool intc = of_property_read_bool(ipar, "interrupt-controller");

		imap = of_get_property(ipar, "interrupt-map", &imaplen);
		if (intc &&
		    (!imap || of_device_compatible_match(ipar, of_irq_imap_abusers))) {
			pr_debug(" -> got it !\n");
			return 0;
		}

		 
		if (addrsize && !addr) {
			pr_debug(" -> no reg passed in when needed !\n");
			goto fail;
		}

		 
		if (imap == NULL) {
			pr_debug(" -> no map, getting parent\n");
			newpar = of_irq_find_parent(ipar);
			goto skiplevel;
		}
		imaplen /= sizeof(u32);

		 
		imask = of_get_property(ipar, "interrupt-map-mask", NULL);
		if (!imask)
			imask = dummy_imask;

		 
		match = 0;
		while (imaplen > (addrsize + intsize + 1) && !match) {
			 
			match = 1;
			for (i = 0; i < (addrsize + intsize); i++, imaplen--)
				match &= !((match_array[i] ^ *imap++) & imask[i]);

			pr_debug(" -> match=%d (imaplen=%d)\n", match, imaplen);

			 
			if (of_irq_workarounds & OF_IMAP_NO_PHANDLE)
				newpar = of_node_get(of_irq_dflt_pic);
			else
				newpar = of_find_node_by_phandle(be32_to_cpup(imap));
			imap++;
			--imaplen;

			 
			if (newpar == NULL) {
				pr_debug(" -> imap parent not found !\n");
				goto fail;
			}

			if (!of_device_is_available(newpar))
				match = 0;

			 
			if (of_property_read_u32(newpar, "#interrupt-cells",
						 &newintsize)) {
				pr_debug(" -> parent lacks #interrupt-cells!\n");
				goto fail;
			}
			if (of_property_read_u32(newpar, "#address-cells",
						 &newaddrsize))
				newaddrsize = 0;

			pr_debug(" -> newintsize=%d, newaddrsize=%d\n",
			    newintsize, newaddrsize);

			 
			if (WARN_ON(newaddrsize + newintsize > MAX_PHANDLE_ARGS)
			    || (imaplen < (newaddrsize + newintsize))) {
				rc = -EFAULT;
				goto fail;
			}

			imap += newaddrsize + newintsize;
			imaplen -= newaddrsize + newintsize;

			pr_debug(" -> imaplen=%d\n", imaplen);
		}
		if (!match) {
			if (intc) {
				 
				WARN(!IS_ENABLED(CONFIG_PPC_PASEMI),
				     "%pOF interrupt-map failed, using interrupt-controller\n",
				     ipar);
				return 0;
			}

			goto fail;
		}

		 
		match_array = imap - newaddrsize - newintsize;
		for (i = 0; i < newintsize; i++)
			out_irq->args[i] = be32_to_cpup(imap - newintsize + i);
		out_irq->args_count = intsize = newintsize;
		addrsize = newaddrsize;

		if (ipar == newpar) {
			pr_debug("%pOF interrupt-map entry to self\n", ipar);
			return 0;
		}

	skiplevel:
		 
		out_irq->np = newpar;
		pr_debug(" -> new parent: %pOF\n", newpar);
		of_node_put(ipar);
		ipar = newpar;
		newpar = NULL;
	}
	rc = -ENOENT;  

 fail:
	of_node_put(ipar);
	of_node_put(newpar);

	return rc;
}
EXPORT_SYMBOL_GPL(of_irq_parse_raw);

 
int of_irq_parse_one(struct device_node *device, int index, struct of_phandle_args *out_irq)
{
	struct device_node *p;
	const __be32 *addr;
	u32 intsize;
	int i, res;

	pr_debug("of_irq_parse_one: dev=%pOF, index=%d\n", device, index);

	 
	if (of_irq_workarounds & OF_IMAP_OLDWORLD_MAC)
		return of_irq_parse_oldworld(device, index, out_irq);

	 
	addr = of_get_property(device, "reg", NULL);

	 
	res = of_parse_phandle_with_args(device, "interrupts-extended",
					"#interrupt-cells", index, out_irq);
	if (!res)
		return of_irq_parse_raw(addr, out_irq);

	 
	p = of_irq_find_parent(device);
	if (p == NULL)
		return -EINVAL;

	 
	if (of_property_read_u32(p, "#interrupt-cells", &intsize)) {
		res = -EINVAL;
		goto out;
	}

	pr_debug(" parent=%pOF, intsize=%d\n", p, intsize);

	 
	out_irq->np = p;
	out_irq->args_count = intsize;
	for (i = 0; i < intsize; i++) {
		res = of_property_read_u32_index(device, "interrupts",
						 (index * intsize) + i,
						 out_irq->args + i);
		if (res)
			goto out;
	}

	pr_debug(" intspec=%d\n", *out_irq->args);


	 
	res = of_irq_parse_raw(addr, out_irq);
 out:
	of_node_put(p);
	return res;
}
EXPORT_SYMBOL_GPL(of_irq_parse_one);

 
int of_irq_to_resource(struct device_node *dev, int index, struct resource *r)
{
	int irq = of_irq_get(dev, index);

	if (irq < 0)
		return irq;

	 
	if (r && irq) {
		const char *name = NULL;

		memset(r, 0, sizeof(*r));
		 
		of_property_read_string_index(dev, "interrupt-names", index,
					      &name);

		r->start = r->end = irq;
		r->flags = IORESOURCE_IRQ | irqd_get_trigger_type(irq_get_irq_data(irq));
		r->name = name ? name : of_node_full_name(dev);
	}

	return irq;
}
EXPORT_SYMBOL_GPL(of_irq_to_resource);

 
int of_irq_get(struct device_node *dev, int index)
{
	int rc;
	struct of_phandle_args oirq;
	struct irq_domain *domain;

	rc = of_irq_parse_one(dev, index, &oirq);
	if (rc)
		return rc;

	domain = irq_find_host(oirq.np);
	if (!domain) {
		rc = -EPROBE_DEFER;
		goto out;
	}

	rc = irq_create_of_mapping(&oirq);
out:
	of_node_put(oirq.np);

	return rc;
}
EXPORT_SYMBOL_GPL(of_irq_get);

 
int of_irq_get_byname(struct device_node *dev, const char *name)
{
	int index;

	if (unlikely(!name))
		return -EINVAL;

	index = of_property_match_string(dev, "interrupt-names", name);
	if (index < 0)
		return index;

	return of_irq_get(dev, index);
}
EXPORT_SYMBOL_GPL(of_irq_get_byname);

 
int of_irq_count(struct device_node *dev)
{
	struct of_phandle_args irq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &irq) == 0)
		nr++;

	return nr;
}

 
int of_irq_to_resource_table(struct device_node *dev, struct resource *res,
		int nr_irqs)
{
	int i;

	for (i = 0; i < nr_irqs; i++, res++)
		if (of_irq_to_resource(dev, i, res) <= 0)
			break;

	return i;
}
EXPORT_SYMBOL_GPL(of_irq_to_resource_table);

struct of_intc_desc {
	struct list_head	list;
	of_irq_init_cb_t	irq_init_cb;
	struct device_node	*dev;
	struct device_node	*interrupt_parent;
};

 
void __init of_irq_init(const struct of_device_id *matches)
{
	const struct of_device_id *match;
	struct device_node *np, *parent = NULL;
	struct of_intc_desc *desc, *temp_desc;
	struct list_head intc_desc_list, intc_parent_list;

	INIT_LIST_HEAD(&intc_desc_list);
	INIT_LIST_HEAD(&intc_parent_list);

	for_each_matching_node_and_match(np, matches, &match) {
		if (!of_property_read_bool(np, "interrupt-controller") ||
				!of_device_is_available(np))
			continue;

		if (WARN(!match->data, "of_irq_init: no init function for %s\n",
			 match->compatible))
			continue;

		 
		desc = kzalloc(sizeof(*desc), GFP_KERNEL);
		if (!desc) {
			of_node_put(np);
			goto err;
		}

		desc->irq_init_cb = match->data;
		desc->dev = of_node_get(np);
		 
		desc->interrupt_parent = of_parse_phandle(np, "interrupts-extended", 0);
		if (!desc->interrupt_parent)
			desc->interrupt_parent = of_irq_find_parent(np);
		if (desc->interrupt_parent == np) {
			of_node_put(desc->interrupt_parent);
			desc->interrupt_parent = NULL;
		}
		list_add_tail(&desc->list, &intc_desc_list);
	}

	 
	while (!list_empty(&intc_desc_list)) {
		 
		list_for_each_entry_safe(desc, temp_desc, &intc_desc_list, list) {
			int ret;

			if (desc->interrupt_parent != parent)
				continue;

			list_del(&desc->list);

			of_node_set_flag(desc->dev, OF_POPULATED);

			pr_debug("of_irq_init: init %pOF (%p), parent %p\n",
				 desc->dev,
				 desc->dev, desc->interrupt_parent);
			ret = desc->irq_init_cb(desc->dev,
						desc->interrupt_parent);
			if (ret) {
				pr_err("%s: Failed to init %pOF (%p), parent %p\n",
				       __func__, desc->dev, desc->dev,
				       desc->interrupt_parent);
				of_node_clear_flag(desc->dev, OF_POPULATED);
				kfree(desc);
				continue;
			}

			 
			list_add_tail(&desc->list, &intc_parent_list);
		}

		 
		desc = list_first_entry_or_null(&intc_parent_list,
						typeof(*desc), list);
		if (!desc) {
			pr_err("of_irq_init: children remain, but no parents\n");
			break;
		}
		list_del(&desc->list);
		parent = desc->dev;
		kfree(desc);
	}

	list_for_each_entry_safe(desc, temp_desc, &intc_parent_list, list) {
		list_del(&desc->list);
		kfree(desc);
	}
err:
	list_for_each_entry_safe(desc, temp_desc, &intc_desc_list, list) {
		list_del(&desc->list);
		of_node_put(desc->dev);
		kfree(desc);
	}
}

static u32 __of_msi_map_id(struct device *dev, struct device_node **np,
			    u32 id_in)
{
	struct device *parent_dev;
	u32 id_out = id_in;

	 
	for (parent_dev = dev; parent_dev; parent_dev = parent_dev->parent)
		if (!of_map_id(parent_dev->of_node, id_in, "msi-map",
				"msi-map-mask", np, &id_out))
			break;
	return id_out;
}

 
u32 of_msi_map_id(struct device *dev, struct device_node *msi_np, u32 id_in)
{
	return __of_msi_map_id(dev, &msi_np, id_in);
}

 
struct irq_domain *of_msi_map_get_device_domain(struct device *dev, u32 id,
						u32 bus_token)
{
	struct device_node *np = NULL;

	__of_msi_map_id(dev, &np, id);
	return irq_find_matching_host(np, bus_token);
}

 
struct irq_domain *of_msi_get_domain(struct device *dev,
				     struct device_node *np,
				     enum irq_domain_bus_token token)
{
	struct device_node *msi_np;
	struct irq_domain *d;

	 
	msi_np = of_parse_phandle(np, "msi-parent", 0);
	if (msi_np && !of_property_read_bool(msi_np, "#msi-cells")) {
		d = irq_find_matching_host(msi_np, token);
		if (!d)
			of_node_put(msi_np);
		return d;
	}

	if (token == DOMAIN_BUS_PLATFORM_MSI) {
		 
		struct of_phandle_args args;
		int index = 0;

		while (!of_parse_phandle_with_args(np, "msi-parent",
						   "#msi-cells",
						   index, &args)) {
			d = irq_find_matching_host(args.np, token);
			if (d)
				return d;

			of_node_put(args.np);
			index++;
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(of_msi_get_domain);

 
void of_msi_configure(struct device *dev, struct device_node *np)
{
	dev_set_msi_domain(dev,
			   of_msi_get_domain(dev, np, DOMAIN_BUS_PLATFORM_MSI));
}
EXPORT_SYMBOL_GPL(of_msi_configure);
