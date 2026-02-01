 
#ifndef _TFRC_H_
#define _TFRC_H_
 
#include <linux/types.h>
#include <linux/math64.h>
#include "../../dccp.h"

 
#include "loss_interval.h"
#include "packet_history.h"

#ifdef CONFIG_IP_DCCP_TFRC_DEBUG
extern bool tfrc_debug;
#define tfrc_pr_debug(format, a...)	DCCP_PR_DEBUG(tfrc_debug, format, ##a)
#else
#define tfrc_pr_debug(format, a...)
#endif

 
static inline u64 scaled_div(u64 a, u64 b)
{
	BUG_ON(b == 0);
	return div64_u64(a * 1000000, b);
}

static inline u32 scaled_div32(u64 a, u64 b)
{
	u64 result = scaled_div(a, b);

	if (result > UINT_MAX) {
		DCCP_CRIT("Overflow: %llu/%llu > UINT_MAX",
			  (unsigned long long)a, (unsigned long long)b);
		return UINT_MAX;
	}
	return result;
}

 
static inline u32 tfrc_ewma(const u32 avg, const u32 newval, const u8 weight)
{
	return avg ? (weight * avg + (10 - weight) * newval) / 10 : newval;
}

u32 tfrc_calc_x(u16 s, u32 R, u32 p);
u32 tfrc_calc_x_reverse_lookup(u32 fvalue);
u32 tfrc_invert_loss_event_rate(u32 loss_event_rate);

int tfrc_tx_packet_history_init(void);
void tfrc_tx_packet_history_exit(void);
int tfrc_rx_packet_history_init(void);
void tfrc_rx_packet_history_exit(void);

int tfrc_li_init(void);
void tfrc_li_exit(void);

#ifdef CONFIG_IP_DCCP_TFRC_LIB
int tfrc_lib_init(void);
void tfrc_lib_exit(void);
#else
#define tfrc_lib_init() (0)
#define tfrc_lib_exit()
#endif
#endif  
