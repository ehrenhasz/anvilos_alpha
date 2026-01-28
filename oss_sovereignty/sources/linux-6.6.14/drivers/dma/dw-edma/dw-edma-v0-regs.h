


#ifndef _DW_EDMA_V0_REGS_H
#define _DW_EDMA_V0_REGS_H

#include <linux/dmaengine.h>

#define EDMA_V0_MAX_NR_CH				8
#define EDMA_V0_VIEWPORT_MASK				GENMASK(2, 0)
#define EDMA_V0_DONE_INT_MASK				GENMASK(7, 0)
#define EDMA_V0_ABORT_INT_MASK				GENMASK(23, 16)
#define EDMA_V0_WRITE_CH_COUNT_MASK			GENMASK(3, 0)
#define EDMA_V0_READ_CH_COUNT_MASK			GENMASK(19, 16)
#define EDMA_V0_CH_STATUS_MASK				GENMASK(6, 5)
#define EDMA_V0_DOORBELL_CH_MASK			GENMASK(2, 0)
#define EDMA_V0_LINKED_LIST_ERR_MASK			GENMASK(7, 0)

#define EDMA_V0_CH_ODD_MSI_DATA_MASK			GENMASK(31, 16)
#define EDMA_V0_CH_EVEN_MSI_DATA_MASK			GENMASK(15, 0)

struct dw_edma_v0_ch_regs {
	u32 ch_control1;				
	u32 ch_control2;				
	u32 transfer_size;				
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} sar;
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} dar;
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} llp;
} __packed;

struct dw_edma_v0_ch {
	struct dw_edma_v0_ch_regs wr;			
	u32 padding_1[55];				
	struct dw_edma_v0_ch_regs rd;			
	u32 padding_2[55];				
} __packed;

struct dw_edma_v0_unroll {
	u32 padding_1;					
	u32 wr_engine_chgroup;				
	u32 rd_engine_chgroup;				
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} wr_engine_hshake_cnt;
	u32 padding_2[2];				
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} rd_engine_hshake_cnt;
	u32 padding_3[2];				
	u32 wr_ch0_pwr_en;				
	u32 wr_ch1_pwr_en;				
	u32 wr_ch2_pwr_en;				
	u32 wr_ch3_pwr_en;				
	u32 wr_ch4_pwr_en;				
	u32 wr_ch5_pwr_en;				
	u32 wr_ch6_pwr_en;				
	u32 wr_ch7_pwr_en;				
	u32 padding_4[8];				
	u32 rd_ch0_pwr_en;				
	u32 rd_ch1_pwr_en;				
	u32 rd_ch2_pwr_en;				
	u32 rd_ch3_pwr_en;				
	u32 rd_ch4_pwr_en;				
	u32 rd_ch5_pwr_en;				
	u32 rd_ch6_pwr_en;				
	u32 rd_ch7_pwr_en;				
	u32 padding_5[30];				
	struct dw_edma_v0_ch ch[EDMA_V0_MAX_NR_CH];	
} __packed;

struct dw_edma_v0_legacy {
	u32 viewport_sel;				
	struct dw_edma_v0_ch_regs ch;			
} __packed;

struct dw_edma_v0_regs {
	
	u32 ctrl_data_arb_prior;			
	u32 padding_1;					
	u32 ctrl;					
	u32 wr_engine_en;				
	u32 wr_doorbell;				
	u32 padding_2;					
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} wr_ch_arb_weight;
	u32 padding_3[3];				
	u32 rd_engine_en;				
	u32 rd_doorbell;				
	u32 padding_4;					
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} rd_ch_arb_weight;
	u32 padding_5[3];				
	
	u32 wr_int_status;				
	u32 padding_6;					
	u32 wr_int_mask;				
	u32 wr_int_clear;				
	u32 wr_err_status;				
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} wr_done_imwr;
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} wr_abort_imwr;
	u32 wr_ch01_imwr_data;				
	u32 wr_ch23_imwr_data;				
	u32 wr_ch45_imwr_data;				
	u32 wr_ch67_imwr_data;				
	u32 padding_7[4];				
	u32 wr_linked_list_err_en;			
	u32 padding_8[3];				
	u32 rd_int_status;				
	u32 padding_9;					
	u32 rd_int_mask;				
	u32 rd_int_clear;				
	u32 padding_10;					
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} rd_err_status;
	u32 padding_11[2];				
	u32 rd_linked_list_err_en;			
	u32 padding_12;					
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} rd_done_imwr;
	union {
		u64 reg;				
		struct {
			u32 lsb;			
			u32 msb;			
		};
	} rd_abort_imwr;
	u32 rd_ch01_imwr_data;				
	u32 rd_ch23_imwr_data;				
	u32 rd_ch45_imwr_data;				
	u32 rd_ch67_imwr_data;				
	u32 padding_13[4];				
	
	union dw_edma_v0_type {
		struct dw_edma_v0_legacy legacy;	
		struct dw_edma_v0_unroll unroll;	
	} type;
} __packed;

struct dw_edma_v0_lli {
	u32 control;
	u32 transfer_size;
	union {
		u64 reg;
		struct {
			u32 lsb;
			u32 msb;
		};
	} sar;
	union {
		u64 reg;
		struct {
			u32 lsb;
			u32 msb;
		};
	} dar;
} __packed;

struct dw_edma_v0_llp {
	u32 control;
	u32 reserved;
	union {
		u64 reg;
		struct {
			u32 lsb;
			u32 msb;
		};
	} llp;
} __packed;

#endif 
