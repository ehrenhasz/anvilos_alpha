


#ifndef EFX_TC_COUNTERS_H
#define EFX_TC_COUNTERS_H
#include <linux/refcount.h>
#include "net_driver.h"

#include "mcdi_pcol.h" 

enum efx_tc_counter_type {
	EFX_TC_COUNTER_TYPE_AR = MAE_COUNTER_TYPE_AR,
	EFX_TC_COUNTER_TYPE_CT = MAE_COUNTER_TYPE_CT,
	EFX_TC_COUNTER_TYPE_OR = MAE_COUNTER_TYPE_OR,
	EFX_TC_COUNTER_TYPE_MAX
};

struct efx_tc_counter {
	u32 fw_id; 
	enum efx_tc_counter_type type;
	struct rhash_head linkage; 
	spinlock_t lock; 
	u32 gen; 
	u64 packets, bytes;
	u64 old_packets, old_bytes; 
	
	unsigned long touched;
	struct work_struct work; 
	
	struct list_head users;
};

struct efx_tc_counter_index {
	unsigned long cookie;
	struct rhash_head linkage; 
	refcount_t ref;
	struct efx_tc_counter *cnt;
};


int efx_tc_init_counters(struct efx_nic *efx);
void efx_tc_destroy_counters(struct efx_nic *efx);
void efx_tc_fini_counters(struct efx_nic *efx);

struct efx_tc_counter *efx_tc_flower_allocate_counter(struct efx_nic *efx,
						      int type);
void efx_tc_flower_release_counter(struct efx_nic *efx,
				   struct efx_tc_counter *cnt);
struct efx_tc_counter_index *efx_tc_flower_get_counter_index(
				struct efx_nic *efx, unsigned long cookie,
				enum efx_tc_counter_type type);
void efx_tc_flower_put_counter_index(struct efx_nic *efx,
				     struct efx_tc_counter_index *ctr);
struct efx_tc_counter_index *efx_tc_flower_find_counter_index(
				struct efx_nic *efx, unsigned long cookie);

extern const struct efx_channel_type efx_tc_channel_type;

#endif 
