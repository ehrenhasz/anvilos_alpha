
 

#include <linux/atomic.h>
#include <linux/coresight.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include "coresight-catu.h"
#include "coresight-etm-perf.h"
#include "coresight-priv.h"
#include "coresight-tmc.h"

struct etr_flat_buf {
	struct device	*dev;
	dma_addr_t	daddr;
	void		*vaddr;
	size_t		size;
};

 
struct etr_perf_buffer {
	struct tmc_drvdata	*drvdata;
	struct etr_buf		*etr_buf;
	pid_t			pid;
	bool			snapshot;
	int			nr_pages;
	void			**pages;
};

 
#define PERF_IDX2OFF(idx, buf)		\
		((idx) % ((unsigned long)(buf)->nr_pages << PAGE_SHIFT))

 
#define TMC_ETR_PERF_MIN_BUF_SIZE	SZ_1M

 

typedef u32 sgte_t;

#define ETR_SG_PAGE_SHIFT		12
#define ETR_SG_PAGE_SIZE		(1UL << ETR_SG_PAGE_SHIFT)
#define ETR_SG_PAGES_PER_SYSPAGE	(PAGE_SIZE / ETR_SG_PAGE_SIZE)
#define ETR_SG_PTRS_PER_PAGE		(ETR_SG_PAGE_SIZE / sizeof(sgte_t))
#define ETR_SG_PTRS_PER_SYSPAGE		(PAGE_SIZE / sizeof(sgte_t))

#define ETR_SG_ET_MASK			0x3
#define ETR_SG_ET_LAST			0x1
#define ETR_SG_ET_NORMAL		0x2
#define ETR_SG_ET_LINK			0x3

#define ETR_SG_ADDR_SHIFT		4

#define ETR_SG_ENTRY(addr, type) \
	(sgte_t)((((addr) >> ETR_SG_PAGE_SHIFT) << ETR_SG_ADDR_SHIFT) | \
		 (type & ETR_SG_ET_MASK))

#define ETR_SG_ADDR(entry) \
	(((dma_addr_t)(entry) >> ETR_SG_ADDR_SHIFT) << ETR_SG_PAGE_SHIFT)
#define ETR_SG_ET(entry)		((entry) & ETR_SG_ET_MASK)

 
struct etr_sg_table {
	struct tmc_sg_table	*sg_table;
	dma_addr_t		hwaddr;
};

 
static inline unsigned long __attribute_const__
tmc_etr_sg_table_entries(int nr_pages)
{
	unsigned long nr_sgpages = nr_pages * ETR_SG_PAGES_PER_SYSPAGE;
	unsigned long nr_sglinks = nr_sgpages / (ETR_SG_PTRS_PER_PAGE - 1);
	 
	if (nr_sglinks && (nr_sgpages % (ETR_SG_PTRS_PER_PAGE - 1) < 2))
		nr_sglinks--;
	return nr_sgpages + nr_sglinks;
}

 
static long
tmc_pages_get_offset(struct tmc_pages *tmc_pages, dma_addr_t addr)
{
	int i;
	dma_addr_t page_start;

	for (i = 0; i < tmc_pages->nr_pages; i++) {
		page_start = tmc_pages->daddrs[i];
		if (addr >= page_start && addr < (page_start + PAGE_SIZE))
			return i * PAGE_SIZE + (addr - page_start);
	}

	return -EINVAL;
}

 
static void tmc_pages_free(struct tmc_pages *tmc_pages,
			   struct device *dev, enum dma_data_direction dir)
{
	int i;
	struct device *real_dev = dev->parent;

	for (i = 0; i < tmc_pages->nr_pages; i++) {
		if (tmc_pages->daddrs && tmc_pages->daddrs[i])
			dma_unmap_page(real_dev, tmc_pages->daddrs[i],
					 PAGE_SIZE, dir);
		if (tmc_pages->pages && tmc_pages->pages[i])
			__free_page(tmc_pages->pages[i]);
	}

	kfree(tmc_pages->pages);
	kfree(tmc_pages->daddrs);
	tmc_pages->pages = NULL;
	tmc_pages->daddrs = NULL;
	tmc_pages->nr_pages = 0;
}

 
static int tmc_pages_alloc(struct tmc_pages *tmc_pages,
			   struct device *dev, int node,
			   enum dma_data_direction dir, void **pages)
{
	int i, nr_pages;
	dma_addr_t paddr;
	struct page *page;
	struct device *real_dev = dev->parent;

	nr_pages = tmc_pages->nr_pages;
	tmc_pages->daddrs = kcalloc(nr_pages, sizeof(*tmc_pages->daddrs),
					 GFP_KERNEL);
	if (!tmc_pages->daddrs)
		return -ENOMEM;
	tmc_pages->pages = kcalloc(nr_pages, sizeof(*tmc_pages->pages),
					 GFP_KERNEL);
	if (!tmc_pages->pages) {
		kfree(tmc_pages->daddrs);
		tmc_pages->daddrs = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < nr_pages; i++) {
		if (pages && pages[i]) {
			page = virt_to_page(pages[i]);
			 
			get_page(page);
		} else {
			page = alloc_pages_node(node,
						GFP_KERNEL | __GFP_ZERO, 0);
			if (!page)
				goto err;
		}
		paddr = dma_map_page(real_dev, page, 0, PAGE_SIZE, dir);
		if (dma_mapping_error(real_dev, paddr))
			goto err;
		tmc_pages->daddrs[i] = paddr;
		tmc_pages->pages[i] = page;
	}
	return 0;
err:
	tmc_pages_free(tmc_pages, dev, dir);
	return -ENOMEM;
}

static inline long
tmc_sg_get_data_page_offset(struct tmc_sg_table *sg_table, dma_addr_t addr)
{
	return tmc_pages_get_offset(&sg_table->data_pages, addr);
}

static inline void tmc_free_table_pages(struct tmc_sg_table *sg_table)
{
	if (sg_table->table_vaddr)
		vunmap(sg_table->table_vaddr);
	tmc_pages_free(&sg_table->table_pages, sg_table->dev, DMA_TO_DEVICE);
}

static void tmc_free_data_pages(struct tmc_sg_table *sg_table)
{
	if (sg_table->data_vaddr)
		vunmap(sg_table->data_vaddr);
	tmc_pages_free(&sg_table->data_pages, sg_table->dev, DMA_FROM_DEVICE);
}

void tmc_free_sg_table(struct tmc_sg_table *sg_table)
{
	tmc_free_table_pages(sg_table);
	tmc_free_data_pages(sg_table);
}
EXPORT_SYMBOL_GPL(tmc_free_sg_table);

 
static int tmc_alloc_table_pages(struct tmc_sg_table *sg_table)
{
	int rc;
	struct tmc_pages *table_pages = &sg_table->table_pages;

	rc = tmc_pages_alloc(table_pages, sg_table->dev,
			     dev_to_node(sg_table->dev),
			     DMA_TO_DEVICE, NULL);
	if (rc)
		return rc;
	sg_table->table_vaddr = vmap(table_pages->pages,
				     table_pages->nr_pages,
				     VM_MAP,
				     PAGE_KERNEL);
	if (!sg_table->table_vaddr)
		rc = -ENOMEM;
	else
		sg_table->table_daddr = table_pages->daddrs[0];
	return rc;
}

static int tmc_alloc_data_pages(struct tmc_sg_table *sg_table, void **pages)
{
	int rc;

	 
	rc = tmc_pages_alloc(&sg_table->data_pages,
			     sg_table->dev, sg_table->node,
			     DMA_FROM_DEVICE, pages);
	if (!rc) {
		sg_table->data_vaddr = vmap(sg_table->data_pages.pages,
					    sg_table->data_pages.nr_pages,
					    VM_MAP,
					    PAGE_KERNEL);
		if (!sg_table->data_vaddr)
			rc = -ENOMEM;
	}
	return rc;
}

 
struct tmc_sg_table *tmc_alloc_sg_table(struct device *dev,
					int node,
					int nr_tpages,
					int nr_dpages,
					void **pages)
{
	long rc;
	struct tmc_sg_table *sg_table;

	sg_table = kzalloc(sizeof(*sg_table), GFP_KERNEL);
	if (!sg_table)
		return ERR_PTR(-ENOMEM);
	sg_table->data_pages.nr_pages = nr_dpages;
	sg_table->table_pages.nr_pages = nr_tpages;
	sg_table->node = node;
	sg_table->dev = dev;

	rc  = tmc_alloc_data_pages(sg_table, pages);
	if (!rc)
		rc = tmc_alloc_table_pages(sg_table);
	if (rc) {
		tmc_free_sg_table(sg_table);
		kfree(sg_table);
		return ERR_PTR(rc);
	}

	return sg_table;
}
EXPORT_SYMBOL_GPL(tmc_alloc_sg_table);

 
void tmc_sg_table_sync_data_range(struct tmc_sg_table *table,
				  u64 offset, u64 size)
{
	int i, index, start;
	int npages = DIV_ROUND_UP(size, PAGE_SIZE);
	struct device *real_dev = table->dev->parent;
	struct tmc_pages *data = &table->data_pages;

	start = offset >> PAGE_SHIFT;
	for (i = start; i < (start + npages); i++) {
		index = i % data->nr_pages;
		dma_sync_single_for_cpu(real_dev, data->daddrs[index],
					PAGE_SIZE, DMA_FROM_DEVICE);
	}
}
EXPORT_SYMBOL_GPL(tmc_sg_table_sync_data_range);

 
void tmc_sg_table_sync_table(struct tmc_sg_table *sg_table)
{
	int i;
	struct device *real_dev = sg_table->dev->parent;
	struct tmc_pages *table_pages = &sg_table->table_pages;

	for (i = 0; i < table_pages->nr_pages; i++)
		dma_sync_single_for_device(real_dev, table_pages->daddrs[i],
					   PAGE_SIZE, DMA_TO_DEVICE);
}
EXPORT_SYMBOL_GPL(tmc_sg_table_sync_table);

 
ssize_t tmc_sg_table_get_data(struct tmc_sg_table *sg_table,
			      u64 offset, size_t len, char **bufpp)
{
	size_t size;
	int pg_idx = offset >> PAGE_SHIFT;
	int pg_offset = offset & (PAGE_SIZE - 1);
	struct tmc_pages *data_pages = &sg_table->data_pages;

	size = tmc_sg_table_buf_size(sg_table);
	if (offset >= size)
		return -EINVAL;

	 
	len = (len < (size - offset)) ? len : size - offset;
	 
	len = (len < (PAGE_SIZE - pg_offset)) ? len : (PAGE_SIZE - pg_offset);
	if (len > 0)
		*bufpp = page_address(data_pages->pages[pg_idx]) + pg_offset;
	return len;
}
EXPORT_SYMBOL_GPL(tmc_sg_table_get_data);

#ifdef ETR_SG_DEBUG
 
static unsigned long
tmc_sg_daddr_to_vaddr(struct tmc_sg_table *sg_table,
		      dma_addr_t addr, bool table)
{
	long offset;
	unsigned long base;
	struct tmc_pages *tmc_pages;

	if (table) {
		tmc_pages = &sg_table->table_pages;
		base = (unsigned long)sg_table->table_vaddr;
	} else {
		tmc_pages = &sg_table->data_pages;
		base = (unsigned long)sg_table->data_vaddr;
	}

	offset = tmc_pages_get_offset(tmc_pages, addr);
	if (offset < 0)
		return 0;
	return base + offset;
}

 
static void tmc_etr_sg_table_dump(struct etr_sg_table *etr_table)
{
	sgte_t *ptr;
	int i = 0;
	dma_addr_t addr;
	struct tmc_sg_table *sg_table = etr_table->sg_table;

	ptr = (sgte_t *)tmc_sg_daddr_to_vaddr(sg_table,
					      etr_table->hwaddr, true);
	while (ptr) {
		addr = ETR_SG_ADDR(*ptr);
		switch (ETR_SG_ET(*ptr)) {
		case ETR_SG_ET_NORMAL:
			dev_dbg(sg_table->dev,
				"%05d: %p\t:[N] 0x%llx\n", i, ptr, addr);
			ptr++;
			break;
		case ETR_SG_ET_LINK:
			dev_dbg(sg_table->dev,
				"%05d: *** %p\t:{L} 0x%llx ***\n",
				 i, ptr, addr);
			ptr = (sgte_t *)tmc_sg_daddr_to_vaddr(sg_table,
							      addr, true);
			break;
		case ETR_SG_ET_LAST:
			dev_dbg(sg_table->dev,
				"%05d: ### %p\t:[L] 0x%llx ###\n",
				 i, ptr, addr);
			return;
		default:
			dev_dbg(sg_table->dev,
				"%05d: xxx %p\t:[INVALID] 0x%llx xxx\n",
				 i, ptr, addr);
			return;
		}
		i++;
	}
	dev_dbg(sg_table->dev, "******* End of Table *****\n");
}
#else
static inline void tmc_etr_sg_table_dump(struct etr_sg_table *etr_table) {}
#endif

 
#define INC_IDX_ROUND(idx, size) ((idx) = ((idx) + 1) % (size))
static void tmc_etr_sg_table_populate(struct etr_sg_table *etr_table)
{
	dma_addr_t paddr;
	int i, type, nr_entries;
	int tpidx = 0;  
	int sgtidx = 0;	 
	int sgtentry = 0;  
	int dpidx = 0;  
	int spidx = 0;  
	sgte_t *ptr;  
	struct tmc_sg_table *sg_table = etr_table->sg_table;
	dma_addr_t *table_daddrs = sg_table->table_pages.daddrs;
	dma_addr_t *data_daddrs = sg_table->data_pages.daddrs;

	nr_entries = tmc_etr_sg_table_entries(sg_table->data_pages.nr_pages);
	 
	ptr = sg_table->table_vaddr;
	 
	for (i = 0; i < nr_entries - 1; i++) {
		if (sgtentry == ETR_SG_PTRS_PER_PAGE - 1) {
			 
			if (sgtidx == ETR_SG_PAGES_PER_SYSPAGE - 1) {
				paddr = table_daddrs[tpidx + 1];
			} else {
				paddr = table_daddrs[tpidx] +
					(ETR_SG_PAGE_SIZE * (sgtidx + 1));
			}
			type = ETR_SG_ET_LINK;
		} else {
			 
			type = ETR_SG_ET_NORMAL;
			paddr = data_daddrs[dpidx] + spidx * ETR_SG_PAGE_SIZE;
			if (!INC_IDX_ROUND(spidx, ETR_SG_PAGES_PER_SYSPAGE))
				dpidx++;
		}
		*ptr++ = ETR_SG_ENTRY(paddr, type);
		 
		if (!INC_IDX_ROUND(sgtentry, ETR_SG_PTRS_PER_PAGE)) {
			if (!INC_IDX_ROUND(sgtidx, ETR_SG_PAGES_PER_SYSPAGE))
				tpidx++;
		}
	}

	 
	paddr = data_daddrs[dpidx] + spidx * ETR_SG_PAGE_SIZE;
	*ptr++ = ETR_SG_ENTRY(paddr, ETR_SG_ET_LAST);
}

 
static struct etr_sg_table *
tmc_init_etr_sg_table(struct device *dev, int node,
		      unsigned long size, void **pages)
{
	int nr_entries, nr_tpages;
	int nr_dpages = size >> PAGE_SHIFT;
	struct tmc_sg_table *sg_table;
	struct etr_sg_table *etr_table;

	etr_table = kzalloc(sizeof(*etr_table), GFP_KERNEL);
	if (!etr_table)
		return ERR_PTR(-ENOMEM);
	nr_entries = tmc_etr_sg_table_entries(nr_dpages);
	nr_tpages = DIV_ROUND_UP(nr_entries, ETR_SG_PTRS_PER_SYSPAGE);

	sg_table = tmc_alloc_sg_table(dev, node, nr_tpages, nr_dpages, pages);
	if (IS_ERR(sg_table)) {
		kfree(etr_table);
		return ERR_CAST(sg_table);
	}

	etr_table->sg_table = sg_table;
	 
	etr_table->hwaddr = sg_table->table_daddr;
	tmc_etr_sg_table_populate(etr_table);
	 
	tmc_sg_table_sync_table(sg_table);
	tmc_etr_sg_table_dump(etr_table);

	return etr_table;
}

 
static int tmc_etr_alloc_flat_buf(struct tmc_drvdata *drvdata,
				  struct etr_buf *etr_buf, int node,
				  void **pages)
{
	struct etr_flat_buf *flat_buf;
	struct device *real_dev = drvdata->csdev->dev.parent;

	 
	if (pages)
		return -EINVAL;

	flat_buf = kzalloc(sizeof(*flat_buf), GFP_KERNEL);
	if (!flat_buf)
		return -ENOMEM;

	flat_buf->vaddr = dma_alloc_noncoherent(real_dev, etr_buf->size,
						&flat_buf->daddr,
						DMA_FROM_DEVICE,
						GFP_KERNEL | __GFP_NOWARN);
	if (!flat_buf->vaddr) {
		kfree(flat_buf);
		return -ENOMEM;
	}

	flat_buf->size = etr_buf->size;
	flat_buf->dev = &drvdata->csdev->dev;
	etr_buf->hwaddr = flat_buf->daddr;
	etr_buf->mode = ETR_MODE_FLAT;
	etr_buf->private = flat_buf;
	return 0;
}

static void tmc_etr_free_flat_buf(struct etr_buf *etr_buf)
{
	struct etr_flat_buf *flat_buf = etr_buf->private;

	if (flat_buf && flat_buf->daddr) {
		struct device *real_dev = flat_buf->dev->parent;

		dma_free_noncoherent(real_dev, etr_buf->size,
				     flat_buf->vaddr, flat_buf->daddr,
				     DMA_FROM_DEVICE);
	}
	kfree(flat_buf);
}

static void tmc_etr_sync_flat_buf(struct etr_buf *etr_buf, u64 rrp, u64 rwp)
{
	struct etr_flat_buf *flat_buf = etr_buf->private;
	struct device *real_dev = flat_buf->dev->parent;

	 
	etr_buf->offset = rrp - etr_buf->hwaddr;
	if (etr_buf->full)
		etr_buf->len = etr_buf->size;
	else
		etr_buf->len = rwp - rrp;

	 
	if (etr_buf->offset + etr_buf->len > etr_buf->size)
		dma_sync_single_for_cpu(real_dev, flat_buf->daddr,
					etr_buf->size, DMA_FROM_DEVICE);
	else
		dma_sync_single_for_cpu(real_dev,
					flat_buf->daddr + etr_buf->offset,
					etr_buf->len, DMA_FROM_DEVICE);
}

static ssize_t tmc_etr_get_data_flat_buf(struct etr_buf *etr_buf,
					 u64 offset, size_t len, char **bufpp)
{
	struct etr_flat_buf *flat_buf = etr_buf->private;

	*bufpp = (char *)flat_buf->vaddr + offset;
	 
	return len;
}

static const struct etr_buf_operations etr_flat_buf_ops = {
	.alloc = tmc_etr_alloc_flat_buf,
	.free = tmc_etr_free_flat_buf,
	.sync = tmc_etr_sync_flat_buf,
	.get_data = tmc_etr_get_data_flat_buf,
};

 
static int tmc_etr_alloc_sg_buf(struct tmc_drvdata *drvdata,
				struct etr_buf *etr_buf, int node,
				void **pages)
{
	struct etr_sg_table *etr_table;
	struct device *dev = &drvdata->csdev->dev;

	etr_table = tmc_init_etr_sg_table(dev, node,
					  etr_buf->size, pages);
	if (IS_ERR(etr_table))
		return -ENOMEM;
	etr_buf->hwaddr = etr_table->hwaddr;
	etr_buf->mode = ETR_MODE_ETR_SG;
	etr_buf->private = etr_table;
	return 0;
}

static void tmc_etr_free_sg_buf(struct etr_buf *etr_buf)
{
	struct etr_sg_table *etr_table = etr_buf->private;

	if (etr_table) {
		tmc_free_sg_table(etr_table->sg_table);
		kfree(etr_table);
	}
}

static ssize_t tmc_etr_get_data_sg_buf(struct etr_buf *etr_buf, u64 offset,
				       size_t len, char **bufpp)
{
	struct etr_sg_table *etr_table = etr_buf->private;

	return tmc_sg_table_get_data(etr_table->sg_table, offset, len, bufpp);
}

static void tmc_etr_sync_sg_buf(struct etr_buf *etr_buf, u64 rrp, u64 rwp)
{
	long r_offset, w_offset;
	struct etr_sg_table *etr_table = etr_buf->private;
	struct tmc_sg_table *table = etr_table->sg_table;

	 
	r_offset = tmc_sg_get_data_page_offset(table, rrp);
	if (r_offset < 0) {
		dev_warn(table->dev,
			 "Unable to map RRP %llx to offset\n", rrp);
		etr_buf->len = 0;
		return;
	}

	w_offset = tmc_sg_get_data_page_offset(table, rwp);
	if (w_offset < 0) {
		dev_warn(table->dev,
			 "Unable to map RWP %llx to offset\n", rwp);
		etr_buf->len = 0;
		return;
	}

	etr_buf->offset = r_offset;
	if (etr_buf->full)
		etr_buf->len = etr_buf->size;
	else
		etr_buf->len = ((w_offset < r_offset) ? etr_buf->size : 0) +
				w_offset - r_offset;
	tmc_sg_table_sync_data_range(table, r_offset, etr_buf->len);
}

static const struct etr_buf_operations etr_sg_buf_ops = {
	.alloc = tmc_etr_alloc_sg_buf,
	.free = tmc_etr_free_sg_buf,
	.sync = tmc_etr_sync_sg_buf,
	.get_data = tmc_etr_get_data_sg_buf,
};

 
struct coresight_device *
tmc_etr_get_catu_device(struct tmc_drvdata *drvdata)
{
	struct coresight_device *etr = drvdata->csdev;
	union coresight_dev_subtype catu_subtype = {
		.helper_subtype = CORESIGHT_DEV_SUBTYPE_HELPER_CATU
	};

	if (!IS_ENABLED(CONFIG_CORESIGHT_CATU))
		return NULL;

	return coresight_find_output_type(etr->pdata, CORESIGHT_DEV_TYPE_HELPER,
					  catu_subtype);
}
EXPORT_SYMBOL_GPL(tmc_etr_get_catu_device);

static const struct etr_buf_operations *etr_buf_ops[] = {
	[ETR_MODE_FLAT] = &etr_flat_buf_ops,
	[ETR_MODE_ETR_SG] = &etr_sg_buf_ops,
	[ETR_MODE_CATU] = NULL,
};

void tmc_etr_set_catu_ops(const struct etr_buf_operations *catu)
{
	etr_buf_ops[ETR_MODE_CATU] = catu;
}
EXPORT_SYMBOL_GPL(tmc_etr_set_catu_ops);

void tmc_etr_remove_catu_ops(void)
{
	etr_buf_ops[ETR_MODE_CATU] = NULL;
}
EXPORT_SYMBOL_GPL(tmc_etr_remove_catu_ops);

static inline int tmc_etr_mode_alloc_buf(int mode,
					 struct tmc_drvdata *drvdata,
					 struct etr_buf *etr_buf, int node,
					 void **pages)
{
	int rc = -EINVAL;

	switch (mode) {
	case ETR_MODE_FLAT:
	case ETR_MODE_ETR_SG:
	case ETR_MODE_CATU:
		if (etr_buf_ops[mode] && etr_buf_ops[mode]->alloc)
			rc = etr_buf_ops[mode]->alloc(drvdata, etr_buf,
						      node, pages);
		if (!rc)
			etr_buf->ops = etr_buf_ops[mode];
		return rc;
	default:
		return -EINVAL;
	}
}

 
static struct etr_buf *tmc_alloc_etr_buf(struct tmc_drvdata *drvdata,
					 ssize_t size, int flags,
					 int node, void **pages)
{
	int rc = -ENOMEM;
	bool has_etr_sg, has_iommu;
	bool has_sg, has_catu;
	struct etr_buf *etr_buf;
	struct device *dev = &drvdata->csdev->dev;

	has_etr_sg = tmc_etr_has_cap(drvdata, TMC_ETR_SG);
	has_iommu = iommu_get_domain_for_dev(dev->parent);
	has_catu = !!tmc_etr_get_catu_device(drvdata);

	has_sg = has_catu || has_etr_sg;

	etr_buf = kzalloc(sizeof(*etr_buf), GFP_KERNEL);
	if (!etr_buf)
		return ERR_PTR(-ENOMEM);

	etr_buf->size = size;

	 
	if (!pages &&
	    (!has_sg || has_iommu || size < SZ_1M))
		rc = tmc_etr_mode_alloc_buf(ETR_MODE_FLAT, drvdata,
					    etr_buf, node, pages);
	if (rc && has_etr_sg)
		rc = tmc_etr_mode_alloc_buf(ETR_MODE_ETR_SG, drvdata,
					    etr_buf, node, pages);
	if (rc && has_catu)
		rc = tmc_etr_mode_alloc_buf(ETR_MODE_CATU, drvdata,
					    etr_buf, node, pages);
	if (rc) {
		kfree(etr_buf);
		return ERR_PTR(rc);
	}

	refcount_set(&etr_buf->refcount, 1);
	dev_dbg(dev, "allocated buffer of size %ldKB in mode %d\n",
		(unsigned long)size >> 10, etr_buf->mode);
	return etr_buf;
}

static void tmc_free_etr_buf(struct etr_buf *etr_buf)
{
	WARN_ON(!etr_buf->ops || !etr_buf->ops->free);
	etr_buf->ops->free(etr_buf);
	kfree(etr_buf);
}

 
static ssize_t tmc_etr_buf_get_data(struct etr_buf *etr_buf,
				    u64 offset, size_t len, char **bufpp)
{
	 
	len = (len < (etr_buf->size - offset)) ? len : etr_buf->size - offset;

	return etr_buf->ops->get_data(etr_buf, (u64)offset, len, bufpp);
}

static inline s64
tmc_etr_buf_insert_barrier_packet(struct etr_buf *etr_buf, u64 offset)
{
	ssize_t len;
	char *bufp;

	len = tmc_etr_buf_get_data(etr_buf, offset,
				   CORESIGHT_BARRIER_PKT_SIZE, &bufp);
	if (WARN_ON(len < 0 || len < CORESIGHT_BARRIER_PKT_SIZE))
		return -EINVAL;
	coresight_insert_barrier_packet(bufp);
	return offset + CORESIGHT_BARRIER_PKT_SIZE;
}

 
static void tmc_sync_etr_buf(struct tmc_drvdata *drvdata)
{
	struct etr_buf *etr_buf = drvdata->etr_buf;
	u64 rrp, rwp;
	u32 status;

	rrp = tmc_read_rrp(drvdata);
	rwp = tmc_read_rwp(drvdata);
	status = readl_relaxed(drvdata->base + TMC_STS);

	 
	if (WARN_ON_ONCE(status & TMC_STS_MEMERR)) {
		dev_dbg(&drvdata->csdev->dev,
			"tmc memory error detected, truncating buffer\n");
		etr_buf->len = 0;
		etr_buf->full = false;
		return;
	}

	etr_buf->full = !!(status & TMC_STS_FULL);

	WARN_ON(!etr_buf->ops || !etr_buf->ops->sync);

	etr_buf->ops->sync(etr_buf, rrp, rwp);
}

static int __tmc_etr_enable_hw(struct tmc_drvdata *drvdata)
{
	u32 axictl, sts;
	struct etr_buf *etr_buf = drvdata->etr_buf;
	int rc = 0;

	CS_UNLOCK(drvdata->base);

	 
	rc = tmc_wait_for_tmcready(drvdata);
	if (rc) {
		dev_err(&drvdata->csdev->dev,
			"Failed to enable : TMC not ready\n");
		CS_LOCK(drvdata->base);
		return rc;
	}

	writel_relaxed(etr_buf->size / 4, drvdata->base + TMC_RSZ);
	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);

	axictl = readl_relaxed(drvdata->base + TMC_AXICTL);
	axictl &= ~TMC_AXICTL_CLEAR_MASK;
	axictl |= TMC_AXICTL_PROT_CTL_B1;
	axictl |= TMC_AXICTL_WR_BURST(drvdata->max_burst_size);
	axictl |= TMC_AXICTL_AXCACHE_OS;

	if (tmc_etr_has_cap(drvdata, TMC_ETR_AXI_ARCACHE)) {
		axictl &= ~TMC_AXICTL_ARCACHE_MASK;
		axictl |= TMC_AXICTL_ARCACHE_OS;
	}

	if (etr_buf->mode == ETR_MODE_ETR_SG)
		axictl |= TMC_AXICTL_SCT_GAT_MODE;

	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	tmc_write_dba(drvdata, etr_buf->hwaddr);
	 
	if (tmc_etr_has_cap(drvdata, TMC_ETR_SAVE_RESTORE)) {
		tmc_write_rrp(drvdata, etr_buf->hwaddr);
		tmc_write_rwp(drvdata, etr_buf->hwaddr);
		sts = readl_relaxed(drvdata->base + TMC_STS) & ~TMC_STS_FULL;
		writel_relaxed(sts, drvdata->base + TMC_STS);
	}

	writel_relaxed(TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI |
		       TMC_FFCR_FON_FLIN | TMC_FFCR_FON_TRIG_EVT |
		       TMC_FFCR_TRIGON_TRIGIN,
		       drvdata->base + TMC_FFCR);
	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);
	return rc;
}

static int tmc_etr_enable_hw(struct tmc_drvdata *drvdata,
			     struct etr_buf *etr_buf)
{
	int rc;

	 
	if (WARN_ON(!etr_buf))
		return -EINVAL;

	if ((etr_buf->mode == ETR_MODE_ETR_SG) &&
	    WARN_ON(!tmc_etr_has_cap(drvdata, TMC_ETR_SG)))
		return -EINVAL;

	if (WARN_ON(drvdata->etr_buf))
		return -EBUSY;

	rc = coresight_claim_device(drvdata->csdev);
	if (!rc) {
		drvdata->etr_buf = etr_buf;
		rc = __tmc_etr_enable_hw(drvdata);
		if (rc) {
			drvdata->etr_buf = NULL;
			coresight_disclaim_device(drvdata->csdev);
		}
	}

	return rc;
}

 
ssize_t tmc_etr_get_sysfs_trace(struct tmc_drvdata *drvdata,
				loff_t pos, size_t len, char **bufpp)
{
	s64 offset;
	ssize_t actual = len;
	struct etr_buf *etr_buf = drvdata->sysfs_buf;

	if (pos + actual > etr_buf->len)
		actual = etr_buf->len - pos;
	if (actual <= 0)
		return actual;

	 
	offset = etr_buf->offset + pos;
	if (offset >= etr_buf->size)
		offset -= etr_buf->size;
	return tmc_etr_buf_get_data(etr_buf, offset, actual, bufpp);
}

static struct etr_buf *
tmc_etr_setup_sysfs_buf(struct tmc_drvdata *drvdata)
{
	return tmc_alloc_etr_buf(drvdata, drvdata->size,
				 0, cpu_to_node(0), NULL);
}

static void
tmc_etr_free_sysfs_buf(struct etr_buf *buf)
{
	if (buf)
		tmc_free_etr_buf(buf);
}

static void tmc_etr_sync_sysfs_buf(struct tmc_drvdata *drvdata)
{
	struct etr_buf *etr_buf = drvdata->etr_buf;

	if (WARN_ON(drvdata->sysfs_buf != etr_buf)) {
		tmc_etr_free_sysfs_buf(drvdata->sysfs_buf);
		drvdata->sysfs_buf = NULL;
	} else {
		tmc_sync_etr_buf(drvdata);
		 
		if (etr_buf->full)
			tmc_etr_buf_insert_barrier_packet(etr_buf,
							  etr_buf->offset);
	}
}

static void __tmc_etr_disable_hw(struct tmc_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	 
	if (drvdata->mode == CS_MODE_SYSFS)
		tmc_etr_sync_sysfs_buf(drvdata);

	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);

}

void tmc_etr_disable_hw(struct tmc_drvdata *drvdata)
{
	__tmc_etr_disable_hw(drvdata);
	coresight_disclaim_device(drvdata->csdev);
	 
	drvdata->etr_buf = NULL;
}

static struct etr_buf *tmc_etr_get_sysfs_buffer(struct coresight_device *csdev)
{
	int ret = 0;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct etr_buf *sysfs_buf = NULL, *new_buf = NULL, *free_buf = NULL;

	 
	spin_lock_irqsave(&drvdata->spinlock, flags);
	sysfs_buf = READ_ONCE(drvdata->sysfs_buf);
	if (!sysfs_buf || (sysfs_buf->size != drvdata->size)) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		 
		free_buf = new_buf = tmc_etr_setup_sysfs_buf(drvdata);
		if (IS_ERR(new_buf))
			return new_buf;

		 
		spin_lock_irqsave(&drvdata->spinlock, flags);
	}

	if (drvdata->reading || drvdata->mode == CS_MODE_PERF) {
		ret = -EBUSY;
		goto out;
	}

	 
	sysfs_buf = READ_ONCE(drvdata->sysfs_buf);
	if (!sysfs_buf || (new_buf && sysfs_buf->size != new_buf->size)) {
		free_buf = sysfs_buf;
		drvdata->sysfs_buf = new_buf;
	}

out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	 
	if (free_buf)
		tmc_etr_free_sysfs_buf(free_buf);
	return ret ? ERR_PTR(ret) : drvdata->sysfs_buf;
}

static int tmc_enable_etr_sink_sysfs(struct coresight_device *csdev)
{
	int ret = 0;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct etr_buf *sysfs_buf = tmc_etr_get_sysfs_buffer(csdev);

	if (IS_ERR(sysfs_buf))
		return PTR_ERR(sysfs_buf);

	spin_lock_irqsave(&drvdata->spinlock, flags);

	 
	if (drvdata->mode == CS_MODE_SYSFS) {
		atomic_inc(&csdev->refcnt);
		goto out;
	}

	ret = tmc_etr_enable_hw(drvdata, sysfs_buf);
	if (!ret) {
		drvdata->mode = CS_MODE_SYSFS;
		atomic_inc(&csdev->refcnt);
	}

out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (!ret)
		dev_dbg(&csdev->dev, "TMC-ETR enabled\n");

	return ret;
}

struct etr_buf *tmc_etr_get_buffer(struct coresight_device *csdev,
				   enum cs_mode mode, void *data)
{
	struct perf_output_handle *handle = data;
	struct etr_perf_buffer *etr_perf;

	switch (mode) {
	case CS_MODE_SYSFS:
		return tmc_etr_get_sysfs_buffer(csdev);
	case CS_MODE_PERF:
		etr_perf = etm_perf_sink_config(handle);
		if (WARN_ON(!etr_perf || !etr_perf->etr_buf))
			return ERR_PTR(-EINVAL);
		return etr_perf->etr_buf;
	default:
		return ERR_PTR(-EINVAL);
	}
}
EXPORT_SYMBOL_GPL(tmc_etr_get_buffer);

 
static struct etr_buf *
alloc_etr_buf(struct tmc_drvdata *drvdata, struct perf_event *event,
	      int nr_pages, void **pages, bool snapshot)
{
	int node;
	struct etr_buf *etr_buf;
	unsigned long size;

	node = (event->cpu == -1) ? NUMA_NO_NODE : cpu_to_node(event->cpu);
	 
	if ((nr_pages << PAGE_SHIFT) > drvdata->size) {
		etr_buf = tmc_alloc_etr_buf(drvdata, ((ssize_t)nr_pages << PAGE_SHIFT),
					    0, node, NULL);
		if (!IS_ERR(etr_buf))
			goto done;
	}

	 
	size = drvdata->size;
	do {
		etr_buf = tmc_alloc_etr_buf(drvdata, size, 0, node, NULL);
		if (!IS_ERR(etr_buf))
			goto done;
		size /= 2;
	} while (size >= TMC_ETR_PERF_MIN_BUF_SIZE);

	return ERR_PTR(-ENOMEM);

done:
	return etr_buf;
}

static struct etr_buf *
get_perf_etr_buf_cpu_wide(struct tmc_drvdata *drvdata,
			  struct perf_event *event, int nr_pages,
			  void **pages, bool snapshot)
{
	int ret;
	pid_t pid = task_pid_nr(event->owner);
	struct etr_buf *etr_buf;

retry:
	 

	 
	mutex_lock(&drvdata->idr_mutex);
	etr_buf = idr_find(&drvdata->idr, pid);
	if (etr_buf) {
		refcount_inc(&etr_buf->refcount);
		mutex_unlock(&drvdata->idr_mutex);
		return etr_buf;
	}

	 
	mutex_unlock(&drvdata->idr_mutex);

	etr_buf = alloc_etr_buf(drvdata, event, nr_pages, pages, snapshot);
	if (IS_ERR(etr_buf))
		return etr_buf;

	 
	mutex_lock(&drvdata->idr_mutex);
	ret = idr_alloc(&drvdata->idr, etr_buf, pid, pid + 1, GFP_KERNEL);
	mutex_unlock(&drvdata->idr_mutex);

	 
	if (ret == -ENOSPC) {
		tmc_free_etr_buf(etr_buf);
		goto retry;
	}

	 
	if (ret == -ENOMEM) {
		tmc_free_etr_buf(etr_buf);
		return ERR_PTR(ret);
	}


	return etr_buf;
}

static struct etr_buf *
get_perf_etr_buf_per_thread(struct tmc_drvdata *drvdata,
			    struct perf_event *event, int nr_pages,
			    void **pages, bool snapshot)
{
	 
	return alloc_etr_buf(drvdata, event, nr_pages, pages, snapshot);
}

static struct etr_buf *
get_perf_etr_buf(struct tmc_drvdata *drvdata, struct perf_event *event,
		 int nr_pages, void **pages, bool snapshot)
{
	if (event->cpu == -1)
		return get_perf_etr_buf_per_thread(drvdata, event, nr_pages,
						   pages, snapshot);

	return get_perf_etr_buf_cpu_wide(drvdata, event, nr_pages,
					 pages, snapshot);
}

static struct etr_perf_buffer *
tmc_etr_setup_perf_buf(struct tmc_drvdata *drvdata, struct perf_event *event,
		       int nr_pages, void **pages, bool snapshot)
{
	int node;
	struct etr_buf *etr_buf;
	struct etr_perf_buffer *etr_perf;

	node = (event->cpu == -1) ? NUMA_NO_NODE : cpu_to_node(event->cpu);

	etr_perf = kzalloc_node(sizeof(*etr_perf), GFP_KERNEL, node);
	if (!etr_perf)
		return ERR_PTR(-ENOMEM);

	etr_buf = get_perf_etr_buf(drvdata, event, nr_pages, pages, snapshot);
	if (!IS_ERR(etr_buf))
		goto done;

	kfree(etr_perf);
	return ERR_PTR(-ENOMEM);

done:
	 
	etr_perf->drvdata = drvdata;
	etr_perf->etr_buf = etr_buf;

	return etr_perf;
}


static void *tmc_alloc_etr_buffer(struct coresight_device *csdev,
				  struct perf_event *event, void **pages,
				  int nr_pages, bool snapshot)
{
	struct etr_perf_buffer *etr_perf;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	etr_perf = tmc_etr_setup_perf_buf(drvdata, event,
					  nr_pages, pages, snapshot);
	if (IS_ERR(etr_perf)) {
		dev_dbg(&csdev->dev, "Unable to allocate ETR buffer\n");
		return NULL;
	}

	etr_perf->pid = task_pid_nr(event->owner);
	etr_perf->snapshot = snapshot;
	etr_perf->nr_pages = nr_pages;
	etr_perf->pages = pages;

	return etr_perf;
}

static void tmc_free_etr_buffer(void *config)
{
	struct etr_perf_buffer *etr_perf = config;
	struct tmc_drvdata *drvdata = etr_perf->drvdata;
	struct etr_buf *buf, *etr_buf = etr_perf->etr_buf;

	if (!etr_buf)
		goto free_etr_perf_buffer;

	mutex_lock(&drvdata->idr_mutex);
	 
	if (!refcount_dec_and_test(&etr_buf->refcount)) {
		mutex_unlock(&drvdata->idr_mutex);
		goto free_etr_perf_buffer;
	}

	 
	buf = idr_remove(&drvdata->idr, etr_perf->pid);
	mutex_unlock(&drvdata->idr_mutex);

	 
	if (buf && WARN_ON(buf != etr_buf))
		goto free_etr_perf_buffer;

	tmc_free_etr_buf(etr_perf->etr_buf);

free_etr_perf_buffer:
	kfree(etr_perf);
}

 
static void tmc_etr_sync_perf_buffer(struct etr_perf_buffer *etr_perf,
				     unsigned long head,
				     unsigned long src_offset,
				     unsigned long to_copy)
{
	long bytes;
	long pg_idx, pg_offset;
	char **dst_pages, *src_buf;
	struct etr_buf *etr_buf = etr_perf->etr_buf;

	head = PERF_IDX2OFF(head, etr_perf);
	pg_idx = head >> PAGE_SHIFT;
	pg_offset = head & (PAGE_SIZE - 1);
	dst_pages = (char **)etr_perf->pages;

	while (to_copy > 0) {
		 
		if (src_offset >= etr_buf->size)
			src_offset -= etr_buf->size;
		bytes = tmc_etr_buf_get_data(etr_buf, src_offset, to_copy,
					     &src_buf);
		if (WARN_ON_ONCE(bytes <= 0))
			break;
		bytes = min(bytes, (long)(PAGE_SIZE - pg_offset));

		memcpy(dst_pages[pg_idx] + pg_offset, src_buf, bytes);

		to_copy -= bytes;

		 
		pg_offset += bytes;
		if (pg_offset == PAGE_SIZE) {
			pg_offset = 0;
			if (++pg_idx == etr_perf->nr_pages)
				pg_idx = 0;
		}

		 
		src_offset += bytes;
	}
}

 
static unsigned long
tmc_update_etr_buffer(struct coresight_device *csdev,
		      struct perf_output_handle *handle,
		      void *config)
{
	bool lost = false;
	unsigned long flags, offset, size = 0;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct etr_perf_buffer *etr_perf = config;
	struct etr_buf *etr_buf = etr_perf->etr_buf;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	 
	if (atomic_read(&csdev->refcnt) != 1) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		goto out;
	}

	if (WARN_ON(drvdata->perf_buf != etr_buf)) {
		lost = true;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		goto out;
	}

	CS_UNLOCK(drvdata->base);

	tmc_flush_and_stop(drvdata);
	tmc_sync_etr_buf(drvdata);

	CS_LOCK(drvdata->base);
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	lost = etr_buf->full;
	offset = etr_buf->offset;
	size = etr_buf->len;

	 
	if (!etr_perf->snapshot && size > handle->size) {
		u32 mask = tmc_get_memwidth_mask(drvdata);

		 
		size = handle->size & mask;
		offset = etr_buf->offset + etr_buf->len - size;

		if (offset >= etr_buf->size)
			offset -= etr_buf->size;
		lost = true;
	}

	 
	if (lost)
		tmc_etr_buf_insert_barrier_packet(etr_buf, offset);
	tmc_etr_sync_perf_buffer(etr_perf, handle->head, offset, size);

	 
	if (etr_perf->snapshot)
		handle->head += size;

	 
	smp_wmb();

out:
	 
	if (!etr_perf->snapshot && lost)
		perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED);
	return size;
}

static int tmc_enable_etr_sink_perf(struct coresight_device *csdev, void *data)
{
	int rc = 0;
	pid_t pid;
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct perf_output_handle *handle = data;
	struct etr_perf_buffer *etr_perf = etm_perf_sink_config(handle);

	spin_lock_irqsave(&drvdata->spinlock, flags);
	  
	if (drvdata->mode == CS_MODE_SYSFS) {
		rc = -EBUSY;
		goto unlock_out;
	}

	if (WARN_ON(!etr_perf || !etr_perf->etr_buf)) {
		rc = -EINVAL;
		goto unlock_out;
	}

	 
	pid = etr_perf->pid;

	 
	if (drvdata->pid != -1 && drvdata->pid != pid) {
		rc = -EBUSY;
		goto unlock_out;
	}

	 
	if (drvdata->pid == pid) {
		atomic_inc(&csdev->refcnt);
		goto unlock_out;
	}

	rc = tmc_etr_enable_hw(drvdata, etr_perf->etr_buf);
	if (!rc) {
		 
		drvdata->pid = pid;
		drvdata->mode = CS_MODE_PERF;
		drvdata->perf_buf = etr_perf->etr_buf;
		atomic_inc(&csdev->refcnt);
	}

unlock_out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	return rc;
}

static int tmc_enable_etr_sink(struct coresight_device *csdev,
			       enum cs_mode mode, void *data)
{
	switch (mode) {
	case CS_MODE_SYSFS:
		return tmc_enable_etr_sink_sysfs(csdev);
	case CS_MODE_PERF:
		return tmc_enable_etr_sink_perf(csdev, data);
	default:
		return -EINVAL;
	}
}

static int tmc_disable_etr_sink(struct coresight_device *csdev)
{
	unsigned long flags;
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock_irqsave(&drvdata->spinlock, flags);

	if (drvdata->reading) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return -EBUSY;
	}

	if (atomic_dec_return(&csdev->refcnt)) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return -EBUSY;
	}

	 
	WARN_ON_ONCE(drvdata->mode == CS_MODE_DISABLED);
	tmc_etr_disable_hw(drvdata);
	 
	drvdata->pid = -1;
	drvdata->mode = CS_MODE_DISABLED;
	 
	drvdata->perf_buf = NULL;

	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_dbg(&csdev->dev, "TMC-ETR disabled\n");
	return 0;
}

static const struct coresight_ops_sink tmc_etr_sink_ops = {
	.enable		= tmc_enable_etr_sink,
	.disable	= tmc_disable_etr_sink,
	.alloc_buffer	= tmc_alloc_etr_buffer,
	.update_buffer	= tmc_update_etr_buffer,
	.free_buffer	= tmc_free_etr_buffer,
};

const struct coresight_ops tmc_etr_cs_ops = {
	.sink_ops	= &tmc_etr_sink_ops,
};

int tmc_read_prepare_etr(struct tmc_drvdata *drvdata)
{
	int ret = 0;
	unsigned long flags;

	 
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETR))
		return -EINVAL;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	 
	if (!drvdata->sysfs_buf) {
		ret = -EINVAL;
		goto out;
	}

	 
	if (drvdata->mode == CS_MODE_SYSFS)
		__tmc_etr_disable_hw(drvdata);

	drvdata->reading = true;
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return ret;
}

int tmc_read_unprepare_etr(struct tmc_drvdata *drvdata)
{
	unsigned long flags;
	struct etr_buf *sysfs_buf = NULL;

	 
	if (WARN_ON_ONCE(drvdata->config_type != TMC_CONFIG_TYPE_ETR))
		return -EINVAL;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	 
	if (drvdata->mode == CS_MODE_SYSFS) {
		 
		__tmc_etr_enable_hw(drvdata);
	} else {
		 
		sysfs_buf = drvdata->sysfs_buf;
		drvdata->sysfs_buf = NULL;
	}

	drvdata->reading = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	 
	if (sysfs_buf)
		tmc_etr_free_sysfs_buf(sysfs_buf);

	return 0;
}
