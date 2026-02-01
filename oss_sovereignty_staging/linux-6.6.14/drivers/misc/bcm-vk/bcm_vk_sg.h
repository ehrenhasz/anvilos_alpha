 
 

#ifndef BCM_VK_SG_H
#define BCM_VK_SG_H

#include <linux/dma-mapping.h>

struct bcm_vk_dma {
	 
	struct page **pages;
	int nr_pages;

	 
	dma_addr_t handle;
	 
	u32 *sglist;
#define SGLIST_NUM_SG		0
#define SGLIST_TOTALSIZE	1
#define SGLIST_VKDATA_START	2

	int sglen;  
	int direction;
};

struct _vk_data {
	u32 size;     
	u64 address;  
} __packed;

 
int bcm_vk_sg_alloc(struct device *dev,
		    struct bcm_vk_dma *dma,
		    int dir,
		    struct _vk_data *vkdata,
		    int num);

int bcm_vk_sg_free(struct device *dev, struct bcm_vk_dma *dma, int num,
		   int *proc_cnt);

#endif

