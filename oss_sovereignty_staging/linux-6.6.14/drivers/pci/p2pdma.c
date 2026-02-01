
 

#define pr_fmt(fmt) "pci-p2pdma: " fmt
#include <linux/ctype.h>
#include <linux/dma-map-ops.h>
#include <linux/pci-p2pdma.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/memremap.h>
#include <linux/percpu-refcount.h>
#include <linux/random.h>
#include <linux/seq_buf.h>
#include <linux/xarray.h>

struct pci_p2pdma {
	struct gen_pool *pool;
	bool p2pmem_published;
	struct xarray map_types;
};

struct pci_p2pdma_pagemap {
	struct dev_pagemap pgmap;
	struct pci_dev *provider;
	u64 bus_offset;
};

static struct pci_p2pdma_pagemap *to_p2p_pgmap(struct dev_pagemap *pgmap)
{
	return container_of(pgmap, struct pci_p2pdma_pagemap, pgmap);
}

static ssize_t size_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_p2pdma *p2pdma;
	size_t size = 0;

	rcu_read_lock();
	p2pdma = rcu_dereference(pdev->p2pdma);
	if (p2pdma && p2pdma->pool)
		size = gen_pool_size(p2pdma->pool);
	rcu_read_unlock();

	return sysfs_emit(buf, "%zd\n", size);
}
static DEVICE_ATTR_RO(size);

static ssize_t available_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_p2pdma *p2pdma;
	size_t avail = 0;

	rcu_read_lock();
	p2pdma = rcu_dereference(pdev->p2pdma);
	if (p2pdma && p2pdma->pool)
		avail = gen_pool_avail(p2pdma->pool);
	rcu_read_unlock();

	return sysfs_emit(buf, "%zd\n", avail);
}
static DEVICE_ATTR_RO(available);

static ssize_t published_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_p2pdma *p2pdma;
	bool published = false;

	rcu_read_lock();
	p2pdma = rcu_dereference(pdev->p2pdma);
	if (p2pdma)
		published = p2pdma->p2pmem_published;
	rcu_read_unlock();

	return sysfs_emit(buf, "%d\n", published);
}
static DEVICE_ATTR_RO(published);

static int p2pmem_alloc_mmap(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, struct vm_area_struct *vma)
{
	struct pci_dev *pdev = to_pci_dev(kobj_to_dev(kobj));
	size_t len = vma->vm_end - vma->vm_start;
	struct pci_p2pdma *p2pdma;
	struct percpu_ref *ref;
	unsigned long vaddr;
	void *kaddr;
	int ret;

	 
	if ((vma->vm_flags & VM_MAYSHARE) != VM_MAYSHARE) {
		pci_info_ratelimited(pdev,
				     "%s: fail, attempted private mapping\n",
				     current->comm);
		return -EINVAL;
	}

	if (vma->vm_pgoff) {
		pci_info_ratelimited(pdev,
				     "%s: fail, attempted mapping with non-zero offset\n",
				     current->comm);
		return -EINVAL;
	}

	rcu_read_lock();
	p2pdma = rcu_dereference(pdev->p2pdma);
	if (!p2pdma) {
		ret = -ENODEV;
		goto out;
	}

	kaddr = (void *)gen_pool_alloc_owner(p2pdma->pool, len, (void **)&ref);
	if (!kaddr) {
		ret = -ENOMEM;
		goto out;
	}

	 
	if (unlikely(!percpu_ref_tryget_live_rcu(ref))) {
		ret = -ENODEV;
		goto out_free_mem;
	}
	rcu_read_unlock();

	for (vaddr = vma->vm_start; vaddr < vma->vm_end; vaddr += PAGE_SIZE) {
		ret = vm_insert_page(vma, vaddr, virt_to_page(kaddr));
		if (ret) {
			gen_pool_free(p2pdma->pool, (uintptr_t)kaddr, len);
			return ret;
		}
		percpu_ref_get(ref);
		put_page(virt_to_page(kaddr));
		kaddr += PAGE_SIZE;
		len -= PAGE_SIZE;
	}

	percpu_ref_put(ref);

	return 0;
out_free_mem:
	gen_pool_free(p2pdma->pool, (uintptr_t)kaddr, len);
out:
	rcu_read_unlock();
	return ret;
}

static struct bin_attribute p2pmem_alloc_attr = {
	.attr = { .name = "allocate", .mode = 0660 },
	.mmap = p2pmem_alloc_mmap,
	 
	.size = SZ_1T,
};

static struct attribute *p2pmem_attrs[] = {
	&dev_attr_size.attr,
	&dev_attr_available.attr,
	&dev_attr_published.attr,
	NULL,
};

static struct bin_attribute *p2pmem_bin_attrs[] = {
	&p2pmem_alloc_attr,
	NULL,
};

static const struct attribute_group p2pmem_group = {
	.attrs = p2pmem_attrs,
	.bin_attrs = p2pmem_bin_attrs,
	.name = "p2pmem",
};

static void p2pdma_page_free(struct page *page)
{
	struct pci_p2pdma_pagemap *pgmap = to_p2p_pgmap(page->pgmap);
	 
	struct pci_p2pdma *p2pdma =
		rcu_dereference_protected(pgmap->provider->p2pdma, 1);
	struct percpu_ref *ref;

	gen_pool_free_owner(p2pdma->pool, (uintptr_t)page_to_virt(page),
			    PAGE_SIZE, (void **)&ref);
	percpu_ref_put(ref);
}

static const struct dev_pagemap_ops p2pdma_pgmap_ops = {
	.page_free = p2pdma_page_free,
};

static void pci_p2pdma_release(void *data)
{
	struct pci_dev *pdev = data;
	struct pci_p2pdma *p2pdma;

	p2pdma = rcu_dereference_protected(pdev->p2pdma, 1);
	if (!p2pdma)
		return;

	 
	pdev->p2pdma = NULL;
	synchronize_rcu();

	gen_pool_destroy(p2pdma->pool);
	sysfs_remove_group(&pdev->dev.kobj, &p2pmem_group);
	xa_destroy(&p2pdma->map_types);
}

static int pci_p2pdma_setup(struct pci_dev *pdev)
{
	int error = -ENOMEM;
	struct pci_p2pdma *p2p;

	p2p = devm_kzalloc(&pdev->dev, sizeof(*p2p), GFP_KERNEL);
	if (!p2p)
		return -ENOMEM;

	xa_init(&p2p->map_types);

	p2p->pool = gen_pool_create(PAGE_SHIFT, dev_to_node(&pdev->dev));
	if (!p2p->pool)
		goto out;

	error = devm_add_action_or_reset(&pdev->dev, pci_p2pdma_release, pdev);
	if (error)
		goto out_pool_destroy;

	error = sysfs_create_group(&pdev->dev.kobj, &p2pmem_group);
	if (error)
		goto out_pool_destroy;

	rcu_assign_pointer(pdev->p2pdma, p2p);
	return 0;

out_pool_destroy:
	gen_pool_destroy(p2p->pool);
out:
	devm_kfree(&pdev->dev, p2p);
	return error;
}

static void pci_p2pdma_unmap_mappings(void *data)
{
	struct pci_dev *pdev = data;

	 
	sysfs_remove_file_from_group(&pdev->dev.kobj, &p2pmem_alloc_attr.attr,
				     p2pmem_group.name);
}

 
int pci_p2pdma_add_resource(struct pci_dev *pdev, int bar, size_t size,
			    u64 offset)
{
	struct pci_p2pdma_pagemap *p2p_pgmap;
	struct dev_pagemap *pgmap;
	struct pci_p2pdma *p2pdma;
	void *addr;
	int error;

	if (!(pci_resource_flags(pdev, bar) & IORESOURCE_MEM))
		return -EINVAL;

	if (offset >= pci_resource_len(pdev, bar))
		return -EINVAL;

	if (!size)
		size = pci_resource_len(pdev, bar) - offset;

	if (size + offset > pci_resource_len(pdev, bar))
		return -EINVAL;

	if (!pdev->p2pdma) {
		error = pci_p2pdma_setup(pdev);
		if (error)
			return error;
	}

	p2p_pgmap = devm_kzalloc(&pdev->dev, sizeof(*p2p_pgmap), GFP_KERNEL);
	if (!p2p_pgmap)
		return -ENOMEM;

	pgmap = &p2p_pgmap->pgmap;
	pgmap->range.start = pci_resource_start(pdev, bar) + offset;
	pgmap->range.end = pgmap->range.start + size - 1;
	pgmap->nr_range = 1;
	pgmap->type = MEMORY_DEVICE_PCI_P2PDMA;
	pgmap->ops = &p2pdma_pgmap_ops;

	p2p_pgmap->provider = pdev;
	p2p_pgmap->bus_offset = pci_bus_address(pdev, bar) -
		pci_resource_start(pdev, bar);

	addr = devm_memremap_pages(&pdev->dev, pgmap);
	if (IS_ERR(addr)) {
		error = PTR_ERR(addr);
		goto pgmap_free;
	}

	error = devm_add_action_or_reset(&pdev->dev, pci_p2pdma_unmap_mappings,
					 pdev);
	if (error)
		goto pages_free;

	p2pdma = rcu_dereference_protected(pdev->p2pdma, 1);
	error = gen_pool_add_owner(p2pdma->pool, (unsigned long)addr,
			pci_bus_address(pdev, bar) + offset,
			range_len(&pgmap->range), dev_to_node(&pdev->dev),
			&pgmap->ref);
	if (error)
		goto pages_free;

	pci_info(pdev, "added peer-to-peer DMA memory %#llx-%#llx\n",
		 pgmap->range.start, pgmap->range.end);

	return 0;

pages_free:
	devm_memunmap_pages(&pdev->dev, pgmap);
pgmap_free:
	devm_kfree(&pdev->dev, pgmap);
	return error;
}
EXPORT_SYMBOL_GPL(pci_p2pdma_add_resource);

 
static struct pci_dev *find_parent_pci_dev(struct device *dev)
{
	struct device *parent;

	dev = get_device(dev);

	while (dev) {
		if (dev_is_pci(dev))
			return to_pci_dev(dev);

		parent = get_device(dev->parent);
		put_device(dev);
		dev = parent;
	}

	return NULL;
}

 
static int pci_bridge_has_acs_redir(struct pci_dev *pdev)
{
	int pos;
	u16 ctrl;

	pos = pdev->acs_cap;
	if (!pos)
		return 0;

	pci_read_config_word(pdev, pos + PCI_ACS_CTRL, &ctrl);

	if (ctrl & (PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_EC))
		return 1;

	return 0;
}

static void seq_buf_print_bus_devfn(struct seq_buf *buf, struct pci_dev *pdev)
{
	if (!buf)
		return;

	seq_buf_printf(buf, "%s;", pci_name(pdev));
}

static bool cpu_supports_p2pdma(void)
{
#ifdef CONFIG_X86
	struct cpuinfo_x86 *c = &cpu_data(0);

	 
	if (c->x86_vendor == X86_VENDOR_AMD && c->x86 >= 0x17)
		return true;
#endif

	return false;
}

static const struct pci_p2pdma_whitelist_entry {
	unsigned short vendor;
	unsigned short device;
	enum {
		REQ_SAME_HOST_BRIDGE	= 1 << 0,
	} flags;
} pci_p2pdma_whitelist[] = {
	 
	{PCI_VENDOR_ID_INTEL,	0x3c00, REQ_SAME_HOST_BRIDGE},
	{PCI_VENDOR_ID_INTEL,	0x3c01, REQ_SAME_HOST_BRIDGE},
	 
	{PCI_VENDOR_ID_INTEL,	0x2f00, REQ_SAME_HOST_BRIDGE},
	{PCI_VENDOR_ID_INTEL,	0x2f01, REQ_SAME_HOST_BRIDGE},
	 
	{PCI_VENDOR_ID_INTEL,	0x2030, 0},
	{PCI_VENDOR_ID_INTEL,	0x2031, 0},
	{PCI_VENDOR_ID_INTEL,	0x2032, 0},
	{PCI_VENDOR_ID_INTEL,	0x2033, 0},
	{PCI_VENDOR_ID_INTEL,	0x2020, 0},
	{PCI_VENDOR_ID_INTEL,	0x09a2, 0},
	{}
};

 
static struct pci_dev *pci_host_bridge_dev(struct pci_host_bridge *host)
{
	struct pci_dev *root;

	root = list_first_entry_or_null(&host->bus->devices,
					struct pci_dev, bus_list);

	if (!root)
		return NULL;

	if (root->devfn == PCI_DEVFN(0, 0))
		return root;

	if (pci_pcie_type(root) == PCI_EXP_TYPE_ROOT_PORT)
		return root;

	return NULL;
}

static bool __host_bridge_whitelist(struct pci_host_bridge *host,
				    bool same_host_bridge, bool warn)
{
	struct pci_dev *root = pci_host_bridge_dev(host);
	const struct pci_p2pdma_whitelist_entry *entry;
	unsigned short vendor, device;

	if (!root)
		return false;

	vendor = root->vendor;
	device = root->device;

	for (entry = pci_p2pdma_whitelist; entry->vendor; entry++) {
		if (vendor != entry->vendor || device != entry->device)
			continue;
		if (entry->flags & REQ_SAME_HOST_BRIDGE && !same_host_bridge)
			return false;

		return true;
	}

	if (warn)
		pci_warn(root, "Host bridge not in P2PDMA whitelist: %04x:%04x\n",
			 vendor, device);

	return false;
}

 
static bool host_bridge_whitelist(struct pci_dev *a, struct pci_dev *b,
				  bool warn)
{
	struct pci_host_bridge *host_a = pci_find_host_bridge(a->bus);
	struct pci_host_bridge *host_b = pci_find_host_bridge(b->bus);

	if (host_a == host_b)
		return __host_bridge_whitelist(host_a, true, warn);

	if (__host_bridge_whitelist(host_a, false, warn) &&
	    __host_bridge_whitelist(host_b, false, warn))
		return true;

	return false;
}

static unsigned long map_types_idx(struct pci_dev *client)
{
	return (pci_domain_nr(client->bus) << 16) | pci_dev_id(client);
}

 
static enum pci_p2pdma_map_type
calc_map_type_and_dist(struct pci_dev *provider, struct pci_dev *client,
		int *dist, bool verbose)
{
	enum pci_p2pdma_map_type map_type = PCI_P2PDMA_MAP_THRU_HOST_BRIDGE;
	struct pci_dev *a = provider, *b = client, *bb;
	bool acs_redirects = false;
	struct pci_p2pdma *p2pdma;
	struct seq_buf acs_list;
	int acs_cnt = 0;
	int dist_a = 0;
	int dist_b = 0;
	char buf[128];

	seq_buf_init(&acs_list, buf, sizeof(buf));

	 
	while (a) {
		dist_b = 0;

		if (pci_bridge_has_acs_redir(a)) {
			seq_buf_print_bus_devfn(&acs_list, a);
			acs_cnt++;
		}

		bb = b;

		while (bb) {
			if (a == bb)
				goto check_b_path_acs;

			bb = pci_upstream_bridge(bb);
			dist_b++;
		}

		a = pci_upstream_bridge(a);
		dist_a++;
	}

	*dist = dist_a + dist_b;
	goto map_through_host_bridge;

check_b_path_acs:
	bb = b;

	while (bb) {
		if (a == bb)
			break;

		if (pci_bridge_has_acs_redir(bb)) {
			seq_buf_print_bus_devfn(&acs_list, bb);
			acs_cnt++;
		}

		bb = pci_upstream_bridge(bb);
	}

	*dist = dist_a + dist_b;

	if (!acs_cnt) {
		map_type = PCI_P2PDMA_MAP_BUS_ADDR;
		goto done;
	}

	if (verbose) {
		acs_list.buffer[acs_list.len-1] = 0;  
		pci_warn(client, "ACS redirect is set between the client and provider (%s)\n",
			 pci_name(provider));
		pci_warn(client, "to disable ACS redirect for this path, add the kernel parameter: pci=disable_acs_redir=%s\n",
			 acs_list.buffer);
	}
	acs_redirects = true;

map_through_host_bridge:
	if (!cpu_supports_p2pdma() &&
	    !host_bridge_whitelist(provider, client, acs_redirects)) {
		if (verbose)
			pci_warn(client, "cannot be used for peer-to-peer DMA as the client and provider (%s) do not share an upstream bridge or whitelisted host bridge\n",
				 pci_name(provider));
		map_type = PCI_P2PDMA_MAP_NOT_SUPPORTED;
	}
done:
	rcu_read_lock();
	p2pdma = rcu_dereference(provider->p2pdma);
	if (p2pdma)
		xa_store(&p2pdma->map_types, map_types_idx(client),
			 xa_mk_value(map_type), GFP_KERNEL);
	rcu_read_unlock();
	return map_type;
}

 
int pci_p2pdma_distance_many(struct pci_dev *provider, struct device **clients,
			     int num_clients, bool verbose)
{
	enum pci_p2pdma_map_type map;
	bool not_supported = false;
	struct pci_dev *pci_client;
	int total_dist = 0;
	int i, distance;

	if (num_clients == 0)
		return -1;

	for (i = 0; i < num_clients; i++) {
		pci_client = find_parent_pci_dev(clients[i]);
		if (!pci_client) {
			if (verbose)
				dev_warn(clients[i],
					 "cannot be used for peer-to-peer DMA as it is not a PCI device\n");
			return -1;
		}

		map = calc_map_type_and_dist(provider, pci_client, &distance,
					     verbose);

		pci_dev_put(pci_client);

		if (map == PCI_P2PDMA_MAP_NOT_SUPPORTED)
			not_supported = true;

		if (not_supported && !verbose)
			break;

		total_dist += distance;
	}

	if (not_supported)
		return -1;

	return total_dist;
}
EXPORT_SYMBOL_GPL(pci_p2pdma_distance_many);

 
bool pci_has_p2pmem(struct pci_dev *pdev)
{
	struct pci_p2pdma *p2pdma;
	bool res;

	rcu_read_lock();
	p2pdma = rcu_dereference(pdev->p2pdma);
	res = p2pdma && p2pdma->p2pmem_published;
	rcu_read_unlock();

	return res;
}
EXPORT_SYMBOL_GPL(pci_has_p2pmem);

 
struct pci_dev *pci_p2pmem_find_many(struct device **clients, int num_clients)
{
	struct pci_dev *pdev = NULL;
	int distance;
	int closest_distance = INT_MAX;
	struct pci_dev **closest_pdevs;
	int dev_cnt = 0;
	const int max_devs = PAGE_SIZE / sizeof(*closest_pdevs);
	int i;

	closest_pdevs = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!closest_pdevs)
		return NULL;

	for_each_pci_dev(pdev) {
		if (!pci_has_p2pmem(pdev))
			continue;

		distance = pci_p2pdma_distance_many(pdev, clients,
						    num_clients, false);
		if (distance < 0 || distance > closest_distance)
			continue;

		if (distance == closest_distance && dev_cnt >= max_devs)
			continue;

		if (distance < closest_distance) {
			for (i = 0; i < dev_cnt; i++)
				pci_dev_put(closest_pdevs[i]);

			dev_cnt = 0;
			closest_distance = distance;
		}

		closest_pdevs[dev_cnt++] = pci_dev_get(pdev);
	}

	if (dev_cnt)
		pdev = pci_dev_get(closest_pdevs[get_random_u32_below(dev_cnt)]);

	for (i = 0; i < dev_cnt; i++)
		pci_dev_put(closest_pdevs[i]);

	kfree(closest_pdevs);
	return pdev;
}
EXPORT_SYMBOL_GPL(pci_p2pmem_find_many);

 
void *pci_alloc_p2pmem(struct pci_dev *pdev, size_t size)
{
	void *ret = NULL;
	struct percpu_ref *ref;
	struct pci_p2pdma *p2pdma;

	 
	rcu_read_lock();
	p2pdma = rcu_dereference(pdev->p2pdma);
	if (unlikely(!p2pdma))
		goto out;

	ret = (void *)gen_pool_alloc_owner(p2pdma->pool, size, (void **) &ref);
	if (!ret)
		goto out;

	if (unlikely(!percpu_ref_tryget_live_rcu(ref))) {
		gen_pool_free(p2pdma->pool, (unsigned long) ret, size);
		ret = NULL;
		goto out;
	}
out:
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(pci_alloc_p2pmem);

 
void pci_free_p2pmem(struct pci_dev *pdev, void *addr, size_t size)
{
	struct percpu_ref *ref;
	struct pci_p2pdma *p2pdma = rcu_dereference_protected(pdev->p2pdma, 1);

	gen_pool_free_owner(p2pdma->pool, (uintptr_t)addr, size,
			(void **) &ref);
	percpu_ref_put(ref);
}
EXPORT_SYMBOL_GPL(pci_free_p2pmem);

 
pci_bus_addr_t pci_p2pmem_virt_to_bus(struct pci_dev *pdev, void *addr)
{
	struct pci_p2pdma *p2pdma;

	if (!addr)
		return 0;

	p2pdma = rcu_dereference_protected(pdev->p2pdma, 1);
	if (!p2pdma)
		return 0;

	 
	return gen_pool_virt_to_phys(p2pdma->pool, (unsigned long)addr);
}
EXPORT_SYMBOL_GPL(pci_p2pmem_virt_to_bus);

 
struct scatterlist *pci_p2pmem_alloc_sgl(struct pci_dev *pdev,
					 unsigned int *nents, u32 length)
{
	struct scatterlist *sg;
	void *addr;

	sg = kmalloc(sizeof(*sg), GFP_KERNEL);
	if (!sg)
		return NULL;

	sg_init_table(sg, 1);

	addr = pci_alloc_p2pmem(pdev, length);
	if (!addr)
		goto out_free_sg;

	sg_set_buf(sg, addr, length);
	*nents = 1;
	return sg;

out_free_sg:
	kfree(sg);
	return NULL;
}
EXPORT_SYMBOL_GPL(pci_p2pmem_alloc_sgl);

 
void pci_p2pmem_free_sgl(struct pci_dev *pdev, struct scatterlist *sgl)
{
	struct scatterlist *sg;
	int count;

	for_each_sg(sgl, sg, INT_MAX, count) {
		if (!sg)
			break;

		pci_free_p2pmem(pdev, sg_virt(sg), sg->length);
	}
	kfree(sgl);
}
EXPORT_SYMBOL_GPL(pci_p2pmem_free_sgl);

 
void pci_p2pmem_publish(struct pci_dev *pdev, bool publish)
{
	struct pci_p2pdma *p2pdma;

	rcu_read_lock();
	p2pdma = rcu_dereference(pdev->p2pdma);
	if (p2pdma)
		p2pdma->p2pmem_published = publish;
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(pci_p2pmem_publish);

static enum pci_p2pdma_map_type pci_p2pdma_map_type(struct dev_pagemap *pgmap,
						    struct device *dev)
{
	enum pci_p2pdma_map_type type = PCI_P2PDMA_MAP_NOT_SUPPORTED;
	struct pci_dev *provider = to_p2p_pgmap(pgmap)->provider;
	struct pci_dev *client;
	struct pci_p2pdma *p2pdma;
	int dist;

	if (!provider->p2pdma)
		return PCI_P2PDMA_MAP_NOT_SUPPORTED;

	if (!dev_is_pci(dev))
		return PCI_P2PDMA_MAP_NOT_SUPPORTED;

	client = to_pci_dev(dev);

	rcu_read_lock();
	p2pdma = rcu_dereference(provider->p2pdma);

	if (p2pdma)
		type = xa_to_value(xa_load(&p2pdma->map_types,
					   map_types_idx(client)));
	rcu_read_unlock();

	if (type == PCI_P2PDMA_MAP_UNKNOWN)
		return calc_map_type_and_dist(provider, client, &dist, true);

	return type;
}

 
enum pci_p2pdma_map_type
pci_p2pdma_map_segment(struct pci_p2pdma_map_state *state, struct device *dev,
		       struct scatterlist *sg)
{
	if (state->pgmap != sg_page(sg)->pgmap) {
		state->pgmap = sg_page(sg)->pgmap;
		state->map = pci_p2pdma_map_type(state->pgmap, dev);
		state->bus_off = to_p2p_pgmap(state->pgmap)->bus_offset;
	}

	if (state->map == PCI_P2PDMA_MAP_BUS_ADDR) {
		sg->dma_address = sg_phys(sg) + state->bus_off;
		sg_dma_len(sg) = sg->length;
		sg_dma_mark_bus_address(sg);
	}

	return state->map;
}

 
int pci_p2pdma_enable_store(const char *page, struct pci_dev **p2p_dev,
			    bool *use_p2pdma)
{
	struct device *dev;

	dev = bus_find_device_by_name(&pci_bus_type, NULL, page);
	if (dev) {
		*use_p2pdma = true;
		*p2p_dev = to_pci_dev(dev);

		if (!pci_has_p2pmem(*p2p_dev)) {
			pci_err(*p2p_dev,
				"PCI device has no peer-to-peer memory: %s\n",
				page);
			pci_dev_put(*p2p_dev);
			return -ENODEV;
		}

		return 0;
	} else if ((page[0] == '0' || page[0] == '1') && !iscntrl(page[1])) {
		 
	} else if (!kstrtobool(page, use_p2pdma)) {
		return 0;
	}

	pr_err("No such PCI device: %.*s\n", (int)strcspn(page, "\n"), page);
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(pci_p2pdma_enable_store);

 
ssize_t pci_p2pdma_enable_show(char *page, struct pci_dev *p2p_dev,
			       bool use_p2pdma)
{
	if (!use_p2pdma)
		return sprintf(page, "0\n");

	if (!p2p_dev)
		return sprintf(page, "1\n");

	return sprintf(page, "%s\n", pci_name(p2p_dev));
}
EXPORT_SYMBOL_GPL(pci_p2pdma_enable_show);
