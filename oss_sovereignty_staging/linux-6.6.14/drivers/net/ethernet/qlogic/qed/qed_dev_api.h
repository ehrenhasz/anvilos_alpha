 
 

#ifndef _QED_DEV_API_H
#define _QED_DEV_API_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/qed/qed_chain.h>
#include <linux/qed/qed_if.h>
#include "qed_int.h"

 
void qed_init_dp(struct qed_dev *cdev,
		 u32 dp_module,
		 u8 dp_level);

 
void qed_init_struct(struct qed_dev *cdev);

 
void qed_resc_free(struct qed_dev *cdev);

 
int qed_resc_alloc(struct qed_dev *cdev);

 
void qed_resc_setup(struct qed_dev *cdev);

enum qed_override_force_load {
	QED_OVERRIDE_FORCE_LOAD_NONE,
	QED_OVERRIDE_FORCE_LOAD_ALWAYS,
	QED_OVERRIDE_FORCE_LOAD_NEVER,
};

struct qed_drv_load_params {
	 
	bool is_crash_kernel;

	 
	u8 mfw_timeout_val;
#define QED_LOAD_REQ_LOCK_TO_DEFAULT    0
#define QED_LOAD_REQ_LOCK_TO_NONE       255

	 
	bool avoid_eng_reset;

	 
	enum qed_override_force_load override_force_load;
};

struct qed_hw_init_params {
	 
	struct qed_tunnel_info *p_tunn;

	bool b_hw_start;

	 
	enum qed_int_mode int_mode;

	 
	bool allow_npar_tx_switch;

	 
	const u8 *bin_fw_data;

	 
	struct qed_drv_load_params *p_drv_load_params;
};

 
int qed_hw_init(struct qed_dev *cdev, struct qed_hw_init_params *p_params);

 
void qed_hw_timers_stop_all(struct qed_dev *cdev);

 
int qed_hw_stop(struct qed_dev *cdev);

 
int qed_hw_stop_fastpath(struct qed_dev *cdev);

 
int qed_hw_start_fastpath(struct qed_hwfn *p_hwfn);

 
int qed_hw_prepare(struct qed_dev *cdev,
		   int personality);

 
void qed_hw_remove(struct qed_dev *cdev);

 
struct qed_ptt *qed_ptt_acquire(struct qed_hwfn *p_hwfn);

 
struct qed_ptt *qed_ptt_acquire_context(struct qed_hwfn *p_hwfn,
					bool is_atomic);

 
void qed_ptt_release(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt);
void qed_reset_vport_stats(struct qed_dev *cdev);

enum qed_dmae_address_type_t {
	QED_DMAE_ADDRESS_HOST_VIRT,
	QED_DMAE_ADDRESS_HOST_PHYS,
	QED_DMAE_ADDRESS_GRC
};

 
int
qed_dmae_host2grc(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt,
		  u64 source_addr,
		  u32 grc_addr,
		  u32 size_in_dwords,
		  struct qed_dmae_params *p_params);

  
int qed_dmae_grc2host(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		      u32 grc_addr, dma_addr_t dest_addr, u32 size_in_dwords,
		      struct qed_dmae_params *p_params);

 
int qed_dmae_host2host(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       dma_addr_t source_addr,
		       dma_addr_t dest_addr,
		       u32 size_in_dwords, struct qed_dmae_params *p_params);

int qed_chain_alloc(struct qed_dev *cdev, struct qed_chain *chain,
		    struct qed_chain_init_params *params);
void qed_chain_free(struct qed_dev *cdev, struct qed_chain *chain);

 
int qed_fw_l2_queue(struct qed_hwfn *p_hwfn,
		    u16 src_id,
		    u16 *dst_id);

 
int qed_fw_vport(struct qed_hwfn *p_hwfn,
		 u8 src_id,
		 u8 *dst_id);

 
int qed_fw_rss_eng(struct qed_hwfn *p_hwfn,
		   u8 src_id,
		   u8 *dst_id);

 
u8 qed_llh_get_num_ppfid(struct qed_dev *cdev);

enum qed_eng {
	QED_ENG0,
	QED_ENG1,
	QED_BOTH_ENG,
};

 
int qed_llh_set_ppfid_affinity(struct qed_dev *cdev,
			       u8 ppfid, enum qed_eng eng);

 
int qed_llh_set_roce_affinity(struct qed_dev *cdev, enum qed_eng eng);

 
int qed_llh_add_mac_filter(struct qed_dev *cdev,
			   u8 ppfid, const u8 mac_addr[ETH_ALEN]);

 
void qed_llh_remove_mac_filter(struct qed_dev *cdev,
			       u8 ppfid, u8 mac_addr[ETH_ALEN]);

enum qed_llh_prot_filter_type_t {
	QED_LLH_FILTER_ETHERTYPE,
	QED_LLH_FILTER_TCP_SRC_PORT,
	QED_LLH_FILTER_TCP_DEST_PORT,
	QED_LLH_FILTER_TCP_SRC_AND_DEST_PORT,
	QED_LLH_FILTER_UDP_SRC_PORT,
	QED_LLH_FILTER_UDP_DEST_PORT,
	QED_LLH_FILTER_UDP_SRC_AND_DEST_PORT
};

 
int
qed_llh_add_protocol_filter(struct qed_dev *cdev,
			    u8 ppfid,
			    enum qed_llh_prot_filter_type_t type,
			    u16 source_port_or_eth_type, u16 dest_port);

 
void
qed_llh_remove_protocol_filter(struct qed_dev *cdev,
			       u8 ppfid,
			       enum qed_llh_prot_filter_type_t type,
			       u16 source_port_or_eth_type, u16 dest_port);

 
int qed_final_cleanup(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt, u16 id, bool is_vf);

 
int qed_get_queue_coalesce(struct qed_hwfn *p_hwfn, u16 *coal, void *handle);

 
int
qed_set_queue_coalesce(u16 rx_coal, u16 tx_coal, void *p_handle);

 
int qed_pglueb_set_pfid_enable(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt, bool b_enable);

 
int qed_db_recovery_add(struct qed_dev *cdev,
			void __iomem *db_addr,
			void *db_data,
			enum qed_db_rec_width db_width,
			enum qed_db_rec_space db_space);

 
int qed_db_recovery_del(struct qed_dev *cdev,
			void __iomem *db_addr, void *db_data);

const char *qed_hw_get_resc_name(enum qed_resources res_id);
#endif
