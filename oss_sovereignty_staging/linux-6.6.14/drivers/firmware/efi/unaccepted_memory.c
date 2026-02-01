

#include <linux/efi.h>
#include <linux/memblock.h>
#include <linux/spinlock.h>
#include <asm/unaccepted_memory.h>

 
static DEFINE_SPINLOCK(unaccepted_memory_lock);

struct accept_range {
	struct list_head list;
	unsigned long start;
	unsigned long end;
};

static LIST_HEAD(accepting_list);

 
void accept_memory(phys_addr_t start, phys_addr_t end)
{
	struct efi_unaccepted_memory *unaccepted;
	unsigned long range_start, range_end;
	struct accept_range range, *entry;
	unsigned long flags;
	u64 unit_size;

	unaccepted = efi_get_unaccepted_table();
	if (!unaccepted)
		return;

	unit_size = unaccepted->unit_size;

	 
	if (start < unaccepted->phys_base)
		start = unaccepted->phys_base;
	if (end < unaccepted->phys_base)
		return;

	 
	start -= unaccepted->phys_base;
	end -= unaccepted->phys_base;

	 
	if (!(end % unit_size))
		end += unit_size;

	 
	if (end > unaccepted->size * unit_size * BITS_PER_BYTE)
		end = unaccepted->size * unit_size * BITS_PER_BYTE;

	range.start = start / unit_size;
	range.end = DIV_ROUND_UP(end, unit_size);
retry:
	spin_lock_irqsave(&unaccepted_memory_lock, flags);

	 
	list_for_each_entry(entry, &accepting_list, list) {
		if (entry->end <= range.start)
			continue;
		if (entry->start >= range.end)
			continue;

		 
		spin_unlock_irqrestore(&unaccepted_memory_lock, flags);
		goto retry;
	}

	 
	list_add(&range.list, &accepting_list);

	range_start = range.start;
	for_each_set_bitrange_from(range_start, range_end, unaccepted->bitmap,
				   range.end) {
		unsigned long phys_start, phys_end;
		unsigned long len = range_end - range_start;

		phys_start = range_start * unit_size + unaccepted->phys_base;
		phys_end = range_end * unit_size + unaccepted->phys_base;

		 
		spin_unlock(&unaccepted_memory_lock);

		arch_accept_memory(phys_start, phys_end);

		spin_lock(&unaccepted_memory_lock);
		bitmap_clear(unaccepted->bitmap, range_start, len);
	}

	list_del(&range.list);
	spin_unlock_irqrestore(&unaccepted_memory_lock, flags);
}

bool range_contains_unaccepted_memory(phys_addr_t start, phys_addr_t end)
{
	struct efi_unaccepted_memory *unaccepted;
	unsigned long flags;
	bool ret = false;
	u64 unit_size;

	unaccepted = efi_get_unaccepted_table();
	if (!unaccepted)
		return false;

	unit_size = unaccepted->unit_size;

	 
	if (start < unaccepted->phys_base)
		start = unaccepted->phys_base;
	if (end < unaccepted->phys_base)
		return false;

	 
	start -= unaccepted->phys_base;
	end -= unaccepted->phys_base;

	 
	if (!(end % unit_size))
		end += unit_size;

	 
	if (end > unaccepted->size * unit_size * BITS_PER_BYTE)
		end = unaccepted->size * unit_size * BITS_PER_BYTE;

	spin_lock_irqsave(&unaccepted_memory_lock, flags);
	while (start < end) {
		if (test_bit(start / unit_size, unaccepted->bitmap)) {
			ret = true;
			break;
		}

		start += unit_size;
	}
	spin_unlock_irqrestore(&unaccepted_memory_lock, flags);

	return ret;
}
