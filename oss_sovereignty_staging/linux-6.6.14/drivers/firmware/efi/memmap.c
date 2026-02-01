
 

#define pr_fmt(fmt) "efi: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/efi.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/slab.h>

#include <asm/early_ioremap.h>
#include <asm/efi.h>

#ifndef __efi_memmap_free
#define __efi_memmap_free(phys, size, flags) do { } while (0)
#endif

 
int __init __efi_memmap_init(struct efi_memory_map_data *data)
{
	struct efi_memory_map map;
	phys_addr_t phys_map;

	phys_map = data->phys_map;

	if (data->flags & EFI_MEMMAP_LATE)
		map.map = memremap(phys_map, data->size, MEMREMAP_WB);
	else
		map.map = early_memremap(phys_map, data->size);

	if (!map.map) {
		pr_err("Could not map the memory map!\n");
		return -ENOMEM;
	}

	if (efi.memmap.flags & (EFI_MEMMAP_MEMBLOCK | EFI_MEMMAP_SLAB))
		__efi_memmap_free(efi.memmap.phys_map,
				  efi.memmap.desc_size * efi.memmap.nr_map,
				  efi.memmap.flags);

	map.phys_map = data->phys_map;
	map.nr_map = data->size / data->desc_size;
	map.map_end = map.map + data->size;

	map.desc_version = data->desc_version;
	map.desc_size = data->desc_size;
	map.flags = data->flags;

	set_bit(EFI_MEMMAP, &efi.flags);

	efi.memmap = map;

	return 0;
}

 
int __init efi_memmap_init_early(struct efi_memory_map_data *data)
{
	 
	WARN_ON(efi.memmap.flags & EFI_MEMMAP_LATE);

	data->flags = 0;
	return __efi_memmap_init(data);
}

void __init efi_memmap_unmap(void)
{
	if (!efi_enabled(EFI_MEMMAP))
		return;

	if (!(efi.memmap.flags & EFI_MEMMAP_LATE)) {
		unsigned long size;

		size = efi.memmap.desc_size * efi.memmap.nr_map;
		early_memunmap(efi.memmap.map, size);
	} else {
		memunmap(efi.memmap.map);
	}

	efi.memmap.map = NULL;
	clear_bit(EFI_MEMMAP, &efi.flags);
}

 
int __init efi_memmap_init_late(phys_addr_t addr, unsigned long size)
{
	struct efi_memory_map_data data = {
		.phys_map = addr,
		.size = size,
		.flags = EFI_MEMMAP_LATE,
	};

	 
	WARN_ON(efi.memmap.map);

	 
	WARN_ON(efi.memmap.flags & EFI_MEMMAP_LATE);

	 
	data.desc_version = efi.memmap.desc_version;
	data.desc_size = efi.memmap.desc_size;

	return __efi_memmap_init(&data);
}
