#ifndef EFX_MCDI_FILTERS_H
#define EFX_MCDI_FILTERS_H
#include "net_driver.h"
#include "filter.h"
#include "mcdi_pcol.h"
#define EFX_EF10_FILTER_DEV_UC_MAX	32
#define EFX_EF10_FILTER_DEV_MC_MAX	256
enum efx_mcdi_filter_default_filters {
	EFX_EF10_BCAST,
	EFX_EF10_UCDEF,
	EFX_EF10_MCDEF,
	EFX_EF10_VXLAN4_UCDEF,
	EFX_EF10_VXLAN4_MCDEF,
	EFX_EF10_VXLAN6_UCDEF,
	EFX_EF10_VXLAN6_MCDEF,
	EFX_EF10_NVGRE4_UCDEF,
	EFX_EF10_NVGRE4_MCDEF,
	EFX_EF10_NVGRE6_UCDEF,
	EFX_EF10_NVGRE6_MCDEF,
	EFX_EF10_GENEVE4_UCDEF,
	EFX_EF10_GENEVE4_MCDEF,
	EFX_EF10_GENEVE6_UCDEF,
	EFX_EF10_GENEVE6_MCDEF,
	EFX_EF10_NUM_DEFAULT_FILTERS
};
struct efx_mcdi_filter_vlan {
	struct list_head list;
	u16 vid;
	u16 uc[EFX_EF10_FILTER_DEV_UC_MAX];
	u16 mc[EFX_EF10_FILTER_DEV_MC_MAX];
	u16 default_filters[EFX_EF10_NUM_DEFAULT_FILTERS];
};
struct efx_mcdi_dev_addr {
	u8 addr[ETH_ALEN];
};
struct efx_mcdi_filter_table {
	u32 rx_match_mcdi_flags[
		MC_CMD_GET_PARSER_DISP_INFO_OUT_SUPPORTED_MATCHES_MAXNUM * 2];
	unsigned int rx_match_count;
	bool rx_rss_context_exclusive;
	struct rw_semaphore lock;  
	struct {
		unsigned long spec;	 
#define EFX_EF10_FILTER_FLAG_AUTO_OLD	2UL
#define EFX_EF10_FILTER_FLAGS		3UL
		u64 handle;		 
	} *entry;
	struct efx_mcdi_dev_addr dev_uc_list[EFX_EF10_FILTER_DEV_UC_MAX];
	struct efx_mcdi_dev_addr dev_mc_list[EFX_EF10_FILTER_DEV_MC_MAX];
	int dev_uc_count;
	int dev_mc_count;
	bool uc_promisc;
	bool mc_promisc;
	bool mc_promisc_last;
	bool mc_overflow;  
	bool must_restore_rss_contexts;
	bool must_restore_filters;
	bool mc_chaining;
	bool vlan_filter;
	struct list_head vlan_list;
};
int efx_mcdi_filter_table_probe(struct efx_nic *efx, bool multicast_chaining);
void efx_mcdi_filter_table_down(struct efx_nic *efx);
void efx_mcdi_filter_table_remove(struct efx_nic *efx);
void efx_mcdi_filter_table_restore(struct efx_nic *efx);
void efx_mcdi_filter_table_reset_mc_allocations(struct efx_nic *efx);
#define EFX_MCDI_FILTER_TBL_ROWS 8192
bool efx_mcdi_filter_match_supported(struct efx_mcdi_filter_table *table,
				     bool encap,
				     enum efx_filter_match_flags match_flags);
void efx_mcdi_filter_sync_rx_mode(struct efx_nic *efx);
s32 efx_mcdi_filter_insert(struct efx_nic *efx, struct efx_filter_spec *spec,
			   bool replace_equal);
int efx_mcdi_filter_remove_safe(struct efx_nic *efx,
				enum efx_filter_priority priority,
				u32 filter_id);
int efx_mcdi_filter_get_safe(struct efx_nic *efx,
			     enum efx_filter_priority priority,
			     u32 filter_id, struct efx_filter_spec *spec);
u32 efx_mcdi_filter_count_rx_used(struct efx_nic *efx,
				  enum efx_filter_priority priority);
int efx_mcdi_filter_clear_rx(struct efx_nic *efx,
			     enum efx_filter_priority priority);
u32 efx_mcdi_filter_get_rx_id_limit(struct efx_nic *efx);
s32 efx_mcdi_filter_get_rx_ids(struct efx_nic *efx,
			       enum efx_filter_priority priority,
			       u32 *buf, u32 size);
void efx_mcdi_filter_cleanup_vlans(struct efx_nic *efx);
int efx_mcdi_filter_add_vlan(struct efx_nic *efx, u16 vid);
struct efx_mcdi_filter_vlan *efx_mcdi_filter_find_vlan(struct efx_nic *efx, u16 vid);
void efx_mcdi_filter_del_vlan(struct efx_nic *efx, u16 vid);
void efx_mcdi_rx_free_indir_table(struct efx_nic *efx);
int efx_mcdi_rx_push_rss_context_config(struct efx_nic *efx,
					struct efx_rss_context *ctx,
					const u32 *rx_indir_table,
					const u8 *key);
int efx_mcdi_pf_rx_push_rss_config(struct efx_nic *efx, bool user,
				   const u32 *rx_indir_table,
				   const u8 *key);
int efx_mcdi_vf_rx_push_rss_config(struct efx_nic *efx, bool user,
				   const u32 *rx_indir_table
				   __attribute__ ((unused)),
				   const u8 *key
				   __attribute__ ((unused)));
int efx_mcdi_push_default_indir_table(struct efx_nic *efx,
				      unsigned int rss_spread);
int efx_mcdi_rx_pull_rss_config(struct efx_nic *efx);
int efx_mcdi_rx_pull_rss_context_config(struct efx_nic *efx,
					struct efx_rss_context *ctx);
int efx_mcdi_get_rss_context_flags(struct efx_nic *efx, u32 context,
				   u32 *flags);
void efx_mcdi_set_rss_context_flags(struct efx_nic *efx,
				    struct efx_rss_context *ctx);
void efx_mcdi_rx_restore_rss_contexts(struct efx_nic *efx);
static inline void efx_mcdi_update_rx_scatter(struct efx_nic *efx)
{
}
bool efx_mcdi_filter_rfs_expire_one(struct efx_nic *efx, u32 flow_id,
				    unsigned int filter_idx);
#endif
