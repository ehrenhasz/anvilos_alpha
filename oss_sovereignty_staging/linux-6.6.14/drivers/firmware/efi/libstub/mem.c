

#include <linux/efi.h>
#include <asm/efi.h>

#include "efistub.h"

 
efi_status_t efi_get_memory_map(struct efi_boot_memmap **map,
				bool install_cfg_tbl)
{
	int memtype = install_cfg_tbl ? EFI_ACPI_RECLAIM_MEMORY
				      : EFI_LOADER_DATA;
	efi_guid_t tbl_guid = LINUX_EFI_BOOT_MEMMAP_GUID;
	struct efi_boot_memmap *m, tmp;
	efi_status_t status;
	unsigned long size;

	tmp.map_size = 0;
	status = efi_bs_call(get_memory_map, &tmp.map_size, NULL, &tmp.map_key,
			     &tmp.desc_size, &tmp.desc_ver);
	if (status != EFI_BUFFER_TOO_SMALL)
		return EFI_LOAD_ERROR;

	size = tmp.map_size + tmp.desc_size * EFI_MMAP_NR_SLACK_SLOTS;
	status = efi_bs_call(allocate_pool, memtype, sizeof(*m) + size,
			     (void **)&m);
	if (status != EFI_SUCCESS)
		return status;

	if (install_cfg_tbl) {
		 
		status = efi_bs_call(install_configuration_table, &tbl_guid, m);
		if (status != EFI_SUCCESS)
			goto free_map;
	}

	m->buff_size = m->map_size = size;
	status = efi_bs_call(get_memory_map, &m->map_size, m->map, &m->map_key,
			     &m->desc_size, &m->desc_ver);
	if (status != EFI_SUCCESS)
		goto uninstall_table;

	*map = m;
	return EFI_SUCCESS;

uninstall_table:
	if (install_cfg_tbl)
		efi_bs_call(install_configuration_table, &tbl_guid, NULL);
free_map:
	efi_bs_call(free_pool, m);
	return status;
}

 
efi_status_t efi_allocate_pages(unsigned long size, unsigned long *addr,
				unsigned long max)
{
	efi_physical_addr_t alloc_addr;
	efi_status_t status;

	max = min(max, EFI_ALLOC_LIMIT);

	if (EFI_ALLOC_ALIGN > EFI_PAGE_SIZE)
		return efi_allocate_pages_aligned(size, addr, max,
						  EFI_ALLOC_ALIGN,
						  EFI_LOADER_DATA);

	alloc_addr = ALIGN_DOWN(max + 1, EFI_ALLOC_ALIGN) - 1;
	status = efi_bs_call(allocate_pages, EFI_ALLOCATE_MAX_ADDRESS,
			     EFI_LOADER_DATA, DIV_ROUND_UP(size, EFI_PAGE_SIZE),
			     &alloc_addr);
	if (status != EFI_SUCCESS)
		return status;

	*addr = alloc_addr;
	return EFI_SUCCESS;
}

 
void efi_free(unsigned long size, unsigned long addr)
{
	unsigned long nr_pages;

	if (!size)
		return;

	nr_pages = round_up(size, EFI_ALLOC_ALIGN) / EFI_PAGE_SIZE;
	efi_bs_call(free_pages, addr, nr_pages);
}
