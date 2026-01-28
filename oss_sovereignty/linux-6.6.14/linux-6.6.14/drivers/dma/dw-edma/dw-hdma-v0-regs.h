#ifndef _DW_HDMA_V0_REGS_H
#define _DW_HDMA_V0_REGS_H
#include <linux/dmaengine.h>
#define HDMA_V0_MAX_NR_CH			8
#define HDMA_V0_LOCAL_ABORT_INT_EN		BIT(6)
#define HDMA_V0_REMOTE_ABORT_INT_EN		BIT(5)
#define HDMA_V0_LOCAL_STOP_INT_EN		BIT(4)
#define HDMA_V0_REMOTEL_STOP_INT_EN		BIT(3)
#define HDMA_V0_ABORT_INT_MASK			BIT(2)
#define HDMA_V0_STOP_INT_MASK			BIT(0)
#define HDMA_V0_LINKLIST_EN			BIT(0)
#define HDMA_V0_CONSUMER_CYCLE_STAT		BIT(1)
#define HDMA_V0_CONSUMER_CYCLE_BIT		BIT(0)
#define HDMA_V0_DOORBELL_START			BIT(0)
#define HDMA_V0_CH_STATUS_MASK			GENMASK(1, 0)
struct dw_hdma_v0_ch_regs {
	u32 ch_en;				 
	u32 doorbell;				 
	u32 prefetch;				 
	u32 handshake;				 
	union {
		u64 reg;			 
		struct {
			u32 lsb;		 
			u32 msb;		 
		};
	} llp;
	u32 cycle_sync;				 
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
	u32 watermark_en;			 
	u32 control1;				 
	u32 func_num;				 
	u32 qos;				 
	u32 padding_1[16];			 
	u32 ch_stat;				 
	u32 int_stat;				 
	u32 int_setup;				 
	u32 int_clear;				 
	union {
		u64 reg;			 
		struct {
			u32 lsb;		 
			u32 msb;		 
		};
	} msi_stop;
	union {
		u64 reg;			 
		struct {
			u32 lsb;		 
			u32 msb;		 
		};
	} msi_watermark;
	union {
		u64 reg;			 
		struct {
			u32 lsb;		 
			u32 msb;		 
		};
	} msi_abort;
	u32 msi_msgdata;			 
	u32 padding_2[21];			 
} __packed;
struct dw_hdma_v0_ch {
	struct dw_hdma_v0_ch_regs wr;		 
	struct dw_hdma_v0_ch_regs rd;		 
} __packed;
struct dw_hdma_v0_regs {
	struct dw_hdma_v0_ch ch[HDMA_V0_MAX_NR_CH];	 
} __packed;
struct dw_hdma_v0_lli {
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
struct dw_hdma_v0_llp {
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
