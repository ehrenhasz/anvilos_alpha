


#ifndef _GSI_REG_H_
#define _GSI_REG_H_



#include <linux/bits.h>

struct platform_device;

struct gsi;




enum gsi_reg_id {
	INTER_EE_SRC_CH_IRQ_MSK,			
	INTER_EE_SRC_EV_CH_IRQ_MSK,			
	CH_C_CNTXT_0,
	CH_C_CNTXT_1,
	CH_C_CNTXT_2,
	CH_C_CNTXT_3,
	CH_C_QOS,
	CH_C_SCRATCH_0,
	CH_C_SCRATCH_1,
	CH_C_SCRATCH_2,
	CH_C_SCRATCH_3,
	EV_CH_E_CNTXT_0,
	EV_CH_E_CNTXT_1,
	EV_CH_E_CNTXT_2,
	EV_CH_E_CNTXT_3,
	EV_CH_E_CNTXT_4,
	EV_CH_E_CNTXT_8,
	EV_CH_E_CNTXT_9,
	EV_CH_E_CNTXT_10,
	EV_CH_E_CNTXT_11,
	EV_CH_E_CNTXT_12,
	EV_CH_E_CNTXT_13,
	EV_CH_E_SCRATCH_0,
	EV_CH_E_SCRATCH_1,
	CH_C_DOORBELL_0,
	EV_CH_E_DOORBELL_0,
	GSI_STATUS,
	CH_CMD,
	EV_CH_CMD,
	GENERIC_CMD,
	HW_PARAM_2,					
	HW_PARAM_4,					
	CNTXT_TYPE_IRQ,
	CNTXT_TYPE_IRQ_MSK,
	CNTXT_SRC_CH_IRQ,
	CNTXT_SRC_CH_IRQ_MSK,
	CNTXT_SRC_CH_IRQ_CLR,
	CNTXT_SRC_EV_CH_IRQ,
	CNTXT_SRC_EV_CH_IRQ_MSK,
	CNTXT_SRC_EV_CH_IRQ_CLR,
	CNTXT_SRC_IEOB_IRQ,
	CNTXT_SRC_IEOB_IRQ_MSK,
	CNTXT_SRC_IEOB_IRQ_CLR,
	CNTXT_GLOB_IRQ_STTS,
	CNTXT_GLOB_IRQ_EN,
	CNTXT_GLOB_IRQ_CLR,
	CNTXT_GSI_IRQ_STTS,
	CNTXT_GSI_IRQ_EN,
	CNTXT_GSI_IRQ_CLR,
	CNTXT_INTSET,
	ERROR_LOG,
	ERROR_LOG_CLR,
	CNTXT_SCRATCH_0,
	GSI_REG_ID_COUNT,				
};


enum gsi_reg_ch_c_cntxt_0_field_id {
	CHTYPE_PROTOCOL,
	CHTYPE_DIR,
	CH_EE,
	CHID,
	CHTYPE_PROTOCOL_MSB,				
	ERINDEX,					
	CHSTATE,
	ELEMENT_SIZE,
};


enum gsi_channel_type {
	GSI_CHANNEL_TYPE_MHI			= 0x0,
	GSI_CHANNEL_TYPE_XHCI			= 0x1,
	GSI_CHANNEL_TYPE_GPI			= 0x2,
	GSI_CHANNEL_TYPE_XDCI			= 0x3,
	GSI_CHANNEL_TYPE_WDI2			= 0x4,
	GSI_CHANNEL_TYPE_GCI			= 0x5,
	GSI_CHANNEL_TYPE_WDI3			= 0x6,
	GSI_CHANNEL_TYPE_MHIP			= 0x7,
	GSI_CHANNEL_TYPE_AQC			= 0x8,
	GSI_CHANNEL_TYPE_11AD			= 0x9,
};


enum gsi_reg_ch_c_cntxt_1_field_id {
	CH_R_LENGTH,
	CH_ERINDEX,					
};


enum gsi_reg_ch_c_qos_field_id {
	WRR_WEIGHT,
	MAX_PREFETCH,
	USE_DB_ENG,
	USE_ESCAPE_BUF_ONLY,				
	PREFETCH_MODE,					
	EMPTY_LVL_THRSHOLD,				
	DB_IN_BYTES,					
	LOW_LATENCY_EN,					
};


enum gsi_prefetch_mode {
	USE_PREFETCH_BUFS			= 0,
	ESCAPE_BUF_ONLY				= 1,
	SMART_PREFETCH				= 2,
	FREE_PREFETCH				= 3,
};


enum gsi_reg_ch_c_ev_ch_e_cntxt_0_field_id {
	EV_CHTYPE,	
	EV_EE,		
	EV_EVCHID,
	EV_INTYPE,
	EV_CHSTATE,
	EV_ELEMENT_SIZE,
};


enum gsi_reg_ev_ch_c_cntxt_1_field_id {
	R_LENGTH,
};


enum gsi_reg_ch_c_ev_ch_e_cntxt_8_field_id {
	EV_MODT,
	EV_MODC,
	EV_MOD_CNT,
};


enum gsi_reg_gsi_status_field_id {
	ENABLED,
};


enum gsi_reg_gsi_ch_cmd_field_id {
	CH_CHID,
	CH_OPCODE,
};


enum gsi_ch_cmd_opcode {
	GSI_CH_ALLOCATE				= 0x0,
	GSI_CH_START				= 0x1,
	GSI_CH_STOP				= 0x2,
	GSI_CH_RESET				= 0x9,
	GSI_CH_DE_ALLOC				= 0xa,
	GSI_CH_DB_STOP				= 0xb,
};


enum gsi_ev_ch_cmd_field_id {
	EV_CHID,
	EV_OPCODE,
};


enum gsi_evt_cmd_opcode {
	GSI_EVT_ALLOCATE			= 0x0,
	GSI_EVT_RESET				= 0x9,
	GSI_EVT_DE_ALLOC			= 0xa,
};


enum gsi_generic_cmd_field_id {
	GENERIC_OPCODE,
	GENERIC_CHID,
	GENERIC_EE,
	GENERIC_PARAMS,					
};


enum gsi_generic_cmd_opcode {
	GSI_GENERIC_HALT_CHANNEL		= 0x1,
	GSI_GENERIC_ALLOCATE_CHANNEL		= 0x2,
	GSI_GENERIC_ENABLE_FLOW_CONTROL		= 0x3,	
	GSI_GENERIC_DISABLE_FLOW_CONTROL	= 0x4,	
	GSI_GENERIC_QUERY_FLOW_CONTROL		= 0x5,	
};

				
enum gsi_hw_param_2_field_id {
	IRAM_SIZE,
	NUM_CH_PER_EE,
	NUM_EV_PER_EE,					
	GSI_CH_PEND_TRANSLATE,
	GSI_CH_FULL_LOGIC,
	GSI_USE_SDMA,					
	GSI_SDMA_N_INT,					
	GSI_SDMA_MAX_BURST,				
	GSI_SDMA_N_IOVEC,				
	GSI_USE_RD_WR_ENG,				
	GSI_USE_INTER_EE,				
};


enum gsi_iram_size {
	IRAM_SIZE_ONE_KB			= 0x0,
	IRAM_SIZE_TWO_KB			= 0x1,
	
	IRAM_SIZE_TWO_N_HALF_KB			= 0x2,
	IRAM_SIZE_THREE_KB			= 0x3,
	
	IRAM_SIZE_THREE_N_HALF_KB		= 0x4,
	IRAM_SIZE_FOUR_KB			= 0x5,
};

				
enum gsi_hw_param_4_field_id {
	EV_PER_EE,
	IRAM_PROTOCOL_COUNT,
};


enum gsi_irq_type_id {
	GSI_CH_CTRL				= BIT(0),
	GSI_EV_CTRL				= BIT(1),
	GSI_GLOB_EE				= BIT(2),
	GSI_IEOB				= BIT(3),
	GSI_INTER_EE_CH_CTRL			= BIT(4),
	GSI_INTER_EE_EV_CTRL			= BIT(5),
	GSI_GENERAL				= BIT(6),
	
};


enum gsi_global_irq_id {
	ERROR_INT				= BIT(0),
	GP_INT1					= BIT(1),
	GP_INT2					= BIT(2),
	GP_INT3					= BIT(3),
	
};


enum gsi_general_irq_id {
	BREAK_POINT				= BIT(0),
	BUS_ERROR				= BIT(1),
	CMD_FIFO_OVRFLOW			= BIT(2),
	MCS_STACK_OVRFLOW			= BIT(3),
	
};


enum gsi_cntxt_intset_field_id {
	INTYPE,
};


enum gsi_error_log_field_id {
	ERR_ARG3,
	ERR_ARG2,
	ERR_ARG1,
	ERR_CODE,
	ERR_VIRT_IDX,
	ERR_TYPE,
	ERR_EE,
};


enum gsi_err_code {
	GSI_INVALID_TRE				= 0x1,
	GSI_OUT_OF_BUFFERS			= 0x2,
	GSI_OUT_OF_RESOURCES			= 0x3,
	GSI_UNSUPPORTED_INTER_EE_OP		= 0x4,
	GSI_EVT_RING_EMPTY			= 0x5,
	GSI_NON_ALLOCATED_EVT_ACCESS		= 0x6,
	
	GSI_HWO_1				= 0x8,
};


enum gsi_err_type {
	GSI_ERR_TYPE_GLOB			= 0x1,
	GSI_ERR_TYPE_CHAN			= 0x2,
	GSI_ERR_TYPE_EVT			= 0x3,
};


enum gsi_cntxt_scratch_0_field_id {
	INTER_EE_RESULT,
	GENERIC_EE_RESULT,
};


enum gsi_generic_ee_result {
	GENERIC_EE_SUCCESS			= 0x1,
	GENERIC_EE_INCORRECT_CHANNEL_STATE	= 0x2,
	GENERIC_EE_INCORRECT_DIRECTION		= 0x3,
	GENERIC_EE_INCORRECT_CHANNEL_TYPE	= 0x4,
	GENERIC_EE_INCORRECT_CHANNEL		= 0x5,
	GENERIC_EE_RETRY			= 0x6,
	GENERIC_EE_NO_RESOURCES			= 0x7,
};

extern const struct regs gsi_regs_v3_1;
extern const struct regs gsi_regs_v3_5_1;
extern const struct regs gsi_regs_v4_0;
extern const struct regs gsi_regs_v4_5;
extern const struct regs gsi_regs_v4_9;
extern const struct regs gsi_regs_v4_11;
extern const struct regs gsi_regs_v5_0;


const struct reg *gsi_reg(struct gsi *gsi, enum gsi_reg_id reg_id);


int gsi_reg_init(struct gsi *gsi, struct platform_device *pdev);


void gsi_reg_exit(struct gsi *gsi);

#endif	
