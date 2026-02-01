
 

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/panic_notifier.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/rculist.h>
#include <linux/remoteproc.h>
#include <linux/iommu.h>
#include <linux/idr.h>
#include <linux/elf.h>
#include <linux/crc32.h>
#include <linux/of_reserved_mem.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <asm/byteorder.h>
#include <linux/platform_device.h>

#include "remoteproc_internal.h"

#define HIGH_BITS_MASK 0xFFFFFFFF00000000ULL

static DEFINE_MUTEX(rproc_list_mutex);
static LIST_HEAD(rproc_list);
static struct notifier_block rproc_panic_nb;

typedef int (*rproc_handle_resource_t)(struct rproc *rproc,
				 void *, int offset, int avail);

static int rproc_alloc_carveout(struct rproc *rproc,
				struct rproc_mem_entry *mem);
static int rproc_release_carveout(struct rproc *rproc,
				  struct rproc_mem_entry *mem);

 
static DEFINE_IDA(rproc_dev_index);
static struct workqueue_struct *rproc_recovery_wq;

static const char * const rproc_crash_names[] = {
	[RPROC_MMUFAULT]	= "mmufault",
	[RPROC_WATCHDOG]	= "watchdog",
	[RPROC_FATAL_ERROR]	= "fatal error",
};

 
static const char *rproc_crash_to_string(enum rproc_crash_type type)
{
	if (type < ARRAY_SIZE(rproc_crash_names))
		return rproc_crash_names[type];
	return "unknown";
}

 
static int rproc_iommu_fault(struct iommu_domain *domain, struct device *dev,
			     unsigned long iova, int flags, void *token)
{
	struct rproc *rproc = token;

	dev_err(dev, "iommu fault: da 0x%lx flags 0x%x\n", iova, flags);

	rproc_report_crash(rproc, RPROC_MMUFAULT);

	 
	return -ENOSYS;
}

static int rproc_enable_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain;
	struct device *dev = rproc->dev.parent;
	int ret;

	if (!rproc->has_iommu) {
		dev_dbg(dev, "iommu not present\n");
		return 0;
	}

	domain = iommu_domain_alloc(dev->bus);
	if (!domain) {
		dev_err(dev, "can't alloc iommu domain\n");
		return -ENOMEM;
	}

	iommu_set_fault_handler(domain, rproc_iommu_fault, rproc);

	ret = iommu_attach_device(domain, dev);
	if (ret) {
		dev_err(dev, "can't attach iommu device: %d\n", ret);
		goto free_domain;
	}

	rproc->domain = domain;

	return 0;

free_domain:
	iommu_domain_free(domain);
	return ret;
}

static void rproc_disable_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain = rproc->domain;
	struct device *dev = rproc->dev.parent;

	if (!domain)
		return;

	iommu_detach_device(domain, dev);
	iommu_domain_free(domain);
}

phys_addr_t rproc_va_to_pa(void *cpu_addr)
{
	 
	if (is_vmalloc_addr(cpu_addr)) {
		return page_to_phys(vmalloc_to_page(cpu_addr)) +
				    offset_in_page(cpu_addr);
	}

	WARN_ON(!virt_addr_valid(cpu_addr));
	return virt_to_phys(cpu_addr);
}
EXPORT_SYMBOL(rproc_va_to_pa);

 
void *rproc_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct rproc_mem_entry *carveout;
	void *ptr = NULL;

	if (rproc->ops->da_to_va) {
		ptr = rproc->ops->da_to_va(rproc, da, len, is_iomem);
		if (ptr)
			goto out;
	}

	list_for_each_entry(carveout, &rproc->carveouts, node) {
		int offset = da - carveout->da;

		 
		if (!carveout->va)
			continue;

		 
		if (offset < 0)
			continue;

		 
		if (offset + len > carveout->len)
			continue;

		ptr = carveout->va + offset;

		if (is_iomem)
			*is_iomem = carveout->is_iomem;

		break;
	}

out:
	return ptr;
}
EXPORT_SYMBOL(rproc_da_to_va);

 
__printf(2, 3)
struct rproc_mem_entry *
rproc_find_carveout_by_name(struct rproc *rproc, const char *name, ...)
{
	va_list args;
	char _name[32];
	struct rproc_mem_entry *carveout, *mem = NULL;

	if (!name)
		return NULL;

	va_start(args, name);
	vsnprintf(_name, sizeof(_name), name, args);
	va_end(args);

	list_for_each_entry(carveout, &rproc->carveouts, node) {
		 
		if (!strcmp(carveout->name, _name)) {
			mem = carveout;
			break;
		}
	}

	return mem;
}

 
static int rproc_check_carveout_da(struct rproc *rproc,
				   struct rproc_mem_entry *mem, u32 da, u32 len)
{
	struct device *dev = &rproc->dev;
	int delta;

	 
	if (len > mem->len) {
		dev_err(dev, "Registered carveout doesn't fit len request\n");
		return -EINVAL;
	}

	if (da != FW_RSC_ADDR_ANY && mem->da == FW_RSC_ADDR_ANY) {
		 
		return -EINVAL;
	} else if (da != FW_RSC_ADDR_ANY && mem->da != FW_RSC_ADDR_ANY) {
		delta = da - mem->da;

		 
		if (delta < 0) {
			dev_err(dev,
				"Registered carveout doesn't fit da request\n");
			return -EINVAL;
		}

		if (delta + len > mem->len) {
			dev_err(dev,
				"Registered carveout doesn't fit len request\n");
			return -EINVAL;
		}
	}

	return 0;
}

int rproc_alloc_vring(struct rproc_vdev *rvdev, int i)
{
	struct rproc *rproc = rvdev->rproc;
	struct device *dev = &rproc->dev;
	struct rproc_vring *rvring = &rvdev->vring[i];
	struct fw_rsc_vdev *rsc;
	int ret, notifyid;
	struct rproc_mem_entry *mem;
	size_t size;

	 
	size = PAGE_ALIGN(vring_size(rvring->num, rvring->align));

	rsc = (void *)rproc->table_ptr + rvdev->rsc_offset;

	 
	mem = rproc_find_carveout_by_name(rproc, "vdev%dvring%d", rvdev->index,
					  i);
	if (mem) {
		if (rproc_check_carveout_da(rproc, mem, rsc->vring[i].da, size))
			return -ENOMEM;
	} else {
		 
		mem = rproc_mem_entry_init(dev, NULL, 0,
					   size, rsc->vring[i].da,
					   rproc_alloc_carveout,
					   rproc_release_carveout,
					   "vdev%dvring%d",
					   rvdev->index, i);
		if (!mem) {
			dev_err(dev, "Can't allocate memory entry structure\n");
			return -ENOMEM;
		}

		rproc_add_carveout(rproc, mem);
	}

	 
	ret = idr_alloc(&rproc->notifyids, rvring, 0, 0, GFP_KERNEL);
	if (ret < 0) {
		dev_err(dev, "idr_alloc failed: %d\n", ret);
		return ret;
	}
	notifyid = ret;

	 
	if (notifyid > rproc->max_notifyid)
		rproc->max_notifyid = notifyid;

	rvring->notifyid = notifyid;

	 
	rsc->vring[i].notifyid = notifyid;
	return 0;
}

int
rproc_parse_vring(struct rproc_vdev *rvdev, struct fw_rsc_vdev *rsc, int i)
{
	struct rproc *rproc = rvdev->rproc;
	struct device *dev = &rproc->dev;
	struct fw_rsc_vdev_vring *vring = &rsc->vring[i];
	struct rproc_vring *rvring = &rvdev->vring[i];

	dev_dbg(dev, "vdev rsc: vring%d: da 0x%x, qsz %d, align %d\n",
		i, vring->da, vring->num, vring->align);

	 
	if (!vring->num || !vring->align) {
		dev_err(dev, "invalid qsz (%d) or alignment (%d)\n",
			vring->num, vring->align);
		return -EINVAL;
	}

	rvring->num = vring->num;
	rvring->align = vring->align;
	rvring->rvdev = rvdev;

	return 0;
}

void rproc_free_vring(struct rproc_vring *rvring)
{
	struct rproc *rproc = rvring->rvdev->rproc;
	int idx = rvring - rvring->rvdev->vring;
	struct fw_rsc_vdev *rsc;

	idr_remove(&rproc->notifyids, rvring->notifyid);

	 
	if (rproc->table_ptr) {
		rsc = (void *)rproc->table_ptr + rvring->rvdev->rsc_offset;
		rsc->vring[idx].da = 0;
		rsc->vring[idx].notifyid = -1;
	}
}

void rproc_add_rvdev(struct rproc *rproc, struct rproc_vdev *rvdev)
{
	if (rvdev && rproc)
		list_add_tail(&rvdev->node, &rproc->rvdevs);
}

void rproc_remove_rvdev(struct rproc_vdev *rvdev)
{
	if (rvdev)
		list_del(&rvdev->node);
}
 
static int rproc_handle_vdev(struct rproc *rproc, void *ptr,
			     int offset, int avail)
{
	struct fw_rsc_vdev *rsc = ptr;
	struct device *dev = &rproc->dev;
	struct rproc_vdev *rvdev;
	size_t rsc_size;
	struct rproc_vdev_data rvdev_data;
	struct platform_device *pdev;

	 
	rsc_size = struct_size(rsc, vring, rsc->num_of_vrings);
	if (size_add(rsc_size, rsc->config_len) > avail) {
		dev_err(dev, "vdev rsc is truncated\n");
		return -EINVAL;
	}

	 
	if (rsc->reserved[0] || rsc->reserved[1]) {
		dev_err(dev, "vdev rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	dev_dbg(dev, "vdev rsc: id %d, dfeatures 0x%x, cfg len %d, %d vrings\n",
		rsc->id, rsc->dfeatures, rsc->config_len, rsc->num_of_vrings);

	 
	if (rsc->num_of_vrings > ARRAY_SIZE(rvdev->vring)) {
		dev_err(dev, "too many vrings: %d\n", rsc->num_of_vrings);
		return -EINVAL;
	}

	rvdev_data.id = rsc->id;
	rvdev_data.index = rproc->nb_vdev++;
	rvdev_data.rsc_offset = offset;
	rvdev_data.rsc = rsc;

	 
	pdev = platform_device_register_data(dev, "rproc-virtio", PLATFORM_DEVID_AUTO, &rvdev_data,
					     sizeof(rvdev_data));
	if (IS_ERR(pdev)) {
		dev_err(dev, "failed to create rproc-virtio device\n");
		return PTR_ERR(pdev);
	}

	return 0;
}

 
static int rproc_handle_trace(struct rproc *rproc, void *ptr,
			      int offset, int avail)
{
	struct fw_rsc_trace *rsc = ptr;
	struct rproc_debug_trace *trace;
	struct device *dev = &rproc->dev;
	char name[15];

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "trace rsc is truncated\n");
		return -EINVAL;
	}

	 
	if (rsc->reserved) {
		dev_err(dev, "trace rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	trace = kzalloc(sizeof(*trace), GFP_KERNEL);
	if (!trace)
		return -ENOMEM;

	 
	trace->trace_mem.len = rsc->len;
	trace->trace_mem.da = rsc->da;

	 
	trace->rproc = rproc;

	 
	snprintf(name, sizeof(name), "trace%d", rproc->num_traces);

	 
	trace->tfile = rproc_create_trace_file(name, rproc, trace);

	list_add_tail(&trace->node, &rproc->traces);

	rproc->num_traces++;

	dev_dbg(dev, "%s added: da 0x%x, len 0x%x\n",
		name, rsc->da, rsc->len);

	return 0;
}

 
static int rproc_handle_devmem(struct rproc *rproc, void *ptr,
			       int offset, int avail)
{
	struct fw_rsc_devmem *rsc = ptr;
	struct rproc_mem_entry *mapping;
	struct device *dev = &rproc->dev;
	int ret;

	 
	if (!rproc->domain)
		return -EINVAL;

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "devmem rsc is truncated\n");
		return -EINVAL;
	}

	 
	if (rsc->reserved) {
		dev_err(dev, "devmem rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	ret = iommu_map(rproc->domain, rsc->da, rsc->pa, rsc->len, rsc->flags,
			GFP_KERNEL);
	if (ret) {
		dev_err(dev, "failed to map devmem: %d\n", ret);
		goto out;
	}

	 
	mapping->da = rsc->da;
	mapping->len = rsc->len;
	list_add_tail(&mapping->node, &rproc->mappings);

	dev_dbg(dev, "mapped devmem pa 0x%x, da 0x%x, len 0x%x\n",
		rsc->pa, rsc->da, rsc->len);

	return 0;

out:
	kfree(mapping);
	return ret;
}

 
static int rproc_alloc_carveout(struct rproc *rproc,
				struct rproc_mem_entry *mem)
{
	struct rproc_mem_entry *mapping = NULL;
	struct device *dev = &rproc->dev;
	dma_addr_t dma;
	void *va;
	int ret;

	va = dma_alloc_coherent(dev->parent, mem->len, &dma, GFP_KERNEL);
	if (!va) {
		dev_err(dev->parent,
			"failed to allocate dma memory: len 0x%zx\n",
			mem->len);
		return -ENOMEM;
	}

	dev_dbg(dev, "carveout va %pK, dma %pad, len 0x%zx\n",
		va, &dma, mem->len);

	if (mem->da != FW_RSC_ADDR_ANY && !rproc->domain) {
		 
		if (mem->da != (u32)dma)
			dev_warn(dev->parent,
				 "Allocated carveout doesn't fit device address request\n");
	}

	 
	if (mem->da != FW_RSC_ADDR_ANY && rproc->domain) {
		mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
		if (!mapping) {
			ret = -ENOMEM;
			goto dma_free;
		}

		ret = iommu_map(rproc->domain, mem->da, dma, mem->len,
				mem->flags, GFP_KERNEL);
		if (ret) {
			dev_err(dev, "iommu_map failed: %d\n", ret);
			goto free_mapping;
		}

		 
		mapping->da = mem->da;
		mapping->len = mem->len;
		list_add_tail(&mapping->node, &rproc->mappings);

		dev_dbg(dev, "carveout mapped 0x%x to %pad\n",
			mem->da, &dma);
	}

	if (mem->da == FW_RSC_ADDR_ANY) {
		 
		if ((u64)dma & HIGH_BITS_MASK)
			dev_warn(dev, "DMA address cast in 32bit to fit resource table format\n");

		mem->da = (u32)dma;
	}

	mem->dma = dma;
	mem->va = va;

	return 0;

free_mapping:
	kfree(mapping);
dma_free:
	dma_free_coherent(dev->parent, mem->len, va, dma);
	return ret;
}

 
static int rproc_release_carveout(struct rproc *rproc,
				  struct rproc_mem_entry *mem)
{
	struct device *dev = &rproc->dev;

	 
	dma_free_coherent(dev->parent, mem->len, mem->va, mem->dma);
	return 0;
}

 
static int rproc_handle_carveout(struct rproc *rproc,
				 void *ptr, int offset, int avail)
{
	struct fw_rsc_carveout *rsc = ptr;
	struct rproc_mem_entry *carveout;
	struct device *dev = &rproc->dev;

	if (sizeof(*rsc) > avail) {
		dev_err(dev, "carveout rsc is truncated\n");
		return -EINVAL;
	}

	 
	if (rsc->reserved) {
		dev_err(dev, "carveout rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	dev_dbg(dev, "carveout rsc: name: %s, da 0x%x, pa 0x%x, len 0x%x, flags 0x%x\n",
		rsc->name, rsc->da, rsc->pa, rsc->len, rsc->flags);

	 
	carveout = rproc_find_carveout_by_name(rproc, rsc->name);

	if (carveout) {
		if (carveout->rsc_offset != FW_RSC_ADDR_ANY) {
			dev_err(dev,
				"Carveout already associated to resource table\n");
			return -ENOMEM;
		}

		if (rproc_check_carveout_da(rproc, carveout, rsc->da, rsc->len))
			return -ENOMEM;

		 
		carveout->rsc_offset = offset;
		carveout->flags = rsc->flags;

		return 0;
	}

	 
	carveout = rproc_mem_entry_init(dev, NULL, 0, rsc->len, rsc->da,
					rproc_alloc_carveout,
					rproc_release_carveout, rsc->name);
	if (!carveout) {
		dev_err(dev, "Can't allocate memory entry structure\n");
		return -ENOMEM;
	}

	carveout->flags = rsc->flags;
	carveout->rsc_offset = offset;
	rproc_add_carveout(rproc, carveout);

	return 0;
}

 
void rproc_add_carveout(struct rproc *rproc, struct rproc_mem_entry *mem)
{
	list_add_tail(&mem->node, &rproc->carveouts);
}
EXPORT_SYMBOL(rproc_add_carveout);

 
__printf(8, 9)
struct rproc_mem_entry *
rproc_mem_entry_init(struct device *dev,
		     void *va, dma_addr_t dma, size_t len, u32 da,
		     int (*alloc)(struct rproc *, struct rproc_mem_entry *),
		     int (*release)(struct rproc *, struct rproc_mem_entry *),
		     const char *name, ...)
{
	struct rproc_mem_entry *mem;
	va_list args;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return mem;

	mem->va = va;
	mem->dma = dma;
	mem->da = da;
	mem->len = len;
	mem->alloc = alloc;
	mem->release = release;
	mem->rsc_offset = FW_RSC_ADDR_ANY;
	mem->of_resm_idx = -1;

	va_start(args, name);
	vsnprintf(mem->name, sizeof(mem->name), name, args);
	va_end(args);

	return mem;
}
EXPORT_SYMBOL(rproc_mem_entry_init);

 
__printf(5, 6)
struct rproc_mem_entry *
rproc_of_resm_mem_entry_init(struct device *dev, u32 of_resm_idx, size_t len,
			     u32 da, const char *name, ...)
{
	struct rproc_mem_entry *mem;
	va_list args;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return mem;

	mem->da = da;
	mem->len = len;
	mem->rsc_offset = FW_RSC_ADDR_ANY;
	mem->of_resm_idx = of_resm_idx;

	va_start(args, name);
	vsnprintf(mem->name, sizeof(mem->name), name, args);
	va_end(args);

	return mem;
}
EXPORT_SYMBOL(rproc_of_resm_mem_entry_init);

 
int rproc_of_parse_firmware(struct device *dev, int index, const char **fw_name)
{
	int ret;

	ret = of_property_read_string_index(dev->of_node, "firmware-name",
					    index, fw_name);
	return ret ? ret : 0;
}
EXPORT_SYMBOL(rproc_of_parse_firmware);

 
static rproc_handle_resource_t rproc_loading_handlers[RSC_LAST] = {
	[RSC_CARVEOUT] = rproc_handle_carveout,
	[RSC_DEVMEM] = rproc_handle_devmem,
	[RSC_TRACE] = rproc_handle_trace,
	[RSC_VDEV] = rproc_handle_vdev,
};

 
static int rproc_handle_resources(struct rproc *rproc,
				  rproc_handle_resource_t handlers[RSC_LAST])
{
	struct device *dev = &rproc->dev;
	rproc_handle_resource_t handler;
	int ret = 0, i;

	if (!rproc->table_ptr)
		return 0;

	for (i = 0; i < rproc->table_ptr->num; i++) {
		int offset = rproc->table_ptr->offset[i];
		struct fw_rsc_hdr *hdr = (void *)rproc->table_ptr + offset;
		int avail = rproc->table_sz - offset - sizeof(*hdr);
		void *rsc = (void *)hdr + sizeof(*hdr);

		 
		if (avail < 0) {
			dev_err(dev, "rsc table is truncated\n");
			return -EINVAL;
		}

		dev_dbg(dev, "rsc: type %d\n", hdr->type);

		if (hdr->type >= RSC_VENDOR_START &&
		    hdr->type <= RSC_VENDOR_END) {
			ret = rproc_handle_rsc(rproc, hdr->type, rsc,
					       offset + sizeof(*hdr), avail);
			if (ret == RSC_HANDLED)
				continue;
			else if (ret < 0)
				break;

			dev_warn(dev, "unsupported vendor resource %d\n",
				 hdr->type);
			continue;
		}

		if (hdr->type >= RSC_LAST) {
			dev_warn(dev, "unsupported resource %d\n", hdr->type);
			continue;
		}

		handler = handlers[hdr->type];
		if (!handler)
			continue;

		ret = handler(rproc, rsc, offset + sizeof(*hdr), avail);
		if (ret)
			break;
	}

	return ret;
}

static int rproc_prepare_subdevices(struct rproc *rproc)
{
	struct rproc_subdev *subdev;
	int ret;

	list_for_each_entry(subdev, &rproc->subdevs, node) {
		if (subdev->prepare) {
			ret = subdev->prepare(subdev);
			if (ret)
				goto unroll_preparation;
		}
	}

	return 0;

unroll_preparation:
	list_for_each_entry_continue_reverse(subdev, &rproc->subdevs, node) {
		if (subdev->unprepare)
			subdev->unprepare(subdev);
	}

	return ret;
}

static int rproc_start_subdevices(struct rproc *rproc)
{
	struct rproc_subdev *subdev;
	int ret;

	list_for_each_entry(subdev, &rproc->subdevs, node) {
		if (subdev->start) {
			ret = subdev->start(subdev);
			if (ret)
				goto unroll_registration;
		}
	}

	return 0;

unroll_registration:
	list_for_each_entry_continue_reverse(subdev, &rproc->subdevs, node) {
		if (subdev->stop)
			subdev->stop(subdev, true);
	}

	return ret;
}

static void rproc_stop_subdevices(struct rproc *rproc, bool crashed)
{
	struct rproc_subdev *subdev;

	list_for_each_entry_reverse(subdev, &rproc->subdevs, node) {
		if (subdev->stop)
			subdev->stop(subdev, crashed);
	}
}

static void rproc_unprepare_subdevices(struct rproc *rproc)
{
	struct rproc_subdev *subdev;

	list_for_each_entry_reverse(subdev, &rproc->subdevs, node) {
		if (subdev->unprepare)
			subdev->unprepare(subdev);
	}
}

 
static int rproc_alloc_registered_carveouts(struct rproc *rproc)
{
	struct rproc_mem_entry *entry, *tmp;
	struct fw_rsc_carveout *rsc;
	struct device *dev = &rproc->dev;
	u64 pa;
	int ret;

	list_for_each_entry_safe(entry, tmp, &rproc->carveouts, node) {
		if (entry->alloc) {
			ret = entry->alloc(rproc, entry);
			if (ret) {
				dev_err(dev, "Unable to allocate carveout %s: %d\n",
					entry->name, ret);
				return -ENOMEM;
			}
		}

		if (entry->rsc_offset != FW_RSC_ADDR_ANY) {
			 
			rsc = (void *)rproc->table_ptr + entry->rsc_offset;

			 

			 
			if (entry->va)
				pa = (u64)rproc_va_to_pa(entry->va);
			else
				pa = (u64)entry->dma;

			if (((u64)pa) & HIGH_BITS_MASK)
				dev_warn(dev,
					 "Physical address cast in 32bit to fit resource table format\n");

			rsc->pa = (u32)pa;
			rsc->da = entry->da;
			rsc->len = entry->len;
		}
	}

	return 0;
}


 
void rproc_resource_cleanup(struct rproc *rproc)
{
	struct rproc_mem_entry *entry, *tmp;
	struct rproc_debug_trace *trace, *ttmp;
	struct rproc_vdev *rvdev, *rvtmp;
	struct device *dev = &rproc->dev;

	 
	list_for_each_entry_safe(trace, ttmp, &rproc->traces, node) {
		rproc_remove_trace_file(trace->tfile);
		rproc->num_traces--;
		list_del(&trace->node);
		kfree(trace);
	}

	 
	list_for_each_entry_safe(entry, tmp, &rproc->mappings, node) {
		size_t unmapped;

		unmapped = iommu_unmap(rproc->domain, entry->da, entry->len);
		if (unmapped != entry->len) {
			 
			dev_err(dev, "failed to unmap %zx/%zu\n", entry->len,
				unmapped);
		}

		list_del(&entry->node);
		kfree(entry);
	}

	 
	list_for_each_entry_safe(entry, tmp, &rproc->carveouts, node) {
		if (entry->release)
			entry->release(rproc, entry);
		list_del(&entry->node);
		kfree(entry);
	}

	 
	list_for_each_entry_safe(rvdev, rvtmp, &rproc->rvdevs, node)
		platform_device_unregister(rvdev->pdev);

	rproc_coredump_cleanup(rproc);
}
EXPORT_SYMBOL(rproc_resource_cleanup);

static int rproc_start(struct rproc *rproc, const struct firmware *fw)
{
	struct resource_table *loaded_table;
	struct device *dev = &rproc->dev;
	int ret;

	 
	ret = rproc_load_segments(rproc, fw);
	if (ret) {
		dev_err(dev, "Failed to load program segments: %d\n", ret);
		return ret;
	}

	 
	loaded_table = rproc_find_loaded_rsc_table(rproc, fw);
	if (loaded_table) {
		memcpy(loaded_table, rproc->cached_table, rproc->table_sz);
		rproc->table_ptr = loaded_table;
	}

	ret = rproc_prepare_subdevices(rproc);
	if (ret) {
		dev_err(dev, "failed to prepare subdevices for %s: %d\n",
			rproc->name, ret);
		goto reset_table_ptr;
	}

	 
	ret = rproc->ops->start(rproc);
	if (ret) {
		dev_err(dev, "can't start rproc %s: %d\n", rproc->name, ret);
		goto unprepare_subdevices;
	}

	 
	ret = rproc_start_subdevices(rproc);
	if (ret) {
		dev_err(dev, "failed to probe subdevices for %s: %d\n",
			rproc->name, ret);
		goto stop_rproc;
	}

	rproc->state = RPROC_RUNNING;

	dev_info(dev, "remote processor %s is now up\n", rproc->name);

	return 0;

stop_rproc:
	rproc->ops->stop(rproc);
unprepare_subdevices:
	rproc_unprepare_subdevices(rproc);
reset_table_ptr:
	rproc->table_ptr = rproc->cached_table;

	return ret;
}

static int __rproc_attach(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret;

	ret = rproc_prepare_subdevices(rproc);
	if (ret) {
		dev_err(dev, "failed to prepare subdevices for %s: %d\n",
			rproc->name, ret);
		goto out;
	}

	 
	ret = rproc_attach_device(rproc);
	if (ret) {
		dev_err(dev, "can't attach to rproc %s: %d\n",
			rproc->name, ret);
		goto unprepare_subdevices;
	}

	 
	ret = rproc_start_subdevices(rproc);
	if (ret) {
		dev_err(dev, "failed to probe subdevices for %s: %d\n",
			rproc->name, ret);
		goto stop_rproc;
	}

	rproc->state = RPROC_ATTACHED;

	dev_info(dev, "remote processor %s is now attached\n", rproc->name);

	return 0;

stop_rproc:
	rproc->ops->stop(rproc);
unprepare_subdevices:
	rproc_unprepare_subdevices(rproc);
out:
	return ret;
}

 
static int rproc_fw_boot(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	const char *name = rproc->firmware;
	int ret;

	ret = rproc_fw_sanity_check(rproc, fw);
	if (ret)
		return ret;

	dev_info(dev, "Booting fw image %s, size %zd\n", name, fw->size);

	 
	ret = rproc_enable_iommu(rproc);
	if (ret) {
		dev_err(dev, "can't enable iommu: %d\n", ret);
		return ret;
	}

	 
	ret = rproc_prepare_device(rproc);
	if (ret) {
		dev_err(dev, "can't prepare rproc %s: %d\n", rproc->name, ret);
		goto disable_iommu;
	}

	rproc->bootaddr = rproc_get_boot_addr(rproc, fw);

	 
	ret = rproc_parse_fw(rproc, fw);
	if (ret)
		goto unprepare_rproc;

	 
	rproc->max_notifyid = -1;

	 
	rproc->nb_vdev = 0;

	 
	ret = rproc_handle_resources(rproc, rproc_loading_handlers);
	if (ret) {
		dev_err(dev, "Failed to process resources: %d\n", ret);
		goto clean_up_resources;
	}

	 
	ret = rproc_alloc_registered_carveouts(rproc);
	if (ret) {
		dev_err(dev, "Failed to allocate associated carveouts: %d\n",
			ret);
		goto clean_up_resources;
	}

	ret = rproc_start(rproc, fw);
	if (ret)
		goto clean_up_resources;

	return 0;

clean_up_resources:
	rproc_resource_cleanup(rproc);
	kfree(rproc->cached_table);
	rproc->cached_table = NULL;
	rproc->table_ptr = NULL;
unprepare_rproc:
	 
	rproc_unprepare_device(rproc);
disable_iommu:
	rproc_disable_iommu(rproc);
	return ret;
}

static int rproc_set_rsc_table(struct rproc *rproc)
{
	struct resource_table *table_ptr;
	struct device *dev = &rproc->dev;
	size_t table_sz;
	int ret;

	table_ptr = rproc_get_loaded_rsc_table(rproc, &table_sz);
	if (!table_ptr) {
		 
		return 0;
	}

	if (IS_ERR(table_ptr)) {
		ret = PTR_ERR(table_ptr);
		dev_err(dev, "can't load resource table: %d\n", ret);
		return ret;
	}

	 
	if (rproc->ops->detach) {
		rproc->clean_table = kmemdup(table_ptr, table_sz, GFP_KERNEL);
		if (!rproc->clean_table)
			return -ENOMEM;
	} else {
		rproc->clean_table = NULL;
	}

	rproc->cached_table = NULL;
	rproc->table_ptr = table_ptr;
	rproc->table_sz = table_sz;

	return 0;
}

static int rproc_reset_rsc_table_on_detach(struct rproc *rproc)
{
	struct resource_table *table_ptr;

	 
	if (!rproc->table_ptr)
		return 0;

	 
	if (WARN_ON(!rproc->clean_table))
		return -EINVAL;

	 
	table_ptr = rproc->table_ptr;

	 
	rproc->cached_table = kmemdup(rproc->table_ptr,
				      rproc->table_sz, GFP_KERNEL);
	if (!rproc->cached_table)
		return -ENOMEM;

	 
	rproc->table_ptr = rproc->cached_table;

	 
	memcpy(table_ptr, rproc->clean_table, rproc->table_sz);

	 
	kfree(rproc->clean_table);

	return 0;
}

static int rproc_reset_rsc_table_on_stop(struct rproc *rproc)
{
	 
	if (!rproc->table_ptr)
		return 0;

	 
	if (rproc->cached_table)
		goto out;

	 
	rproc->cached_table = kmemdup(rproc->table_ptr,
				      rproc->table_sz, GFP_KERNEL);
	if (!rproc->cached_table)
		return -ENOMEM;

	 
	kfree(rproc->clean_table);

out:
	 
	rproc->table_ptr = rproc->cached_table;
	return 0;
}

 
static int rproc_attach(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret;

	 
	ret = rproc_enable_iommu(rproc);
	if (ret) {
		dev_err(dev, "can't enable iommu: %d\n", ret);
		return ret;
	}

	 
	ret = rproc_prepare_device(rproc);
	if (ret) {
		dev_err(dev, "can't prepare rproc %s: %d\n", rproc->name, ret);
		goto disable_iommu;
	}

	ret = rproc_set_rsc_table(rproc);
	if (ret) {
		dev_err(dev, "can't load resource table: %d\n", ret);
		goto unprepare_device;
	}

	 
	rproc->max_notifyid = -1;

	 
	rproc->nb_vdev = 0;

	 
	ret = rproc_handle_resources(rproc, rproc_loading_handlers);
	if (ret) {
		dev_err(dev, "Failed to process resources: %d\n", ret);
		goto unprepare_device;
	}

	 
	ret = rproc_alloc_registered_carveouts(rproc);
	if (ret) {
		dev_err(dev, "Failed to allocate associated carveouts: %d\n",
			ret);
		goto clean_up_resources;
	}

	ret = __rproc_attach(rproc);
	if (ret)
		goto clean_up_resources;

	return 0;

clean_up_resources:
	rproc_resource_cleanup(rproc);
unprepare_device:
	 
	rproc_unprepare_device(rproc);
disable_iommu:
	rproc_disable_iommu(rproc);
	return ret;
}

 
static void rproc_auto_boot_callback(const struct firmware *fw, void *context)
{
	struct rproc *rproc = context;

	rproc_boot(rproc);

	release_firmware(fw);
}

static int rproc_trigger_auto_boot(struct rproc *rproc)
{
	int ret;

	 
	if (rproc->state == RPROC_DETACHED)
		return rproc_boot(rproc);

	 
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT,
				      rproc->firmware, &rproc->dev, GFP_KERNEL,
				      rproc, rproc_auto_boot_callback);
	if (ret < 0)
		dev_err(&rproc->dev, "request_firmware_nowait err: %d\n", ret);

	return ret;
}

static int rproc_stop(struct rproc *rproc, bool crashed)
{
	struct device *dev = &rproc->dev;
	int ret;

	 
	if (!rproc->ops->stop)
		return -EINVAL;

	 
	rproc_stop_subdevices(rproc, crashed);

	 
	ret = rproc_reset_rsc_table_on_stop(rproc);
	if (ret) {
		dev_err(dev, "can't reset resource table: %d\n", ret);
		return ret;
	}


	 
	ret = rproc->ops->stop(rproc);
	if (ret) {
		dev_err(dev, "can't stop rproc: %d\n", ret);
		return ret;
	}

	rproc_unprepare_subdevices(rproc);

	rproc->state = RPROC_OFFLINE;

	dev_info(dev, "stopped remote processor %s\n", rproc->name);

	return 0;
}

 
static int __rproc_detach(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret;

	 
	if (!rproc->ops->detach)
		return -EINVAL;

	 
	rproc_stop_subdevices(rproc, false);

	 
	ret = rproc_reset_rsc_table_on_detach(rproc);
	if (ret) {
		dev_err(dev, "can't reset resource table: %d\n", ret);
		return ret;
	}

	 
	ret = rproc->ops->detach(rproc);
	if (ret) {
		dev_err(dev, "can't detach from rproc: %d\n", ret);
		return ret;
	}

	rproc_unprepare_subdevices(rproc);

	rproc->state = RPROC_DETACHED;

	dev_info(dev, "detached remote processor %s\n", rproc->name);

	return 0;
}

static int rproc_attach_recovery(struct rproc *rproc)
{
	int ret;

	ret = __rproc_detach(rproc);
	if (ret)
		return ret;

	return __rproc_attach(rproc);
}

static int rproc_boot_recovery(struct rproc *rproc)
{
	const struct firmware *firmware_p;
	struct device *dev = &rproc->dev;
	int ret;

	ret = rproc_stop(rproc, true);
	if (ret)
		return ret;

	 
	rproc->ops->coredump(rproc);

	 
	ret = request_firmware(&firmware_p, rproc->firmware, dev);
	if (ret < 0) {
		dev_err(dev, "request_firmware failed: %d\n", ret);
		return ret;
	}

	 
	ret = rproc_start(rproc, firmware_p);

	release_firmware(firmware_p);

	return ret;
}

 
int rproc_trigger_recovery(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret)
		return ret;

	 
	if (rproc->state != RPROC_CRASHED)
		goto unlock_mutex;

	dev_err(dev, "recovering %s\n", rproc->name);

	if (rproc_has_feature(rproc, RPROC_FEAT_ATTACH_ON_RECOVERY))
		ret = rproc_attach_recovery(rproc);
	else
		ret = rproc_boot_recovery(rproc);

unlock_mutex:
	mutex_unlock(&rproc->lock);
	return ret;
}

 
static void rproc_crash_handler_work(struct work_struct *work)
{
	struct rproc *rproc = container_of(work, struct rproc, crash_handler);
	struct device *dev = &rproc->dev;

	dev_dbg(dev, "enter %s\n", __func__);

	mutex_lock(&rproc->lock);

	if (rproc->state == RPROC_CRASHED) {
		 
		mutex_unlock(&rproc->lock);
		return;
	}

	if (rproc->state == RPROC_OFFLINE) {
		 
		mutex_unlock(&rproc->lock);
		goto out;
	}

	rproc->state = RPROC_CRASHED;
	dev_err(dev, "handling crash #%u in %s\n", ++rproc->crash_cnt,
		rproc->name);

	mutex_unlock(&rproc->lock);

	if (!rproc->recovery_disabled)
		rproc_trigger_recovery(rproc);

out:
	pm_relax(rproc->dev.parent);
}

 
int rproc_boot(struct rproc *rproc)
{
	const struct firmware *firmware_p;
	struct device *dev;
	int ret;

	if (!rproc) {
		pr_err("invalid rproc handle\n");
		return -EINVAL;
	}

	dev = &rproc->dev;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return ret;
	}

	if (rproc->state == RPROC_DELETED) {
		ret = -ENODEV;
		dev_err(dev, "can't boot deleted rproc %s\n", rproc->name);
		goto unlock_mutex;
	}

	 
	if (atomic_inc_return(&rproc->power) > 1) {
		ret = 0;
		goto unlock_mutex;
	}

	if (rproc->state == RPROC_DETACHED) {
		dev_info(dev, "attaching to %s\n", rproc->name);

		ret = rproc_attach(rproc);
	} else {
		dev_info(dev, "powering up %s\n", rproc->name);

		 
		ret = request_firmware(&firmware_p, rproc->firmware, dev);
		if (ret < 0) {
			dev_err(dev, "request_firmware failed: %d\n", ret);
			goto downref_rproc;
		}

		ret = rproc_fw_boot(rproc, firmware_p);

		release_firmware(firmware_p);
	}

downref_rproc:
	if (ret)
		atomic_dec(&rproc->power);
unlock_mutex:
	mutex_unlock(&rproc->lock);
	return ret;
}
EXPORT_SYMBOL(rproc_boot);

 
int rproc_shutdown(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret = 0;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return ret;
	}

	if (rproc->state != RPROC_RUNNING &&
	    rproc->state != RPROC_ATTACHED) {
		ret = -EINVAL;
		goto out;
	}

	 
	if (!atomic_dec_and_test(&rproc->power))
		goto out;

	ret = rproc_stop(rproc, false);
	if (ret) {
		atomic_inc(&rproc->power);
		goto out;
	}

	 
	rproc_resource_cleanup(rproc);

	 
	rproc_unprepare_device(rproc);

	rproc_disable_iommu(rproc);

	 
	kfree(rproc->cached_table);
	rproc->cached_table = NULL;
	rproc->table_ptr = NULL;
out:
	mutex_unlock(&rproc->lock);
	return ret;
}
EXPORT_SYMBOL(rproc_shutdown);

 
int rproc_detach(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return ret;
	}

	if (rproc->state != RPROC_ATTACHED) {
		ret = -EINVAL;
		goto out;
	}

	 
	if (!atomic_dec_and_test(&rproc->power)) {
		ret = 0;
		goto out;
	}

	ret = __rproc_detach(rproc);
	if (ret) {
		atomic_inc(&rproc->power);
		goto out;
	}

	 
	rproc_resource_cleanup(rproc);

	 
	rproc_unprepare_device(rproc);

	rproc_disable_iommu(rproc);

	 
	kfree(rproc->cached_table);
	rproc->cached_table = NULL;
	rproc->table_ptr = NULL;
out:
	mutex_unlock(&rproc->lock);
	return ret;
}
EXPORT_SYMBOL(rproc_detach);

 
#ifdef CONFIG_OF
struct rproc *rproc_get_by_phandle(phandle phandle)
{
	struct rproc *rproc = NULL, *r;
	struct device_node *np;

	np = of_find_node_by_phandle(phandle);
	if (!np)
		return NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(r, &rproc_list, node) {
		if (r->dev.parent && device_match_of_node(r->dev.parent, np)) {
			 
			if (!try_module_get(r->dev.parent->driver->owner)) {
				dev_err(&r->dev, "can't get owner\n");
				break;
			}

			rproc = r;
			get_device(&rproc->dev);
			break;
		}
	}
	rcu_read_unlock();

	of_node_put(np);

	return rproc;
}
#else
struct rproc *rproc_get_by_phandle(phandle phandle)
{
	return NULL;
}
#endif
EXPORT_SYMBOL(rproc_get_by_phandle);

 
int rproc_set_firmware(struct rproc *rproc, const char *fw_name)
{
	struct device *dev;
	int ret, len;
	char *p;

	if (!rproc || !fw_name)
		return -EINVAL;

	dev = rproc->dev.parent;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return -EINVAL;
	}

	if (rproc->state != RPROC_OFFLINE) {
		dev_err(dev, "can't change firmware while running\n");
		ret = -EBUSY;
		goto out;
	}

	len = strcspn(fw_name, "\n");
	if (!len) {
		dev_err(dev, "can't provide empty string for firmware name\n");
		ret = -EINVAL;
		goto out;
	}

	p = kstrndup(fw_name, len, GFP_KERNEL);
	if (!p) {
		ret = -ENOMEM;
		goto out;
	}

	kfree_const(rproc->firmware);
	rproc->firmware = p;

out:
	mutex_unlock(&rproc->lock);
	return ret;
}
EXPORT_SYMBOL(rproc_set_firmware);

static int rproc_validate(struct rproc *rproc)
{
	switch (rproc->state) {
	case RPROC_OFFLINE:
		 
		if (!rproc->ops->start)
			return -EINVAL;
		break;
	case RPROC_DETACHED:
		 
		if (!rproc->ops->attach)
			return -EINVAL;
		 
		if (rproc->cached_table)
			return -EINVAL;
		break;
	default:
		 
		return -EINVAL;
	}

	return 0;
}

 
int rproc_add(struct rproc *rproc)
{
	struct device *dev = &rproc->dev;
	int ret;

	ret = rproc_validate(rproc);
	if (ret < 0)
		return ret;

	 
	ret = rproc_char_device_add(rproc);
	if (ret < 0)
		return ret;

	ret = device_add(dev);
	if (ret < 0) {
		put_device(dev);
		goto rproc_remove_cdev;
	}

	dev_info(dev, "%s is available\n", rproc->name);

	 
	rproc_create_debug_dir(rproc);

	 
	if (rproc->auto_boot) {
		ret = rproc_trigger_auto_boot(rproc);
		if (ret < 0)
			goto rproc_remove_dev;
	}

	 
	mutex_lock(&rproc_list_mutex);
	list_add_rcu(&rproc->node, &rproc_list);
	mutex_unlock(&rproc_list_mutex);

	return 0;

rproc_remove_dev:
	rproc_delete_debug_dir(rproc);
	device_del(dev);
rproc_remove_cdev:
	rproc_char_device_remove(rproc);
	return ret;
}
EXPORT_SYMBOL(rproc_add);

static void devm_rproc_remove(void *rproc)
{
	rproc_del(rproc);
}

 
int devm_rproc_add(struct device *dev, struct rproc *rproc)
{
	int err;

	err = rproc_add(rproc);
	if (err)
		return err;

	return devm_add_action_or_reset(dev, devm_rproc_remove, rproc);
}
EXPORT_SYMBOL(devm_rproc_add);

 
static void rproc_type_release(struct device *dev)
{
	struct rproc *rproc = container_of(dev, struct rproc, dev);

	dev_info(&rproc->dev, "releasing %s\n", rproc->name);

	idr_destroy(&rproc->notifyids);

	if (rproc->index >= 0)
		ida_free(&rproc_dev_index, rproc->index);

	kfree_const(rproc->firmware);
	kfree_const(rproc->name);
	kfree(rproc->ops);
	kfree(rproc);
}

static const struct device_type rproc_type = {
	.name		= "remoteproc",
	.release	= rproc_type_release,
};

static int rproc_alloc_firmware(struct rproc *rproc,
				const char *name, const char *firmware)
{
	const char *p;

	 
	if (firmware)
		p = kstrdup_const(firmware, GFP_KERNEL);
	else
		p = kasprintf(GFP_KERNEL, "rproc-%s-fw", name);

	if (!p)
		return -ENOMEM;

	rproc->firmware = p;

	return 0;
}

static int rproc_alloc_ops(struct rproc *rproc, const struct rproc_ops *ops)
{
	rproc->ops = kmemdup(ops, sizeof(*ops), GFP_KERNEL);
	if (!rproc->ops)
		return -ENOMEM;

	 
	if (!rproc->ops->coredump)
		rproc->ops->coredump = rproc_coredump;

	if (rproc->ops->load)
		return 0;

	 
	rproc->ops->load = rproc_elf_load_segments;
	rproc->ops->parse_fw = rproc_elf_load_rsc_table;
	rproc->ops->find_loaded_rsc_table = rproc_elf_find_loaded_rsc_table;
	rproc->ops->sanity_check = rproc_elf_sanity_check;
	rproc->ops->get_boot_addr = rproc_elf_get_boot_addr;

	return 0;
}

 
struct rproc *rproc_alloc(struct device *dev, const char *name,
			  const struct rproc_ops *ops,
			  const char *firmware, int len)
{
	struct rproc *rproc;

	if (!dev || !name || !ops)
		return NULL;

	rproc = kzalloc(sizeof(struct rproc) + len, GFP_KERNEL);
	if (!rproc)
		return NULL;

	rproc->priv = &rproc[1];
	rproc->auto_boot = true;
	rproc->elf_class = ELFCLASSNONE;
	rproc->elf_machine = EM_NONE;

	device_initialize(&rproc->dev);
	rproc->dev.parent = dev;
	rproc->dev.type = &rproc_type;
	rproc->dev.class = &rproc_class;
	rproc->dev.driver_data = rproc;
	idr_init(&rproc->notifyids);

	rproc->name = kstrdup_const(name, GFP_KERNEL);
	if (!rproc->name)
		goto put_device;

	if (rproc_alloc_firmware(rproc, name, firmware))
		goto put_device;

	if (rproc_alloc_ops(rproc, ops))
		goto put_device;

	 
	rproc->index = ida_alloc(&rproc_dev_index, GFP_KERNEL);
	if (rproc->index < 0) {
		dev_err(dev, "ida_alloc failed: %d\n", rproc->index);
		goto put_device;
	}

	dev_set_name(&rproc->dev, "remoteproc%d", rproc->index);

	atomic_set(&rproc->power, 0);

	mutex_init(&rproc->lock);

	INIT_LIST_HEAD(&rproc->carveouts);
	INIT_LIST_HEAD(&rproc->mappings);
	INIT_LIST_HEAD(&rproc->traces);
	INIT_LIST_HEAD(&rproc->rvdevs);
	INIT_LIST_HEAD(&rproc->subdevs);
	INIT_LIST_HEAD(&rproc->dump_segments);

	INIT_WORK(&rproc->crash_handler, rproc_crash_handler_work);

	rproc->state = RPROC_OFFLINE;

	return rproc;

put_device:
	put_device(&rproc->dev);
	return NULL;
}
EXPORT_SYMBOL(rproc_alloc);

 
void rproc_free(struct rproc *rproc)
{
	put_device(&rproc->dev);
}
EXPORT_SYMBOL(rproc_free);

 
void rproc_put(struct rproc *rproc)
{
	module_put(rproc->dev.parent->driver->owner);
	put_device(&rproc->dev);
}
EXPORT_SYMBOL(rproc_put);

 
int rproc_del(struct rproc *rproc)
{
	if (!rproc)
		return -EINVAL;

	 
	rproc_shutdown(rproc);

	mutex_lock(&rproc->lock);
	rproc->state = RPROC_DELETED;
	mutex_unlock(&rproc->lock);

	rproc_delete_debug_dir(rproc);

	 
	mutex_lock(&rproc_list_mutex);
	list_del_rcu(&rproc->node);
	mutex_unlock(&rproc_list_mutex);

	 
	synchronize_rcu();

	device_del(&rproc->dev);
	rproc_char_device_remove(rproc);

	return 0;
}
EXPORT_SYMBOL(rproc_del);

static void devm_rproc_free(struct device *dev, void *res)
{
	rproc_free(*(struct rproc **)res);
}

 
struct rproc *devm_rproc_alloc(struct device *dev, const char *name,
			       const struct rproc_ops *ops,
			       const char *firmware, int len)
{
	struct rproc **ptr, *rproc;

	ptr = devres_alloc(devm_rproc_free, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	rproc = rproc_alloc(dev, name, ops, firmware, len);
	if (rproc) {
		*ptr = rproc;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return rproc;
}
EXPORT_SYMBOL(devm_rproc_alloc);

 
void rproc_add_subdev(struct rproc *rproc, struct rproc_subdev *subdev)
{
	list_add_tail(&subdev->node, &rproc->subdevs);
}
EXPORT_SYMBOL(rproc_add_subdev);

 
void rproc_remove_subdev(struct rproc *rproc, struct rproc_subdev *subdev)
{
	list_del(&subdev->node);
}
EXPORT_SYMBOL(rproc_remove_subdev);

 
struct rproc *rproc_get_by_child(struct device *dev)
{
	for (dev = dev->parent; dev; dev = dev->parent) {
		if (dev->type == &rproc_type)
			return dev->driver_data;
	}

	return NULL;
}
EXPORT_SYMBOL(rproc_get_by_child);

 
void rproc_report_crash(struct rproc *rproc, enum rproc_crash_type type)
{
	if (!rproc) {
		pr_err("NULL rproc pointer\n");
		return;
	}

	 
	pm_stay_awake(rproc->dev.parent);

	dev_err(&rproc->dev, "crash detected in %s: type %s\n",
		rproc->name, rproc_crash_to_string(type));

	queue_work(rproc_recovery_wq, &rproc->crash_handler);
}
EXPORT_SYMBOL(rproc_report_crash);

static int rproc_panic_handler(struct notifier_block *nb, unsigned long event,
			       void *ptr)
{
	unsigned int longest = 0;
	struct rproc *rproc;
	unsigned int d;

	rcu_read_lock();
	list_for_each_entry_rcu(rproc, &rproc_list, node) {
		if (!rproc->ops->panic)
			continue;

		if (rproc->state != RPROC_RUNNING &&
		    rproc->state != RPROC_ATTACHED)
			continue;

		d = rproc->ops->panic(rproc);
		longest = max(longest, d);
	}
	rcu_read_unlock();

	 
	mdelay(longest);

	return NOTIFY_DONE;
}

static void __init rproc_init_panic(void)
{
	rproc_panic_nb.notifier_call = rproc_panic_handler;
	atomic_notifier_chain_register(&panic_notifier_list, &rproc_panic_nb);
}

static void __exit rproc_exit_panic(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &rproc_panic_nb);
}

static int __init remoteproc_init(void)
{
	rproc_recovery_wq = alloc_workqueue("rproc_recovery_wq",
						WQ_UNBOUND | WQ_FREEZABLE, 0);
	if (!rproc_recovery_wq) {
		pr_err("remoteproc: creation of rproc_recovery_wq failed\n");
		return -ENOMEM;
	}

	rproc_init_sysfs();
	rproc_init_debugfs();
	rproc_init_cdev();
	rproc_init_panic();

	return 0;
}
subsys_initcall(remoteproc_init);

static void __exit remoteproc_exit(void)
{
	ida_destroy(&rproc_dev_index);

	if (!rproc_recovery_wq)
		return;

	rproc_exit_panic();
	rproc_exit_debugfs();
	rproc_exit_sysfs();
	destroy_workqueue(rproc_recovery_wq);
}
module_exit(remoteproc_exit);

MODULE_DESCRIPTION("Generic Remote Processor Framework");
