#ifndef __MMU_PUBLIC_H_INCLUDED__
#define __MMU_PUBLIC_H_INCLUDED__
#include "system_local.h"
#include "device_access.h"
#include "assert_support.h"
void mmu_set_page_table_base_index(
    const mmu_ID_t		ID,
    const hrt_data		base_index);
hrt_data mmu_get_page_table_base_index(
    const mmu_ID_t		ID);
void mmu_invalidate_cache(
    const mmu_ID_t		ID);
void mmu_invalidate_cache_all(void);
static inline void mmu_reg_store(
    const mmu_ID_t		ID,
    const unsigned int	reg,
    const hrt_data		value)
{
	assert(ID < N_MMU_ID);
	assert(MMU_BASE[ID] != (hrt_address) - 1);
	ia_css_device_store_uint32(MMU_BASE[ID] + reg * sizeof(hrt_data), value);
	return;
}
static inline hrt_data mmu_reg_load(
    const mmu_ID_t		ID,
    const unsigned int	reg)
{
	assert(ID < N_MMU_ID);
	assert(MMU_BASE[ID] != (hrt_address) - 1);
	return ia_css_device_load_uint32(MMU_BASE[ID] + reg * sizeof(hrt_data));
}
#endif  
