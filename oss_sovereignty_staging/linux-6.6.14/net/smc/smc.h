 
 
#ifndef __SMC_H
#define __SMC_H

#include <linux/socket.h>
#include <linux/types.h>
#include <linux/compiler.h>  
#include <net/genetlink.h>
#include <net/sock.h>

#include "smc_ib.h"

#define SMC_V1		1		 
#define SMC_V2		2		 

#define SMC_RELEASE_0 0
#define SMC_RELEASE_1 1
#define SMC_RELEASE	SMC_RELEASE_1  

#define SMCPROTO_SMC		0	 
#define SMCPROTO_SMC6		1	 

#define SMC_MAX_ISM_DEVS	8	 
#define SMC_AUTOCORKING_DEFAULT_SIZE	0x10000	 

extern struct proto smc_proto;
extern struct proto smc_proto6;

#ifdef ATOMIC64_INIT
#define KERNEL_HAS_ATOMIC64
#endif

enum smc_state {		 
	SMC_ACTIVE	= 1,
	SMC_INIT	= 2,
	SMC_CLOSED	= 7,
	SMC_LISTEN	= 10,
	 
	SMC_PEERCLOSEWAIT1	= 20,
	SMC_PEERCLOSEWAIT2	= 21,
	SMC_APPFINCLOSEWAIT	= 24,
	SMC_APPCLOSEWAIT1	= 22,
	SMC_APPCLOSEWAIT2	= 23,
	SMC_PEERFINCLOSEWAIT	= 25,
	 
	SMC_PEERABORTWAIT	= 26,
	SMC_PROCESSABORT	= 27,
};

struct smc_link_group;

struct smc_wr_rx_hdr {	 
	union {
		u8 type;
#if defined(__BIG_ENDIAN_BITFIELD)
		struct {
			u8 llc_version:4,
			   llc_type:4;
		};
#elif defined(__LITTLE_ENDIAN_BITFIELD)
		struct {
			u8 llc_type:4,
			   llc_version:4;
		};
#endif
	};
} __aligned(1);

struct smc_cdc_conn_state_flags {
#if defined(__BIG_ENDIAN_BITFIELD)
	u8	peer_done_writing : 1;	 
	u8	peer_conn_closed : 1;	 
	u8	peer_conn_abort : 1;	 
	u8	reserved : 5;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8	reserved : 5;
	u8	peer_conn_abort : 1;
	u8	peer_conn_closed : 1;
	u8	peer_done_writing : 1;
#endif
};

struct smc_cdc_producer_flags {
#if defined(__BIG_ENDIAN_BITFIELD)
	u8	write_blocked : 1;	 
	u8	urg_data_pending : 1;	 
	u8	urg_data_present : 1;	 
	u8	cons_curs_upd_req : 1;	 
	u8	failover_validation : 1; 
	u8	reserved : 3;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8	reserved : 3;
	u8	failover_validation : 1;
	u8	cons_curs_upd_req : 1;
	u8	urg_data_present : 1;
	u8	urg_data_pending : 1;
	u8	write_blocked : 1;
#endif
};

 
union smc_host_cursor {	 
	struct {
		u16	reserved;
		u16	wrap;		 
		u32	count;		 
	};
#ifdef KERNEL_HAS_ATOMIC64
	atomic64_t		acurs;	 
#else
	u64			acurs;	 
#endif
} __aligned(8);

 
struct smc_host_cdc_msg {		 
	struct smc_wr_rx_hdr		common;  
	u8				len;	 
	u16				seqno;	 
	u32				token;	 
	union smc_host_cursor		prod;		 
	union smc_host_cursor		cons;		 
	struct smc_cdc_producer_flags	prod_flags;	 
	struct smc_cdc_conn_state_flags	conn_state_flags;  
	u8				reserved[18];
} __aligned(8);

enum smc_urg_state {
	SMC_URG_VALID	= 1,			 
	SMC_URG_NOTYET	= 2,			 
	SMC_URG_READ	= 3,			 
};

struct smc_mark_woken {
	bool woken;
	void *key;
	wait_queue_entry_t wait_entry;
};

struct smc_connection {
	struct rb_node		alert_node;
	struct smc_link_group	*lgr;		 
	struct smc_link		*lnk;		 
	u32			alert_token_local;  
	u8			peer_rmbe_idx;	 
	int			peer_rmbe_size;	 
	atomic_t		peer_rmbe_space; 
	int			rtoken_idx;	 

	struct smc_buf_desc	*sndbuf_desc;	 
	struct smc_buf_desc	*rmb_desc;	 
	int                     rmbe_size_comp;  
	int			rmbe_update_limit;
						 

	struct smc_host_cdc_msg	local_tx_ctrl;	 
	union smc_host_cursor	local_tx_ctrl_fin;
						 
	union smc_host_cursor	tx_curs_prep;	 
	union smc_host_cursor	tx_curs_sent;	 
	union smc_host_cursor	tx_curs_fin;	 
	atomic_t		sndbuf_space;	 
	u16			tx_cdc_seq;	 
	u16			tx_cdc_seq_fin;	 
	spinlock_t		send_lock;	 
	atomic_t		cdc_pend_tx_wr;  
	wait_queue_head_t	cdc_pend_tx_wq;  
	atomic_t		tx_pushing;      
	struct delayed_work	tx_work;	 
	u32			tx_off;		 

	struct smc_host_cdc_msg	local_rx_ctrl;	 
	union smc_host_cursor	rx_curs_confirmed;  
	union smc_host_cursor	urg_curs;	 
	enum smc_urg_state	urg_state;
	bool			urg_tx_pend;	 
	bool			urg_rx_skip_pend;
						 
	char			urg_rx_byte;	 
	bool			tx_in_release_sock;
						 
	atomic_t		bytes_to_rcv;	 
	atomic_t		splice_pending;	 
#ifndef KERNEL_HAS_ATOMIC64
	spinlock_t		acurs_lock;	 
#endif
	struct work_struct	close_work;	 
	struct work_struct	abort_work;	 
	struct tasklet_struct	rx_tsklet;	 
	u8			rx_off;		 
	u64			peer_token;	 
	u8			killed : 1;	 
	u8			freed : 1;	 
	u8			out_of_sync : 1;  
};

struct smc_sock {				 
	struct sock		sk;
	struct socket		*clcsock;	 
	void			(*clcsk_state_change)(struct sock *sk);
						 
	void			(*clcsk_data_ready)(struct sock *sk);
						 
	void			(*clcsk_write_space)(struct sock *sk);
						 
	void			(*clcsk_error_report)(struct sock *sk);
						 
	struct smc_connection	conn;		 
	struct smc_sock		*listen_smc;	 
	struct work_struct	connect_work;	 
	struct work_struct	tcp_listen_work; 
	struct work_struct	smc_listen_work; 
	struct list_head	accept_q;	 
	spinlock_t		accept_q_lock;	 
	bool			limit_smc_hs;	 
	bool			use_fallback;	 
	int			fallback_rsn;	 
	u32			peer_diagnosis;  
	atomic_t                queued_smc_hs;   
	struct inet_connection_sock_af_ops		af_ops;
	const struct inet_connection_sock_af_ops	*ori_af_ops;
						 
	int			sockopt_defer_accept;
						 
	u8			wait_close_tx_prepared : 1;
						 
	u8			connect_nonblock : 1;
						 
	struct mutex            clcsock_release_lock;
						 
};

#define smc_sk(ptr) container_of_const(ptr, struct smc_sock, sk)

static inline void smc_init_saved_callbacks(struct smc_sock *smc)
{
	smc->clcsk_state_change	= NULL;
	smc->clcsk_data_ready	= NULL;
	smc->clcsk_write_space	= NULL;
	smc->clcsk_error_report	= NULL;
}

static inline struct smc_sock *smc_clcsock_user_data(const struct sock *clcsk)
{
	return (struct smc_sock *)
	       ((uintptr_t)clcsk->sk_user_data & ~SK_USER_DATA_NOCOPY);
}

 
static inline void smc_clcsock_replace_cb(void (**target_cb)(struct sock *),
					  void (*new_cb)(struct sock *),
					  void (**saved_cb)(struct sock *))
{
	 
	if (!*saved_cb)
		*saved_cb = *target_cb;
	*target_cb = new_cb;
}

 
static inline void smc_clcsock_restore_cb(void (**target_cb)(struct sock *),
					  void (**saved_cb)(struct sock *))
{
	if (!*saved_cb)
		return;
	*target_cb = *saved_cb;
	*saved_cb = NULL;
}

extern struct workqueue_struct	*smc_hs_wq;	 
extern struct workqueue_struct	*smc_close_wq;	 

#define SMC_SYSTEMID_LEN		8

extern u8	local_systemid[SMC_SYSTEMID_LEN];  

#define ntohll(x) be64_to_cpu(x)
#define htonll(x) cpu_to_be64(x)

 
static inline void hton24(u8 *net, u32 host)
{
	__be32 t;

	t = cpu_to_be32(host);
	memcpy(net, ((u8 *)&t) + 1, 3);
}

 
static inline u32 ntoh24(u8 *net)
{
	__be32 t = 0;

	memcpy(((u8 *)&t) + 1, net, 3);
	return be32_to_cpu(t);
}

#ifdef CONFIG_XFRM
static inline bool using_ipsec(struct smc_sock *smc)
{
	return (smc->clcsock->sk->sk_policy[0] ||
		smc->clcsock->sk->sk_policy[1]) ? true : false;
}
#else
static inline bool using_ipsec(struct smc_sock *smc)
{
	return false;
}
#endif

struct smc_gidlist;

struct sock *smc_accept_dequeue(struct sock *parent, struct socket *new_sock);
void smc_close_non_accepted(struct sock *sk);
void smc_fill_gid_list(struct smc_link_group *lgr,
		       struct smc_gidlist *gidlist,
		       struct smc_ib_device *known_dev, u8 *known_gid);

 
int smc_nl_dump_hs_limitation(struct sk_buff *skb, struct netlink_callback *cb);
int smc_nl_enable_hs_limitation(struct sk_buff *skb, struct genl_info *info);
int smc_nl_disable_hs_limitation(struct sk_buff *skb, struct genl_info *info);

static inline void smc_sock_set_flag(struct sock *sk, enum sock_flags flag)
{
	set_bit(flag, &sk->sk_flags);
}

#endif	 
