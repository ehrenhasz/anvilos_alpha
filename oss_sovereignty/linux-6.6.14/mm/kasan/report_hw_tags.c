
 

#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/types.h>

#include "kasan.h"

const void *kasan_find_first_bad_addr(const void *addr, size_t size)
{
	 
	return kasan_reset_tag(addr);
}

size_t kasan_get_alloc_size(void *object, struct kmem_cache *cache)
{
	size_t size = 0;
	int i = 0;
	u8 memory_tag;

	 

	 
	while (size < cache->object_size) {
		memory_tag = hw_get_mem_tag(object + i * KASAN_GRANULE_SIZE);
		if (memory_tag != KASAN_TAG_INVALID)
			size += KASAN_GRANULE_SIZE;
		else
			return size;
		i++;
	}

	return cache->object_size;
}

void kasan_metadata_fetch_row(char *buffer, void *row)
{
	int i;

	for (i = 0; i < META_BYTES_PER_ROW; i++)
		buffer[i] = hw_get_mem_tag(row + i * KASAN_GRANULE_SIZE);
}

void kasan_print_tags(u8 addr_tag, const void *addr)
{
	u8 memory_tag = hw_get_mem_tag((void *)addr);

	pr_err("Pointer tag: [%02x], memory tag: [%02x]\n",
		addr_tag, memory_tag);
}
