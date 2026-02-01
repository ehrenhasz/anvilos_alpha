 
 

#ifndef VIDTV_MUX_H
#define VIDTV_MUX_H

#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include <media/dvb_frontend.h>

#include "vidtv_psi.h"

 
struct vidtv_mux_timing {
	u64 start_jiffies;
	u64 current_jiffies;
	u64 past_jiffies;

	u64 clk;

	u64 pcr_period_usecs;
	u64 si_period_usecs;
};

 
struct vidtv_mux_si {
	 
	struct vidtv_psi_table_pat *pat;
	struct vidtv_psi_table_pmt **pmt_secs;  
	struct vidtv_psi_table_sdt *sdt;
	struct vidtv_psi_table_nit *nit;
	struct vidtv_psi_table_eit *eit;
};

 
struct vidtv_mux_pid_ctx {
	u16 pid;
	u8 cc;  
	struct hlist_node h;
};

 
struct vidtv_mux {
	struct dvb_frontend *fe;
	struct device *dev;

	struct vidtv_mux_timing timing;

	u32 mux_rate_kbytes_sec;

	DECLARE_HASHTABLE(pid_ctx, 3);

	void (*on_new_packets_available_cb)(void *priv, u8 *buf, u32 npackets);

	u8 *mux_buf;
	u32 mux_buf_sz;
	u32 mux_buf_offset;

	struct vidtv_channel  *channels;

	struct vidtv_mux_si si;
	u64 num_streamed_pcr;
	u64 num_streamed_si;

	struct work_struct mpeg_thread;
	bool streaming;

	u16 pcr_pid;
	u16 transport_stream_id;
	u16 network_id;
	char *network_name;
	void *priv;
};

 
struct vidtv_mux_init_args {
	u32 mux_rate_kbytes_sec;
	void (*on_new_packets_available_cb)(void *priv, u8 *buf, u32 npackets);
	u32 mux_buf_sz;
	u64 pcr_period_usecs;
	u64 si_period_usecs;
	u16 pcr_pid;
	u16 transport_stream_id;
	struct vidtv_channel *channels;
	u16 network_id;
	char *network_name;
	void *priv;
};

struct vidtv_mux *vidtv_mux_init(struct dvb_frontend *fe,
				 struct device *dev,
				 struct vidtv_mux_init_args *args);
void vidtv_mux_destroy(struct vidtv_mux *m);

void vidtv_mux_start_thread(struct vidtv_mux *m);
void vidtv_mux_stop_thread(struct vidtv_mux *m);

#endif 
