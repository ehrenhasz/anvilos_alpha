
 
#include <linux/efi.h>

#include "efistub.h"

 
u32 efi_kaslr_get_phys_seed(efi_handle_t image_handle)
{
	efi_status_t status;
	u32 phys_seed;
	efi_guid_t li_fixed_proto = LINUX_EFI_LOADED_IMAGE_FIXED_GUID;
	void *p;

	if (!IS_ENABLED(CONFIG_RANDOMIZE_BASE))
		return 0;

	if (efi_nokaslr) {
		efi_info("KASLR disabled on kernel command line\n");
	} else if (efi_bs_call(handle_protocol, image_handle,
			       &li_fixed_proto, &p) == EFI_SUCCESS) {
		efi_info("Image placement fixed by loader\n");
	} else {
		status = efi_get_random_bytes(sizeof(phys_seed),
					      (u8 *)&phys_seed);
		if (status == EFI_SUCCESS) {
			return phys_seed;
		} else if (status == EFI_NOT_FOUND) {
			efi_info("EFI_RNG_PROTOCOL unavailable\n");
			efi_nokaslr = true;
		} else if (status != EFI_SUCCESS) {
			efi_err("efi_get_random_bytes() failed (0x%lx)\n",
				status);
			efi_nokaslr = true;
		}
	}

	return 0;
}

 
static bool check_image_region(u64 base, u64 size)
{
	struct efi_boot_memmap *map;
	efi_status_t status;
	bool ret = false;
	int map_offset;

	status = efi_get_memory_map(&map, false);
	if (status != EFI_SUCCESS)
		return false;

	for (map_offset = 0; map_offset < map->map_size; map_offset += map->desc_size) {
		efi_memory_desc_t *md = (void *)map->map + map_offset;
		u64 end = md->phys_addr + md->num_pages * EFI_PAGE_SIZE;

		 
		if (base >= md->phys_addr && base < end) {
			ret = (base + size) <= end;
			break;
		}
	}

	efi_bs_call(free_pool, map);

	return ret;
}

 
efi_status_t efi_kaslr_relocate_kernel(unsigned long *image_addr,
				       unsigned long *reserve_addr,
				       unsigned long *reserve_size,
				       unsigned long kernel_size,
				       unsigned long kernel_codesize,
				       unsigned long kernel_memsize,
				       u32 phys_seed)
{
	efi_status_t status;
	u64 min_kimg_align = efi_get_kimg_min_align();

	if (IS_ENABLED(CONFIG_RANDOMIZE_BASE) && phys_seed != 0) {
		 
		status = efi_random_alloc(*reserve_size, min_kimg_align,
					  reserve_addr, phys_seed,
					  EFI_LOADER_CODE, EFI_ALLOC_LIMIT);
		if (status != EFI_SUCCESS)
			efi_warn("efi_random_alloc() failed: 0x%lx\n", status);
	} else {
		status = EFI_OUT_OF_RESOURCES;
	}

	if (status != EFI_SUCCESS) {
		if (!check_image_region(*image_addr, kernel_memsize)) {
			efi_err("FIRMWARE BUG: Image BSS overlaps adjacent EFI memory region\n");
		} else if (IS_ALIGNED(*image_addr, min_kimg_align) &&
			   (unsigned long)_end < EFI_ALLOC_LIMIT) {
			 
			*reserve_size = 0;
			return EFI_SUCCESS;
		}

		status = efi_allocate_pages_aligned(*reserve_size, reserve_addr,
						    ULONG_MAX, min_kimg_align,
						    EFI_LOADER_CODE);

		if (status != EFI_SUCCESS) {
			efi_err("Failed to relocate kernel\n");
			*reserve_size = 0;
			return status;
		}
	}

	memcpy((void *)*reserve_addr, (void *)*image_addr, kernel_size);
	*image_addr = *reserve_addr;
	efi_icache_sync(*image_addr, *image_addr + kernel_codesize);
	efi_remap_image(*image_addr, *reserve_size, kernel_codesize);

	return status;
}
