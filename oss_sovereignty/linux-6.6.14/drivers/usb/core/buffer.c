
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/genalloc.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>


 

 
static size_t pool_max[HCD_BUFFER_POOLS] = {
	32, 128, 512, 2048,
};

void __init usb_init_pool_max(void)
{
	 
	if (ARCH_DMA_MINALIGN <= 32)
		;			 
	else if (ARCH_DMA_MINALIGN <= 64)
		pool_max[0] = 64;
	else if (ARCH_DMA_MINALIGN <= 128)
		pool_max[0] = 0;	 
	else
		BUILD_BUG();		 
}

 

 
int hcd_buffer_create(struct usb_hcd *hcd)
{
	char		name[16];
	int		i, size;

	if (hcd->localmem_pool || !hcd_uses_dma(hcd))
		return 0;

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		size = pool_max[i];
		if (!size)
			continue;
		snprintf(name, sizeof(name), "buffer-%d", size);
		hcd->pool[i] = dma_pool_create(name, hcd->self.sysdev,
				size, size, 0);
		if (!hcd->pool[i]) {
			hcd_buffer_destroy(hcd);
			return -ENOMEM;
		}
	}
	return 0;
}


 
void hcd_buffer_destroy(struct usb_hcd *hcd)
{
	int i;

	if (!IS_ENABLED(CONFIG_HAS_DMA))
		return;

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		dma_pool_destroy(hcd->pool[i]);
		hcd->pool[i] = NULL;
	}
}


 

void *hcd_buffer_alloc(
	struct usb_bus		*bus,
	size_t			size,
	gfp_t			mem_flags,
	dma_addr_t		*dma
)
{
	struct usb_hcd		*hcd = bus_to_hcd(bus);
	int			i;

	if (size == 0)
		return NULL;

	if (hcd->localmem_pool)
		return gen_pool_dma_alloc(hcd->localmem_pool, size, dma);

	 
	if (!hcd_uses_dma(hcd)) {
		*dma = ~(dma_addr_t) 0;
		return kmalloc(size, mem_flags);
	}

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		if (size <= pool_max[i])
			return dma_pool_alloc(hcd->pool[i], mem_flags, dma);
	}
	return dma_alloc_coherent(hcd->self.sysdev, size, dma, mem_flags);
}

void hcd_buffer_free(
	struct usb_bus		*bus,
	size_t			size,
	void			*addr,
	dma_addr_t		dma
)
{
	struct usb_hcd		*hcd = bus_to_hcd(bus);
	int			i;

	if (!addr)
		return;

	if (hcd->localmem_pool) {
		gen_pool_free(hcd->localmem_pool, (unsigned long)addr, size);
		return;
	}

	if (!hcd_uses_dma(hcd)) {
		kfree(addr);
		return;
	}

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		if (size <= pool_max[i]) {
			dma_pool_free(hcd->pool[i], addr, dma);
			return;
		}
	}
	dma_free_coherent(hcd->self.sysdev, size, addr, dma);
}

void *hcd_buffer_alloc_pages(struct usb_hcd *hcd,
		size_t size, gfp_t mem_flags, dma_addr_t *dma)
{
	if (size == 0)
		return NULL;

	if (hcd->localmem_pool)
		return gen_pool_dma_alloc_align(hcd->localmem_pool,
				size, dma, PAGE_SIZE);

	 
	if (!hcd_uses_dma(hcd)) {
		*dma = DMA_MAPPING_ERROR;
		return (void *)__get_free_pages(mem_flags,
				get_order(size));
	}

	return dma_alloc_coherent(hcd->self.sysdev,
			size, dma, mem_flags);
}

void hcd_buffer_free_pages(struct usb_hcd *hcd,
		size_t size, void *addr, dma_addr_t dma)
{
	if (!addr)
		return;

	if (hcd->localmem_pool) {
		gen_pool_free(hcd->localmem_pool,
				(unsigned long)addr, size);
		return;
	}

	if (!hcd_uses_dma(hcd)) {
		free_pages((unsigned long)addr, get_order(size));
		return;
	}

	dma_free_coherent(hcd->self.sysdev, size, addr, dma);
}
