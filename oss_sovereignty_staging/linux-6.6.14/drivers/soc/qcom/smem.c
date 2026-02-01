
 

#include <linux/hwspinlock.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/socinfo.h>

 

 
#define SMEM_MASTER_SBL_VERSION_INDEX	7
#define SMEM_GLOBAL_HEAP_VERSION	11
#define SMEM_GLOBAL_PART_VERSION	12

 
#define SMEM_ITEM_LAST_FIXED	8

 
#define SMEM_ITEM_COUNT		512

 
#define SMEM_HOST_APPS		0

 
#define SMEM_GLOBAL_HOST	0xfffe

 
#define SMEM_HOST_COUNT		20

 
struct smem_proc_comm {
	__le32 command;
	__le32 status;
	__le32 params[2];
};

 
struct smem_global_entry {
	__le32 allocated;
	__le32 offset;
	__le32 size;
	__le32 aux_base;  
};
#define AUX_BASE_MASK		0xfffffffc

 
struct smem_header {
	struct smem_proc_comm proc_comm[4];
	__le32 version[32];
	__le32 initialized;
	__le32 free_offset;
	__le32 available;
	__le32 reserved;
	struct smem_global_entry toc[SMEM_ITEM_COUNT];
};

 
struct smem_ptable_entry {
	__le32 offset;
	__le32 size;
	__le32 flags;
	__le16 host0;
	__le16 host1;
	__le32 cacheline;
	__le32 reserved[7];
};

 
struct smem_ptable {
	u8 magic[4];
	__le32 version;
	__le32 num_entries;
	__le32 reserved[5];
	struct smem_ptable_entry entry[];
};

static const u8 SMEM_PTABLE_MAGIC[] = { 0x24, 0x54, 0x4f, 0x43 };  

 
struct smem_partition_header {
	u8 magic[4];
	__le16 host0;
	__le16 host1;
	__le32 size;
	__le32 offset_free_uncached;
	__le32 offset_free_cached;
	__le32 reserved[3];
};

 
struct smem_partition {
	void __iomem *virt_base;
	phys_addr_t phys_base;
	size_t cacheline;
	size_t size;
};

static const u8 SMEM_PART_MAGIC[] = { 0x24, 0x50, 0x52, 0x54 };

 
struct smem_private_entry {
	u16 canary;  
	__le16 item;
	__le32 size;  
	__le16 padding_data;
	__le16 padding_hdr;
	__le32 reserved;
};
#define SMEM_PRIVATE_CANARY	0xa5a5

 
struct smem_info {
	u8 magic[4];
	__le32 size;
	__le32 base_addr;
	__le32 reserved;
	__le16 num_items;
};

static const u8 SMEM_INFO_MAGIC[] = { 0x53, 0x49, 0x49, 0x49 };  

 
struct smem_region {
	phys_addr_t aux_base;
	void __iomem *virt_base;
	size_t size;
};

 
struct qcom_smem {
	struct device *dev;

	struct hwspinlock *hwlock;

	u32 item_count;
	struct platform_device *socinfo;
	struct smem_ptable *ptable;
	struct smem_partition global_partition;
	struct smem_partition partitions[SMEM_HOST_COUNT];

	unsigned num_regions;
	struct smem_region regions[];
};

static void *
phdr_to_last_uncached_entry(struct smem_partition_header *phdr)
{
	void *p = phdr;

	return p + le32_to_cpu(phdr->offset_free_uncached);
}

static struct smem_private_entry *
phdr_to_first_cached_entry(struct smem_partition_header *phdr,
					size_t cacheline)
{
	void *p = phdr;
	struct smem_private_entry *e;

	return p + le32_to_cpu(phdr->size) - ALIGN(sizeof(*e), cacheline);
}

static void *
phdr_to_last_cached_entry(struct smem_partition_header *phdr)
{
	void *p = phdr;

	return p + le32_to_cpu(phdr->offset_free_cached);
}

static struct smem_private_entry *
phdr_to_first_uncached_entry(struct smem_partition_header *phdr)
{
	void *p = phdr;

	return p + sizeof(*phdr);
}

static struct smem_private_entry *
uncached_entry_next(struct smem_private_entry *e)
{
	void *p = e;

	return p + sizeof(*e) + le16_to_cpu(e->padding_hdr) +
	       le32_to_cpu(e->size);
}

static struct smem_private_entry *
cached_entry_next(struct smem_private_entry *e, size_t cacheline)
{
	void *p = e;

	return p - le32_to_cpu(e->size) - ALIGN(sizeof(*e), cacheline);
}

static void *uncached_entry_to_item(struct smem_private_entry *e)
{
	void *p = e;

	return p + sizeof(*e) + le16_to_cpu(e->padding_hdr);
}

static void *cached_entry_to_item(struct smem_private_entry *e)
{
	void *p = e;

	return p - le32_to_cpu(e->size);
}

 
static struct qcom_smem *__smem;

 
#define HWSPINLOCK_TIMEOUT	1000

 
bool qcom_smem_is_available(void)
{
	return !!__smem;
}
EXPORT_SYMBOL(qcom_smem_is_available);

static int qcom_smem_alloc_private(struct qcom_smem *smem,
				   struct smem_partition *part,
				   unsigned item,
				   size_t size)
{
	struct smem_private_entry *hdr, *end;
	struct smem_partition_header *phdr;
	size_t alloc_size;
	void *cached;
	void *p_end;

	phdr = (struct smem_partition_header __force *)part->virt_base;
	p_end = (void *)phdr + part->size;

	hdr = phdr_to_first_uncached_entry(phdr);
	end = phdr_to_last_uncached_entry(phdr);
	cached = phdr_to_last_cached_entry(phdr);

	if (WARN_ON((void *)end > p_end || cached > p_end))
		return -EINVAL;

	while (hdr < end) {
		if (hdr->canary != SMEM_PRIVATE_CANARY)
			goto bad_canary;
		if (le16_to_cpu(hdr->item) == item)
			return -EEXIST;

		hdr = uncached_entry_next(hdr);
	}

	if (WARN_ON((void *)hdr > p_end))
		return -EINVAL;

	 
	alloc_size = sizeof(*hdr) + ALIGN(size, 8);
	if ((void *)hdr + alloc_size > cached) {
		dev_err(smem->dev, "Out of memory\n");
		return -ENOSPC;
	}

	hdr->canary = SMEM_PRIVATE_CANARY;
	hdr->item = cpu_to_le16(item);
	hdr->size = cpu_to_le32(ALIGN(size, 8));
	hdr->padding_data = cpu_to_le16(le32_to_cpu(hdr->size) - size);
	hdr->padding_hdr = 0;

	 
	wmb();
	le32_add_cpu(&phdr->offset_free_uncached, alloc_size);

	return 0;
bad_canary:
	dev_err(smem->dev, "Found invalid canary in hosts %hu:%hu partition\n",
		le16_to_cpu(phdr->host0), le16_to_cpu(phdr->host1));

	return -EINVAL;
}

static int qcom_smem_alloc_global(struct qcom_smem *smem,
				  unsigned item,
				  size_t size)
{
	struct smem_global_entry *entry;
	struct smem_header *header;

	header = smem->regions[0].virt_base;
	entry = &header->toc[item];
	if (entry->allocated)
		return -EEXIST;

	size = ALIGN(size, 8);
	if (WARN_ON(size > le32_to_cpu(header->available)))
		return -ENOMEM;

	entry->offset = header->free_offset;
	entry->size = cpu_to_le32(size);

	 
	wmb();
	entry->allocated = cpu_to_le32(1);

	le32_add_cpu(&header->free_offset, size);
	le32_add_cpu(&header->available, -size);

	return 0;
}

 
int qcom_smem_alloc(unsigned host, unsigned item, size_t size)
{
	struct smem_partition *part;
	unsigned long flags;
	int ret;

	if (!__smem)
		return -EPROBE_DEFER;

	if (item < SMEM_ITEM_LAST_FIXED) {
		dev_err(__smem->dev,
			"Rejecting allocation of static entry %d\n", item);
		return -EINVAL;
	}

	if (WARN_ON(item >= __smem->item_count))
		return -EINVAL;

	ret = hwspin_lock_timeout_irqsave(__smem->hwlock,
					  HWSPINLOCK_TIMEOUT,
					  &flags);
	if (ret)
		return ret;

	if (host < SMEM_HOST_COUNT && __smem->partitions[host].virt_base) {
		part = &__smem->partitions[host];
		ret = qcom_smem_alloc_private(__smem, part, item, size);
	} else if (__smem->global_partition.virt_base) {
		part = &__smem->global_partition;
		ret = qcom_smem_alloc_private(__smem, part, item, size);
	} else {
		ret = qcom_smem_alloc_global(__smem, item, size);
	}

	hwspin_unlock_irqrestore(__smem->hwlock, &flags);

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_smem_alloc);

static void *qcom_smem_get_global(struct qcom_smem *smem,
				  unsigned item,
				  size_t *size)
{
	struct smem_header *header;
	struct smem_region *region;
	struct smem_global_entry *entry;
	u64 entry_offset;
	u32 e_size;
	u32 aux_base;
	unsigned i;

	header = smem->regions[0].virt_base;
	entry = &header->toc[item];
	if (!entry->allocated)
		return ERR_PTR(-ENXIO);

	aux_base = le32_to_cpu(entry->aux_base) & AUX_BASE_MASK;

	for (i = 0; i < smem->num_regions; i++) {
		region = &smem->regions[i];

		if ((u32)region->aux_base == aux_base || !aux_base) {
			e_size = le32_to_cpu(entry->size);
			entry_offset = le32_to_cpu(entry->offset);

			if (WARN_ON(e_size + entry_offset > region->size))
				return ERR_PTR(-EINVAL);

			if (size != NULL)
				*size = e_size;

			return region->virt_base + entry_offset;
		}
	}

	return ERR_PTR(-ENOENT);
}

static void *qcom_smem_get_private(struct qcom_smem *smem,
				   struct smem_partition *part,
				   unsigned item,
				   size_t *size)
{
	struct smem_private_entry *e, *end;
	struct smem_partition_header *phdr;
	void *item_ptr, *p_end;
	u32 padding_data;
	u32 e_size;

	phdr = (struct smem_partition_header __force *)part->virt_base;
	p_end = (void *)phdr + part->size;

	e = phdr_to_first_uncached_entry(phdr);
	end = phdr_to_last_uncached_entry(phdr);

	while (e < end) {
		if (e->canary != SMEM_PRIVATE_CANARY)
			goto invalid_canary;

		if (le16_to_cpu(e->item) == item) {
			if (size != NULL) {
				e_size = le32_to_cpu(e->size);
				padding_data = le16_to_cpu(e->padding_data);

				if (WARN_ON(e_size > part->size || padding_data > e_size))
					return ERR_PTR(-EINVAL);

				*size = e_size - padding_data;
			}

			item_ptr = uncached_entry_to_item(e);
			if (WARN_ON(item_ptr > p_end))
				return ERR_PTR(-EINVAL);

			return item_ptr;
		}

		e = uncached_entry_next(e);
	}

	if (WARN_ON((void *)e > p_end))
		return ERR_PTR(-EINVAL);

	 

	e = phdr_to_first_cached_entry(phdr, part->cacheline);
	end = phdr_to_last_cached_entry(phdr);

	if (WARN_ON((void *)e < (void *)phdr || (void *)end > p_end))
		return ERR_PTR(-EINVAL);

	while (e > end) {
		if (e->canary != SMEM_PRIVATE_CANARY)
			goto invalid_canary;

		if (le16_to_cpu(e->item) == item) {
			if (size != NULL) {
				e_size = le32_to_cpu(e->size);
				padding_data = le16_to_cpu(e->padding_data);

				if (WARN_ON(e_size > part->size || padding_data > e_size))
					return ERR_PTR(-EINVAL);

				*size = e_size - padding_data;
			}

			item_ptr = cached_entry_to_item(e);
			if (WARN_ON(item_ptr < (void *)phdr))
				return ERR_PTR(-EINVAL);

			return item_ptr;
		}

		e = cached_entry_next(e, part->cacheline);
	}

	if (WARN_ON((void *)e < (void *)phdr))
		return ERR_PTR(-EINVAL);

	return ERR_PTR(-ENOENT);

invalid_canary:
	dev_err(smem->dev, "Found invalid canary in hosts %hu:%hu partition\n",
			le16_to_cpu(phdr->host0), le16_to_cpu(phdr->host1));

	return ERR_PTR(-EINVAL);
}

 
void *qcom_smem_get(unsigned host, unsigned item, size_t *size)
{
	struct smem_partition *part;
	unsigned long flags;
	int ret;
	void *ptr = ERR_PTR(-EPROBE_DEFER);

	if (!__smem)
		return ptr;

	if (WARN_ON(item >= __smem->item_count))
		return ERR_PTR(-EINVAL);

	ret = hwspin_lock_timeout_irqsave(__smem->hwlock,
					  HWSPINLOCK_TIMEOUT,
					  &flags);
	if (ret)
		return ERR_PTR(ret);

	if (host < SMEM_HOST_COUNT && __smem->partitions[host].virt_base) {
		part = &__smem->partitions[host];
		ptr = qcom_smem_get_private(__smem, part, item, size);
	} else if (__smem->global_partition.virt_base) {
		part = &__smem->global_partition;
		ptr = qcom_smem_get_private(__smem, part, item, size);
	} else {
		ptr = qcom_smem_get_global(__smem, item, size);
	}

	hwspin_unlock_irqrestore(__smem->hwlock, &flags);

	return ptr;

}
EXPORT_SYMBOL_GPL(qcom_smem_get);

 
int qcom_smem_get_free_space(unsigned host)
{
	struct smem_partition *part;
	struct smem_partition_header *phdr;
	struct smem_header *header;
	unsigned ret;

	if (!__smem)
		return -EPROBE_DEFER;

	if (host < SMEM_HOST_COUNT && __smem->partitions[host].virt_base) {
		part = &__smem->partitions[host];
		phdr = part->virt_base;
		ret = le32_to_cpu(phdr->offset_free_cached) -
		      le32_to_cpu(phdr->offset_free_uncached);

		if (ret > le32_to_cpu(part->size))
			return -EINVAL;
	} else if (__smem->global_partition.virt_base) {
		part = &__smem->global_partition;
		phdr = part->virt_base;
		ret = le32_to_cpu(phdr->offset_free_cached) -
		      le32_to_cpu(phdr->offset_free_uncached);

		if (ret > le32_to_cpu(part->size))
			return -EINVAL;
	} else {
		header = __smem->regions[0].virt_base;
		ret = le32_to_cpu(header->available);

		if (ret > __smem->regions[0].size)
			return -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_smem_get_free_space);

static bool addr_in_range(void __iomem *base, size_t size, void *addr)
{
	return base && ((void __iomem *)addr >= base && (void __iomem *)addr < base + size);
}

 
phys_addr_t qcom_smem_virt_to_phys(void *p)
{
	struct smem_partition *part;
	struct smem_region *area;
	u64 offset;
	u32 i;

	for (i = 0; i < SMEM_HOST_COUNT; i++) {
		part = &__smem->partitions[i];

		if (addr_in_range(part->virt_base, part->size, p)) {
			offset = p - part->virt_base;

			return (phys_addr_t)part->phys_base + offset;
		}
	}

	part = &__smem->global_partition;

	if (addr_in_range(part->virt_base, part->size, p)) {
		offset = p - part->virt_base;

		return (phys_addr_t)part->phys_base + offset;
	}

	for (i = 0; i < __smem->num_regions; i++) {
		area = &__smem->regions[i];

		if (addr_in_range(area->virt_base, area->size, p)) {
			offset = p - area->virt_base;

			return (phys_addr_t)area->aux_base + offset;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_smem_virt_to_phys);

 
int qcom_smem_get_soc_id(u32 *id)
{
	struct socinfo *info;

	info = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_HW_SW_BUILD_ID, NULL);
	if (IS_ERR(info))
		return PTR_ERR(info);

	*id = __le32_to_cpu(info->id);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_smem_get_soc_id);

static int qcom_smem_get_sbl_version(struct qcom_smem *smem)
{
	struct smem_header *header;
	__le32 *versions;

	header = smem->regions[0].virt_base;
	versions = header->version;

	return le32_to_cpu(versions[SMEM_MASTER_SBL_VERSION_INDEX]);
}

static struct smem_ptable *qcom_smem_get_ptable(struct qcom_smem *smem)
{
	struct smem_ptable *ptable;
	u32 version;

	ptable = smem->ptable;
	if (memcmp(ptable->magic, SMEM_PTABLE_MAGIC, sizeof(ptable->magic)))
		return ERR_PTR(-ENOENT);

	version = le32_to_cpu(ptable->version);
	if (version != 1) {
		dev_err(smem->dev,
			"Unsupported partition header version %d\n", version);
		return ERR_PTR(-EINVAL);
	}
	return ptable;
}

static u32 qcom_smem_get_item_count(struct qcom_smem *smem)
{
	struct smem_ptable *ptable;
	struct smem_info *info;

	ptable = qcom_smem_get_ptable(smem);
	if (IS_ERR_OR_NULL(ptable))
		return SMEM_ITEM_COUNT;

	info = (struct smem_info *)&ptable->entry[ptable->num_entries];
	if (memcmp(info->magic, SMEM_INFO_MAGIC, sizeof(info->magic)))
		return SMEM_ITEM_COUNT;

	return le16_to_cpu(info->num_items);
}

 
static struct smem_partition_header *
qcom_smem_partition_header(struct qcom_smem *smem,
		struct smem_ptable_entry *entry, u16 host0, u16 host1)
{
	struct smem_partition_header *header;
	u32 phys_addr;
	u32 size;

	phys_addr = smem->regions[0].aux_base + le32_to_cpu(entry->offset);
	header = devm_ioremap_wc(smem->dev, phys_addr, le32_to_cpu(entry->size));

	if (!header)
		return NULL;

	if (memcmp(header->magic, SMEM_PART_MAGIC, sizeof(header->magic))) {
		dev_err(smem->dev, "bad partition magic %4ph\n", header->magic);
		return NULL;
	}

	if (host0 != le16_to_cpu(header->host0)) {
		dev_err(smem->dev, "bad host0 (%hu != %hu)\n",
				host0, le16_to_cpu(header->host0));
		return NULL;
	}
	if (host1 != le16_to_cpu(header->host1)) {
		dev_err(smem->dev, "bad host1 (%hu != %hu)\n",
				host1, le16_to_cpu(header->host1));
		return NULL;
	}

	size = le32_to_cpu(header->size);
	if (size != le32_to_cpu(entry->size)) {
		dev_err(smem->dev, "bad partition size (%u != %u)\n",
			size, le32_to_cpu(entry->size));
		return NULL;
	}

	if (le32_to_cpu(header->offset_free_uncached) > size) {
		dev_err(smem->dev, "bad partition free uncached (%u > %u)\n",
			le32_to_cpu(header->offset_free_uncached), size);
		return NULL;
	}

	return header;
}

static int qcom_smem_set_global_partition(struct qcom_smem *smem)
{
	struct smem_partition_header *header;
	struct smem_ptable_entry *entry;
	struct smem_ptable *ptable;
	bool found = false;
	int i;

	if (smem->global_partition.virt_base) {
		dev_err(smem->dev, "Already found the global partition\n");
		return -EINVAL;
	}

	ptable = qcom_smem_get_ptable(smem);
	if (IS_ERR(ptable))
		return PTR_ERR(ptable);

	for (i = 0; i < le32_to_cpu(ptable->num_entries); i++) {
		entry = &ptable->entry[i];
		if (!le32_to_cpu(entry->offset))
			continue;
		if (!le32_to_cpu(entry->size))
			continue;

		if (le16_to_cpu(entry->host0) != SMEM_GLOBAL_HOST)
			continue;

		if (le16_to_cpu(entry->host1) == SMEM_GLOBAL_HOST) {
			found = true;
			break;
		}
	}

	if (!found) {
		dev_err(smem->dev, "Missing entry for global partition\n");
		return -EINVAL;
	}

	header = qcom_smem_partition_header(smem, entry,
				SMEM_GLOBAL_HOST, SMEM_GLOBAL_HOST);
	if (!header)
		return -EINVAL;

	smem->global_partition.virt_base = (void __iomem *)header;
	smem->global_partition.phys_base = smem->regions[0].aux_base +
								le32_to_cpu(entry->offset);
	smem->global_partition.size = le32_to_cpu(entry->size);
	smem->global_partition.cacheline = le32_to_cpu(entry->cacheline);

	return 0;
}

static int
qcom_smem_enumerate_partitions(struct qcom_smem *smem, u16 local_host)
{
	struct smem_partition_header *header;
	struct smem_ptable_entry *entry;
	struct smem_ptable *ptable;
	u16 remote_host;
	u16 host0, host1;
	int i;

	ptable = qcom_smem_get_ptable(smem);
	if (IS_ERR(ptable))
		return PTR_ERR(ptable);

	for (i = 0; i < le32_to_cpu(ptable->num_entries); i++) {
		entry = &ptable->entry[i];
		if (!le32_to_cpu(entry->offset))
			continue;
		if (!le32_to_cpu(entry->size))
			continue;

		host0 = le16_to_cpu(entry->host0);
		host1 = le16_to_cpu(entry->host1);
		if (host0 == local_host)
			remote_host = host1;
		else if (host1 == local_host)
			remote_host = host0;
		else
			continue;

		if (remote_host >= SMEM_HOST_COUNT) {
			dev_err(smem->dev, "bad host %u\n", remote_host);
			return -EINVAL;
		}

		if (smem->partitions[remote_host].virt_base) {
			dev_err(smem->dev, "duplicate host %u\n", remote_host);
			return -EINVAL;
		}

		header = qcom_smem_partition_header(smem, entry, host0, host1);
		if (!header)
			return -EINVAL;

		smem->partitions[remote_host].virt_base = (void __iomem *)header;
		smem->partitions[remote_host].phys_base = smem->regions[0].aux_base +
										le32_to_cpu(entry->offset);
		smem->partitions[remote_host].size = le32_to_cpu(entry->size);
		smem->partitions[remote_host].cacheline = le32_to_cpu(entry->cacheline);
	}

	return 0;
}

static int qcom_smem_map_toc(struct qcom_smem *smem, struct smem_region *region)
{
	u32 ptable_start;

	 
	region->virt_base = devm_ioremap_wc(smem->dev, region->aux_base, SZ_4K);
	ptable_start = region->aux_base + region->size - SZ_4K;
	 
	smem->ptable = devm_ioremap_wc(smem->dev, ptable_start, SZ_4K);

	if (!region->virt_base || !smem->ptable)
		return -ENOMEM;

	return 0;
}

static int qcom_smem_map_global(struct qcom_smem *smem, u32 size)
{
	u32 phys_addr;

	phys_addr = smem->regions[0].aux_base;

	smem->regions[0].size = size;
	smem->regions[0].virt_base = devm_ioremap_wc(smem->dev, phys_addr, size);

	if (!smem->regions[0].virt_base)
		return -ENOMEM;

	return 0;
}

static int qcom_smem_resolve_mem(struct qcom_smem *smem, const char *name,
				 struct smem_region *region)
{
	struct device *dev = smem->dev;
	struct device_node *np;
	struct resource r;
	int ret;

	np = of_parse_phandle(dev->of_node, name, 0);
	if (!np) {
		dev_err(dev, "No %s specified\n", name);
		return -EINVAL;
	}

	ret = of_address_to_resource(np, 0, &r);
	of_node_put(np);
	if (ret)
		return ret;

	region->aux_base = r.start;
	region->size = resource_size(&r);

	return 0;
}

static int qcom_smem_probe(struct platform_device *pdev)
{
	struct smem_header *header;
	struct reserved_mem *rmem;
	struct qcom_smem *smem;
	unsigned long flags;
	int num_regions;
	int hwlock_id;
	u32 version;
	u32 size;
	int ret;
	int i;

	num_regions = 1;
	if (of_property_present(pdev->dev.of_node, "qcom,rpm-msg-ram"))
		num_regions++;

	smem = devm_kzalloc(&pdev->dev, struct_size(smem, regions, num_regions),
			    GFP_KERNEL);
	if (!smem)
		return -ENOMEM;

	smem->dev = &pdev->dev;
	smem->num_regions = num_regions;

	rmem = of_reserved_mem_lookup(pdev->dev.of_node);
	if (rmem) {
		smem->regions[0].aux_base = rmem->base;
		smem->regions[0].size = rmem->size;
	} else {
		 
		ret = qcom_smem_resolve_mem(smem, "memory-region", &smem->regions[0]);
		if (ret)
			return ret;
	}

	if (num_regions > 1) {
		ret = qcom_smem_resolve_mem(smem, "qcom,rpm-msg-ram", &smem->regions[1]);
		if (ret)
			return ret;
	}


	ret = qcom_smem_map_toc(smem, &smem->regions[0]);
	if (ret)
		return ret;

	for (i = 1; i < num_regions; i++) {
		smem->regions[i].virt_base = devm_ioremap_wc(&pdev->dev,
							     smem->regions[i].aux_base,
							     smem->regions[i].size);
		if (!smem->regions[i].virt_base) {
			dev_err(&pdev->dev, "failed to remap %pa\n", &smem->regions[i].aux_base);
			return -ENOMEM;
		}
	}

	header = smem->regions[0].virt_base;
	if (le32_to_cpu(header->initialized) != 1 ||
	    le32_to_cpu(header->reserved)) {
		dev_err(&pdev->dev, "SMEM is not initialized by SBL\n");
		return -EINVAL;
	}

	hwlock_id = of_hwspin_lock_get_id(pdev->dev.of_node, 0);
	if (hwlock_id < 0) {
		if (hwlock_id != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to retrieve hwlock\n");
		return hwlock_id;
	}

	smem->hwlock = hwspin_lock_request_specific(hwlock_id);
	if (!smem->hwlock)
		return -ENXIO;

	ret = hwspin_lock_timeout_irqsave(smem->hwlock, HWSPINLOCK_TIMEOUT, &flags);
	if (ret)
		return ret;
	size = readl_relaxed(&header->available) + readl_relaxed(&header->free_offset);
	hwspin_unlock_irqrestore(smem->hwlock, &flags);

	version = qcom_smem_get_sbl_version(smem);
	 
	devm_iounmap(smem->dev, smem->regions[0].virt_base);
	switch (version >> 16) {
	case SMEM_GLOBAL_PART_VERSION:
		ret = qcom_smem_set_global_partition(smem);
		if (ret < 0)
			return ret;
		smem->item_count = qcom_smem_get_item_count(smem);
		break;
	case SMEM_GLOBAL_HEAP_VERSION:
		qcom_smem_map_global(smem, size);
		smem->item_count = SMEM_ITEM_COUNT;
		break;
	default:
		dev_err(&pdev->dev, "Unsupported SMEM version 0x%x\n", version);
		return -EINVAL;
	}

	BUILD_BUG_ON(SMEM_HOST_APPS >= SMEM_HOST_COUNT);
	ret = qcom_smem_enumerate_partitions(smem, SMEM_HOST_APPS);
	if (ret < 0 && ret != -ENOENT)
		return ret;

	__smem = smem;

	smem->socinfo = platform_device_register_data(&pdev->dev, "qcom-socinfo",
						      PLATFORM_DEVID_NONE, NULL,
						      0);
	if (IS_ERR(smem->socinfo))
		dev_dbg(&pdev->dev, "failed to register socinfo device\n");

	return 0;
}

static int qcom_smem_remove(struct platform_device *pdev)
{
	platform_device_unregister(__smem->socinfo);

	hwspin_lock_free(__smem->hwlock);
	__smem = NULL;

	return 0;
}

static const struct of_device_id qcom_smem_of_match[] = {
	{ .compatible = "qcom,smem" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_smem_of_match);

static struct platform_driver qcom_smem_driver = {
	.probe = qcom_smem_probe,
	.remove = qcom_smem_remove,
	.driver  = {
		.name = "qcom-smem",
		.of_match_table = qcom_smem_of_match,
		.suppress_bind_attrs = true,
	},
};

static int __init qcom_smem_init(void)
{
	return platform_driver_register(&qcom_smem_driver);
}
arch_initcall(qcom_smem_init);

static void __exit qcom_smem_exit(void)
{
	platform_driver_unregister(&qcom_smem_driver);
}
module_exit(qcom_smem_exit)

MODULE_AUTHOR("Bjorn Andersson <bjorn.andersson@sonymobile.com>");
MODULE_DESCRIPTION("Qualcomm Shared Memory Manager");
MODULE_LICENSE("GPL v2");
