 
 

#ifndef ISCSI_SW_TCP_H
#define ISCSI_SW_TCP_H

#include <scsi/libiscsi.h>
#include <scsi/libiscsi_tcp.h>

struct socket;
struct iscsi_tcp_conn;

 
struct iscsi_sw_tcp_send {
	struct iscsi_hdr	*hdr;
	struct iscsi_segment	segment;
	struct iscsi_segment	data_segment;
};

struct iscsi_sw_tcp_conn {
	struct socket		*sock;
	 
	struct mutex		sock_lock;

	struct work_struct	recvwork;
	bool			queue_recv;

	struct iscsi_sw_tcp_send out;
	 
	void			(*old_data_ready)(struct sock *);
	void			(*old_state_change)(struct sock *);
	void			(*old_write_space)(struct sock *);

	 
	struct ahash_request	*tx_hash;	 
	struct ahash_request	*rx_hash;	 

	 
	uint32_t		sendpage_failures_cnt;
	uint32_t		discontiguous_hdr_cnt;
};

struct iscsi_sw_tcp_host {
	struct iscsi_session	*session;
};

struct iscsi_sw_tcp_hdrbuf {
	struct iscsi_hdr	hdrbuf;
	char			hdrextbuf[ISCSI_MAX_AHS_SIZE +
		                                  ISCSI_DIGEST_SIZE];
};

#endif  
