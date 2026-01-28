#ifndef _DCCP_CCID3_H_
#define _DCCP_CCID3_H_
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/tfrc.h>
#include "lib/tfrc.h"
#include "../ccid.h"
#define TFRC_INITIAL_TIMEOUT	   (2 * USEC_PER_SEC)
#define TFRC_T_MBI		   64
#if (HZ >= 500)
# define TFRC_T_DELTA		   USEC_PER_MSEC
#else
# define TFRC_T_DELTA		   (USEC_PER_SEC / (2 * HZ))
#endif
enum ccid3_options {
	TFRC_OPT_LOSS_EVENT_RATE = 192,
	TFRC_OPT_LOSS_INTERVALS	 = 193,
	TFRC_OPT_RECEIVE_RATE	 = 194,
};
enum ccid3_hc_tx_states {
	TFRC_SSTATE_NO_SENT = 1,
	TFRC_SSTATE_NO_FBACK,
	TFRC_SSTATE_FBACK,
};
struct ccid3_hc_tx_sock {
	u64				tx_x;
	u64				tx_x_recv;
	u32				tx_x_calc;
	u32				tx_rtt;
	u32				tx_p;
	u32				tx_t_rto;
	u32				tx_t_ipi;
	u16				tx_s;
	enum ccid3_hc_tx_states		tx_state:8;
	u8				tx_last_win_count;
	ktime_t				tx_t_last_win_count;
	struct timer_list		tx_no_feedback_timer;
	struct sock			*sk;
	ktime_t				tx_t_ld;
	ktime_t				tx_t_nom;
	struct tfrc_tx_hist_entry	*tx_hist;
};
static inline struct ccid3_hc_tx_sock *ccid3_hc_tx_sk(const struct sock *sk)
{
	struct ccid3_hc_tx_sock *hctx = ccid_priv(dccp_sk(sk)->dccps_hc_tx_ccid);
	BUG_ON(hctx == NULL);
	return hctx;
}
enum ccid3_hc_rx_states {
	TFRC_RSTATE_NO_DATA = 1,
	TFRC_RSTATE_DATA,
};
struct ccid3_hc_rx_sock {
	u8				rx_last_counter:4;
	enum ccid3_hc_rx_states		rx_state:8;
	u32				rx_bytes_recv;
	u32				rx_x_recv;
	u32				rx_rtt;
	ktime_t				rx_tstamp_last_feedback;
	struct tfrc_rx_hist		rx_hist;
	struct tfrc_loss_hist		rx_li_hist;
	u16				rx_s;
#define rx_pinv				rx_li_hist.i_mean
};
static inline struct ccid3_hc_rx_sock *ccid3_hc_rx_sk(const struct sock *sk)
{
	struct ccid3_hc_rx_sock *hcrx = ccid_priv(dccp_sk(sk)->dccps_hc_rx_ccid);
	BUG_ON(hcrx == NULL);
	return hcrx;
}
#endif  
