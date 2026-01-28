#ifndef _HNS_DSAF_PPE_H
#define _HNS_DSAF_PPE_H
#include <linux/platform_device.h>
#include "hns_dsaf_main.h"
#include "hns_dsaf_mac.h"
#include "hns_dsaf_rcb.h"
#define HNS_PPE_SERVICE_NW_ENGINE_NUM DSAF_COMM_CHN
#define HNS_PPE_DEBUG_NW_ENGINE_NUM 1
#define HNS_PPE_COM_NUM DSAF_COMM_DEV_NUM
#define PPE_COMMON_REG_OFFSET 0x70000
#define PPE_REG_OFFSET 0x10000
#define ETH_PPE_DUMP_NUM 576
#define ETH_PPE_STATIC_NUM 12
#define HNS_PPEV2_RSS_IND_TBL_SIZE 256
#define HNS_PPEV2_RSS_KEY_SIZE 40  
#define HNS_PPEV2_RSS_KEY_NUM (HNS_PPEV2_RSS_KEY_SIZE / sizeof(u32))
#define HNS_PPEV2_MAX_FRAME_LEN 0X980
enum ppe_qid_mode {
	PPE_QID_MODE0 = 0,  
	PPE_QID_MODE1,	    
	PPE_QID_MODE2,	    
	PPE_QID_MODE3,	    
	PPE_QID_MODE4,	    
	PPE_QID_MODE5,	    
	PPE_QID_MODE6,	    
	PPE_QID_MODE7,	    
	PPE_QID_MODE8,	    
	PPE_QID_MODE9,	    
	PPE_QID_MODE10,	    
	PPE_QID_MODE11,	    
};
enum ppe_port_mode {
	PPE_MODE_GE = 0,
	PPE_MODE_XGE,
};
enum ppe_common_mode {
	PPE_COMMON_MODE_DEBUG = 0,
	PPE_COMMON_MODE_SERVICE,
	PPE_COMMON_MODE_MAX
};
struct hns_ppe_hw_stats {
	u64 rx_pkts_from_sw;
	u64 rx_pkts;
	u64 rx_drop_no_bd;
	u64 rx_alloc_buf_fail;
	u64 rx_alloc_buf_wait;
	u64 rx_drop_no_buf;
	u64 rx_err_fifo_full;
	u64 tx_bd_form_rcb;
	u64 tx_pkts_from_rcb;
	u64 tx_pkts;
	u64 tx_err_fifo_empty;
	u64 tx_err_checksum;
};
struct hns_ppe_cb {
	struct device *dev;
	struct hns_ppe_cb *next;	 
	struct ppe_common_cb *ppe_common_cb;  
	struct hns_ppe_hw_stats hw_stats;
	u8 index;	 
	u8 __iomem *io_base;
	int virq;
	u32 rss_indir_table[HNS_PPEV2_RSS_IND_TBL_SIZE];  
	u32 rss_key[HNS_PPEV2_RSS_KEY_NUM];  
};
struct ppe_common_cb {
	struct device *dev;
	struct dsaf_device *dsaf_dev;
	u8 __iomem *io_base;
	enum ppe_common_mode ppe_mode;
	u8 comm_index;    
	u32 ppe_num;
	struct hns_ppe_cb ppe_cb[];
};
int hns_ppe_wait_tx_fifo_clean(struct hns_ppe_cb *ppe_cb);
int hns_ppe_init(struct dsaf_device *dsaf_dev);
void hns_ppe_uninit(struct dsaf_device *dsaf_dev);
void hns_ppe_reset_common(struct dsaf_device *dsaf_dev, u8 ppe_common_index);
void hns_ppe_update_stats(struct hns_ppe_cb *ppe_cb);
int hns_ppe_get_sset_count(int stringset);
int hns_ppe_get_regs_count(void);
void hns_ppe_get_regs(struct hns_ppe_cb *ppe_cb, void *data);
void hns_ppe_get_strings(struct hns_ppe_cb *ppe_cb, int stringset, u8 *data);
void hns_ppe_get_stats(struct hns_ppe_cb *ppe_cb, u64 *data);
void hns_ppe_set_tso_enable(struct hns_ppe_cb *ppe_cb, u32 value);
void hns_ppe_set_rss_key(struct hns_ppe_cb *ppe_cb,
			 const u32 rss_key[HNS_PPEV2_RSS_KEY_NUM]);
void hns_ppe_set_indir_table(struct hns_ppe_cb *ppe_cb,
			     const u32 rss_tab[HNS_PPEV2_RSS_IND_TBL_SIZE]);
#endif  
