


#ifndef _IPA_REG_H_
#define _IPA_REG_H_

#include <linux/bitfield.h>
#include <linux/bug.h>

#include "ipa_version.h"
#include "reg.h"

struct ipa;




enum ipa_reg_id {
	COMP_CFG,
	CLKON_CFG,
	ROUTE,
	SHARED_MEM_SIZE,
	QSB_MAX_WRITES,
	QSB_MAX_READS,
	FILT_ROUT_HASH_EN,				
	FILT_ROUT_HASH_FLUSH,			
	FILT_ROUT_CACHE_FLUSH,				
	STATE_AGGR_ACTIVE,
	IPA_BCR,					
	LOCAL_PKT_PROC_CNTXT,
	AGGR_FORCE_CLOSE,
	COUNTER_CFG,					
	IPA_TX_CFG,					
	FLAVOR_0,					
	IDLE_INDICATION_CFG,				
	QTIME_TIMESTAMP_CFG,				
	TIMERS_XO_CLK_DIV_CFG,				
	TIMERS_PULSE_GRAN_CFG,				
	SRC_RSRC_GRP_01_RSRC_TYPE,
	SRC_RSRC_GRP_23_RSRC_TYPE,
	SRC_RSRC_GRP_45_RSRC_TYPE,	
	SRC_RSRC_GRP_67_RSRC_TYPE,		
	DST_RSRC_GRP_01_RSRC_TYPE,
	DST_RSRC_GRP_23_RSRC_TYPE,
	DST_RSRC_GRP_45_RSRC_TYPE,	
	DST_RSRC_GRP_67_RSRC_TYPE,		
	ENDP_INIT_CTRL,		
	ENDP_INIT_CFG,
	ENDP_INIT_NAT,			
	ENDP_INIT_HDR,
	ENDP_INIT_HDR_EXT,
	ENDP_INIT_HDR_METADATA_MASK,	
	ENDP_INIT_MODE,			
	ENDP_INIT_AGGR,
	ENDP_INIT_HOL_BLOCK_EN,		
	ENDP_INIT_HOL_BLOCK_TIMER,	
	ENDP_INIT_DEAGGR,		
	ENDP_INIT_RSRC_GRP,
	ENDP_INIT_SEQ,			
	ENDP_STATUS,
	ENDP_FILTER_ROUTER_HSH_CFG,			
	ENDP_FILTER_CACHE_CFG,				
	ENDP_ROUTER_CACHE_CFG,				
	
	IPA_IRQ_STTS,
	IPA_IRQ_EN,
	IPA_IRQ_CLR,
	IPA_IRQ_UC,
	IRQ_SUSPEND_INFO,
	IRQ_SUSPEND_EN,					
	IRQ_SUSPEND_CLR,				
	IPA_REG_ID_COUNT,				
};


enum ipa_reg_comp_cfg_field_id {
	COMP_CFG_ENABLE,				
	RAM_ARB_PRI_CLIENT_SAMP_FIX_DIS,		
	GSI_SNOC_BYPASS_DIS,
	GEN_QMB_0_SNOC_BYPASS_DIS,
	GEN_QMB_1_SNOC_BYPASS_DIS,
	IPA_DCMP_FAST_CLK_EN,				
	IPA_QMB_SELECT_CONS_EN,				
	IPA_QMB_SELECT_PROD_EN,				
	GSI_MULTI_INORDER_RD_DIS,			
	GSI_MULTI_INORDER_WR_DIS,			
	GEN_QMB_0_MULTI_INORDER_RD_DIS,			
	GEN_QMB_1_MULTI_INORDER_RD_DIS,			
	GEN_QMB_0_MULTI_INORDER_WR_DIS,			
	GEN_QMB_1_MULTI_INORDER_WR_DIS,			
	GEN_QMB_0_SNOC_CNOC_LOOP_PROT_DIS,		
	GSI_SNOC_CNOC_LOOP_PROT_DISABLE,		
	GSI_MULTI_AXI_MASTERS_DIS,			
	IPA_QMB_SELECT_GLOBAL_EN,			
	QMB_RAM_RD_CACHE_DISABLE,			
	GENQMB_AOOOWR,					
	IF_OUT_OF_BUF_STOP_RESET_MASK_EN,		
	GEN_QMB_1_DYNAMIC_ASIZE,			
	GEN_QMB_0_DYNAMIC_ASIZE,			
	ATOMIC_FETCHER_ARB_LOCK_DIS,			
	FULL_FLUSH_WAIT_RS_CLOSURE_EN,			
};


enum ipa_reg_clkon_cfg_field_id {
	CLKON_RX,
	CLKON_PROC,
	TX_WRAPPER,
	CLKON_MISC,
	RAM_ARB,
	FTCH_HPS,
	FTCH_DPS,
	CLKON_HPS,
	CLKON_DPS,
	RX_HPS_CMDQS,
	HPS_DPS_CMDQS,
	DPS_TX_CMDQS,
	RSRC_MNGR,
	CTX_HANDLER,
	ACK_MNGR,
	D_DCPH,
	H_DCPH,
	CLKON_DCMP,					
	NTF_TX_CMDQS,					
	CLKON_TX_0,					
	CLKON_TX_1,					
	CLKON_FNR,					
	QSB2AXI_CMDQ_L,					
	AGGR_WRAPPER,					
	RAM_SLAVEWAY,					
	CLKON_QMB,					
	WEIGHT_ARB,					
	GSI_IF,						
	CLKON_GLOBAL,					
	GLOBAL_2X_CLK,					
	DPL_FIFO,					
	DRBIP,						
};


enum ipa_reg_route_field_id {
	ROUTE_DIS,
	ROUTE_DEF_PIPE,
	ROUTE_DEF_HDR_TABLE,
	ROUTE_DEF_HDR_OFST,
	ROUTE_FRAG_DEF_PIPE,
	ROUTE_DEF_RETAIN_HDR,
};


enum ipa_reg_shared_mem_size_field_id {
	MEM_SIZE,
	MEM_BADDR,
};


enum ipa_reg_qsb_max_writes_field_id {
	GEN_QMB_0_MAX_WRITES,
	GEN_QMB_1_MAX_WRITES,
};


enum ipa_reg_qsb_max_reads_field_id {
	GEN_QMB_0_MAX_READS,
	GEN_QMB_1_MAX_READS,
	GEN_QMB_0_MAX_READS_BEATS,			
	GEN_QMB_1_MAX_READS_BEATS,			
};


enum ipa_reg_filt_rout_hash_field_id {
	IPV6_ROUTER_HASH,
	IPV6_FILTER_HASH,
	IPV4_ROUTER_HASH,
	IPV4_FILTER_HASH,
};


enum ipa_reg_filt_rout_cache_field_id {
	ROUTER_CACHE,
	FILTER_CACHE,
};


enum ipa_bcr_compat {
	BCR_CMDQ_L_LACK_ONE_ENTRY		= 0x0,	
	BCR_TX_NOT_USING_BRESP			= 0x1,	
	BCR_TX_SUSPEND_IRQ_ASSERT_ONCE		= 0x2,	
	BCR_SUSPEND_L2_IRQ			= 0x3,	
	BCR_HOLB_DROP_L2_IRQ			= 0x4,	
	BCR_DUAL_TX				= 0x5,	
	BCR_ENABLE_FILTER_DATA_CACHE		= 0x6,	
	BCR_NOTIF_PRIORITY_OVER_ZLT		= 0x7,	
	BCR_FILTER_PREFETCH_EN			= 0x8,	
	BCR_ROUTER_PREFETCH_EN			= 0x9,	
};


enum ipa_reg_local_pkt_proc_cntxt_field_id {
	IPA_BASE_ADDR,
};


enum ipa_reg_counter_cfg_field_id {
	EOT_COAL_GRANULARITY,				
	AGGR_GRANULARITY,
};


enum ipa_reg_ipa_tx_cfg_field_id {
	TX0_PREFETCH_DISABLE,				
	TX1_PREFETCH_DISABLE,				
	PREFETCH_ALMOST_EMPTY_SIZE,			
	PREFETCH_ALMOST_EMPTY_SIZE_TX0,			
	DMAW_SCND_OUTSD_PRED_THRESHOLD,			
	DMAW_SCND_OUTSD_PRED_EN,			
	DMAW_MAX_BEATS_256_DIS,				
	PA_MASK_EN,					
	PREFETCH_ALMOST_EMPTY_SIZE_TX1,			
	DUAL_TX_ENABLE,					
	SSPND_PA_NO_START_STATE,			
	SSPND_PA_NO_BQ_STATE,				
	HOLB_STICKY_DROP_EN,				
};


enum ipa_reg_flavor_0_field_id {
	MAX_PIPES,
	MAX_CONS_PIPES,
	MAX_PROD_PIPES,
	PROD_LOWEST,
};


enum ipa_reg_idle_indication_cfg_field_id {
	ENTER_IDLE_DEBOUNCE_THRESH,
	CONST_NON_IDLE_ENABLE,
};


enum ipa_reg_qtime_timestamp_cfg_field_id {
	DPL_TIMESTAMP_LSB,
	DPL_TIMESTAMP_SEL,
	TAG_TIMESTAMP_LSB,
	NAT_TIMESTAMP_LSB,
};


enum ipa_reg_timers_xo_clk_div_cfg_field_id {
	DIV_VALUE,
	DIV_ENABLE,
};


enum ipa_reg_timers_pulse_gran_cfg_field_id {
	PULSE_GRAN_0,
	PULSE_GRAN_1,
	PULSE_GRAN_2,
	PULSE_GRAN_3,
};


enum ipa_pulse_gran {
	IPA_GRAN_10_US				= 0x0,
	IPA_GRAN_20_US				= 0x1,
	IPA_GRAN_50_US				= 0x2,
	IPA_GRAN_100_US				= 0x3,
	IPA_GRAN_1_MS				= 0x4,
	IPA_GRAN_10_MS				= 0x5,
	IPA_GRAN_100_MS				= 0x6,
	IPA_GRAN_655350_US			= 0x7,
};


enum ipa_reg_rsrc_grp_rsrc_type_field_id {
	X_MIN_LIM,
	X_MAX_LIM,
	Y_MIN_LIM,
	Y_MAX_LIM,
};


enum ipa_reg_endp_init_ctrl_field_id {
	ENDP_SUSPEND,					
	ENDP_DELAY,					
};


enum ipa_reg_endp_init_cfg_field_id {
	FRAG_OFFLOAD_EN,
	CS_OFFLOAD_EN,
	CS_METADATA_HDR_OFFSET,
	CS_GEN_QMB_MASTER_SEL,
};


enum ipa_cs_offload_en {
	IPA_CS_OFFLOAD_NONE			= 0x0,
	IPA_CS_OFFLOAD_UL		= 0x1,	
	IPA_CS_OFFLOAD_DL		= 0x2,	
	IPA_CS_OFFLOAD_INLINE		= 0x1,	
};


enum ipa_reg_endp_init_nat_field_id {
	NAT_EN,
};


enum ipa_nat_type {
	IPA_NAT_TYPE_BYPASS			= 0,
	IPA_NAT_TYPE_SRC			= 1,
	IPA_NAT_TYPE_DST			= 2,
};


enum ipa_reg_endp_init_hdr_field_id {
	HDR_LEN,
	HDR_OFST_METADATA_VALID,
	HDR_OFST_METADATA,
	HDR_ADDITIONAL_CONST_LEN,
	HDR_OFST_PKT_SIZE_VALID,
	HDR_OFST_PKT_SIZE,
	HDR_A5_MUX,					
	HDR_LEN_INC_DEAGG_HDR,
	HDR_METADATA_REG_VALID,				
	HDR_LEN_MSB,					
	HDR_OFST_METADATA_MSB,				
};


enum ipa_reg_endp_init_hdr_ext_field_id {
	HDR_ENDIANNESS,
	HDR_TOTAL_LEN_OR_PAD_VALID,
	HDR_TOTAL_LEN_OR_PAD,
	HDR_PAYLOAD_LEN_INC_PADDING,
	HDR_TOTAL_LEN_OR_PAD_OFFSET,
	HDR_PAD_TO_ALIGNMENT,
	HDR_TOTAL_LEN_OR_PAD_OFFSET_MSB,		
	HDR_OFST_PKT_SIZE_MSB,				
	HDR_ADDITIONAL_CONST_LEN_MSB,			
	HDR_BYTES_TO_REMOVE_VALID,			
	HDR_BYTES_TO_REMOVE,				
};


enum ipa_reg_endp_init_mode_field_id {
	ENDP_MODE,
	DCPH_ENABLE,					
	DEST_PIPE_INDEX,
	BYTE_THRESHOLD,
	PIPE_REPLICATION_EN,
	PAD_EN,
	HDR_FTCH_DISABLE,				
	DRBIP_ACL_ENABLE,				
};


enum ipa_mode {
	IPA_BASIC				= 0x0,
	IPA_ENABLE_FRAMING_HDLC			= 0x1,
	IPA_ENABLE_DEFRAMING_HDLC		= 0x2,
	IPA_DMA					= 0x3,
};


enum ipa_reg_endp_init_aggr_field_id {
	AGGR_EN,
	AGGR_TYPE,
	BYTE_LIMIT,
	TIME_LIMIT,
	PKT_LIMIT,
	SW_EOF_ACTIVE,
	FORCE_CLOSE,
	HARD_BYTE_LIMIT_EN,
	AGGR_GRAN_SEL,
};


enum ipa_aggr_en {
	IPA_BYPASS_AGGR			= 0x0,
	IPA_ENABLE_AGGR			= 0x1,
	IPA_ENABLE_DEAGGR		= 0x2,
};


enum ipa_aggr_type {
	IPA_MBIM_16				= 0x0,
	IPA_HDLC				= 0x1,
	IPA_TLP					= 0x2,
	IPA_RNDIS				= 0x3,
	IPA_GENERIC				= 0x4,
	IPA_COALESCE				= 0x5,
	IPA_QCMAP				= 0x6,
};


enum ipa_reg_endp_init_hol_block_en_field_id {
	HOL_BLOCK_EN,
};


enum ipa_reg_endp_init_hol_block_timer_field_id {
	TIMER_BASE_VALUE,				
	TIMER_SCALE,					
	TIMER_LIMIT,					
	TIMER_GRAN_SEL,					
};


enum ipa_reg_endp_deaggr_field_id {
	DEAGGR_HDR_LEN,
	SYSPIPE_ERR_DETECTION,
	PACKET_OFFSET_VALID,
	PACKET_OFFSET_LOCATION,
	IGNORE_MIN_PKT_ERR,
	MAX_PACKET_LEN,
};


enum ipa_reg_endp_init_rsrc_grp_field_id {
	ENDP_RSRC_GRP,
};


enum ipa_reg_endp_init_seq_field_id {
	SEQ_TYPE,
	SEQ_REP_TYPE,					
};


enum ipa_seq_type {
	IPA_SEQ_DMA				= 0x00,
	IPA_SEQ_1_PASS				= 0x02,
	IPA_SEQ_2_PASS_SKIP_LAST_UC		= 0x04,
	IPA_SEQ_1_PASS_SKIP_LAST_UC		= 0x06,
	IPA_SEQ_2_PASS				= 0x0a,
	IPA_SEQ_3_PASS_SKIP_LAST_UC		= 0x0c,
	
	IPA_SEQ_DECIPHER			= 0x11,
};


enum ipa_seq_rep_type {
	IPA_SEQ_REP_DMA_PARSER			= 0x08,
};


enum ipa_reg_endp_status_field_id {
	STATUS_EN,
	STATUS_ENDP,
	STATUS_LOCATION,				
	STATUS_PKT_SUPPRESS,				
};


enum ipa_reg_endp_filter_router_hsh_cfg_field_id {
	FILTER_HASH_MSK_SRC_ID,
	FILTER_HASH_MSK_SRC_IP,
	FILTER_HASH_MSK_DST_IP,
	FILTER_HASH_MSK_SRC_PORT,
	FILTER_HASH_MSK_DST_PORT,
	FILTER_HASH_MSK_PROTOCOL,
	FILTER_HASH_MSK_METADATA,
	FILTER_HASH_MSK_ALL,		

	ROUTER_HASH_MSK_SRC_ID,
	ROUTER_HASH_MSK_SRC_IP,
	ROUTER_HASH_MSK_DST_IP,
	ROUTER_HASH_MSK_SRC_PORT,
	ROUTER_HASH_MSK_DST_PORT,
	ROUTER_HASH_MSK_PROTOCOL,
	ROUTER_HASH_MSK_METADATA,
	ROUTER_HASH_MSK_ALL,		
};


enum ipa_reg_endp_cache_cfg_field_id {
	CACHE_MSK_SRC_ID,
	CACHE_MSK_SRC_IP,
	CACHE_MSK_DST_IP,
	CACHE_MSK_SRC_PORT,
	CACHE_MSK_DST_PORT,
	CACHE_MSK_PROTOCOL,
	CACHE_MSK_METADATA,
};



enum ipa_irq_id {
	IPA_IRQ_BAD_SNOC_ACCESS			= 0x0,
	
	IPA_IRQ_EOT_COAL			= 0x1,
	IPA_IRQ_UC_0				= 0x2,
	IPA_IRQ_UC_1				= 0x3,
	IPA_IRQ_UC_2				= 0x4,
	IPA_IRQ_UC_3				= 0x5,
	IPA_IRQ_UC_IN_Q_NOT_EMPTY		= 0x6,
	IPA_IRQ_UC_RX_CMD_Q_NOT_FULL		= 0x7,
	IPA_IRQ_PROC_UC_ACK_Q_NOT_EMPTY		= 0x8,
	IPA_IRQ_RX_ERR				= 0x9,
	IPA_IRQ_DEAGGR_ERR			= 0xa,
	IPA_IRQ_TX_ERR				= 0xb,
	IPA_IRQ_STEP_MODE			= 0xc,
	IPA_IRQ_PROC_ERR			= 0xd,
	IPA_IRQ_TX_SUSPEND			= 0xe,
	IPA_IRQ_TX_HOLB_DROP			= 0xf,
	IPA_IRQ_BAM_GSI_IDLE			= 0x10,
	IPA_IRQ_PIPE_YELLOW_BELOW		= 0x11,
	IPA_IRQ_PIPE_RED_BELOW			= 0x12,
	IPA_IRQ_PIPE_YELLOW_ABOVE		= 0x13,
	IPA_IRQ_PIPE_RED_ABOVE			= 0x14,
	IPA_IRQ_UCP				= 0x15,
	
	IPA_IRQ_DCMP				= 0x16,
	IPA_IRQ_GSI_EE				= 0x17,
	IPA_IRQ_GSI_IPA_IF_TLV_RCVD		= 0x18,
	IPA_IRQ_GSI_UC				= 0x19,
	
	IPA_IRQ_TLV_LEN_MIN_DSM			= 0x1a,
	
	IPA_IRQ_DRBIP_PKT_EXCEED_MAX_SIZE_EN	= 0x1b,
	IPA_IRQ_DRBIP_DATA_SCTR_CFG_ERROR_EN	= 0x1c,
	IPA_IRQ_DRBIP_IMM_CMD_NO_FLSH_HZRD_EN	= 0x1d,
	IPA_IRQ_COUNT,				
};


enum ipa_reg_ipa_irq_uc_field_id {
	UC_INTR,
};

extern const struct regs ipa_regs_v3_1;
extern const struct regs ipa_regs_v3_5_1;
extern const struct regs ipa_regs_v4_2;
extern const struct regs ipa_regs_v4_5;
extern const struct regs ipa_regs_v4_7;
extern const struct regs ipa_regs_v4_9;
extern const struct regs ipa_regs_v4_11;
extern const struct regs ipa_regs_v5_0;

const struct reg *ipa_reg(struct ipa *ipa, enum ipa_reg_id reg_id);

int ipa_reg_init(struct ipa *ipa);
void ipa_reg_exit(struct ipa *ipa);

#endif 
