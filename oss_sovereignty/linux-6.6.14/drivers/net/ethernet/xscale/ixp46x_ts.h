 
 

#ifndef _IXP46X_TS_H_
#define _IXP46X_TS_H_

#define DEFAULT_ADDEND 0xF0000029
#define TICKS_NS_SHIFT 4

struct ixp46x_channel_ctl {
	u32 ch_control;   
	u32 ch_event;     
	u32 tx_snap_lo;   
	u32 tx_snap_hi;   
	u32 rx_snap_lo;   
	u32 rx_snap_hi;   
	u32 src_uuid_lo;  
	u32 src_uuid_hi;  
};

struct ixp46x_ts_regs {
	u32 control;      
	u32 event;        
	u32 addend;       
	u32 accum;        
	u32 test;         
	u32 unused;       
	u32 rsystime_lo;  
	u32 rsystime_hi;  
	u32 systime_lo;   
	u32 systime_hi;   
	u32 trgt_lo;      
	u32 trgt_hi;      
	u32 asms_lo;      
	u32 asms_hi;      
	u32 amms_lo;      
	u32 amms_hi;      

	struct ixp46x_channel_ctl channel[3];
};

 
#define TSCR_AMM (1<<3)
#define TSCR_ASM (1<<2)
#define TSCR_TTM (1<<1)
#define TSCR_RST (1<<0)

 
#define TSER_SNM (1<<3)
#define TSER_SNS (1<<2)
#define TTIPEND  (1<<1)

 
#define MASTER_MODE   (1<<0)
#define TIMESTAMP_ALL (1<<1)

 
#define TX_SNAPSHOT_LOCKED (1<<0)
#define RX_SNAPSHOT_LOCKED (1<<1)

#if IS_ENABLED(CONFIG_PTP_1588_CLOCK_IXP46X)
int ixp46x_ptp_find(struct ixp46x_ts_regs *__iomem *regs, int *phc_index);
#else
static inline int ixp46x_ptp_find(struct ixp46x_ts_regs *__iomem *regs, int *phc_index)
{
	*regs = NULL;
	*phc_index = -1;

	return -ENODEV;
}
#endif

#endif
