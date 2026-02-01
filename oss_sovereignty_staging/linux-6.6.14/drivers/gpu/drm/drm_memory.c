 

 

#include <linux/export.h>
#include <linux/highmem.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>

#include <drm/drm_cache.h>
#include <drm/drm_device.h>

#include "drm_legacy.h"

#if IS_ENABLED(CONFIG_AGP)

#ifdef HAVE_PAGE_AGP
# include <asm/agp.h>
#else
# ifdef __powerpc__
#  define PAGE_AGP	pgprot_noncached_wc(PAGE_KERNEL)
# else
#  define PAGE_AGP	PAGE_KERNEL
# endif
#endif

static void *agp_remap(unsigned long offset, unsigned long size,
		       struct drm_device *dev)
{
	unsigned long i, num_pages =
	    PAGE_ALIGN(size) / PAGE_SIZE;
	struct drm_agp_mem *agpmem;
	struct page **page_map;
	struct page **phys_page_map;
	void *addr;

	size = PAGE_ALIGN(size);

#ifdef __alpha__
	offset -= dev->hose->mem_space->start;
#endif

	list_for_each_entry(agpmem, &dev->agp->memory, head)
		if (agpmem->bound <= offset
		    && (agpmem->bound + (agpmem->pages << PAGE_SHIFT)) >=
		    (offset + size))
			break;
	if (&agpmem->head == &dev->agp->memory)
		return NULL;

	 
	 
	page_map = vmalloc(array_size(num_pages, sizeof(struct page *)));
	if (!page_map)
		return NULL;

	phys_page_map = (agpmem->memory->pages + (offset - agpmem->bound) / PAGE_SIZE);
	for (i = 0; i < num_pages; ++i)
		page_map[i] = phys_page_map[i];
	addr = vmap(page_map, num_pages, VM_IOREMAP, PAGE_AGP);
	vfree(page_map);

	return addr;
}

#else  
static inline void *agp_remap(unsigned long offset, unsigned long size,
			      struct drm_device *dev)
{
	return NULL;
}

#endif  

void drm_legacy_ioremap(struct drm_local_map *map, struct drm_device *dev)
{
	if (dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		map->handle = agp_remap(map->offset, map->size, dev);
	else
		map->handle = ioremap(map->offset, map->size);
}
EXPORT_SYMBOL(drm_legacy_ioremap);

void drm_legacy_ioremap_wc(struct drm_local_map *map, struct drm_device *dev)
{
	if (dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		map->handle = agp_remap(map->offset, map->size, dev);
	else
		map->handle = ioremap_wc(map->offset, map->size);
}
EXPORT_SYMBOL(drm_legacy_ioremap_wc);

void drm_legacy_ioremapfree(struct drm_local_map *map, struct drm_device *dev)
{
	if (!map->handle || !map->size)
		return;

	if (dev->agp && dev->agp->cant_use_aperture && map->type == _DRM_AGP)
		vunmap(map->handle);
	else
		iounmap(map->handle);
}
EXPORT_SYMBOL(drm_legacy_ioremapfree);
