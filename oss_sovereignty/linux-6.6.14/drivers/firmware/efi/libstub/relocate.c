

#include <linux/efi.h>
#include <asm/efi.h>

#include "efistub.h"

 
efi_status_t efi_low_alloc_above(unsigned long size, unsigned long align,
				 unsigned long *addr, unsigned long min)
{
	struct efi_boot_memmap *map;
	efi_status_t status;
	unsigned long nr_pages;
	int i;

	status = efi_get_memory_map(&map, false);
	if (status != EFI_SUCCESS)
		goto fail;

	 
	if (align < EFI_ALLOC_ALIGN)
		align = EFI_ALLOC_ALIGN;

	size = round_up(size, EFI_ALLOC_ALIGN);
	nr_pages = size / EFI_PAGE_SIZE;
	for (i = 0; i < map->map_size / map->desc_size; i++) {
		efi_memory_desc_t *desc;
		unsigned long m = (unsigned long)map->map;
		u64 start, end;

		desc = efi_early_memdesc_ptr(m, map->desc_size, i);

		if (desc->type != EFI_CONVENTIONAL_MEMORY)
			continue;

		if (efi_soft_reserve_enabled() &&
		    (desc->attribute & EFI_MEMORY_SP))
			continue;

		if (desc->num_pages < nr_pages)
			continue;

		start = desc->phys_addr;
		end = start + desc->num_pages * EFI_PAGE_SIZE;

		if (start < min)
			start = min;

		start = round_up(start, align);
		if ((start + size) > end)
			continue;

		status = efi_bs_call(allocate_pages, EFI_ALLOCATE_ADDRESS,
				     EFI_LOADER_DATA, nr_pages, &start);
		if (status == EFI_SUCCESS) {
			*addr = start;
			break;
		}
	}

	if (i == map->map_size / map->desc_size)
		status = EFI_NOT_FOUND;

	efi_bs_call(free_pool, map);
fail:
	return status;
}

 
efi_status_t efi_relocate_kernel(unsigned long *image_addr,
				 unsigned long image_size,
				 unsigned long alloc_size,
				 unsigned long preferred_addr,
				 unsigned long alignment,
				 unsigned long min_addr)
{
	unsigned long cur_image_addr;
	unsigned long new_addr = 0;
	efi_status_t status;
	unsigned long nr_pages;
	efi_physical_addr_t efi_addr = preferred_addr;

	if (!image_addr || !image_size || !alloc_size)
		return EFI_INVALID_PARAMETER;
	if (alloc_size < image_size)
		return EFI_INVALID_PARAMETER;

	cur_image_addr = *image_addr;

	 
	nr_pages = round_up(alloc_size, EFI_ALLOC_ALIGN) / EFI_PAGE_SIZE;
	status = efi_bs_call(allocate_pages, EFI_ALLOCATE_ADDRESS,
			     EFI_LOADER_DATA, nr_pages, &efi_addr);
	new_addr = efi_addr;
	 
	if (status != EFI_SUCCESS) {
		status = efi_low_alloc_above(alloc_size, alignment, &new_addr,
					     min_addr);
	}
	if (status != EFI_SUCCESS) {
		efi_err("Failed to allocate usable memory for kernel.\n");
		return status;
	}

	 
	memcpy((void *)new_addr, (void *)cur_image_addr, image_size);

	 
	*image_addr = new_addr;

	return status;
}
