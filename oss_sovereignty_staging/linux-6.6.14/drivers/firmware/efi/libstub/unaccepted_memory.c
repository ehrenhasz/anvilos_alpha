

#include <linux/efi.h>
#include <asm/efi.h>
#include "efistub.h"

struct efi_unaccepted_memory *unaccepted_table;

efi_status_t allocate_unaccepted_bitmap(__u32 nr_desc,
					struct efi_boot_memmap *map)
{
	efi_guid_t unaccepted_table_guid = LINUX_EFI_UNACCEPTED_MEM_TABLE_GUID;
	u64 unaccepted_start = ULLONG_MAX, unaccepted_end = 0, bitmap_size;
	efi_status_t status;
	int i;

	 
	unaccepted_table = get_efi_config_table(unaccepted_table_guid);
	if (unaccepted_table) {
		if (unaccepted_table->version != 1) {
			efi_err("Unknown version of unaccepted memory table\n");
			return EFI_UNSUPPORTED;
		}
		return EFI_SUCCESS;
	}

	 
	for (i = 0; i < nr_desc; i++) {
		efi_memory_desc_t *d;
		unsigned long m = (unsigned long)map->map;

		d = efi_early_memdesc_ptr(m, map->desc_size, i);
		if (d->type != EFI_UNACCEPTED_MEMORY)
			continue;

		unaccepted_start = min(unaccepted_start, d->phys_addr);
		unaccepted_end = max(unaccepted_end,
				     d->phys_addr + d->num_pages * PAGE_SIZE);
	}

	if (unaccepted_start == ULLONG_MAX)
		return EFI_SUCCESS;

	unaccepted_start = round_down(unaccepted_start,
				      EFI_UNACCEPTED_UNIT_SIZE);
	unaccepted_end = round_up(unaccepted_end, EFI_UNACCEPTED_UNIT_SIZE);

	 
	bitmap_size = DIV_ROUND_UP(unaccepted_end - unaccepted_start,
				   EFI_UNACCEPTED_UNIT_SIZE * BITS_PER_BYTE);

	status = efi_bs_call(allocate_pool, EFI_ACPI_RECLAIM_MEMORY,
			     sizeof(*unaccepted_table) + bitmap_size,
			     (void **)&unaccepted_table);
	if (status != EFI_SUCCESS) {
		efi_err("Failed to allocate unaccepted memory config table\n");
		return status;
	}

	unaccepted_table->version = 1;
	unaccepted_table->unit_size = EFI_UNACCEPTED_UNIT_SIZE;
	unaccepted_table->phys_base = unaccepted_start;
	unaccepted_table->size = bitmap_size;
	memset(unaccepted_table->bitmap, 0, bitmap_size);

	status = efi_bs_call(install_configuration_table,
			     &unaccepted_table_guid, unaccepted_table);
	if (status != EFI_SUCCESS) {
		efi_bs_call(free_pool, unaccepted_table);
		efi_err("Failed to install unaccepted memory config table!\n");
	}

	return status;
}

 
void process_unaccepted_memory(u64 start, u64 end)
{
	u64 unit_size = unaccepted_table->unit_size;
	u64 unit_mask = unaccepted_table->unit_size - 1;
	u64 bitmap_size = unaccepted_table->size;

	 
	if (end - start < 2 * unit_size) {
		arch_accept_memory(start, end);
		return;
	}

	 

	 
	if (start & unit_mask) {
		arch_accept_memory(start, round_up(start, unit_size));
		start = round_up(start, unit_size);
	}

	 
	if (end & unit_mask) {
		arch_accept_memory(round_down(end, unit_size), end);
		end = round_down(end, unit_size);
	}

	 
	if (start < unaccepted_table->phys_base) {
		arch_accept_memory(start,
				   min(unaccepted_table->phys_base, end));
		start = unaccepted_table->phys_base;
	}

	 
	if (end < unaccepted_table->phys_base)
		return;

	 
	start -= unaccepted_table->phys_base;
	end -= unaccepted_table->phys_base;

	 
	if (end > bitmap_size * unit_size * BITS_PER_BYTE) {
		unsigned long phys_start, phys_end;

		phys_start = bitmap_size * unit_size * BITS_PER_BYTE +
			     unaccepted_table->phys_base;
		phys_end = end + unaccepted_table->phys_base;

		arch_accept_memory(phys_start, phys_end);
		end = bitmap_size * unit_size * BITS_PER_BYTE;
	}

	 
	bitmap_set(unaccepted_table->bitmap,
		   start / unit_size, (end - start) / unit_size);
}

void accept_memory(phys_addr_t start, phys_addr_t end)
{
	unsigned long range_start, range_end;
	unsigned long bitmap_size;
	u64 unit_size;

	if (!unaccepted_table)
		return;

	unit_size = unaccepted_table->unit_size;

	 
	if (start < unaccepted_table->phys_base)
		start = unaccepted_table->phys_base;
	if (end < unaccepted_table->phys_base)
		return;

	 
	start -= unaccepted_table->phys_base;
	end -= unaccepted_table->phys_base;

	 
	if (end > unaccepted_table->size * unit_size * BITS_PER_BYTE)
		end = unaccepted_table->size * unit_size * BITS_PER_BYTE;

	range_start = start / unit_size;
	bitmap_size = DIV_ROUND_UP(end, unit_size);

	for_each_set_bitrange_from(range_start, range_end,
				   unaccepted_table->bitmap, bitmap_size) {
		unsigned long phys_start, phys_end;

		phys_start = range_start * unit_size + unaccepted_table->phys_base;
		phys_end = range_end * unit_size + unaccepted_table->phys_base;

		arch_accept_memory(phys_start, phys_end);
		bitmap_clear(unaccepted_table->bitmap,
			     range_start, range_end - range_start);
	}
}
