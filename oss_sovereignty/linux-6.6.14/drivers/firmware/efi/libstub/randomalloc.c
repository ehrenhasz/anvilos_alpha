
 

#include <linux/efi.h>
#include <linux/log2.h>
#include <asm/efi.h>

#include "efistub.h"

 
static unsigned long get_entry_num_slots(efi_memory_desc_t *md,
					 unsigned long size,
					 unsigned long align_shift,
					 u64 alloc_limit)
{
	unsigned long align = 1UL << align_shift;
	u64 first_slot, last_slot, region_end;

	if (md->type != EFI_CONVENTIONAL_MEMORY)
		return 0;

	if (efi_soft_reserve_enabled() &&
	    (md->attribute & EFI_MEMORY_SP))
		return 0;

	region_end = min(md->phys_addr + md->num_pages * EFI_PAGE_SIZE - 1,
			 alloc_limit);
	if (region_end < size)
		return 0;

	first_slot = round_up(md->phys_addr, align);
	last_slot = round_down(region_end - size + 1, align);

	if (first_slot > last_slot)
		return 0;

	return ((unsigned long)(last_slot - first_slot) >> align_shift) + 1;
}

 
#define MD_NUM_SLOTS(md)	((md)->virt_addr)

efi_status_t efi_random_alloc(unsigned long size,
			      unsigned long align,
			      unsigned long *addr,
			      unsigned long random_seed,
			      int memory_type,
			      unsigned long alloc_limit)
{
	unsigned long total_slots = 0, target_slot;
	unsigned long total_mirrored_slots = 0;
	struct efi_boot_memmap *map;
	efi_status_t status;
	int map_offset;

	status = efi_get_memory_map(&map, false);
	if (status != EFI_SUCCESS)
		return status;

	if (align < EFI_ALLOC_ALIGN)
		align = EFI_ALLOC_ALIGN;

	size = round_up(size, EFI_ALLOC_ALIGN);

	 
	for (map_offset = 0; map_offset < map->map_size; map_offset += map->desc_size) {
		efi_memory_desc_t *md = (void *)map->map + map_offset;
		unsigned long slots;

		slots = get_entry_num_slots(md, size, ilog2(align), alloc_limit);
		MD_NUM_SLOTS(md) = slots;
		total_slots += slots;
		if (md->attribute & EFI_MEMORY_MORE_RELIABLE)
			total_mirrored_slots += slots;
	}

	 
	if (total_mirrored_slots > 0)
		total_slots = total_mirrored_slots;

	 
	target_slot = (total_slots * (u64)(random_seed & U32_MAX)) >> 32;

	 
	status = EFI_OUT_OF_RESOURCES;
	for (map_offset = 0; map_offset < map->map_size; map_offset += map->desc_size) {
		efi_memory_desc_t *md = (void *)map->map + map_offset;
		efi_physical_addr_t target;
		unsigned long pages;

		if (total_mirrored_slots > 0 &&
		    !(md->attribute & EFI_MEMORY_MORE_RELIABLE))
			continue;

		if (target_slot >= MD_NUM_SLOTS(md)) {
			target_slot -= MD_NUM_SLOTS(md);
			continue;
		}

		target = round_up(md->phys_addr, align) + target_slot * align;
		pages = size / EFI_PAGE_SIZE;

		status = efi_bs_call(allocate_pages, EFI_ALLOCATE_ADDRESS,
				     memory_type, pages, &target);
		if (status == EFI_SUCCESS)
			*addr = target;
		break;
	}

	efi_bs_call(free_pool, map);

	return status;
}
