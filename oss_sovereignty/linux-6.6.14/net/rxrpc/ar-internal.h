 
 

#include <linux/atomic.h>
#include <linux/seqlock.h>
#include <linux/win_minmax.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/sock.h>
#include <net/af_rxrpc.h>
#include <keys/rxrpc-type.h>
#include "protocol.h"

#define FCRYPT_BSIZE 8
struct rxrpc_crypt {
	union {
		u8	x[FCRYPT_BSIZE];
		__be32	n[2];
	};
} __attribute__((aligned(8)));

#define rxrpc_queue_work(WS)	queue_work(rxrpc_workqueue, (WS))
#define rxrpc_queue_delayed_work(WS,D)	\
	queue_delayed_work(rxrpc_workqueue, (WS), (D))

struct key_preparsed_payload;
struct rxrpc_connection;
struct rxrpc_txbuf;

 
enum rxrpc_skb_mark {
	RXRPC_SKB_MARK_PACKET,		 
	RXRPC_SKB_MARK_ERROR,		 
	RXRPC_SKB_MARK_SERVICE_CONN_SECURED,  
	RXRPC_SKB_MARK_REJECT_BUSY,	 
	RXRPC_SKB_MARK_REJECT_ABORT,	 
};

 
enum {
	RXRPC_UNBOUND = 0,
	RXRPC_CLIENT_UNBOUND,		 
	RXRPC_CLIENT_BOUND,		 
	RXRPC_SERVER_BOUND,		 
	RXRPC_SERVER_BOUND2,		 
	RXRPC_SERVER_LISTENING,		 
	RXRPC_SERVER_LISTEN_DISABLED,	 
	RXRPC_CLOSE,			 
};

 
struct rxrpc_net {
	struct proc_dir_entry	*proc_net;	 
	u32			epoch;		 
	struct list_head	calls;		 
	spinlock_t		call_lock;	 
	atomic_t		nr_calls;	 

	atomic_t		nr_conns;
	struct list_head	conn_proc_list;	 
	struct list_head	service_conns;	 
	rwlock_t		conn_lock;	 
	struct work_struct	service_conn_reaper;
	struct timer_list	service_conn_reap_timer;

	bool			live;

	atomic_t		nr_client_conns;

	struct hlist_head	local_endpoints;
	struct mutex		local_mutex;	 

	DECLARE_HASHTABLE	(peer_hash, 10);
	spinlock_t		peer_hash_lock;	 

#define RXRPC_KEEPALIVE_TIME 20  
	u8			peer_keepalive_cursor;
	time64_t		peer_keepalive_base;
	struct list_head	peer_keepalive[32];
	struct list_head	peer_keepalive_new;
	struct timer_list	peer_keepalive_timer;
	struct work_struct	peer_keepalive_work;

	atomic_t		stat_tx_data;
	atomic_t		stat_tx_data_retrans;
	atomic_t		stat_tx_data_send;
	atomic_t		stat_tx_data_send_frag;
	atomic_t		stat_tx_data_send_fail;
	atomic_t		stat_tx_data_underflow;
	atomic_t		stat_tx_data_cwnd_reset;
	atomic_t		stat_rx_data;
	atomic_t		stat_rx_data_reqack;
	atomic_t		stat_rx_data_jumbo;

	atomic_t		stat_tx_ack_fill;
	atomic_t		stat_tx_ack_send;
	atomic_t		stat_tx_ack_skip;
	atomic_t		stat_tx_acks[256];
	atomic_t		stat_rx_acks[256];

	atomic_t		stat_why_req_ack[8];

	atomic_t		stat_io_loop;
};

 
struct rxrpc_backlog {
	unsigned short		peer_backlog_head;
	unsigned short		peer_backlog_tail;
	unsigned short		conn_backlog_head;
	unsigned short		conn_backlog_tail;
	unsigned short		call_backlog_head;
	unsigned short		call_backlog_tail;
#define RXRPC_BACKLOG_MAX	32
	struct rxrpc_peer	*peer_backlog[RXRPC_BACKLOG_MAX];
	struct rxrpc_connection	*conn_backlog[RXRPC_BACKLOG_MAX];
	struct rxrpc_call	*call_backlog[RXRPC_BACKLOG_MAX];
};

 
struct rxrpc_sock {
	 
	struct sock		sk;
	rxrpc_notify_new_call_t	notify_new_call;  
	rxrpc_discard_new_call_t discard_new_call;  
	struct rxrpc_local	*local;		 
	struct rxrpc_backlog	*backlog;	 
	spinlock_t		incoming_lock;	 
	struct list_head	sock_calls;	 
	struct list_head	to_be_accepted;	 
	struct list_head	recvmsg_q;	 
	spinlock_t		recvmsg_lock;	 
	struct key		*key;		 
	struct key		*securities;	 
	struct rb_root		calls;		 
	unsigned long		flags;
#define RXRPC_SOCK_CONNECTED		0	 
	rwlock_t		call_lock;	 
	u32			min_sec_level;	 
#define RXRPC_SECURITY_MAX	RXRPC_SECURITY_ENCRYPT
	bool			exclusive;	 
	u16			second_service;	 
	struct {
		 
		u16		from;		 
		u16		to;		 
	} service_upgrade;
	sa_family_t		family;		 
	struct sockaddr_rxrpc	srx;		 
	struct sockaddr_rxrpc	connect_srx;	 
};

#define rxrpc_sk(__sk) container_of((__sk), struct rxrpc_sock, sk)

 
struct rxrpc_host_header {
	u32		epoch;		 
	u32		cid;		 
	u32		callNumber;	 
	u32		seq;		 
	u32		serial;		 
	u8		type;		 
	u8		flags;		 
	u8		userStatus;	 
	u8		securityIndex;	 
	union {
		u16	_rsvd;		 
		u16	cksum;		 
	};
	u16		serviceId;	 
} __packed;

 
struct rxrpc_skb_priv {
	struct rxrpc_connection *conn;	 
	u16		offset;		 
	u16		len;		 
	u8		flags;
#define RXRPC_RX_VERIFIED	0x01

	struct rxrpc_host_header hdr;	 
};

#define rxrpc_skb(__skb) ((struct rxrpc_skb_priv *) &(__skb)->cb)

 
struct rxrpc_security {
	const char		*name;		 
	u8			security_index;	 
	u32			no_key_abort;	 

	 
	int (*init)(void);

	 
	void (*exit)(void);

	 
	int (*preparse_server_key)(struct key_preparsed_payload *);

	 
	void (*free_preparse_server_key)(struct key_preparsed_payload *);

	 
	void (*destroy_server_key)(struct key *);

	 
	void (*describe_server_key)(const struct key *, struct seq_file *);

	 
	int (*init_connection_security)(struct rxrpc_connection *,
					struct rxrpc_key_token *);

	 
	int (*how_much_data)(struct rxrpc_call *, size_t,
			     size_t *, size_t *, size_t *);

	 
	int (*secure_packet)(struct rxrpc_call *, struct rxrpc_txbuf *);

	 
	int (*verify_packet)(struct rxrpc_call *, struct sk_buff *);

	 
	void (*free_call_crypto)(struct rxrpc_call *);

	 
	int (*issue_challenge)(struct rxrpc_connection *);

	 
	int (*respond_to_challenge)(struct rxrpc_connection *,
				    struct sk_buff *);

	 
	int (*verify_response)(struct rxrpc_connection *,
			       struct sk_buff *);

	 
	void (*clear)(struct rxrpc_connection *);
};

 
struct rxrpc_local {
	struct rcu_head		rcu;
	atomic_t		active_users;	 
	refcount_t		ref;		 
	struct net		*net;		 
	struct rxrpc_net	*rxnet;		 
	struct hlist_node	link;
	struct socket		*socket;	 
	struct task_struct	*io_thread;
	struct completion	io_thread_ready;  
	struct rxrpc_sock	*service;	 
#ifdef CONFIG_AF_RXRPC_INJECT_RX_DELAY
	struct sk_buff_head	rx_delay_queue;	 
#endif
	struct sk_buff_head	rx_queue;	 
	struct list_head	conn_attend_q;	 
	struct list_head	call_attend_q;	 

	struct rb_root		client_bundles;	 
	spinlock_t		client_bundles_lock;  
	bool			kill_all_client_conns;
	struct list_head	idle_client_conns;
	struct timer_list	client_conn_reap_timer;
	unsigned long		client_conn_flags;
#define RXRPC_CLIENT_CONN_REAP_TIMER	0	 

	spinlock_t		lock;		 
	rwlock_t		services_lock;	 
	int			debug_id;	 
	bool			dead;
	bool			service_closed;	 
	struct idr		conn_ids;	 
	struct list_head	new_client_calls;  
	spinlock_t		client_call_lock;  
	struct sockaddr_rxrpc	srx;		 
};

 
struct rxrpc_peer {
	struct rcu_head		rcu;		 
	refcount_t		ref;
	unsigned long		hash_key;
	struct hlist_node	hash_link;
	struct rxrpc_local	*local;
	struct hlist_head	error_targets;	 
	struct rb_root		service_conns;	 
	struct list_head	keepalive_link;	 
	time64_t		last_tx_at;	 
	seqlock_t		service_conn_lock;
	spinlock_t		lock;		 
	unsigned int		if_mtu;		 
	unsigned int		mtu;		 
	unsigned int		maxdata;	 
	unsigned short		hdrsize;	 
	int			debug_id;	 
	struct sockaddr_rxrpc	srx;		 

	 
#define RXRPC_RTT_CACHE_SIZE 32
	spinlock_t		rtt_input_lock;	 
	ktime_t			rtt_last_req;	 
	unsigned int		rtt_count;	 

	u32			srtt_us;	 
	u32			mdev_us;	 
	u32			mdev_max_us;	 
	u32			rttvar_us;	 
	u32			rto_j;		 
	u8			backoff;	 

	u8			cong_ssthresh;	 
};

 
struct rxrpc_conn_proto {
	union {
		struct {
			u32	epoch;		 
			u32	cid;		 
		};
		u64		index_key;
	};
};

struct rxrpc_conn_parameters {
	struct rxrpc_local	*local;		 
	struct key		*key;		 
	bool			exclusive;	 
	bool			upgrade;	 
	u16			service_id;	 
	u32			security_level;	 
};

 
enum rxrpc_call_completion {
	RXRPC_CALL_SUCCEEDED,		 
	RXRPC_CALL_REMOTELY_ABORTED,	 
	RXRPC_CALL_LOCALLY_ABORTED,	 
	RXRPC_CALL_LOCAL_ERROR,		 
	RXRPC_CALL_NETWORK_ERROR,	 
	NR__RXRPC_CALL_COMPLETIONS
};

 
enum rxrpc_conn_flag {
	RXRPC_CONN_IN_SERVICE_CONNS,	 
	RXRPC_CONN_DONT_REUSE,		 
	RXRPC_CONN_PROBING_FOR_UPGRADE,	 
	RXRPC_CONN_FINAL_ACK_0,		 
	RXRPC_CONN_FINAL_ACK_1,		 
	RXRPC_CONN_FINAL_ACK_2,		 
	RXRPC_CONN_FINAL_ACK_3,		 
};

#define RXRPC_CONN_FINAL_ACK_MASK ((1UL << RXRPC_CONN_FINAL_ACK_0) |	\
				   (1UL << RXRPC_CONN_FINAL_ACK_1) |	\
				   (1UL << RXRPC_CONN_FINAL_ACK_2) |	\
				   (1UL << RXRPC_CONN_FINAL_ACK_3))

 
enum rxrpc_conn_event {
	RXRPC_CONN_EV_CHALLENGE,	 
	RXRPC_CONN_EV_ABORT_CALLS,	 
};

 
enum rxrpc_conn_proto_state {
	RXRPC_CONN_UNUSED,		 
	RXRPC_CONN_CLIENT_UNSECURED,	 
	RXRPC_CONN_CLIENT,		 
	RXRPC_CONN_SERVICE_PREALLOC,	 
	RXRPC_CONN_SERVICE_UNSECURED,	 
	RXRPC_CONN_SERVICE_CHALLENGING,	 
	RXRPC_CONN_SERVICE,		 
	RXRPC_CONN_ABORTED,		 
	RXRPC_CONN__NR_STATES
};

 
struct rxrpc_bundle {
	struct rxrpc_local	*local;		 
	struct rxrpc_peer	*peer;		 
	struct key		*key;		 
	const struct rxrpc_security *security;	 
	refcount_t		ref;
	atomic_t		active;		 
	unsigned int		debug_id;
	u32			security_level;	 
	u16			service_id;	 
	bool			try_upgrade;	 
	bool			exclusive;	 
	bool			upgrade;	 
	unsigned short		alloc_error;	 
	struct rb_node		local_node;	 
	struct list_head	waiting_calls;	 
	unsigned long		avail_chans;	 
	struct rxrpc_connection	*conns[4];	 
};

 
struct rxrpc_connection {
	struct rxrpc_conn_proto	proto;
	struct rxrpc_local	*local;		 
	struct rxrpc_peer	*peer;		 
	struct rxrpc_net	*rxnet;		 
	struct key		*key;		 
	struct list_head	attend_link;	 

	refcount_t		ref;
	atomic_t		active;		 
	struct rcu_head		rcu;
	struct list_head	cache_link;

	unsigned char		act_chans;	 
	struct rxrpc_channel {
		unsigned long		final_ack_at;	 
		struct rxrpc_call	*call;		 
		unsigned int		call_debug_id;	 
		u32			call_id;	 
		u32			call_counter;	 
		u32			last_call;	 
		u8			last_type;	 
		union {
			u32		last_seq;
			u32		last_abort;
		};
	} channels[RXRPC_MAXCALLS];

	struct timer_list	timer;		 
	struct work_struct	processor;	 
	struct work_struct	destructor;	 
	struct rxrpc_bundle	*bundle;	 
	struct rb_node		service_node;	 
	struct list_head	proc_link;	 
	struct list_head	link;		 
	struct sk_buff_head	rx_queue;	 

	struct mutex		security_lock;	 
	const struct rxrpc_security *security;	 
	union {
		struct {
			struct crypto_sync_skcipher *cipher;	 
			struct rxrpc_crypt csum_iv;	 
			u32	nonce;		 
		} rxkad;
	};
	unsigned long		flags;
	unsigned long		events;
	unsigned long		idle_timestamp;	 
	spinlock_t		state_lock;	 
	enum rxrpc_conn_proto_state state;	 
	enum rxrpc_call_completion completion;	 
	s32			abort_code;	 
	int			debug_id;	 
	atomic_t		serial;		 
	unsigned int		hi_serial;	 
	u32			service_id;	 
	u32			security_level;	 
	u8			security_ix;	 
	u8			out_clientflag;	 
	u8			bundle_shift;	 
	bool			exclusive;	 
	bool			upgrade;	 
	u16			orig_service_id;  
	short			error;		 
};

static inline bool rxrpc_to_server(const struct rxrpc_skb_priv *sp)
{
	return sp->hdr.flags & RXRPC_CLIENT_INITIATED;
}

static inline bool rxrpc_to_client(const struct rxrpc_skb_priv *sp)
{
	return !rxrpc_to_server(sp);
}

 
enum rxrpc_call_flag {
	RXRPC_CALL_RELEASED,		 
	RXRPC_CALL_HAS_USERID,		 
	RXRPC_CALL_IS_SERVICE,		 
	RXRPC_CALL_EXPOSED,		 
	RXRPC_CALL_RX_LAST,		 
	RXRPC_CALL_TX_LAST,		 
	RXRPC_CALL_TX_ALL_ACKED,	 
	RXRPC_CALL_SEND_PING,		 
	RXRPC_CALL_RETRANS_TIMEOUT,	 
	RXRPC_CALL_BEGAN_RX_TIMER,	 
	RXRPC_CALL_RX_HEARD,		 
	RXRPC_CALL_DISCONNECTED,	 
	RXRPC_CALL_KERNEL,		 
	RXRPC_CALL_UPGRADE,		 
	RXRPC_CALL_EXCLUSIVE,		 
	RXRPC_CALL_RX_IS_IDLE,		 
	RXRPC_CALL_RECVMSG_READ_ALL,	 
};

 
enum rxrpc_call_event {
	RXRPC_CALL_EV_ACK_LOST,		 
	RXRPC_CALL_EV_INITIAL_PING,	 
};

 
enum rxrpc_call_state {
	RXRPC_CALL_UNINITIALISED,
	RXRPC_CALL_CLIENT_AWAIT_CONN,	 
	RXRPC_CALL_CLIENT_SEND_REQUEST,	 
	RXRPC_CALL_CLIENT_AWAIT_REPLY,	 
	RXRPC_CALL_CLIENT_RECV_REPLY,	 
	RXRPC_CALL_SERVER_PREALLOC,	 
	RXRPC_CALL_SERVER_SECURING,	 
	RXRPC_CALL_SERVER_RECV_REQUEST,	 
	RXRPC_CALL_SERVER_ACK_REQUEST,	 
	RXRPC_CALL_SERVER_SEND_REPLY,	 
	RXRPC_CALL_SERVER_AWAIT_ACK,	 
	RXRPC_CALL_COMPLETE,		 
	NR__RXRPC_CALL_STATES
};

 
enum rxrpc_congest_mode {
	RXRPC_CALL_SLOW_START,
	RXRPC_CALL_CONGEST_AVOIDANCE,
	RXRPC_CALL_PACKET_LOSS,
	RXRPC_CALL_FAST_RETRANSMIT,
	NR__RXRPC_CONGEST_MODES
};

 
struct rxrpc_call {
	struct rcu_head		rcu;
	struct rxrpc_connection	*conn;		 
	struct rxrpc_bundle	*bundle;	 
	struct rxrpc_peer	*peer;		 
	struct rxrpc_local	*local;		 
	struct rxrpc_sock __rcu	*socket;	 
	struct rxrpc_net	*rxnet;		 
	struct key		*key;		 
	const struct rxrpc_security *security;	 
	struct mutex		user_mutex;	 
	struct sockaddr_rxrpc	dest_srx;	 
	unsigned long		delay_ack_at;	 
	unsigned long		ack_lost_at;	 
	unsigned long		resend_at;	 
	unsigned long		ping_at;	 
	unsigned long		keepalive_at;	 
	unsigned long		expect_rx_by;	 
	unsigned long		expect_req_by;	 
	unsigned long		expect_term_by;	 
	u32			next_rx_timo;	 
	u32			next_req_timo;	 
	u32			hard_timo;	 
	struct timer_list	timer;		 
	struct work_struct	destroyer;	 
	rxrpc_notify_rx_t	notify_rx;	 
	struct list_head	link;		 
	struct list_head	wait_link;	 
	struct hlist_node	error_link;	 
	struct list_head	accept_link;	 
	struct list_head	recvmsg_link;	 
	struct list_head	sock_link;	 
	struct rb_node		sock_node;	 
	struct list_head	attend_link;	 
	struct rxrpc_txbuf	*tx_pending;	 
	wait_queue_head_t	waitq;		 
	s64			tx_total_len;	 
	unsigned long		user_call_ID;	 
	unsigned long		flags;
	unsigned long		events;
	spinlock_t		notify_lock;	 
	unsigned int		send_abort_why;  
	s32			send_abort;	 
	short			send_abort_err;	 
	rxrpc_seq_t		send_abort_seq;	 
	s32			abort_code;	 
	int			error;		 
	enum rxrpc_call_state	_state;		 
	enum rxrpc_call_completion completion;	 
	refcount_t		ref;
	u8			security_ix;	 
	enum rxrpc_interruptibility interruptibility;  
	u32			call_id;	 
	u32			cid;		 
	u32			security_level;	 
	int			debug_id;	 
	unsigned short		rx_pkt_offset;	 
	unsigned short		rx_pkt_len;	 

	 
	spinlock_t		tx_lock;	 
	struct list_head	tx_sendmsg;	 
	struct list_head	tx_buffer;	 
	rxrpc_seq_t		tx_bottom;	 
	rxrpc_seq_t		tx_transmitted;	 
	rxrpc_seq_t		tx_prepared;	 
	rxrpc_seq_t		tx_top;		 
	u16			tx_backoff;	 
	u8			tx_winsize;	 
#define RXRPC_TX_MAX_WINDOW	128
	ktime_t			tx_last_sent;	 

	 
	struct sk_buff_head	recvmsg_queue;	 
	struct sk_buff_head	rx_oos_queue;	 

	rxrpc_seq_t		rx_highest_seq;	 
	rxrpc_seq_t		rx_consumed;	 
	rxrpc_serial_t		rx_serial;	 
	u8			rx_winsize;	 

	 
#define RXRPC_TX_SMSS		RXRPC_JUMBO_DATALEN
#define RXRPC_MIN_CWND		(RXRPC_TX_SMSS > 2190 ? 2 : RXRPC_TX_SMSS > 1095 ? 3 : 4)
	u8			cong_cwnd;	 
	u8			cong_extra;	 
	u8			cong_ssthresh;	 
	enum rxrpc_congest_mode	cong_mode:8;	 
	u8			cong_dup_acks;	 
	u8			cong_cumul_acks;  
	ktime_t			cong_tstamp;	 

	 
	u8			ackr_reason;	 
	u16			ackr_sack_base;	 
	rxrpc_serial_t		ackr_serial;	 
	rxrpc_seq_t		ackr_window;	 
	rxrpc_seq_t		ackr_wtop;	 
	unsigned int		ackr_nr_unacked;  
	atomic_t		ackr_nr_consumed;  
	struct {
#define RXRPC_SACK_SIZE 256
		  
		u8		ackr_sack_table[RXRPC_SACK_SIZE];
	} __aligned(8);

	 
	rxrpc_serial_t		rtt_serial[4];	 
	ktime_t			rtt_sent_at[4];	 
	unsigned long		rtt_avail;	 
#define RXRPC_CALL_RTT_AVAIL_MASK	0xf
#define RXRPC_CALL_RTT_PEND_SHIFT	8

	 
	ktime_t			acks_latest_ts;	 
	rxrpc_seq_t		acks_first_seq;	 
	rxrpc_seq_t		acks_prev_seq;	 
	rxrpc_seq_t		acks_hard_ack;	 
	rxrpc_seq_t		acks_lowest_nak;  
	rxrpc_serial_t		acks_highest_serial;  
};

 
struct rxrpc_ack_summary {
	u16			nr_acks;		 
	u16			nr_new_acks;		 
	u16			nr_rot_new_acks;	 
	u8			ack_reason;
	bool			saw_nacks;		 
	bool			new_low_nack;		 
	bool			retrans_timeo;		 
	u8			flight_size;		 
	 
	enum rxrpc_congest_mode	mode:8;
	u8			cwnd;
	u8			ssthresh;
	u8			dup_acks;
	u8			cumulative_acks;
};

 
enum rxrpc_command {
	RXRPC_CMD_SEND_DATA,		 
	RXRPC_CMD_SEND_ABORT,		 
	RXRPC_CMD_REJECT_BUSY,		 
	RXRPC_CMD_CHARGE_ACCEPT,	 
};

struct rxrpc_call_params {
	s64			tx_total_len;	 
	unsigned long		user_call_ID;	 
	struct {
		u32		hard;		 
		u32		idle;		 
		u32		normal;		 
	} timeouts;
	u8			nr_timeouts;	 
	bool			kernel;		 
	enum rxrpc_interruptibility interruptibility;  
};

struct rxrpc_send_params {
	struct rxrpc_call_params call;
	u32			abort_code;	 
	enum rxrpc_command	command : 8;	 
	bool			exclusive;	 
	bool			upgrade;	 
};

 
struct rxrpc_txbuf {
	struct rcu_head		rcu;
	struct list_head	call_link;	 
	struct list_head	tx_link;	 
	ktime_t			last_sent;	 
	refcount_t		ref;
	rxrpc_seq_t		seq;		 
	unsigned int		call_debug_id;
	unsigned int		debug_id;
	unsigned int		len;		 
	unsigned int		space;		 
	unsigned int		offset;		 
	unsigned long		flags;
#define RXRPC_TXBUF_LAST	0		 
#define RXRPC_TXBUF_RESENT	1		 
	u8   ack_why;	 
	struct {
		 
		u8		pad[64 - sizeof(struct rxrpc_wire_header)];
		struct rxrpc_wire_header wire;	 
		union {
			u8	data[RXRPC_JUMBO_DATALEN];  
			struct {
				struct rxrpc_ackpacket ack;
				DECLARE_FLEX_ARRAY(u8, acks);
			};
		};
	} __aligned(64);
};

static inline bool rxrpc_sending_to_server(const struct rxrpc_txbuf *txb)
{
	return txb->wire.flags & RXRPC_CLIENT_INITIATED;
}

static inline bool rxrpc_sending_to_client(const struct rxrpc_txbuf *txb)
{
	return !rxrpc_sending_to_server(txb);
}

#include <trace/events/rxrpc.h>

 
extern atomic_t rxrpc_n_rx_skbs;
extern struct workqueue_struct *rxrpc_workqueue;

 
int rxrpc_service_prealloc(struct rxrpc_sock *, gfp_t);
void rxrpc_discard_prealloc(struct rxrpc_sock *);
bool rxrpc_new_incoming_call(struct rxrpc_local *local,
			     struct rxrpc_peer *peer,
			     struct rxrpc_connection *conn,
			     struct sockaddr_rxrpc *peer_srx,
			     struct sk_buff *skb);
void rxrpc_accept_incoming_calls(struct rxrpc_local *);
int rxrpc_user_charge_accept(struct rxrpc_sock *, unsigned long);

 
void rxrpc_propose_ping(struct rxrpc_call *call, u32 serial,
			enum rxrpc_propose_ack_trace why);
void rxrpc_send_ACK(struct rxrpc_call *, u8, rxrpc_serial_t, enum rxrpc_propose_ack_trace);
void rxrpc_propose_delay_ACK(struct rxrpc_call *, rxrpc_serial_t,
			     enum rxrpc_propose_ack_trace);
void rxrpc_shrink_call_tx_buffer(struct rxrpc_call *);
void rxrpc_resend(struct rxrpc_call *call, struct sk_buff *ack_skb);

void rxrpc_reduce_call_timer(struct rxrpc_call *call,
			     unsigned long expire_at,
			     unsigned long now,
			     enum rxrpc_timer_trace why);

bool rxrpc_input_call_event(struct rxrpc_call *call, struct sk_buff *skb);

 
extern const char *const rxrpc_call_states[];
extern const char *const rxrpc_call_completions[];
extern struct kmem_cache *rxrpc_call_jar;

void rxrpc_poke_call(struct rxrpc_call *call, enum rxrpc_call_poke_trace what);
struct rxrpc_call *rxrpc_find_call_by_user_ID(struct rxrpc_sock *, unsigned long);
struct rxrpc_call *rxrpc_alloc_call(struct rxrpc_sock *, gfp_t, unsigned int);
struct rxrpc_call *rxrpc_new_client_call(struct rxrpc_sock *,
					 struct rxrpc_conn_parameters *,
					 struct sockaddr_rxrpc *,
					 struct rxrpc_call_params *, gfp_t,
					 unsigned int);
void rxrpc_start_call_timer(struct rxrpc_call *call);
void rxrpc_incoming_call(struct rxrpc_sock *, struct rxrpc_call *,
			 struct sk_buff *);
void rxrpc_release_call(struct rxrpc_sock *, struct rxrpc_call *);
void rxrpc_release_calls_on_socket(struct rxrpc_sock *);
void rxrpc_see_call(struct rxrpc_call *, enum rxrpc_call_trace);
struct rxrpc_call *rxrpc_try_get_call(struct rxrpc_call *, enum rxrpc_call_trace);
void rxrpc_get_call(struct rxrpc_call *, enum rxrpc_call_trace);
void rxrpc_put_call(struct rxrpc_call *, enum rxrpc_call_trace);
void rxrpc_cleanup_call(struct rxrpc_call *);
void rxrpc_destroy_all_calls(struct rxrpc_net *);

static inline bool rxrpc_is_service_call(const struct rxrpc_call *call)
{
	return test_bit(RXRPC_CALL_IS_SERVICE, &call->flags);
}

static inline bool rxrpc_is_client_call(const struct rxrpc_call *call)
{
	return !rxrpc_is_service_call(call);
}

 
bool rxrpc_set_call_completion(struct rxrpc_call *call,
			       enum rxrpc_call_completion compl,
			       u32 abort_code,
			       int error);
bool rxrpc_call_completed(struct rxrpc_call *call);
bool rxrpc_abort_call(struct rxrpc_call *call, rxrpc_seq_t seq,
		      u32 abort_code, int error, enum rxrpc_abort_reason why);
void rxrpc_prefail_call(struct rxrpc_call *call, enum rxrpc_call_completion compl,
			int error);

static inline void rxrpc_set_call_state(struct rxrpc_call *call,
					enum rxrpc_call_state state)
{
	 
	smp_store_release(&call->_state, state);
	wake_up(&call->waitq);
}

static inline enum rxrpc_call_state __rxrpc_call_state(const struct rxrpc_call *call)
{
	return call->_state;  
}

static inline bool __rxrpc_call_is_complete(const struct rxrpc_call *call)
{
	return __rxrpc_call_state(call) == RXRPC_CALL_COMPLETE;
}

static inline enum rxrpc_call_state rxrpc_call_state(const struct rxrpc_call *call)
{
	 
	return smp_load_acquire(&call->_state);
}

static inline bool rxrpc_call_is_complete(const struct rxrpc_call *call)
{
	return rxrpc_call_state(call) == RXRPC_CALL_COMPLETE;
}

static inline bool rxrpc_call_has_failed(const struct rxrpc_call *call)
{
	return rxrpc_call_is_complete(call) && call->completion != RXRPC_CALL_SUCCEEDED;
}

 
extern unsigned int rxrpc_reap_client_connections;
extern unsigned long rxrpc_conn_idle_client_expiry;
extern unsigned long rxrpc_conn_idle_client_fast_expiry;

void rxrpc_purge_client_connections(struct rxrpc_local *local);
struct rxrpc_bundle *rxrpc_get_bundle(struct rxrpc_bundle *, enum rxrpc_bundle_trace);
void rxrpc_put_bundle(struct rxrpc_bundle *, enum rxrpc_bundle_trace);
int rxrpc_look_up_bundle(struct rxrpc_call *call, gfp_t gfp);
void rxrpc_connect_client_calls(struct rxrpc_local *local);
void rxrpc_expose_client_call(struct rxrpc_call *);
void rxrpc_disconnect_client_call(struct rxrpc_bundle *, struct rxrpc_call *);
void rxrpc_deactivate_bundle(struct rxrpc_bundle *bundle);
void rxrpc_put_client_conn(struct rxrpc_connection *, enum rxrpc_conn_trace);
void rxrpc_discard_expired_client_conns(struct rxrpc_local *local);
void rxrpc_clean_up_local_conns(struct rxrpc_local *);

 
void rxrpc_conn_retransmit_call(struct rxrpc_connection *conn, struct sk_buff *skb,
				unsigned int channel);
int rxrpc_abort_conn(struct rxrpc_connection *conn, struct sk_buff *skb,
		     s32 abort_code, int err, enum rxrpc_abort_reason why);
void rxrpc_process_connection(struct work_struct *);
void rxrpc_process_delayed_final_acks(struct rxrpc_connection *, bool);
bool rxrpc_input_conn_packet(struct rxrpc_connection *conn, struct sk_buff *skb);
void rxrpc_input_conn_event(struct rxrpc_connection *conn, struct sk_buff *skb);

static inline bool rxrpc_is_conn_aborted(const struct rxrpc_connection *conn)
{
	 
	return smp_load_acquire(&conn->state) == RXRPC_CONN_ABORTED;
}

 
extern unsigned int rxrpc_connection_expiry;
extern unsigned int rxrpc_closed_conn_expiry;

void rxrpc_poke_conn(struct rxrpc_connection *conn, enum rxrpc_conn_trace why);
struct rxrpc_connection *rxrpc_alloc_connection(struct rxrpc_net *, gfp_t);
struct rxrpc_connection *rxrpc_find_client_connection_rcu(struct rxrpc_local *,
							  struct sockaddr_rxrpc *,
							  struct sk_buff *);
void __rxrpc_disconnect_call(struct rxrpc_connection *, struct rxrpc_call *);
void rxrpc_disconnect_call(struct rxrpc_call *);
void rxrpc_kill_client_conn(struct rxrpc_connection *);
void rxrpc_queue_conn(struct rxrpc_connection *, enum rxrpc_conn_trace);
void rxrpc_see_connection(struct rxrpc_connection *, enum rxrpc_conn_trace);
struct rxrpc_connection *rxrpc_get_connection(struct rxrpc_connection *,
					      enum rxrpc_conn_trace);
struct rxrpc_connection *rxrpc_get_connection_maybe(struct rxrpc_connection *,
						    enum rxrpc_conn_trace);
void rxrpc_put_connection(struct rxrpc_connection *, enum rxrpc_conn_trace);
void rxrpc_service_connection_reaper(struct work_struct *);
void rxrpc_destroy_all_connections(struct rxrpc_net *);

static inline bool rxrpc_conn_is_client(const struct rxrpc_connection *conn)
{
	return conn->out_clientflag;
}

static inline bool rxrpc_conn_is_service(const struct rxrpc_connection *conn)
{
	return !rxrpc_conn_is_client(conn);
}

static inline void rxrpc_reduce_conn_timer(struct rxrpc_connection *conn,
					   unsigned long expire_at)
{
	timer_reduce(&conn->timer, expire_at);
}

 
struct rxrpc_connection *rxrpc_find_service_conn_rcu(struct rxrpc_peer *,
						     struct sk_buff *);
struct rxrpc_connection *rxrpc_prealloc_service_connection(struct rxrpc_net *, gfp_t);
void rxrpc_new_incoming_connection(struct rxrpc_sock *, struct rxrpc_connection *,
				   const struct rxrpc_security *, struct sk_buff *);
void rxrpc_unpublish_service_conn(struct rxrpc_connection *);

 
void rxrpc_congestion_degrade(struct rxrpc_call *);
void rxrpc_input_call_packet(struct rxrpc_call *, struct sk_buff *);
void rxrpc_implicit_end_call(struct rxrpc_call *, struct sk_buff *);

 
int rxrpc_encap_rcv(struct sock *, struct sk_buff *);
void rxrpc_error_report(struct sock *);
bool rxrpc_direct_abort(struct sk_buff *skb, enum rxrpc_abort_reason why,
			s32 abort_code, int err);
int rxrpc_io_thread(void *data);
static inline void rxrpc_wake_up_io_thread(struct rxrpc_local *local)
{
	wake_up_process(local->io_thread);
}

static inline bool rxrpc_protocol_error(struct sk_buff *skb, enum rxrpc_abort_reason why)
{
	return rxrpc_direct_abort(skb, why, RX_PROTOCOL_ERROR, -EPROTO);
}

 
extern const struct rxrpc_security rxrpc_no_security;

 
extern struct key_type key_type_rxrpc;

int rxrpc_request_key(struct rxrpc_sock *, sockptr_t , int);
int rxrpc_get_server_data_key(struct rxrpc_connection *, const void *, time64_t,
			      u32);

 
void rxrpc_gen_version_string(void);
void rxrpc_send_version_request(struct rxrpc_local *local,
				struct rxrpc_host_header *hdr,
				struct sk_buff *skb);

 
void rxrpc_local_dont_fragment(const struct rxrpc_local *local, bool set);
struct rxrpc_local *rxrpc_lookup_local(struct net *, const struct sockaddr_rxrpc *);
struct rxrpc_local *rxrpc_get_local(struct rxrpc_local *, enum rxrpc_local_trace);
struct rxrpc_local *rxrpc_get_local_maybe(struct rxrpc_local *, enum rxrpc_local_trace);
void rxrpc_put_local(struct rxrpc_local *, enum rxrpc_local_trace);
struct rxrpc_local *rxrpc_use_local(struct rxrpc_local *, enum rxrpc_local_trace);
void rxrpc_unuse_local(struct rxrpc_local *, enum rxrpc_local_trace);
void rxrpc_destroy_local(struct rxrpc_local *local);
void rxrpc_destroy_all_locals(struct rxrpc_net *);

static inline bool __rxrpc_use_local(struct rxrpc_local *local,
				     enum rxrpc_local_trace why)
{
	int r, u;

	r = refcount_read(&local->ref);
	u = atomic_fetch_add_unless(&local->active_users, 1, 0);
	trace_rxrpc_local(local->debug_id, why, r, u);
	return u != 0;
}

static inline void rxrpc_see_local(struct rxrpc_local *local,
				   enum rxrpc_local_trace why)
{
	int r, u;

	r = refcount_read(&local->ref);
	u = atomic_read(&local->active_users);
	trace_rxrpc_local(local->debug_id, why, r, u);
}

 
extern unsigned int rxrpc_max_backlog __read_mostly;
extern unsigned long rxrpc_soft_ack_delay;
extern unsigned long rxrpc_idle_ack_delay;
extern unsigned int rxrpc_rx_window_size;
extern unsigned int rxrpc_rx_mtu;
extern unsigned int rxrpc_rx_jumbo_max;
#ifdef CONFIG_AF_RXRPC_INJECT_RX_DELAY
extern unsigned long rxrpc_inject_rx_delay;
#endif

 
extern unsigned int rxrpc_net_id;
extern struct pernet_operations rxrpc_net_ops;

static inline struct rxrpc_net *rxrpc_net(struct net *net)
{
	return net_generic(net, rxrpc_net_id);
}

 
int rxrpc_send_ack_packet(struct rxrpc_call *call, struct rxrpc_txbuf *txb);
int rxrpc_send_abort_packet(struct rxrpc_call *);
int rxrpc_send_data_packet(struct rxrpc_call *, struct rxrpc_txbuf *);
void rxrpc_send_conn_abort(struct rxrpc_connection *conn);
void rxrpc_reject_packet(struct rxrpc_local *local, struct sk_buff *skb);
void rxrpc_send_keepalive(struct rxrpc_peer *);
void rxrpc_transmit_one(struct rxrpc_call *call, struct rxrpc_txbuf *txb);

 
void rxrpc_input_error(struct rxrpc_local *, struct sk_buff *);
void rxrpc_peer_keepalive_worker(struct work_struct *);

 
struct rxrpc_peer *rxrpc_lookup_peer_rcu(struct rxrpc_local *,
					 const struct sockaddr_rxrpc *);
struct rxrpc_peer *rxrpc_lookup_peer(struct rxrpc_local *local,
				     struct sockaddr_rxrpc *srx, gfp_t gfp);
struct rxrpc_peer *rxrpc_alloc_peer(struct rxrpc_local *, gfp_t,
				    enum rxrpc_peer_trace);
void rxrpc_new_incoming_peer(struct rxrpc_local *local, struct rxrpc_peer *peer);
void rxrpc_destroy_all_peers(struct rxrpc_net *);
struct rxrpc_peer *rxrpc_get_peer(struct rxrpc_peer *, enum rxrpc_peer_trace);
struct rxrpc_peer *rxrpc_get_peer_maybe(struct rxrpc_peer *, enum rxrpc_peer_trace);
void rxrpc_put_peer(struct rxrpc_peer *, enum rxrpc_peer_trace);

 
extern const struct seq_operations rxrpc_call_seq_ops;
extern const struct seq_operations rxrpc_connection_seq_ops;
extern const struct seq_operations rxrpc_peer_seq_ops;
extern const struct seq_operations rxrpc_local_seq_ops;

 
void rxrpc_notify_socket(struct rxrpc_call *);
int rxrpc_recvmsg(struct socket *, struct msghdr *, size_t, int);

 
static inline int rxrpc_abort_eproto(struct rxrpc_call *call,
				     struct sk_buff *skb,
				     s32 abort_code,
				     enum rxrpc_abort_reason why)
{
	struct rxrpc_skb_priv *sp = rxrpc_skb(skb);

	rxrpc_abort_call(call, sp->hdr.seq, abort_code, -EPROTO, why);
	return -EPROTO;
}

 
void rxrpc_peer_add_rtt(struct rxrpc_call *, enum rxrpc_rtt_rx_trace, int,
			rxrpc_serial_t, rxrpc_serial_t, ktime_t, ktime_t);
unsigned long rxrpc_get_rto_backoff(struct rxrpc_peer *, bool);
void rxrpc_peer_init_rtt(struct rxrpc_peer *);

 
#ifdef CONFIG_RXKAD
extern const struct rxrpc_security rxkad;
#endif

 
int __init rxrpc_init_security(void);
const struct rxrpc_security *rxrpc_security_lookup(u8);
void rxrpc_exit_security(void);
int rxrpc_init_client_call_security(struct rxrpc_call *);
int rxrpc_init_client_conn_security(struct rxrpc_connection *);
const struct rxrpc_security *rxrpc_get_incoming_security(struct rxrpc_sock *,
							 struct sk_buff *);
struct key *rxrpc_look_up_server_security(struct rxrpc_connection *,
					  struct sk_buff *, u32, u32);

 
bool rxrpc_propose_abort(struct rxrpc_call *call, s32 abort_code, int error,
			 enum rxrpc_abort_reason why);
int rxrpc_do_sendmsg(struct rxrpc_sock *, struct msghdr *, size_t);

 
extern struct key_type key_type_rxrpc_s;

int rxrpc_server_keyring(struct rxrpc_sock *, sockptr_t, int);

 
void rxrpc_kernel_data_consumed(struct rxrpc_call *, struct sk_buff *);
void rxrpc_new_skb(struct sk_buff *, enum rxrpc_skb_trace);
void rxrpc_see_skb(struct sk_buff *, enum rxrpc_skb_trace);
void rxrpc_eaten_skb(struct sk_buff *, enum rxrpc_skb_trace);
void rxrpc_get_skb(struct sk_buff *, enum rxrpc_skb_trace);
void rxrpc_free_skb(struct sk_buff *, enum rxrpc_skb_trace);
void rxrpc_purge_queue(struct sk_buff_head *);

 
int rxrpc_stats_show(struct seq_file *seq, void *v);
int rxrpc_stats_clear(struct file *file, char *buf, size_t size);

#define rxrpc_inc_stat(rxnet, s) atomic_inc(&(rxnet)->s)
#define rxrpc_dec_stat(rxnet, s) atomic_dec(&(rxnet)->s)

 
#ifdef CONFIG_SYSCTL
extern int __init rxrpc_sysctl_init(void);
extern void rxrpc_sysctl_exit(void);
#else
static inline int __init rxrpc_sysctl_init(void) { return 0; }
static inline void rxrpc_sysctl_exit(void) {}
#endif

 
extern atomic_t rxrpc_nr_txbuf;
struct rxrpc_txbuf *rxrpc_alloc_txbuf(struct rxrpc_call *call, u8 packet_type,
				      gfp_t gfp);
void rxrpc_get_txbuf(struct rxrpc_txbuf *txb, enum rxrpc_txbuf_trace what);
void rxrpc_see_txbuf(struct rxrpc_txbuf *txb, enum rxrpc_txbuf_trace what);
void rxrpc_put_txbuf(struct rxrpc_txbuf *txb, enum rxrpc_txbuf_trace what);

 
int rxrpc_extract_addr_from_skb(struct sockaddr_rxrpc *, struct sk_buff *);

static inline bool before(u32 seq1, u32 seq2)
{
        return (s32)(seq1 - seq2) < 0;
}
static inline bool before_eq(u32 seq1, u32 seq2)
{
        return (s32)(seq1 - seq2) <= 0;
}
static inline bool after(u32 seq1, u32 seq2)
{
        return (s32)(seq1 - seq2) > 0;
}
static inline bool after_eq(u32 seq1, u32 seq2)
{
        return (s32)(seq1 - seq2) >= 0;
}

 
extern unsigned int rxrpc_debug;

#define dbgprintk(FMT,...) \
	printk("[%-6.6s] "FMT"\n", current->comm ,##__VA_ARGS__)

#define kenter(FMT,...)	dbgprintk("==> %s("FMT")",__func__ ,##__VA_ARGS__)
#define kleave(FMT,...)	dbgprintk("<== %s()"FMT"",__func__ ,##__VA_ARGS__)
#define kdebug(FMT,...)	dbgprintk("    "FMT ,##__VA_ARGS__)


#if defined(__KDEBUG)
#define _enter(FMT,...)	kenter(FMT,##__VA_ARGS__)
#define _leave(FMT,...)	kleave(FMT,##__VA_ARGS__)
#define _debug(FMT,...)	kdebug(FMT,##__VA_ARGS__)

#elif defined(CONFIG_AF_RXRPC_DEBUG)
#define RXRPC_DEBUG_KENTER	0x01
#define RXRPC_DEBUG_KLEAVE	0x02
#define RXRPC_DEBUG_KDEBUG	0x04

#define _enter(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KENTER))	\
		kenter(FMT,##__VA_ARGS__);		\
} while (0)

#define _leave(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KLEAVE))	\
		kleave(FMT,##__VA_ARGS__);		\
} while (0)

#define _debug(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KDEBUG))	\
		kdebug(FMT,##__VA_ARGS__);		\
} while (0)

#else
#define _enter(FMT,...)	no_printk("==> %s("FMT")",__func__ ,##__VA_ARGS__)
#define _leave(FMT,...)	no_printk("<== %s()"FMT"",__func__ ,##__VA_ARGS__)
#define _debug(FMT,...)	no_printk("    "FMT ,##__VA_ARGS__)
#endif

 
#if 1 

#define ASSERT(X)						\
do {								\
	if (unlikely(!(X))) {					\
		pr_err("Assertion failed\n");			\
		BUG();						\
	}							\
} while (0)

#define ASSERTCMP(X, OP, Y)						\
do {									\
	__typeof__(X) _x = (X);						\
	__typeof__(Y) _y = (__typeof__(X))(Y);				\
	if (unlikely(!(_x OP _y))) {					\
		pr_err("Assertion failed - %lu(0x%lx) %s %lu(0x%lx) is false\n", \
		       (unsigned long)_x, (unsigned long)_x, #OP,	\
		       (unsigned long)_y, (unsigned long)_y);		\
		BUG();							\
	}								\
} while (0)

#define ASSERTIF(C, X)						\
do {								\
	if (unlikely((C) && !(X))) {				\
		pr_err("Assertion failed\n");			\
		BUG();						\
	}							\
} while (0)

#define ASSERTIFCMP(C, X, OP, Y)					\
do {									\
	__typeof__(X) _x = (X);						\
	__typeof__(Y) _y = (__typeof__(X))(Y);				\
	if (unlikely((C) && !(_x OP _y))) {				\
		pr_err("Assertion failed - %lu(0x%lx) %s %lu(0x%lx) is false\n", \
		       (unsigned long)_x, (unsigned long)_x, #OP,	\
		       (unsigned long)_y, (unsigned long)_y);		\
		BUG();							\
	}								\
} while (0)

#else

#define ASSERT(X)				\
do {						\
} while (0)

#define ASSERTCMP(X, OP, Y)			\
do {						\
} while (0)

#define ASSERTIF(C, X)				\
do {						\
} while (0)

#define ASSERTIFCMP(C, X, OP, Y)		\
do {						\
} while (0)

#endif  
