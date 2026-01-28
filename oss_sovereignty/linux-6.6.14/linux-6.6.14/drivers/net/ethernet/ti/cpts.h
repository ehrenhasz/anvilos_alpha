#ifndef _TI_CPTS_H_
#define _TI_CPTS_H_
#if IS_ENABLED(CONFIG_TI_CPTS)
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clocksource.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/skbuff.h>
#include <linux/ptp_classify.h>
#include <linux/timecounter.h>
struct cpsw_cpts {
	u32 idver;                 
	u32 control;               
	u32 rftclk_sel;		   
	u32 ts_push;               
	u32 ts_load_val;           
	u32 ts_load_en;            
	u32 res2[2];
	u32 intstat_raw;           
	u32 intstat_masked;        
	u32 int_enable;            
	u32 res3;
	u32 event_pop;             
	u32 event_low;             
	u32 event_high;            
};
#define TX_IDENT_SHIFT       (16)     
#define TX_IDENT_MASK        (0xffff)
#define RTL_VER_SHIFT        (11)     
#define RTL_VER_MASK         (0x1f)
#define MAJOR_VER_SHIFT      (8)      
#define MAJOR_VER_MASK       (0x7)
#define MINOR_VER_SHIFT      (0)      
#define MINOR_VER_MASK       (0xff)
#define HW4_TS_PUSH_EN       (1<<11)  
#define HW3_TS_PUSH_EN       (1<<10)  
#define HW2_TS_PUSH_EN       (1<<9)   
#define HW1_TS_PUSH_EN       (1<<8)   
#define INT_TEST             (1<<1)   
#define CPTS_EN              (1<<0)   
#define TS_PUSH             (1<<0)   
#define TS_LOAD_EN          (1<<0)   
#define TS_PEND_RAW         (1<<0)   
#define TS_PEND             (1<<0)   
#define TS_PEND_EN          (1<<0)   
#define EVENT_POP           (1<<0)   
#define PORT_NUMBER_SHIFT    (24)     
#define PORT_NUMBER_MASK     (0x1f)
#define EVENT_TYPE_SHIFT     (20)     
#define EVENT_TYPE_MASK      (0xf)
#define MESSAGE_TYPE_SHIFT   (16)     
#define MESSAGE_TYPE_MASK    (0xf)
#define SEQUENCE_ID_SHIFT    (0)      
#define SEQUENCE_ID_MASK     (0xffff)
enum {
	CPTS_EV_PUSH,  
	CPTS_EV_ROLL,  
	CPTS_EV_HALF,  
	CPTS_EV_HW,    
	CPTS_EV_RX,    
	CPTS_EV_TX,    
};
#define CPTS_FIFO_DEPTH 16
#define CPTS_MAX_EVENTS 32
struct cpts_event {
	struct list_head list;
	unsigned long tmo;
	u32 high;
	u32 low;
	u64 timestamp;
};
struct cpts {
	struct device *dev;
	struct cpsw_cpts __iomem *reg;
	int tx_enable;
	int rx_enable;
	struct ptp_clock_info info;
	struct ptp_clock *clock;
	spinlock_t lock;  
	u32 cc_mult;  
	struct cyclecounter cc;
	struct timecounter tc;
	int phc_index;
	struct clk *refclk;
	struct list_head events;
	struct list_head pool;
	struct cpts_event pool_data[CPTS_MAX_EVENTS];
	unsigned long ov_check_period;
	struct sk_buff_head txq;
	u64 cur_timestamp;
	u32 mult_new;
	struct mutex ptp_clk_mutex;  
	bool irq_poll;
	struct completion	ts_push_complete;
	u32 hw_ts_enable;
};
void cpts_rx_timestamp(struct cpts *cpts, struct sk_buff *skb);
void cpts_tx_timestamp(struct cpts *cpts, struct sk_buff *skb);
int cpts_register(struct cpts *cpts);
void cpts_unregister(struct cpts *cpts);
struct cpts *cpts_create(struct device *dev, void __iomem *regs,
			 struct device_node *node, u32 n_ext_ts);
void cpts_release(struct cpts *cpts);
void cpts_misc_interrupt(struct cpts *cpts);
static inline bool cpts_can_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
	unsigned int class = ptp_classify_raw(skb);
	if (class == PTP_CLASS_NONE)
		return false;
	return true;
}
static inline void cpts_set_irqpoll(struct cpts *cpts, bool en)
{
	cpts->irq_poll = en;
}
#else
struct cpts;
static inline void cpts_rx_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
}
static inline void cpts_tx_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
}
static inline
struct cpts *cpts_create(struct device *dev, void __iomem *regs,
			 struct device_node *node, u32 n_ext_ts)
{
	return NULL;
}
static inline void cpts_release(struct cpts *cpts)
{
}
static inline int
cpts_register(struct cpts *cpts)
{
	return 0;
}
static inline void cpts_unregister(struct cpts *cpts)
{
}
static inline bool cpts_can_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
	return false;
}
static inline void cpts_misc_interrupt(struct cpts *cpts)
{
}
static inline void cpts_set_irqpoll(struct cpts *cpts, bool en)
{
}
#endif
#endif
