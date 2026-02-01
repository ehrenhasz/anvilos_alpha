 
 

#ifndef __TI_SCI_H
#define __TI_SCI_H

 
#define TI_SCI_MSG_ENABLE_WDT	0x0000
#define TI_SCI_MSG_WAKE_RESET	0x0001
#define TI_SCI_MSG_VERSION	0x0002
#define TI_SCI_MSG_WAKE_REASON	0x0003
#define TI_SCI_MSG_GOODBYE	0x0004
#define TI_SCI_MSG_SYS_RESET	0x0005

 
#define TI_SCI_MSG_SET_DEVICE_STATE	0x0200
#define TI_SCI_MSG_GET_DEVICE_STATE	0x0201
#define TI_SCI_MSG_SET_DEVICE_RESETS	0x0202

 
#define TI_SCI_MSG_SET_CLOCK_STATE	0x0100
#define TI_SCI_MSG_GET_CLOCK_STATE	0x0101
#define TI_SCI_MSG_SET_CLOCK_PARENT	0x0102
#define TI_SCI_MSG_GET_CLOCK_PARENT	0x0103
#define TI_SCI_MSG_GET_NUM_CLOCK_PARENTS 0x0104
#define TI_SCI_MSG_SET_CLOCK_FREQ	0x010c
#define TI_SCI_MSG_QUERY_CLOCK_FREQ	0x010d
#define TI_SCI_MSG_GET_CLOCK_FREQ	0x010e

 
#define TI_SCI_MSG_GET_RESOURCE_RANGE	0x1500

 
#define TI_SCI_MSG_SET_IRQ		0x1000
#define TI_SCI_MSG_FREE_IRQ		0x1001

 
 
#define TI_SCI_MSG_RM_RING_ALLOCATE		0x1100
#define TI_SCI_MSG_RM_RING_FREE			0x1101
#define TI_SCI_MSG_RM_RING_RECONFIG		0x1102
#define TI_SCI_MSG_RM_RING_RESET		0x1103
#define TI_SCI_MSG_RM_RING_CFG			0x1110

 
#define TI_SCI_MSG_RM_PSIL_PAIR			0x1280
#define TI_SCI_MSG_RM_PSIL_UNPAIR		0x1281

#define TI_SCI_MSG_RM_UDMAP_TX_ALLOC		0x1200
#define TI_SCI_MSG_RM_UDMAP_TX_FREE		0x1201
#define TI_SCI_MSG_RM_UDMAP_RX_ALLOC		0x1210
#define TI_SCI_MSG_RM_UDMAP_RX_FREE		0x1211
#define TI_SCI_MSG_RM_UDMAP_FLOW_CFG		0x1220
#define TI_SCI_MSG_RM_UDMAP_OPT_FLOW_CFG	0x1221

#define TISCI_MSG_RM_UDMAP_TX_CH_CFG		0x1205
#define TISCI_MSG_RM_UDMAP_TX_CH_GET_CFG	0x1206
#define TISCI_MSG_RM_UDMAP_RX_CH_CFG		0x1215
#define TISCI_MSG_RM_UDMAP_RX_CH_GET_CFG	0x1216
#define TISCI_MSG_RM_UDMAP_FLOW_CFG		0x1230
#define TISCI_MSG_RM_UDMAP_FLOW_SIZE_THRESH_CFG	0x1231
#define TISCI_MSG_RM_UDMAP_FLOW_GET_CFG		0x1232
#define TISCI_MSG_RM_UDMAP_FLOW_SIZE_THRESH_GET_CFG	0x1233

 
#define TI_SCI_MSG_PROC_REQUEST		0xc000
#define TI_SCI_MSG_PROC_RELEASE		0xc001
#define TI_SCI_MSG_PROC_HANDOVER	0xc005
#define TI_SCI_MSG_SET_CONFIG		0xc100
#define TI_SCI_MSG_SET_CTRL		0xc101
#define TI_SCI_MSG_GET_STATUS		0xc400

 
struct ti_sci_msg_hdr {
	u16 type;
	u8 host;
	u8 seq;
#define TI_SCI_MSG_FLAG(val)			(1 << (val))
#define TI_SCI_FLAG_REQ_GENERIC_NORESPONSE	0x0
#define TI_SCI_FLAG_REQ_ACK_ON_RECEIVED		TI_SCI_MSG_FLAG(0)
#define TI_SCI_FLAG_REQ_ACK_ON_PROCESSED	TI_SCI_MSG_FLAG(1)
#define TI_SCI_FLAG_RESP_GENERIC_NACK		0x0
#define TI_SCI_FLAG_RESP_GENERIC_ACK		TI_SCI_MSG_FLAG(1)
	 
	u32 flags;
} __packed;

 
struct ti_sci_msg_resp_version {
	struct ti_sci_msg_hdr hdr;
	char firmware_description[32];
	u16 firmware_revision;
	u8 abi_major;
	u8 abi_minor;
} __packed;

 
struct ti_sci_msg_req_reboot {
	struct ti_sci_msg_hdr hdr;
} __packed;

 
struct ti_sci_msg_req_set_device_state {
	 
#define MSG_FLAG_DEVICE_WAKE_ENABLED	TI_SCI_MSG_FLAG(8)
#define MSG_FLAG_DEVICE_RESET_ISO	TI_SCI_MSG_FLAG(9)
#define MSG_FLAG_DEVICE_EXCLUSIVE	TI_SCI_MSG_FLAG(10)
	struct ti_sci_msg_hdr hdr;
	u32 id;
	u32 reserved;

#define MSG_DEVICE_SW_STATE_AUTO_OFF	0
#define MSG_DEVICE_SW_STATE_RETENTION	1
#define MSG_DEVICE_SW_STATE_ON		2
	u8 state;
} __packed;

 
struct ti_sci_msg_req_get_device_state {
	struct ti_sci_msg_hdr hdr;
	u32 id;
} __packed;

 
struct ti_sci_msg_resp_get_device_state {
	struct ti_sci_msg_hdr hdr;
	u32 context_loss_count;
	u32 resets;
	u8 programmed_state;
#define MSG_DEVICE_HW_STATE_OFF		0
#define MSG_DEVICE_HW_STATE_ON		1
#define MSG_DEVICE_HW_STATE_TRANS	2
	u8 current_state;
} __packed;

 
struct ti_sci_msg_req_set_device_resets {
	struct ti_sci_msg_hdr hdr;
	u32 id;
	u32 resets;
} __packed;

 
struct ti_sci_msg_req_set_clock_state {
	 
#define MSG_FLAG_CLOCK_ALLOW_SSC		TI_SCI_MSG_FLAG(8)
#define MSG_FLAG_CLOCK_ALLOW_FREQ_CHANGE	TI_SCI_MSG_FLAG(9)
#define MSG_FLAG_CLOCK_INPUT_TERM		TI_SCI_MSG_FLAG(10)
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u8 clk_id;
#define MSG_CLOCK_SW_STATE_UNREQ	0
#define MSG_CLOCK_SW_STATE_AUTO		1
#define MSG_CLOCK_SW_STATE_REQ		2
	u8 request_state;
	u32 clk_id_32;
} __packed;

 
struct ti_sci_msg_req_get_clock_state {
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u8 clk_id;
	u32 clk_id_32;
} __packed;

 
struct ti_sci_msg_resp_get_clock_state {
	struct ti_sci_msg_hdr hdr;
	u8 programmed_state;
#define MSG_CLOCK_HW_STATE_NOT_READY	0
#define MSG_CLOCK_HW_STATE_READY	1
	u8 current_state;
} __packed;

 
struct ti_sci_msg_req_set_clock_parent {
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u8 clk_id;
	u8 parent_id;
	u32 clk_id_32;
	u32 parent_id_32;
} __packed;

 
struct ti_sci_msg_req_get_clock_parent {
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u8 clk_id;
	u32 clk_id_32;
} __packed;

 
struct ti_sci_msg_resp_get_clock_parent {
	struct ti_sci_msg_hdr hdr;
	u8 parent_id;
	u32 parent_id_32;
} __packed;

 
struct ti_sci_msg_req_get_clock_num_parents {
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u8 clk_id;
	u32 clk_id_32;
} __packed;

 
struct ti_sci_msg_resp_get_clock_num_parents {
	struct ti_sci_msg_hdr hdr;
	u8 num_parents;
	u32 num_parents_32;
} __packed;

 
struct ti_sci_msg_req_query_clock_freq {
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u64 min_freq_hz;
	u64 target_freq_hz;
	u64 max_freq_hz;
	u8 clk_id;
	u32 clk_id_32;
} __packed;

 
struct ti_sci_msg_resp_query_clock_freq {
	struct ti_sci_msg_hdr hdr;
	u64 freq_hz;
} __packed;

 
struct ti_sci_msg_req_set_clock_freq {
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u64 min_freq_hz;
	u64 target_freq_hz;
	u64 max_freq_hz;
	u8 clk_id;
	u32 clk_id_32;
} __packed;

 
struct ti_sci_msg_req_get_clock_freq {
	struct ti_sci_msg_hdr hdr;
	u32 dev_id;
	u8 clk_id;
	u32 clk_id_32;
} __packed;

 
struct ti_sci_msg_resp_get_clock_freq {
	struct ti_sci_msg_hdr hdr;
	u64 freq_hz;
} __packed;

#define TI_SCI_IRQ_SECONDARY_HOST_INVALID	0xff

 
struct ti_sci_msg_req_get_resource_range {
	struct ti_sci_msg_hdr hdr;
#define MSG_RM_RESOURCE_TYPE_MASK	GENMASK(9, 0)
#define MSG_RM_RESOURCE_SUBTYPE_MASK	GENMASK(5, 0)
	u16 type;
	u8 subtype;
	u8 secondary_host;
} __packed;

 
struct ti_sci_msg_resp_get_resource_range {
	struct ti_sci_msg_hdr hdr;
	u16 range_start;
	u16 range_num;
	u16 range_start_sec;
	u16 range_num_sec;
} __packed;

 
struct ti_sci_msg_req_manage_irq {
	struct ti_sci_msg_hdr hdr;
#define MSG_FLAG_DST_ID_VALID			TI_SCI_MSG_FLAG(0)
#define MSG_FLAG_DST_HOST_IRQ_VALID		TI_SCI_MSG_FLAG(1)
#define MSG_FLAG_IA_ID_VALID			TI_SCI_MSG_FLAG(2)
#define MSG_FLAG_VINT_VALID			TI_SCI_MSG_FLAG(3)
#define MSG_FLAG_GLB_EVNT_VALID			TI_SCI_MSG_FLAG(4)
#define MSG_FLAG_VINT_STS_BIT_VALID		TI_SCI_MSG_FLAG(5)
#define MSG_FLAG_SHOST_VALID			TI_SCI_MSG_FLAG(31)
	u32 valid_params;
	u16 src_id;
	u16 src_index;
	u16 dst_id;
	u16 dst_host_irq;
	u16 ia_id;
	u16 vint;
	u16 global_event;
	u8 vint_status_bit;
	u8 secondary_host;
} __packed;

 
struct ti_sci_msg_rm_ring_cfg_req {
	struct ti_sci_msg_hdr hdr;
	u32 valid_params;
	u16 nav_id;
	u16 index;
	u32 addr_lo;
	u32 addr_hi;
	u32 count;
	u8 mode;
	u8 size;
	u8 order_id;
	u16 virtid;
	u8 asel;
} __packed;

 
struct ti_sci_msg_psil_pair {
	struct ti_sci_msg_hdr hdr;
	u32 nav_id;
	u32 src_thread;
	u32 dst_thread;
} __packed;

 
struct ti_sci_msg_psil_unpair {
	struct ti_sci_msg_hdr hdr;
	u32 nav_id;
	u32 src_thread;
	u32 dst_thread;
} __packed;

 
struct ti_sci_msg_udmap_rx_flow_cfg {
	struct ti_sci_msg_hdr hdr;
	u32 nav_id;
	u32 flow_index;
	u32 rx_ch_index;
	u8 rx_einfo_present;
	u8 rx_psinfo_present;
	u8 rx_error_handling;
	u8 rx_desc_type;
	u16 rx_sop_offset;
	u16 rx_dest_qnum;
	u8 rx_ps_location;
	u8 rx_src_tag_hi;
	u8 rx_src_tag_lo;
	u8 rx_dest_tag_hi;
	u8 rx_dest_tag_lo;
	u8 rx_src_tag_hi_sel;
	u8 rx_src_tag_lo_sel;
	u8 rx_dest_tag_hi_sel;
	u8 rx_dest_tag_lo_sel;
	u8 rx_size_thresh_en;
	u16 rx_fdq0_sz0_qnum;
	u16 rx_fdq1_qnum;
	u16 rx_fdq2_qnum;
	u16 rx_fdq3_qnum;
} __packed;

 
struct rm_ti_sci_msg_udmap_rx_flow_opt_cfg {
	struct ti_sci_msg_hdr hdr;
	u32 nav_id;
	u32 flow_index;
	u32 rx_ch_index;
	u16 rx_size_thresh0;
	u16 rx_size_thresh1;
	u16 rx_size_thresh2;
	u16 rx_fdq0_sz1_qnum;
	u16 rx_fdq0_sz2_qnum;
	u16 rx_fdq0_sz3_qnum;
} __packed;

 
struct ti_sci_msg_rm_udmap_tx_ch_cfg_req {
	struct ti_sci_msg_hdr hdr;
	u32 valid_params;
	u16 nav_id;
	u16 index;
	u8 tx_pause_on_err;
	u8 tx_filt_einfo;
	u8 tx_filt_pswords;
	u8 tx_atype;
	u8 tx_chan_type;
	u8 tx_supr_tdpkt;
	u16 tx_fetch_size;
	u8 tx_credit_count;
	u16 txcq_qnum;
	u8 tx_priority;
	u8 tx_qos;
	u8 tx_orderid;
	u16 fdepth;
	u8 tx_sched_priority;
	u8 tx_burst_size;
	u8 tx_tdtype;
	u8 extended_ch_type;
} __packed;

 
struct ti_sci_msg_rm_udmap_rx_ch_cfg_req {
	struct ti_sci_msg_hdr hdr;
	u32 valid_params;
	u16 nav_id;
	u16 index;
	u16 rx_fetch_size;
	u16 rxcq_qnum;
	u8 rx_priority;
	u8 rx_qos;
	u8 rx_orderid;
	u8 rx_sched_priority;
	u16 flowid_start;
	u16 flowid_cnt;
	u8 rx_pause_on_err;
	u8 rx_atype;
	u8 rx_chan_type;
	u8 rx_ignore_short;
	u8 rx_ignore_long;
	u8 rx_burst_size;
} __packed;

 
struct ti_sci_msg_rm_udmap_flow_cfg_req {
	struct ti_sci_msg_hdr hdr;
	u32 valid_params;
	u16 nav_id;
	u16 flow_index;
	u8 rx_einfo_present;
	u8 rx_psinfo_present;
	u8 rx_error_handling;
	u8 rx_desc_type;
	u16 rx_sop_offset;
	u16 rx_dest_qnum;
	u8 rx_src_tag_hi;
	u8 rx_src_tag_lo;
	u8 rx_dest_tag_hi;
	u8 rx_dest_tag_lo;
	u8 rx_src_tag_hi_sel;
	u8 rx_src_tag_lo_sel;
	u8 rx_dest_tag_hi_sel;
	u8 rx_dest_tag_lo_sel;
	u16 rx_fdq0_sz0_qnum;
	u16 rx_fdq1_qnum;
	u16 rx_fdq2_qnum;
	u16 rx_fdq3_qnum;
	u8 rx_ps_location;
} __packed;

 
struct ti_sci_msg_req_proc_request {
	struct ti_sci_msg_hdr hdr;
	u8 processor_id;
} __packed;

 
struct ti_sci_msg_req_proc_release {
	struct ti_sci_msg_hdr hdr;
	u8 processor_id;
} __packed;

 
struct ti_sci_msg_req_proc_handover {
	struct ti_sci_msg_hdr hdr;
	u8 processor_id;
	u8 host_id;
} __packed;

 
#define TI_SCI_ADDR_LOW_MASK			GENMASK_ULL(31, 0)
#define TI_SCI_ADDR_HIGH_MASK			GENMASK_ULL(63, 32)
#define TI_SCI_ADDR_HIGH_SHIFT			32

 
struct ti_sci_msg_req_set_config {
	struct ti_sci_msg_hdr hdr;
	u8 processor_id;
	u32 bootvector_low;
	u32 bootvector_high;
	u32 config_flags_set;
	u32 config_flags_clear;
} __packed;

 
struct ti_sci_msg_req_set_ctrl {
	struct ti_sci_msg_hdr hdr;
	u8 processor_id;
	u32 control_flags_set;
	u32 control_flags_clear;
} __packed;

 
struct ti_sci_msg_req_get_status {
	struct ti_sci_msg_hdr hdr;
	u8 processor_id;
} __packed;

 
struct ti_sci_msg_resp_get_status {
	struct ti_sci_msg_hdr hdr;
	u8 processor_id;
	u32 bootvector_low;
	u32 bootvector_high;
	u32 config_flags;
	u32 control_flags;
	u32 status_flags;
} __packed;

#endif  
