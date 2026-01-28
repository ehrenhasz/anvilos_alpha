#ifndef _ICE_PTP_H_
#define _ICE_PTP_H_
#include <linux/ptp_clock_kernel.h>
#include <linux/kthread.h>
#include "ice_ptp_hw.h"
enum ice_ptp_pin_e810 {
	GPIO_20 = 0,
	GPIO_21,
	GPIO_22,
	GPIO_23,
	NUM_PTP_PIN_E810
};
enum ice_ptp_pin_e810t {
	GNSS = 0,
	SMA1,
	UFL1,
	SMA2,
	UFL2,
	NUM_PTP_PINS_E810T
};
struct ice_perout_channel {
	bool ena;
	u32 gpio_pin;
	u64 period;
	u64 start_time;
};
struct ice_tx_tstamp {
	struct sk_buff *skb;
	unsigned long start;
	u64 cached_tstamp;
};
enum ice_tx_tstamp_work {
	ICE_TX_TSTAMP_WORK_DONE = 0,
	ICE_TX_TSTAMP_WORK_PENDING,
};
struct ice_ptp_tx {
	spinlock_t lock;  
	struct ice_tx_tstamp *tstamps;
	unsigned long *in_use;
	unsigned long *stale;
	u8 block;
	u8 offset;
	u8 len;
	u8 init : 1;
	u8 calibrating : 1;
	u8 verify_cached : 1;
};
#define INDEX_PER_QUAD			64
#define INDEX_PER_PORT_E822		16
#define INDEX_PER_PORT_E810		64
struct ice_ptp_port {
	struct ice_ptp_tx tx;
	struct kthread_delayed_work ov_work;
	struct mutex ps_lock;  
	bool link_up;
	u8 tx_fifo_busy_cnt;
	u8 port_num;
};
#define GLTSYN_TGT_H_IDX_MAX		4
struct ice_ptp {
	struct ice_ptp_port port;
	struct kthread_delayed_work work;
	u64 cached_phc_time;
	unsigned long cached_phc_jiffies;
	u8 ext_ts_chan;
	u8 ext_ts_irq;
	struct kthread_worker *kworker;
	struct ice_perout_channel perout_channels[GLTSYN_TGT_H_IDX_MAX];
	struct ptp_clock_info info;
	struct ptp_clock *clock;
	struct hwtstamp_config tstamp_config;
	u64 reset_time;
	u32 tx_hwtstamp_skipped;
	u32 tx_hwtstamp_timeouts;
	u32 tx_hwtstamp_flushed;
	u32 tx_hwtstamp_discarded;
	u32 late_cached_phc_updates;
};
#define __ptp_port_to_ptp(p) \
	container_of((p), struct ice_ptp, port)
#define ptp_port_to_pf(p) \
	container_of(__ptp_port_to_ptp((p)), struct ice_pf, ptp)
#define __ptp_info_to_ptp(i) \
	container_of((i), struct ice_ptp, info)
#define ptp_info_to_pf(i) \
	container_of(__ptp_info_to_ptp((i)), struct ice_pf, ptp)
#define PFTSYN_SEM_BYTES		4
#define PTP_SHARED_CLK_IDX_VALID	BIT(31)
#define TS_CMD_MASK			0xF
#define SYNC_EXEC_CMD			0x3
#define ICE_PTP_TS_VALID		BIT(0)
#define FIFO_EMPTY			BIT(2)
#define FIFO_OK				0xFF
#define ICE_PTP_FIFO_NUM_CHECKS		5
#define GLTSYN_AUX_OUT(_chan, _idx)	(GLTSYN_AUX_OUT_0(_idx) + ((_chan) * 8))
#define GLTSYN_AUX_IN(_chan, _idx)	(GLTSYN_AUX_IN_0(_idx) + ((_chan) * 8))
#define GLTSYN_CLKO(_chan, _idx)	(GLTSYN_CLKO_0(_idx) + ((_chan) * 8))
#define GLTSYN_TGT_L(_chan, _idx)	(GLTSYN_TGT_L_0(_idx) + ((_chan) * 16))
#define GLTSYN_TGT_H(_chan, _idx)	(GLTSYN_TGT_H_0(_idx) + ((_chan) * 16))
#define GLTSYN_EVNT_L(_chan, _idx)	(GLTSYN_EVNT_L_0(_idx) + ((_chan) * 16))
#define GLTSYN_EVNT_H(_chan, _idx)	(GLTSYN_EVNT_H_0(_idx) + ((_chan) * 16))
#define GLTSYN_EVNT_H_IDX_MAX		3
#define PPS_CLK_GEN_CHAN		3
#define PPS_CLK_SRC_CHAN		2
#define PPS_PIN_INDEX			5
#define TIME_SYNC_PIN_INDEX		4
#define N_EXT_TS_E810			3
#define N_PER_OUT_E810			4
#define N_PER_OUT_E810T			3
#define N_PER_OUT_NO_SMA_E810T		2
#define N_EXT_TS_NO_SMA_E810T		2
#define ETH_GLTSYN_ENA(_i)		(0x03000348 + ((_i) * 4))
#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
struct ice_pf;
int ice_ptp_set_ts_config(struct ice_pf *pf, struct ifreq *ifr);
int ice_ptp_get_ts_config(struct ice_pf *pf, struct ifreq *ifr);
void ice_ptp_cfg_timestamp(struct ice_pf *pf, bool ena);
int ice_get_ptp_clock_index(struct ice_pf *pf);
void ice_ptp_extts_event(struct ice_pf *pf);
s8 ice_ptp_request_ts(struct ice_ptp_tx *tx, struct sk_buff *skb);
enum ice_tx_tstamp_work ice_ptp_process_ts(struct ice_pf *pf);
void
ice_ptp_rx_hwtstamp(struct ice_rx_ring *rx_ring,
		    union ice_32b_rx_flex_desc *rx_desc, struct sk_buff *skb);
void ice_ptp_reset(struct ice_pf *pf);
void ice_ptp_prepare_for_reset(struct ice_pf *pf);
void ice_ptp_init(struct ice_pf *pf);
void ice_ptp_release(struct ice_pf *pf);
void ice_ptp_link_change(struct ice_pf *pf, u8 port, bool linkup);
#else  
static inline int ice_ptp_set_ts_config(struct ice_pf *pf, struct ifreq *ifr)
{
	return -EOPNOTSUPP;
}
static inline int ice_ptp_get_ts_config(struct ice_pf *pf, struct ifreq *ifr)
{
	return -EOPNOTSUPP;
}
static inline void ice_ptp_cfg_timestamp(struct ice_pf *pf, bool ena) { }
static inline int ice_get_ptp_clock_index(struct ice_pf *pf)
{
	return -1;
}
static inline void ice_ptp_extts_event(struct ice_pf *pf) { }
static inline s8
ice_ptp_request_ts(struct ice_ptp_tx *tx, struct sk_buff *skb)
{
	return -1;
}
static inline bool ice_ptp_process_ts(struct ice_pf *pf)
{
	return true;
}
static inline void
ice_ptp_rx_hwtstamp(struct ice_rx_ring *rx_ring,
		    union ice_32b_rx_flex_desc *rx_desc, struct sk_buff *skb) { }
static inline void ice_ptp_reset(struct ice_pf *pf) { }
static inline void ice_ptp_prepare_for_reset(struct ice_pf *pf) { }
static inline void ice_ptp_init(struct ice_pf *pf) { }
static inline void ice_ptp_release(struct ice_pf *pf) { }
static inline void ice_ptp_link_change(struct ice_pf *pf, u8 port, bool linkup)
{
}
#endif  
#endif  
