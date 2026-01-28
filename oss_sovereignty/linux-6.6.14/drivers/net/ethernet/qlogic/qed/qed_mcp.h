#ifndef _QED_MCP_H
#define _QED_MCP_H
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/qed/qed_fcoe_if.h>
#include "qed_hsi.h"
#include "qed_dev_api.h"
#define QED_MFW_REPORT_STR_SIZE	256
struct qed_mcp_link_speed_params {
	bool					autoneg;
	u32					advertised_speeds;
#define QED_EXT_SPEED_MASK_RES			0x1
#define QED_EXT_SPEED_MASK_1G			0x2
#define QED_EXT_SPEED_MASK_10G			0x4
#define QED_EXT_SPEED_MASK_20G			0x8
#define QED_EXT_SPEED_MASK_25G			0x10
#define QED_EXT_SPEED_MASK_40G			0x20
#define QED_EXT_SPEED_MASK_50G_R		0x40
#define QED_EXT_SPEED_MASK_50G_R2		0x80
#define QED_EXT_SPEED_MASK_100G_R2		0x100
#define QED_EXT_SPEED_MASK_100G_R4		0x200
#define QED_EXT_SPEED_MASK_100G_P4		0x400
	u32					forced_speed;	    
#define QED_EXT_SPEED_1G			0x1
#define QED_EXT_SPEED_10G			0x2
#define QED_EXT_SPEED_20G			0x4
#define QED_EXT_SPEED_25G			0x8
#define QED_EXT_SPEED_40G			0x10
#define QED_EXT_SPEED_50G_R			0x20
#define QED_EXT_SPEED_50G_R2			0x40
#define QED_EXT_SPEED_100G_R2			0x80
#define QED_EXT_SPEED_100G_R4			0x100
#define QED_EXT_SPEED_100G_P4			0x200
};
struct qed_mcp_link_pause_params {
	bool					autoneg;
	bool					forced_rx;
	bool					forced_tx;
};
enum qed_mcp_eee_mode {
	QED_MCP_EEE_DISABLED,
	QED_MCP_EEE_ENABLED,
	QED_MCP_EEE_UNSUPPORTED
};
struct qed_mcp_link_params {
	struct qed_mcp_link_speed_params	speed;
	struct qed_mcp_link_pause_params	pause;
	u32					loopback_mode;
	struct qed_link_eee_params		eee;
	u32					fec;
	struct qed_mcp_link_speed_params	ext_speed;
	u32					ext_fec_mode;
};
struct qed_mcp_link_capabilities {
	u32					speed_capabilities;
	bool					default_speed_autoneg;
	u32					fec_default;
	enum qed_mcp_eee_mode			default_eee;
	u32					eee_lpi_timer;
	u8					eee_speed_caps;
	u32					default_ext_speed_caps;
	u32					default_ext_autoneg;
	u32					default_ext_speed;
	u32					default_ext_fec;
};
struct qed_mcp_link_state {
	bool					link_up;
	u32					min_pf_rate;
	u32					line_speed;
	u32					speed;
	bool					full_duplex;
	bool					an;
	bool					an_complete;
	bool					parallel_detection;
	bool					pfc_enabled;
	u32					partner_adv_speed;
#define QED_LINK_PARTNER_SPEED_1G_HD		BIT(0)
#define QED_LINK_PARTNER_SPEED_1G_FD		BIT(1)
#define QED_LINK_PARTNER_SPEED_10G		BIT(2)
#define QED_LINK_PARTNER_SPEED_20G		BIT(3)
#define QED_LINK_PARTNER_SPEED_25G		BIT(4)
#define QED_LINK_PARTNER_SPEED_40G		BIT(5)
#define QED_LINK_PARTNER_SPEED_50G		BIT(6)
#define QED_LINK_PARTNER_SPEED_100G		BIT(7)
	bool					partner_tx_flow_ctrl_en;
	bool					partner_rx_flow_ctrl_en;
	u8					partner_adv_pause;
#define QED_LINK_PARTNER_SYMMETRIC_PAUSE	0x1
#define QED_LINK_PARTNER_ASYMMETRIC_PAUSE	0x2
#define QED_LINK_PARTNER_BOTH_PAUSE		0x3
	bool					sfp_tx_fault;
	bool					eee_active;
	u8					eee_adv_caps;
	u8					eee_lp_adv_caps;
	u32					fec_active;
};
struct qed_mcp_function_info {
	u8				pause_on_host;
	enum qed_pci_personality	protocol;
	u8				bandwidth_min;
	u8				bandwidth_max;
	u8				mac[ETH_ALEN];
	u64				wwn_port;
	u64				wwn_node;
#define QED_MCP_VLAN_UNSET              (0xffff)
	u16				ovlan;
	u16				mtu;
};
struct qed_mcp_nvm_common {
	u32	offset;
	u32	param;
	u32	resp;
	u32	cmd;
};
struct qed_mcp_drv_version {
	u32	version;
	u8	name[MCP_DRV_VER_STR_SIZE - 4];
};
struct qed_mcp_lan_stats {
	u64 ucast_rx_pkts;
	u64 ucast_tx_pkts;
	u32 fcs_err;
};
struct qed_mcp_fcoe_stats {
	u64 rx_pkts;
	u64 tx_pkts;
	u32 fcs_err;
	u32 login_failure;
};
struct qed_mcp_iscsi_stats {
	u64 rx_pdus;
	u64 tx_pdus;
	u64 rx_bytes;
	u64 tx_bytes;
};
struct qed_mcp_rdma_stats {
	u64 rx_pkts;
	u64 tx_pkts;
	u64 rx_bytes;
	u64 tx_byts;
};
enum qed_mcp_protocol_type {
	QED_MCP_LAN_STATS,
	QED_MCP_FCOE_STATS,
	QED_MCP_ISCSI_STATS,
	QED_MCP_RDMA_STATS
};
union qed_mcp_protocol_stats {
	struct qed_mcp_lan_stats lan_stats;
	struct qed_mcp_fcoe_stats fcoe_stats;
	struct qed_mcp_iscsi_stats iscsi_stats;
	struct qed_mcp_rdma_stats rdma_stats;
};
enum qed_ov_eswitch {
	QED_OV_ESWITCH_NONE,
	QED_OV_ESWITCH_VEB,
	QED_OV_ESWITCH_VEPA
};
enum qed_ov_client {
	QED_OV_CLIENT_DRV,
	QED_OV_CLIENT_USER,
	QED_OV_CLIENT_VENDOR_SPEC
};
enum qed_ov_driver_state {
	QED_OV_DRIVER_STATE_NOT_LOADED,
	QED_OV_DRIVER_STATE_DISABLED,
	QED_OV_DRIVER_STATE_ACTIVE
};
enum qed_ov_wol {
	QED_OV_WOL_DEFAULT,
	QED_OV_WOL_DISABLED,
	QED_OV_WOL_ENABLED
};
enum qed_mfw_tlv_type {
	QED_MFW_TLV_GENERIC = 0x1,	 
	QED_MFW_TLV_ETH = 0x2,		 
	QED_MFW_TLV_FCOE = 0x4,		 
	QED_MFW_TLV_ISCSI = 0x8,	 
	QED_MFW_TLV_MAX = 0x16,
};
struct qed_mfw_tlv_generic {
#define QED_MFW_TLV_FLAGS_SIZE	2
	struct {
		u8 ipv4_csum_offload;
		u8 lso_supported;
		bool b_set;
	} flags;
#define QED_MFW_TLV_MAC_COUNT 3
	u8 mac[QED_MFW_TLV_MAC_COUNT][6];
	bool mac_set[QED_MFW_TLV_MAC_COUNT];
	u64 rx_frames;
	bool rx_frames_set;
	u64 rx_bytes;
	bool rx_bytes_set;
	u64 tx_frames;
	bool tx_frames_set;
	u64 tx_bytes;
	bool tx_bytes_set;
};
union qed_mfw_tlv_data {
	struct qed_mfw_tlv_generic generic;
	struct qed_mfw_tlv_eth eth;
	struct qed_mfw_tlv_fcoe fcoe;
	struct qed_mfw_tlv_iscsi iscsi;
};
#define QED_NVM_CFG_OPTION_ALL		BIT(0)
#define QED_NVM_CFG_OPTION_INIT		BIT(1)
#define QED_NVM_CFG_OPTION_COMMIT       BIT(2)
#define QED_NVM_CFG_OPTION_FREE		BIT(3)
#define QED_NVM_CFG_OPTION_ENTITY_SEL	BIT(4)
struct qed_mcp_link_params *qed_mcp_get_link_params(struct qed_hwfn *p_hwfn);
struct qed_mcp_link_state *qed_mcp_get_link_state(struct qed_hwfn *p_hwfn);
struct qed_mcp_link_capabilities
	*qed_mcp_get_link_capabilities(struct qed_hwfn *p_hwfn);
int qed_mcp_set_link(struct qed_hwfn   *p_hwfn,
		     struct qed_ptt     *p_ptt,
		     bool               b_up);
int qed_mcp_get_mfw_ver(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 *p_mfw_ver, u32 *p_running_bundle_id);
int qed_mcp_get_mbi_ver(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt, u32 *p_mbi_ver);
int qed_mcp_get_media_type(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u32 *media_type);
int qed_mcp_get_transceiver_data(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 u32 *p_transceiver_state,
				 u32 *p_tranceiver_type);
int qed_mcp_trans_speed_mask(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 *p_speed_mask);
int qed_mcp_get_board_config(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u32 *p_board_config);
int qed_mcp_cmd(struct qed_hwfn *p_hwfn,
		struct qed_ptt *p_ptt,
		u32 cmd,
		u32 param,
		u32 *o_mcp_resp,
		u32 *o_mcp_param);
int qed_mcp_cmd_nosleep(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			u32 cmd,
			u32 param,
			u32 *o_mcp_resp,
			u32 *o_mcp_param);
int qed_mcp_drain(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt);
int qed_mcp_get_flash_size(struct qed_hwfn     *p_hwfn,
			   struct qed_ptt       *p_ptt,
			   u32 *p_flash_size);
int
qed_mcp_send_drv_version(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct qed_mcp_drv_version *p_ver);
u32 qed_get_process_kill_counter(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt);
int qed_start_recovery_process(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_recovery_prolog(struct qed_dev *cdev);
int qed_mcp_ov_update_current_config(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     enum qed_ov_client client);
int qed_mcp_ov_update_driver_state(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   enum qed_ov_driver_state drv_state);
int qed_mcp_ov_update_mtu(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u16 mtu);
int qed_mcp_ov_update_mac(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, const u8 *mac);
int qed_mcp_ov_update_wol(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  enum qed_ov_wol wol);
int qed_mcp_set_led(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    enum qed_led_mode mode);
int qed_mcp_nvm_read(struct qed_dev *cdev, u32 addr, u8 *p_buf, u32 len);
int qed_mcp_nvm_write(struct qed_dev *cdev,
		      u32 cmd, u32 addr, u8 *p_buf, u32 len);
int qed_mcp_nvm_resp(struct qed_dev *cdev, u8 *p_buf);
struct qed_nvm_image_att {
	u32 start_addr;
	u32 length;
};
int
qed_mcp_get_nvm_image_att(struct qed_hwfn *p_hwfn,
			  enum qed_nvm_images image_id,
			  struct qed_nvm_image_att *p_image_att);
int qed_mcp_get_nvm_image(struct qed_hwfn *p_hwfn,
			  enum qed_nvm_images image_id,
			  u8 *p_buffer, u32 buffer_len);
int qed_mcp_bist_register_test(struct qed_hwfn *p_hwfn,
			       struct qed_ptt *p_ptt);
int qed_mcp_bist_clock_test(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt);
int qed_mcp_bist_nvm_get_num_images(struct qed_hwfn *p_hwfn,
				    struct qed_ptt *p_ptt,
				    u32 *num_images);
int qed_mcp_bist_nvm_get_image_att(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt,
				   struct bist_nvm_image_att *p_image_att,
				   u32 image_index);
int qed_mfw_process_tlv_req(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int
qed_mcp_send_raw_debug_data(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt, u8 *p_buf, u32 size);
#define MCP_PF_ID_BY_REL(p_hwfn, rel_pfid) (QED_IS_BB((p_hwfn)->cdev) ?	       \
					    ((rel_pfid) |		       \
					     ((p_hwfn)->abs_pf_id & 1) << 3) : \
					    rel_pfid)
#define MCP_PF_ID(p_hwfn) MCP_PF_ID_BY_REL(p_hwfn, (p_hwfn)->rel_pf_id)
struct qed_mcp_info {
	struct list_head			cmd_list;
	spinlock_t				cmd_lock;
	bool					b_block_cmd;
	spinlock_t				link_lock;
	u32					public_base;
	u32					drv_mb_addr;
	u32					mfw_mb_addr;
	u32					port_addr;
	u16					drv_mb_seq;
	u16					drv_pulse_seq;
	struct qed_mcp_link_params		link_input;
	struct qed_mcp_link_state		link_output;
	struct qed_mcp_link_capabilities	link_capabilities;
	struct qed_mcp_function_info		func_info;
	u8					*mfw_mb_cur;
	u8					*mfw_mb_shadow;
	u16					mfw_mb_length;
	u32					mcp_hist;
	u32					capabilities;
	atomic_t dbg_data_seq;
	spinlock_t unload_lock;
	unsigned long mcp_handling_status;
#define QED_MCP_BYPASS_PROC_BIT 0
#define QED_MCP_IN_PROCESSING_BIT       1
};
struct qed_mcp_mb_params {
	u32 cmd;
	u32 param;
	void *p_data_src;
	void *p_data_dst;
	u8 data_src_size;
	u8 data_dst_size;
	u32 mcp_resp;
	u32 mcp_param;
	u32 flags;
#define QED_MB_FLAG_CAN_SLEEP	(0x1 << 0)
#define QED_MB_FLAG_AVOID_BLOCK	(0x1 << 1)
#define QED_MB_FLAGS_IS_SET(params, flag) \
	({ typeof(params) __params = (params); \
	   (__params && (__params->flags & QED_MB_FLAG_ ## flag)); })
};
struct qed_drv_tlv_hdr {
	u8 tlv_type;
	u8 tlv_length;	 
	u8 tlv_reserved;
#define QED_DRV_TLV_FLAGS_CHANGED 0x01
	u8 tlv_flags;
};
static inline bool
qed_mcp_is_ext_speed_supported(const struct qed_hwfn *p_hwfn)
{
	return !!(p_hwfn->mcp_info->capabilities &
		  FW_MB_PARAM_FEATURE_SUPPORT_EXT_SPEED_FEC_CONTROL);
}
int qed_mcp_cmd_init(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt);
void qed_mcp_cmd_port_init(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt);
int qed_mcp_free(struct qed_hwfn *p_hwfn);
int qed_mcp_handle_events(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt);
enum qed_drv_role {
	QED_DRV_ROLE_OS,
	QED_DRV_ROLE_KDUMP,
};
struct qed_load_req_params {
	enum qed_drv_role drv_role;
	u8 timeout_val;
	bool avoid_eng_reset;
	enum qed_override_force_load override_force_load;
	u32 load_code;
};
int qed_mcp_load_req(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     struct qed_load_req_params *p_params);
int qed_mcp_load_done(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_mcp_unload_req(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_mcp_unload_done(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
void qed_mcp_read_mb(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt);
int qed_mcp_ack_vf_flr(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u32 *vfs_to_ack);
int qed_mcp_fill_shmem_func_info(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt);
int qed_mcp_reset(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt);
int qed_mcp_nvm_rd_cmd(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u32 cmd,
		       u32 param,
		       u32 *o_mcp_resp,
		       u32 *o_mcp_param,
		       u32 *o_txn_size, u32 *o_buf, bool b_can_sleep);
int qed_mcp_phy_sfp_read(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			 u32 port, u32 addr, u32 offset, u32 len, u8 *p_buf);
bool qed_mcp_is_init(struct qed_hwfn *p_hwfn);
int qed_mcp_config_vf_msix(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt, u8 vf_id, u8 num);
int qed_mcp_halt(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_mcp_resume(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_configure_pf_min_bandwidth(struct qed_dev *cdev, u8 min_bw);
int qed_configure_pf_max_bandwidth(struct qed_dev *cdev, u8 max_bw);
int __qed_configure_pf_max_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 max_bw);
int __qed_configure_pf_min_bandwidth(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     struct qed_mcp_link_state *p_link,
				     u8 min_bw);
int qed_mcp_mask_parities(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u32 mask_parities);
int
qed_mcp_mdump_get_retain(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 struct mdump_retain_data_stc *p_mdump_retain);
int
qed_mcp_set_resc_max_val(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 enum qed_resources res_id,
			 u32 resc_max_val, u32 *p_mcp_resp);
int
qed_mcp_get_resc_info(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      enum qed_resources res_id,
		      u32 *p_mcp_resp, u32 *p_resc_num, u32 *p_resc_start);
int qed_mcp_ov_update_eswitch(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      enum qed_ov_eswitch eswitch);
#define QED_MCP_RESC_LOCK_MIN_VAL       RESOURCE_DUMP
#define QED_MCP_RESC_LOCK_MAX_VAL       31
enum qed_resc_lock {
	QED_RESC_LOCK_DBG_DUMP = QED_MCP_RESC_LOCK_MIN_VAL,
	QED_RESC_LOCK_PTP_PORT0,
	QED_RESC_LOCK_PTP_PORT1,
	QED_RESC_LOCK_PTP_PORT2,
	QED_RESC_LOCK_PTP_PORT3,
	QED_RESC_LOCK_RESC_ALLOC = QED_MCP_RESC_LOCK_MAX_VAL,
	QED_RESC_LOCK_RESC_INVALID
};
int qed_mcp_initiate_pf_flr(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
struct qed_resc_lock_params {
	u8 resource;
	u8 timeout;
#define QED_MCP_RESC_LOCK_TO_DEFAULT    0
#define QED_MCP_RESC_LOCK_TO_NONE       255
	u8 retry_num;
#define QED_MCP_RESC_LOCK_RETRY_CNT_DFLT        10
	u16 retry_interval;
#define QED_MCP_RESC_LOCK_RETRY_VAL_DFLT        10000
	bool sleep_b4_retry;
	bool b_granted;
	u8 owner;
};
int
qed_mcp_resc_lock(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt, struct qed_resc_lock_params *p_params);
struct qed_resc_unlock_params {
	u8 resource;
	bool b_force;
	bool b_released;
};
int
qed_mcp_resc_unlock(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_resc_unlock_params *p_params);
void qed_mcp_resc_lock_default_init(struct qed_resc_lock_params *p_lock,
				    struct qed_resc_unlock_params *p_unlock,
				    enum qed_resc_lock
				    resource, bool b_is_permanent);
bool qed_mcp_is_smart_an_supported(struct qed_hwfn *p_hwfn);
int qed_mcp_get_capabilities(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_mcp_set_capabilities(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
void qed_mcp_read_ufp_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_mcp_nvm_info_populate(struct qed_hwfn *p_hwfn);
void qed_mcp_nvm_info_free(struct qed_hwfn *p_hwfn);
int qed_mcp_get_engine_config(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_mcp_get_ppfid_bitmap(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);
int qed_mcp_nvm_get_cfg(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			u16 option_id, u8 entity_id, u16 flags, u8 *p_buf,
			u32 *p_len);
int qed_mcp_nvm_set_cfg(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			u16 option_id, u8 entity_id, u16 flags, u8 *p_buf,
			u32 len);
bool qed_mcp_is_esl_supported(struct qed_hwfn *p_hwfn);
int qed_mcp_get_esl_status(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, bool *active);
#endif
