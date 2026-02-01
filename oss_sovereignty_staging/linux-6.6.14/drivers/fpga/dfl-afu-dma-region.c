
 

#include <linux/dma-mapping.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include "dfl-afu.h"

void afu_dma_region_init(struct dfl_feature_platform_data *pdata)
{
	struct dfl_afu *afu = dfl_fpga_pdata_get_private(pdata);

	afu->dma_regions = RB_ROOT;
}

 
static int afu_dma_pin_pages(struct dfl_feature_platform_data *pdata,
			     struct dfl_afu_dma_region *region)
{
	int npages = region->length >> PAGE_SHIFT;
	struct device *dev = &pdata->dev->dev;
	int ret, pinned;

	ret = account_locked_vm(current->mm, npages, true);
	if (ret)
		return ret;

	region->pages = kcalloc(npages, sizeof(struct page *), GFP_KERNEL);
	if (!region->pages) {
		ret = -ENOMEM;
		goto unlock_vm;
	}

	pinned = pin_user_pages_fast(region->user_addr, npages, FOLL_WRITE,
				     region->pages);
	if (pinned < 0) {
		ret = pinned;
		goto free_pages;
	} else if (pinned != npages) {
		ret = -EFAULT;
		goto unpin_pages;
	}

	dev_dbg(dev, "%d pages pinned\n", pinned);

	return 0;

unpin_pages:
	unpin_user_pages(region->pages, pinned);
free_pages:
	kfree(region->pages);
unlock_vm:
	account_locked_vm(current->mm, npages, false);
	return ret;
}

 
static void afu_dma_unpin_pages(struct dfl_feature_platform_data *pdata,
				struct dfl_afu_dma_region *region)
{
	long npages = region->length >> PAGE_SHIFT;
	struct device *dev = &pdata->dev->dev;

	unpin_user_pages(region->pages, npages);
	kfree(region->pages);
	account_locked_vm(current->mm, npages, false);

	dev_dbg(dev, "%ld pages unpinned\n", npages);
}

 
static bool afu_dma_check_continuous_pages(struct dfl_afu_dma_region *region)
{
	int npages = region->length >> PAGE_SHIFT;
	int i;

	for (i = 0; i < npages - 1; i++)
		if (page_to_pfn(region->pages[i]) + 1 !=
				page_to_pfn(region->pages[i + 1]))
			return false;

	return true;
}

 
static bool dma_region_check_iova(struct dfl_afu_dma_region *region,
				  u64 iova, u64 size)
{
	if (!size && region->iova != iova)
		return false;

	return (region->iova <= iova) &&
		(region->length + region->iova >= iova + size);
}

 
static int afu_dma_region_add(struct dfl_feature_platform_data *pdata,
			      struct dfl_afu_dma_region *region)
{
	struct dfl_afu *afu = dfl_fpga_pdata_get_private(pdata);
	struct rb_node **new, *parent = NULL;

	dev_dbg(&pdata->dev->dev, "add region (iova = %llx)\n",
		(unsigned long long)region->iova);

	new = &afu->dma_regions.rb_node;

	while (*new) {
		struct dfl_afu_dma_region *this;

		this = container_of(*new, struct dfl_afu_dma_region, node);

		parent = *new;

		if (dma_region_check_iova(this, region->iova, region->length))
			return -EEXIST;

		if (region->iova < this->iova)
			new = &((*new)->rb_left);
		else if (region->iova > this->iova)
			new = &((*new)->rb_right);
		else
			return -EEXIST;
	}

	rb_link_node(&region->node, parent, new);
	rb_insert_color(&region->node, &afu->dma_regions);

	return 0;
}

 
static void afu_dma_region_remove(struct dfl_feature_platform_data *pdata,
				  struct dfl_afu_dma_region *region)
{
	struct dfl_afu *afu;

	dev_dbg(&pdata->dev->dev, "del region (iova = %llx)\n",
		(unsigned long long)region->iova);

	afu = dfl_fpga_pdata_get_private(pdata);
	rb_erase(&region->node, &afu->dma_regions);
}

 
void afu_dma_region_destroy(struct dfl_feature_platform_data *pdata)
{
	struct dfl_afu *afu = dfl_fpga_pdata_get_private(pdata);
	struct rb_node *node = rb_first(&afu->dma_regions);
	struct dfl_afu_dma_region *region;

	while (node) {
		region = container_of(node, struct dfl_afu_dma_region, node);

		dev_dbg(&pdata->dev->dev, "del region (iova = %llx)\n",
			(unsigned long long)region->iova);

		rb_erase(node, &afu->dma_regions);

		if (region->iova)
			dma_unmap_page(dfl_fpga_pdata_to_parent(pdata),
				       region->iova, region->length,
				       DMA_BIDIRECTIONAL);

		if (region->pages)
			afu_dma_unpin_pages(pdata, region);

		node = rb_next(node);
		kfree(region);
	}
}

 
struct dfl_afu_dma_region *
afu_dma_region_find(struct dfl_feature_platform_data *pdata, u64 iova, u64 size)
{
	struct dfl_afu *afu = dfl_fpga_pdata_get_private(pdata);
	struct rb_node *node = afu->dma_regions.rb_node;
	struct device *dev = &pdata->dev->dev;

	while (node) {
		struct dfl_afu_dma_region *region;

		region = container_of(node, struct dfl_afu_dma_region, node);

		if (dma_region_check_iova(region, iova, size)) {
			dev_dbg(dev, "find region (iova = %llx)\n",
				(unsigned long long)region->iova);
			return region;
		}

		if (iova < region->iova)
			node = node->rb_left;
		else if (iova > region->iova)
			node = node->rb_right;
		else
			 
			break;
	}

	dev_dbg(dev, "region with iova %llx and size %llx is not found\n",
		(unsigned long long)iova, (unsigned long long)size);

	return NULL;
}

 
static struct dfl_afu_dma_region *
afu_dma_region_find_iova(struct dfl_feature_platform_data *pdata, u64 iova)
{
	return afu_dma_region_find(pdata, iova, 0);
}

 
int afu_dma_map_region(struct dfl_feature_platform_data *pdata,
		       u64 user_addr, u64 length, u64 *iova)
{
	struct dfl_afu_dma_region *region;
	int ret;

	 
	if (!PAGE_ALIGNED(user_addr) || !PAGE_ALIGNED(length) || !length)
		return -EINVAL;

	 
	if (user_addr + length < user_addr)
		return -EINVAL;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->user_addr = user_addr;
	region->length = length;

	 
	ret = afu_dma_pin_pages(pdata, region);
	if (ret) {
		dev_err(&pdata->dev->dev, "failed to pin memory region\n");
		goto free_region;
	}

	 
	if (!afu_dma_check_continuous_pages(region)) {
		dev_err(&pdata->dev->dev, "pages are not continuous\n");
		ret = -EINVAL;
		goto unpin_pages;
	}

	 
	region->iova = dma_map_page(dfl_fpga_pdata_to_parent(pdata),
				    region->pages[0], 0,
				    region->length,
				    DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dfl_fpga_pdata_to_parent(pdata), region->iova)) {
		dev_err(&pdata->dev->dev, "failed to map for dma\n");
		ret = -EFAULT;
		goto unpin_pages;
	}

	*iova = region->iova;

	mutex_lock(&pdata->lock);
	ret = afu_dma_region_add(pdata, region);
	mutex_unlock(&pdata->lock);
	if (ret) {
		dev_err(&pdata->dev->dev, "failed to add dma region\n");
		goto unmap_dma;
	}

	return 0;

unmap_dma:
	dma_unmap_page(dfl_fpga_pdata_to_parent(pdata),
		       region->iova, region->length, DMA_BIDIRECTIONAL);
unpin_pages:
	afu_dma_unpin_pages(pdata, region);
free_region:
	kfree(region);
	return ret;
}

 
int afu_dma_unmap_region(struct dfl_feature_platform_data *pdata, u64 iova)
{
	struct dfl_afu_dma_region *region;

	mutex_lock(&pdata->lock);
	region = afu_dma_region_find_iova(pdata, iova);
	if (!region) {
		mutex_unlock(&pdata->lock);
		return -EINVAL;
	}

	if (region->in_use) {
		mutex_unlock(&pdata->lock);
		return -EBUSY;
	}

	afu_dma_region_remove(pdata, region);
	mutex_unlock(&pdata->lock);

	dma_unmap_page(dfl_fpga_pdata_to_parent(pdata),
		       region->iova, region->length, DMA_BIDIRECTIONAL);
	afu_dma_unpin_pages(pdata, region);
	kfree(region);

	return 0;
}
