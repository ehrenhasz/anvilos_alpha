
#include <linux/err.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/of_address.h>

enum devm_ioremap_type {
	DEVM_IOREMAP = 0,
	DEVM_IOREMAP_UC,
	DEVM_IOREMAP_WC,
	DEVM_IOREMAP_NP,
};

void devm_ioremap_release(struct device *dev, void *res)
{
	iounmap(*(void __iomem **)res);
}

static int devm_ioremap_match(struct device *dev, void *res, void *match_data)
{
	return *(void **)res == match_data;
}

static void __iomem *__devm_ioremap(struct device *dev, resource_size_t offset,
				    resource_size_t size,
				    enum devm_ioremap_type type)
{
	void __iomem **ptr, *addr = NULL;

	ptr = devres_alloc_node(devm_ioremap_release, sizeof(*ptr), GFP_KERNEL,
				dev_to_node(dev));
	if (!ptr)
		return NULL;

	switch (type) {
	case DEVM_IOREMAP:
		addr = ioremap(offset, size);
		break;
	case DEVM_IOREMAP_UC:
		addr = ioremap_uc(offset, size);
		break;
	case DEVM_IOREMAP_WC:
		addr = ioremap_wc(offset, size);
		break;
	case DEVM_IOREMAP_NP:
		addr = ioremap_np(offset, size);
		break;
	}

	if (addr) {
		*ptr = addr;
		devres_add(dev, ptr);
	} else
		devres_free(ptr);

	return addr;
}

 
void __iomem *devm_ioremap(struct device *dev, resource_size_t offset,
			   resource_size_t size)
{
	return __devm_ioremap(dev, offset, size, DEVM_IOREMAP);
}
EXPORT_SYMBOL(devm_ioremap);

 
void __iomem *devm_ioremap_uc(struct device *dev, resource_size_t offset,
			      resource_size_t size)
{
	return __devm_ioremap(dev, offset, size, DEVM_IOREMAP_UC);
}
EXPORT_SYMBOL_GPL(devm_ioremap_uc);

 
void __iomem *devm_ioremap_wc(struct device *dev, resource_size_t offset,
			      resource_size_t size)
{
	return __devm_ioremap(dev, offset, size, DEVM_IOREMAP_WC);
}
EXPORT_SYMBOL(devm_ioremap_wc);

 
void devm_iounmap(struct device *dev, void __iomem *addr)
{
	WARN_ON(devres_destroy(dev, devm_ioremap_release, devm_ioremap_match,
			       (__force void *)addr));
	iounmap(addr);
}
EXPORT_SYMBOL(devm_iounmap);

static void __iomem *
__devm_ioremap_resource(struct device *dev, const struct resource *res,
			enum devm_ioremap_type type)
{
	resource_size_t size;
	void __iomem *dest_ptr;
	char *pretty_name;

	BUG_ON(!dev);

	if (!res || resource_type(res) != IORESOURCE_MEM) {
		dev_err(dev, "invalid resource %pR\n", res);
		return IOMEM_ERR_PTR(-EINVAL);
	}

	if (type == DEVM_IOREMAP && res->flags & IORESOURCE_MEM_NONPOSTED)
		type = DEVM_IOREMAP_NP;

	size = resource_size(res);

	if (res->name)
		pretty_name = devm_kasprintf(dev, GFP_KERNEL, "%s %s",
					     dev_name(dev), res->name);
	else
		pretty_name = devm_kstrdup(dev, dev_name(dev), GFP_KERNEL);
	if (!pretty_name) {
		dev_err(dev, "can't generate pretty name for resource %pR\n", res);
		return IOMEM_ERR_PTR(-ENOMEM);
	}

	if (!devm_request_mem_region(dev, res->start, size, pretty_name)) {
		dev_err(dev, "can't request region for resource %pR\n", res);
		return IOMEM_ERR_PTR(-EBUSY);
	}

	dest_ptr = __devm_ioremap(dev, res->start, size, type);
	if (!dest_ptr) {
		dev_err(dev, "ioremap failed for resource %pR\n", res);
		devm_release_mem_region(dev, res->start, size);
		dest_ptr = IOMEM_ERR_PTR(-ENOMEM);
	}

	return dest_ptr;
}

 
void __iomem *devm_ioremap_resource(struct device *dev,
				    const struct resource *res)
{
	return __devm_ioremap_resource(dev, res, DEVM_IOREMAP);
}
EXPORT_SYMBOL(devm_ioremap_resource);

 
void __iomem *devm_ioremap_resource_wc(struct device *dev,
				       const struct resource *res)
{
	return __devm_ioremap_resource(dev, res, DEVM_IOREMAP_WC);
}

 
void __iomem *devm_of_iomap(struct device *dev, struct device_node *node, int index,
			    resource_size_t *size)
{
	struct resource res;

	if (of_address_to_resource(node, index, &res))
		return IOMEM_ERR_PTR(-EINVAL);
	if (size)
		*size = resource_size(&res);
	return devm_ioremap_resource(dev, &res);
}
EXPORT_SYMBOL(devm_of_iomap);

#ifdef CONFIG_HAS_IOPORT_MAP
 
static void devm_ioport_map_release(struct device *dev, void *res)
{
	ioport_unmap(*(void __iomem **)res);
}

static int devm_ioport_map_match(struct device *dev, void *res,
				 void *match_data)
{
	return *(void **)res == match_data;
}

 
void __iomem *devm_ioport_map(struct device *dev, unsigned long port,
			       unsigned int nr)
{
	void __iomem **ptr, *addr;

	ptr = devres_alloc_node(devm_ioport_map_release, sizeof(*ptr), GFP_KERNEL,
				dev_to_node(dev));
	if (!ptr)
		return NULL;

	addr = ioport_map(port, nr);
	if (addr) {
		*ptr = addr;
		devres_add(dev, ptr);
	} else
		devres_free(ptr);

	return addr;
}
EXPORT_SYMBOL(devm_ioport_map);

 
void devm_ioport_unmap(struct device *dev, void __iomem *addr)
{
	ioport_unmap(addr);
	WARN_ON(devres_destroy(dev, devm_ioport_map_release,
			       devm_ioport_map_match, (__force void *)addr));
}
EXPORT_SYMBOL(devm_ioport_unmap);
#endif  

#ifdef CONFIG_PCI
 
#define PCIM_IOMAP_MAX	PCI_STD_NUM_BARS

struct pcim_iomap_devres {
	void __iomem *table[PCIM_IOMAP_MAX];
};

static void pcim_iomap_release(struct device *gendev, void *res)
{
	struct pci_dev *dev = to_pci_dev(gendev);
	struct pcim_iomap_devres *this = res;
	int i;

	for (i = 0; i < PCIM_IOMAP_MAX; i++)
		if (this->table[i])
			pci_iounmap(dev, this->table[i]);
}

 
void __iomem * const *pcim_iomap_table(struct pci_dev *pdev)
{
	struct pcim_iomap_devres *dr, *new_dr;

	dr = devres_find(&pdev->dev, pcim_iomap_release, NULL, NULL);
	if (dr)
		return dr->table;

	new_dr = devres_alloc_node(pcim_iomap_release, sizeof(*new_dr), GFP_KERNEL,
				   dev_to_node(&pdev->dev));
	if (!new_dr)
		return NULL;
	dr = devres_get(&pdev->dev, new_dr, NULL, NULL);
	return dr->table;
}
EXPORT_SYMBOL(pcim_iomap_table);

 
void __iomem *pcim_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen)
{
	void __iomem **tbl;

	BUG_ON(bar >= PCIM_IOMAP_MAX);

	tbl = (void __iomem **)pcim_iomap_table(pdev);
	if (!tbl || tbl[bar])	 
		return NULL;

	tbl[bar] = pci_iomap(pdev, bar, maxlen);
	return tbl[bar];
}
EXPORT_SYMBOL(pcim_iomap);

 
void pcim_iounmap(struct pci_dev *pdev, void __iomem *addr)
{
	void __iomem **tbl;
	int i;

	pci_iounmap(pdev, addr);

	tbl = (void __iomem **)pcim_iomap_table(pdev);
	BUG_ON(!tbl);

	for (i = 0; i < PCIM_IOMAP_MAX; i++)
		if (tbl[i] == addr) {
			tbl[i] = NULL;
			return;
		}
	WARN_ON(1);
}
EXPORT_SYMBOL(pcim_iounmap);

 
int pcim_iomap_regions(struct pci_dev *pdev, int mask, const char *name)
{
	void __iomem * const *iomap;
	int i, rc;

	iomap = pcim_iomap_table(pdev);
	if (!iomap)
		return -ENOMEM;

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		unsigned long len;

		if (!(mask & (1 << i)))
			continue;

		rc = -EINVAL;
		len = pci_resource_len(pdev, i);
		if (!len)
			goto err_inval;

		rc = pci_request_region(pdev, i, name);
		if (rc)
			goto err_inval;

		rc = -ENOMEM;
		if (!pcim_iomap(pdev, i, 0))
			goto err_region;
	}

	return 0;

 err_region:
	pci_release_region(pdev, i);
 err_inval:
	while (--i >= 0) {
		if (!(mask & (1 << i)))
			continue;
		pcim_iounmap(pdev, iomap[i]);
		pci_release_region(pdev, i);
	}

	return rc;
}
EXPORT_SYMBOL(pcim_iomap_regions);

 
int pcim_iomap_regions_request_all(struct pci_dev *pdev, int mask,
				   const char *name)
{
	int request_mask = ((1 << 6) - 1) & ~mask;
	int rc;

	rc = pci_request_selected_regions(pdev, request_mask, name);
	if (rc)
		return rc;

	rc = pcim_iomap_regions(pdev, mask, name);
	if (rc)
		pci_release_selected_regions(pdev, request_mask);
	return rc;
}
EXPORT_SYMBOL(pcim_iomap_regions_request_all);

 
void pcim_iounmap_regions(struct pci_dev *pdev, int mask)
{
	void __iomem * const *iomap;
	int i;

	iomap = pcim_iomap_table(pdev);
	if (!iomap)
		return;

	for (i = 0; i < PCIM_IOMAP_MAX; i++) {
		if (!(mask & (1 << i)))
			continue;

		pcim_iounmap(pdev, iomap[i]);
		pci_release_region(pdev, i);
	}
}
EXPORT_SYMBOL(pcim_iounmap_regions);
#endif  

static void devm_arch_phys_ac_add_release(struct device *dev, void *res)
{
	arch_phys_wc_del(*((int *)res));
}

 
int devm_arch_phys_wc_add(struct device *dev, unsigned long base, unsigned long size)
{
	int *mtrr;
	int ret;

	mtrr = devres_alloc_node(devm_arch_phys_ac_add_release, sizeof(*mtrr), GFP_KERNEL,
				 dev_to_node(dev));
	if (!mtrr)
		return -ENOMEM;

	ret = arch_phys_wc_add(base, size);
	if (ret < 0) {
		devres_free(mtrr);
		return ret;
	}

	*mtrr = ret;
	devres_add(dev, mtrr);

	return ret;
}
EXPORT_SYMBOL(devm_arch_phys_wc_add);

struct arch_io_reserve_memtype_wc_devres {
	resource_size_t start;
	resource_size_t size;
};

static void devm_arch_io_free_memtype_wc_release(struct device *dev, void *res)
{
	const struct arch_io_reserve_memtype_wc_devres *this = res;

	arch_io_free_memtype_wc(this->start, this->size);
}

 
int devm_arch_io_reserve_memtype_wc(struct device *dev, resource_size_t start,
				    resource_size_t size)
{
	struct arch_io_reserve_memtype_wc_devres *dr;
	int ret;

	dr = devres_alloc_node(devm_arch_io_free_memtype_wc_release, sizeof(*dr), GFP_KERNEL,
			       dev_to_node(dev));
	if (!dr)
		return -ENOMEM;

	ret = arch_io_reserve_memtype_wc(start, size);
	if (ret < 0) {
		devres_free(dr);
		return ret;
	}

	dr->start = start;
	dr->size = size;
	devres_add(dev, dr);

	return ret;
}
EXPORT_SYMBOL(devm_arch_io_reserve_memtype_wc);
