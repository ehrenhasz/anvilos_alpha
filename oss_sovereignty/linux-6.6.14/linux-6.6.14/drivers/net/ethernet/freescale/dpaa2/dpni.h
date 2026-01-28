#ifndef __FSL_DPNI_H
#define __FSL_DPNI_H
#include "dpkg.h"
struct fsl_mc_io;
#define DPNI_MAX_TC				8
#define DPNI_MAX_DPBP				8
#define DPNI_ALL_TCS				(u8)(-1)
#define DPNI_ALL_TC_FLOWS			(u16)(-1)
#define DPNI_NEW_FLOW_ID			(u16)(-1)
#define DPNI_OPT_TX_FRM_RELEASE			0x000001
#define DPNI_OPT_NO_MAC_FILTER			0x000002
#define DPNI_OPT_HAS_POLICING			0x000004
#define DPNI_OPT_SHARED_CONGESTION		0x000008
#define DPNI_OPT_HAS_KEY_MASKING		0x000010
#define DPNI_OPT_NO_FS				0x000020
#define DPNI_OPT_SHARED_FS			0x001000
int dpni_open(struct fsl_mc_io	*mc_io,
	      u32		cmd_flags,
	      int		dpni_id,
	      u16		*token);
int dpni_close(struct fsl_mc_io	*mc_io,
	       u32		cmd_flags,
	       u16		token);
#define DPNI_POOL_ASSOC_QPRI	0
#define DPNI_POOL_ASSOC_QDBIN	1
struct dpni_pools_cfg {
	u8		num_dpbp;
	u8		pool_options;
	struct {
		int	dpbp_id;
		u8	priority_mask;
		u16	buffer_size;
		int	backup_pool;
	} pools[DPNI_MAX_DPBP];
};
int dpni_set_pools(struct fsl_mc_io		*mc_io,
		   u32				cmd_flags,
		   u16				token,
		   const struct dpni_pools_cfg	*cfg);
int dpni_enable(struct fsl_mc_io	*mc_io,
		u32			cmd_flags,
		u16			token);
int dpni_disable(struct fsl_mc_io	*mc_io,
		 u32			cmd_flags,
		 u16			token);
int dpni_is_enabled(struct fsl_mc_io	*mc_io,
		    u32			cmd_flags,
		    u16			token,
		    int			*en);
int dpni_reset(struct fsl_mc_io	*mc_io,
	       u32		cmd_flags,
	       u16		token);
#define DPNI_IRQ_INDEX				0
#define DPNI_IRQ_EVENT_LINK_CHANGED		0x00000001
#define DPNI_IRQ_EVENT_ENDPOINT_CHANGED		0x00000002
int dpni_set_irq_enable(struct fsl_mc_io	*mc_io,
			u32			cmd_flags,
			u16			token,
			u8			irq_index,
			u8			en);
int dpni_get_irq_enable(struct fsl_mc_io	*mc_io,
			u32			cmd_flags,
			u16			token,
			u8			irq_index,
			u8			*en);
int dpni_set_irq_mask(struct fsl_mc_io	*mc_io,
		      u32		cmd_flags,
		      u16		token,
		      u8		irq_index,
		      u32		mask);
int dpni_get_irq_mask(struct fsl_mc_io	*mc_io,
		      u32		cmd_flags,
		      u16		token,
		      u8		irq_index,
		      u32		*mask);
int dpni_get_irq_status(struct fsl_mc_io	*mc_io,
			u32			cmd_flags,
			u16			token,
			u8			irq_index,
			u32			*status);
int dpni_clear_irq_status(struct fsl_mc_io	*mc_io,
			  u32			cmd_flags,
			  u16			token,
			  u8			irq_index,
			  u32			status);
struct dpni_attr {
	u32 options;
	u8 num_queues;
	u8 num_tcs;
	u8 mac_filter_entries;
	u8 vlan_filter_entries;
	u8 qos_entries;
	u16 fs_entries;
	u8 qos_key_size;
	u8 fs_key_size;
	u16 wriop_version;
};
int dpni_get_attributes(struct fsl_mc_io	*mc_io,
			u32			cmd_flags,
			u16			token,
			struct dpni_attr	*attr);
#define DPNI_ERROR_EOFHE	0x00020000
#define DPNI_ERROR_FLE		0x00002000
#define DPNI_ERROR_FPE		0x00001000
#define DPNI_ERROR_PHE		0x00000020
#define DPNI_ERROR_L3CE		0x00000004
#define DPNI_ERROR_L4CE		0x00000001
enum dpni_error_action {
	DPNI_ERROR_ACTION_DISCARD = 0,
	DPNI_ERROR_ACTION_CONTINUE = 1,
	DPNI_ERROR_ACTION_SEND_TO_ERROR_QUEUE = 2
};
struct dpni_error_cfg {
	u32			errors;
	enum dpni_error_action	error_action;
	int			set_frame_annotation;
};
int dpni_set_errors_behavior(struct fsl_mc_io		*mc_io,
			     u32			cmd_flags,
			     u16			token,
			     struct dpni_error_cfg	*cfg);
#define DPNI_BUF_LAYOUT_OPT_TIMESTAMP		0x00000001
#define DPNI_BUF_LAYOUT_OPT_PARSER_RESULT	0x00000002
#define DPNI_BUF_LAYOUT_OPT_FRAME_STATUS	0x00000004
#define DPNI_BUF_LAYOUT_OPT_PRIVATE_DATA_SIZE	0x00000008
#define DPNI_BUF_LAYOUT_OPT_DATA_ALIGN		0x00000010
#define DPNI_BUF_LAYOUT_OPT_DATA_HEAD_ROOM	0x00000020
#define DPNI_BUF_LAYOUT_OPT_DATA_TAIL_ROOM	0x00000040
struct dpni_buffer_layout {
	u32	options;
	int	pass_timestamp;
	int	pass_parser_result;
	int	pass_frame_status;
	u16	private_data_size;
	u16	data_align;
	u16	data_head_room;
	u16	data_tail_room;
};
enum dpni_queue_type {
	DPNI_QUEUE_RX,
	DPNI_QUEUE_TX,
	DPNI_QUEUE_TX_CONFIRM,
	DPNI_QUEUE_RX_ERR,
};
int dpni_get_buffer_layout(struct fsl_mc_io		*mc_io,
			   u32				cmd_flags,
			   u16				token,
			   enum dpni_queue_type		qtype,
			   struct dpni_buffer_layout	*layout);
int dpni_set_buffer_layout(struct fsl_mc_io		   *mc_io,
			   u32				   cmd_flags,
			   u16				   token,
			   enum dpni_queue_type		   qtype,
			   const struct dpni_buffer_layout *layout);
enum dpni_offload {
	DPNI_OFF_RX_L3_CSUM,
	DPNI_OFF_RX_L4_CSUM,
	DPNI_OFF_TX_L3_CSUM,
	DPNI_OFF_TX_L4_CSUM,
};
int dpni_set_offload(struct fsl_mc_io	*mc_io,
		     u32		cmd_flags,
		     u16		token,
		     enum dpni_offload	type,
		     u32		config);
int dpni_get_offload(struct fsl_mc_io	*mc_io,
		     u32		cmd_flags,
		     u16		token,
		     enum dpni_offload	type,
		     u32		*config);
int dpni_get_qdid(struct fsl_mc_io	*mc_io,
		  u32			cmd_flags,
		  u16			token,
		  enum dpni_queue_type	qtype,
		  u16			*qdid);
int dpni_get_tx_data_offset(struct fsl_mc_io	*mc_io,
			    u32			cmd_flags,
			    u16			token,
			    u16			*data_offset);
#define DPNI_STATISTICS_CNT		7
union dpni_statistics {
	struct {
		u64 ingress_all_frames;
		u64 ingress_all_bytes;
		u64 ingress_multicast_frames;
		u64 ingress_multicast_bytes;
		u64 ingress_broadcast_frames;
		u64 ingress_broadcast_bytes;
	} page_0;
	struct {
		u64 egress_all_frames;
		u64 egress_all_bytes;
		u64 egress_multicast_frames;
		u64 egress_multicast_bytes;
		u64 egress_broadcast_frames;
		u64 egress_broadcast_bytes;
	} page_1;
	struct {
		u64 ingress_filtered_frames;
		u64 ingress_discarded_frames;
		u64 ingress_nobuffer_discards;
		u64 egress_discarded_frames;
		u64 egress_confirmed_frames;
	} page_2;
	struct {
		u64 egress_dequeue_bytes;
		u64 egress_dequeue_frames;
		u64 egress_reject_bytes;
		u64 egress_reject_frames;
	} page_3;
	struct {
		u64 cgr_reject_frames;
		u64 cgr_reject_bytes;
	} page_4;
	struct {
		u64 policer_cnt_red;
		u64 policer_cnt_yellow;
		u64 policer_cnt_green;
		u64 policer_cnt_re_red;
		u64 policer_cnt_re_yellow;
	} page_5;
	struct {
		u64 tx_pending_frames;
	} page_6;
	struct {
		u64 counter[DPNI_STATISTICS_CNT];
	} raw;
};
int dpni_get_statistics(struct fsl_mc_io	*mc_io,
			u32			cmd_flags,
			u16			token,
			u8			page,
			union dpni_statistics	*stat);
#define DPNI_LINK_OPT_AUTONEG		0x0000000000000001ULL
#define DPNI_LINK_OPT_HALF_DUPLEX	0x0000000000000002ULL
#define DPNI_LINK_OPT_PAUSE		0x0000000000000004ULL
#define DPNI_LINK_OPT_ASYM_PAUSE	0x0000000000000008ULL
#define DPNI_LINK_OPT_PFC_PAUSE		0x0000000000000010ULL
struct dpni_link_cfg {
	u32 rate;
	u64 options;
};
int dpni_set_link_cfg(struct fsl_mc_io			*mc_io,
		      u32				cmd_flags,
		      u16				token,
		      const struct dpni_link_cfg	*cfg);
int dpni_get_link_cfg(struct fsl_mc_io			*mc_io,
		      u32				cmd_flags,
		      u16				token,
		      struct dpni_link_cfg		*cfg);
struct dpni_link_state {
	u32	rate;
	u64	options;
	int	up;
};
int dpni_get_link_state(struct fsl_mc_io	*mc_io,
			u32			cmd_flags,
			u16			token,
			struct dpni_link_state	*state);
int dpni_set_max_frame_length(struct fsl_mc_io	*mc_io,
			      u32		cmd_flags,
			      u16		token,
			      u16		max_frame_length);
int dpni_get_max_frame_length(struct fsl_mc_io	*mc_io,
			      u32		cmd_flags,
			      u16		token,
			      u16		*max_frame_length);
int dpni_set_multicast_promisc(struct fsl_mc_io *mc_io,
			       u32		cmd_flags,
			       u16		token,
			       int		en);
int dpni_get_multicast_promisc(struct fsl_mc_io *mc_io,
			       u32		cmd_flags,
			       u16		token,
			       int		*en);
int dpni_set_unicast_promisc(struct fsl_mc_io	*mc_io,
			     u32		cmd_flags,
			     u16		token,
			     int		en);
int dpni_get_unicast_promisc(struct fsl_mc_io	*mc_io,
			     u32		cmd_flags,
			     u16		token,
			     int		*en);
int dpni_set_primary_mac_addr(struct fsl_mc_io *mc_io,
			      u32		cmd_flags,
			      u16		token,
			      const u8		mac_addr[6]);
int dpni_get_primary_mac_addr(struct fsl_mc_io	*mc_io,
			      u32		cmd_flags,
			      u16		token,
			      u8		mac_addr[6]);
int dpni_get_port_mac_addr(struct fsl_mc_io	*mc_io,
			   u32			cm_flags,
			   u16			token,
			   u8			mac_addr[6]);
int dpni_add_mac_addr(struct fsl_mc_io	*mc_io,
		      u32		cmd_flags,
		      u16		token,
		      const u8		mac_addr[6]);
int dpni_remove_mac_addr(struct fsl_mc_io	*mc_io,
			 u32			cmd_flags,
			 u16			token,
			 const u8		mac_addr[6]);
int dpni_clear_mac_filters(struct fsl_mc_io	*mc_io,
			   u32			cmd_flags,
			   u16			token,
			   int			unicast,
			   int			multicast);
enum dpni_dist_mode {
	DPNI_DIST_MODE_NONE = 0,
	DPNI_DIST_MODE_HASH = 1,
	DPNI_DIST_MODE_FS = 2
};
enum dpni_fs_miss_action {
	DPNI_FS_MISS_DROP = 0,
	DPNI_FS_MISS_EXPLICIT_FLOWID = 1,
	DPNI_FS_MISS_HASH = 2
};
struct dpni_fs_tbl_cfg {
	enum dpni_fs_miss_action	miss_action;
	u16				default_flow_id;
};
int dpni_prepare_key_cfg(const struct dpkg_profile_cfg *cfg,
			 u8 *key_cfg_buf);
struct dpni_rx_tc_dist_cfg {
	u16			dist_size;
	enum dpni_dist_mode	dist_mode;
	u64			key_cfg_iova;
	struct dpni_fs_tbl_cfg	fs_cfg;
};
int dpni_set_rx_tc_dist(struct fsl_mc_io			*mc_io,
			u32					cmd_flags,
			u16					token,
			u8					tc_id,
			const struct dpni_rx_tc_dist_cfg	*cfg);
#define DPNI_FS_MISS_DROP		((uint16_t)-1)
struct dpni_rx_dist_cfg {
	u16 dist_size;
	u64 key_cfg_iova;
	u8 enable;
	u8 tc;
	u16 fs_miss_flow_id;
};
int dpni_set_rx_fs_dist(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			const struct dpni_rx_dist_cfg *cfg);
int dpni_set_rx_hash_dist(struct fsl_mc_io *mc_io,
			  u32 cmd_flags,
			  u16 token,
			  const struct dpni_rx_dist_cfg *cfg);
struct dpni_qos_tbl_cfg {
	u64 key_cfg_iova;
	int discard_on_miss;
	u8 default_tc;
};
int dpni_set_qos_table(struct fsl_mc_io *mc_io,
		       u32 cmd_flags,
		       u16 token,
		       const struct dpni_qos_tbl_cfg *cfg);
enum dpni_dest {
	DPNI_DEST_NONE = 0,
	DPNI_DEST_DPIO = 1,
	DPNI_DEST_DPCON = 2
};
struct dpni_queue {
	struct {
		u16 id;
		enum dpni_dest type;
		char hold_active;
		u8 priority;
	} destination;
	u64 user_context;
	struct {
		u64 value;
		char stash_control;
	} flc;
};
struct dpni_queue_id {
	u32 fqid;
	u16 qdbin;
};
#define DPNI_QUEUE_OPT_USER_CTX		0x00000001
#define DPNI_QUEUE_OPT_DEST		0x00000002
#define DPNI_QUEUE_OPT_FLC		0x00000004
#define DPNI_QUEUE_OPT_HOLD_ACTIVE	0x00000008
int dpni_set_queue(struct fsl_mc_io	*mc_io,
		   u32			cmd_flags,
		   u16			token,
		   enum dpni_queue_type	qtype,
		   u8			tc,
		   u8			index,
		   u8			options,
		   const struct dpni_queue *queue);
int dpni_get_queue(struct fsl_mc_io	*mc_io,
		   u32			cmd_flags,
		   u16			token,
		   enum dpni_queue_type	qtype,
		   u8			tc,
		   u8			index,
		   struct dpni_queue	*queue,
		   struct dpni_queue_id	*qid);
enum dpni_congestion_unit {
	DPNI_CONGESTION_UNIT_BYTES = 0,
	DPNI_CONGESTION_UNIT_FRAMES
};
enum dpni_congestion_point {
	DPNI_CP_QUEUE,
	DPNI_CP_GROUP,
};
struct dpni_dest_cfg {
	enum dpni_dest dest_type;
	int dest_id;
	u8 priority;
};
#define DPNI_CONG_OPT_FLOW_CONTROL		0x00000040
struct dpni_congestion_notification_cfg {
	enum dpni_congestion_unit units;
	u32 threshold_entry;
	u32 threshold_exit;
	u64 message_ctx;
	u64 message_iova;
	struct dpni_dest_cfg dest_cfg;
	u16 notification_mode;
};
int dpni_set_congestion_notification(
			struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			enum dpni_queue_type qtype,
			u8 tc_id,
			const struct dpni_congestion_notification_cfg *cfg);
struct dpni_taildrop {
	char enable;
	enum dpni_congestion_unit units;
	u32 threshold;
};
int dpni_set_taildrop(struct fsl_mc_io *mc_io,
		      u32 cmd_flags,
		      u16 token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type q_type,
		      u8 tc,
		      u8 q_index,
		      struct dpni_taildrop *taildrop);
int dpni_get_taildrop(struct fsl_mc_io *mc_io,
		      u32 cmd_flags,
		      u16 token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type q_type,
		      u8 tc,
		      u8 q_index,
		      struct dpni_taildrop *taildrop);
struct dpni_rule_cfg {
	u64	key_iova;
	u64	mask_iova;
	u8	key_size;
};
 #define DPNI_FS_OPT_DISCARD            0x1
#define DPNI_FS_OPT_SET_FLC            0x2
#define DPNI_FS_OPT_SET_STASH_CONTROL  0x4
struct dpni_fs_action_cfg {
	u64 flc;
	u16 flow_id;
	u16 options;
};
int dpni_add_fs_entry(struct fsl_mc_io *mc_io,
		      u32 cmd_flags,
		      u16 token,
		      u8 tc_id,
		      u16 index,
		      const struct dpni_rule_cfg *cfg,
		      const struct dpni_fs_action_cfg *action);
int dpni_remove_fs_entry(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token,
			 u8 tc_id,
			 const struct dpni_rule_cfg *cfg);
int dpni_add_qos_entry(struct fsl_mc_io *mc_io,
		       u32 cmd_flags,
		       u16 token,
		       const struct dpni_rule_cfg *cfg,
		       u8 tc_id,
		       u16 index);
int dpni_remove_qos_entry(struct fsl_mc_io *mc_io,
			  u32 cmd_flags,
			  u16 token,
			  const struct dpni_rule_cfg *cfg);
int dpni_clear_qos_table(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token);
int dpni_get_api_version(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 *major_ver,
			 u16 *minor_ver);
struct dpni_tx_shaping_cfg {
	u32 rate_limit;
	u16 max_burst_size;
};
int dpni_set_tx_shaping(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			const struct dpni_tx_shaping_cfg *tx_cr_shaper,
			const struct dpni_tx_shaping_cfg *tx_er_shaper,
			int coupled);
struct dpni_single_step_cfg {
	u8	en;
	u8	ch_update;
	u16	offset;
	u32	peer_delay;
	u32	ptp_onestep_reg_base;
};
int dpni_set_single_step_cfg(struct fsl_mc_io *mc_io,
			     u32 cmd_flags,
			     u16 token,
			     struct dpni_single_step_cfg *ptp_cfg);
int dpni_get_single_step_cfg(struct fsl_mc_io *mc_io,
			     u32 cmd_flags,
			     u16 token,
			     struct dpni_single_step_cfg *ptp_cfg);
int dpni_enable_vlan_filter(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			    u32 en);
int dpni_add_vlan_id(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		     u16 vlan_id, u8 flags, u8 tc_id, u8 flow_id);
int dpni_remove_vlan_id(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u16 vlan_id);
#endif  
