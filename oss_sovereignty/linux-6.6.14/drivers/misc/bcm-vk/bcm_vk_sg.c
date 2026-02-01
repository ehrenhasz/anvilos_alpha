
 
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/pgtable.h>
#include <linux/vmalloc.h>

#include <asm/page.h>
#include <asm/unaligned.h>

#include <uapi/linux/misc/bcm_vk.h>

#include "bcm_vk.h"
#include "bcm_vk_msg.h"
#include "bcm_vk_sg.h"

 
#define BCM_VK_MAX_SGL_CHUNK SZ_16M

static int bcm_vk_dma_alloc(struct device *dev,
			    struct bcm_vk_dma *dma,
			    int dir,
			    struct _vk_data *vkdata);
static int bcm_vk_dma_free(struct device *dev, struct bcm_vk_dma *dma);

 
 

static int bcm_vk_dma_alloc(struct device *dev,
			    struct bcm_vk_dma *dma,
			    int direction,
			    struct _vk_data *vkdata)
{
	dma_addr_t addr, sg_addr;
	int err;
	int i;
	int offset;
	u32 size;
	u32 remaining_size;
	u32 transfer_size;
	u64 data;
	unsigned long first, last;
	struct _vk_data *sgdata;

	 
	data = get_unaligned(&vkdata->address);

	 
	offset = offset_in_page(data);

	 
	first = (data & PAGE_MASK) >> PAGE_SHIFT;
	last  = ((data + vkdata->size - 1) & PAGE_MASK) >> PAGE_SHIFT;
	dma->nr_pages = last - first + 1;

	 
	dma->pages = kmalloc_array(dma->nr_pages,
				   sizeof(struct page *),
				   GFP_KERNEL);
	if (!dma->pages)
		return -ENOMEM;

	dev_dbg(dev, "Alloc DMA Pages [0x%llx+0x%x => %d pages]\n",
		data, vkdata->size, dma->nr_pages);

	dma->direction = direction;

	 
	err = get_user_pages_fast(data & PAGE_MASK,
				  dma->nr_pages,
				  direction == DMA_FROM_DEVICE,
				  dma->pages);
	if (err != dma->nr_pages) {
		dma->nr_pages = (err >= 0) ? err : 0;
		dev_err(dev, "get_user_pages_fast, err=%d [%d]\n",
			err, dma->nr_pages);
		return err < 0 ? err : -EINVAL;
	}

	 
	dma->sglen = (dma->nr_pages * sizeof(*sgdata)) +
		     (sizeof(u32) * SGLIST_VKDATA_START);

	 
	dma->sglist = dma_alloc_coherent(dev,
					 dma->sglen,
					 &dma->handle,
					 GFP_KERNEL);
	if (!dma->sglist)
		return -ENOMEM;

	dma->sglist[SGLIST_NUM_SG] = 0;
	dma->sglist[SGLIST_TOTALSIZE] = vkdata->size;
	remaining_size = vkdata->size;
	sgdata = (struct _vk_data *)&dma->sglist[SGLIST_VKDATA_START];

	 
	size = min_t(size_t, PAGE_SIZE - offset, remaining_size);
	remaining_size -= size;
	sg_addr = dma_map_page(dev,
			       dma->pages[0],
			       offset,
			       size,
			       dma->direction);
	transfer_size = size;
	if (unlikely(dma_mapping_error(dev, sg_addr))) {
		__free_page(dma->pages[0]);
		return -EIO;
	}

	for (i = 1; i < dma->nr_pages; i++) {
		size = min_t(size_t, PAGE_SIZE, remaining_size);
		remaining_size -= size;
		addr = dma_map_page(dev,
				    dma->pages[i],
				    0,
				    size,
				    dma->direction);
		if (unlikely(dma_mapping_error(dev, addr))) {
			__free_page(dma->pages[i]);
			return -EIO;
		}

		 
		if ((addr == (sg_addr + transfer_size)) &&
		    ((transfer_size + size) <= BCM_VK_MAX_SGL_CHUNK)) {
			 
			transfer_size += size;
		} else {
			 
			sgdata->size = transfer_size;
			put_unaligned(sg_addr, (u64 *)&sgdata->address);
			dma->sglist[SGLIST_NUM_SG]++;

			 
			sgdata++;
			sg_addr = addr;
			transfer_size = size;
		}
	}
	 
	sgdata->size = transfer_size;
	put_unaligned(sg_addr, (u64 *)&sgdata->address);
	dma->sglist[SGLIST_NUM_SG]++;

	 
	put_unaligned((u64)dma->handle, &vkdata->address);
	vkdata->size = (dma->sglist[SGLIST_NUM_SG] * sizeof(*sgdata)) +
		       (sizeof(u32) * SGLIST_VKDATA_START);

#ifdef BCM_VK_DUMP_SGLIST
	dev_dbg(dev,
		"sgl 0x%llx handle 0x%llx, sglen: 0x%x sgsize: 0x%x\n",
		(u64)dma->sglist,
		dma->handle,
		dma->sglen,
		vkdata->size);
	for (i = 0; i < vkdata->size / sizeof(u32); i++)
		dev_dbg(dev, "i:0x%x 0x%x\n", i, dma->sglist[i]);
#endif

	return 0;
}

int bcm_vk_sg_alloc(struct device *dev,
		    struct bcm_vk_dma *dma,
		    int dir,
		    struct _vk_data *vkdata,
		    int num)
{
	int i;
	int rc = -EINVAL;

	 
	for (i = 0; i < num; i++) {
		if (vkdata[i].size && vkdata[i].address) {
			 
			rc = bcm_vk_dma_alloc(dev,
					      &dma[i],
					      dir,
					      &vkdata[i]);
		} else if (vkdata[i].size ||
			   vkdata[i].address) {
			 
			dev_err(dev,
				"Invalid vkdata %x 0x%x 0x%llx\n",
				i, vkdata[i].size, vkdata[i].address);
			rc = -EINVAL;
		} else {
			 
			rc = 0;
		}

		if (rc)
			goto fail_alloc;
	}
	return rc;

fail_alloc:
	while (i > 0) {
		i--;
		if (dma[i].sglist)
			bcm_vk_dma_free(dev, &dma[i]);
	}
	return rc;
}

static int bcm_vk_dma_free(struct device *dev, struct bcm_vk_dma *dma)
{
	dma_addr_t addr;
	int i;
	int num_sg;
	u32 size;
	struct _vk_data *vkdata;

	dev_dbg(dev, "free sglist=%p sglen=0x%x\n", dma->sglist, dma->sglen);

	 
	num_sg = dma->sglist[SGLIST_NUM_SG];
	vkdata = (struct _vk_data *)&dma->sglist[SGLIST_VKDATA_START];
	for (i = 0; i < num_sg; i++) {
		size = vkdata[i].size;
		addr = get_unaligned(&vkdata[i].address);

		dma_unmap_page(dev, addr, size, dma->direction);
	}

	 
	dma_free_coherent(dev, dma->sglen, dma->sglist, dma->handle);

	 
	for (i = 0; i < dma->nr_pages; i++)
		put_page(dma->pages[i]);

	 
	kfree(dma->pages);
	dma->sglist = NULL;

	return 0;
}

int bcm_vk_sg_free(struct device *dev, struct bcm_vk_dma *dma, int num,
		   int *proc_cnt)
{
	int i;

	*proc_cnt = 0;
	 
	for (i = 0; i < num; i++) {
		if (dma[i].sglist) {
			bcm_vk_dma_free(dev, &dma[i]);
			*proc_cnt += 1;
		}
	}

	return 0;
}
