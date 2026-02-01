 







#ifndef _J1939_PRIV_H_
#define _J1939_PRIV_H_

#include <linux/can/j1939.h>
#include <net/sock.h>

 
#define J1939_XTP_ABORT_TIMEOUT_MS 500
#define J1939_SIMPLE_ECHO_TIMEOUT_MS (10 * 1000)

struct j1939_session;
enum j1939_sk_errqueue_type {
	J1939_ERRQUEUE_TX_ACK,
	J1939_ERRQUEUE_TX_SCHED,
	J1939_ERRQUEUE_TX_ABORT,
	J1939_ERRQUEUE_RX_RTS,
	J1939_ERRQUEUE_RX_DPO,
	J1939_ERRQUEUE_RX_ABORT,
};

 
struct j1939_ecu {
	struct list_head list;
	name_t name;
	u8 addr;

	 
	struct hrtimer ac_timer;
	struct kref kref;
	struct j1939_priv *priv;

	 
	int nusers;
};

struct j1939_priv {
	struct list_head ecus;
	 

	 
	rwlock_t lock;

	struct net_device *ndev;

	 
	struct j1939_addr_ent {
		struct j1939_ecu *ecu;
		 
		int nusers;
	} ents[256];

	struct kref kref;

	 
	struct list_head active_session_list;

	 
	spinlock_t active_session_list_lock;

	unsigned int tp_max_packet_size;

	 
	spinlock_t j1939_socks_lock;
	struct list_head j1939_socks;

	struct kref rx_kref;
	u32 rx_tskey;
};

void j1939_ecu_put(struct j1939_ecu *ecu);

 
int j1939_local_ecu_get(struct j1939_priv *priv, name_t name, u8 sa);
void j1939_local_ecu_put(struct j1939_priv *priv, name_t name, u8 sa);

static inline bool j1939_address_is_unicast(u8 addr)
{
	return addr <= J1939_MAX_UNICAST_ADDR;
}

static inline bool j1939_address_is_idle(u8 addr)
{
	return addr == J1939_IDLE_ADDR;
}

static inline bool j1939_address_is_valid(u8 addr)
{
	return addr != J1939_NO_ADDR;
}

static inline bool j1939_pgn_is_pdu1(pgn_t pgn)
{
	 
	return (pgn & 0xff00) < 0xf000;
}

 
void j1939_ecu_unmap_locked(struct j1939_ecu *ecu);
void j1939_ecu_unmap(struct j1939_ecu *ecu);

u8 j1939_name_to_addr(struct j1939_priv *priv, name_t name);
struct j1939_ecu *j1939_ecu_find_by_addr_locked(struct j1939_priv *priv,
						u8 addr);
struct j1939_ecu *j1939_ecu_get_by_addr(struct j1939_priv *priv, u8 addr);
struct j1939_ecu *j1939_ecu_get_by_addr_locked(struct j1939_priv *priv,
					       u8 addr);
struct j1939_ecu *j1939_ecu_get_by_name(struct j1939_priv *priv, name_t name);
struct j1939_ecu *j1939_ecu_get_by_name_locked(struct j1939_priv *priv,
					       name_t name);

enum j1939_transfer_type {
	J1939_TP,
	J1939_ETP,
	J1939_SIMPLE,
};

struct j1939_addr {
	name_t src_name;
	name_t dst_name;
	pgn_t pgn;

	u8 sa;
	u8 da;

	u8 type;
};

 
struct j1939_sk_buff_cb {
	 
	u32 offset;

	 
	u32 msg_flags;
	u32 tskey;

	struct j1939_addr addr;

	 
#define J1939_ECU_LOCAL_SRC BIT(0)
#define J1939_ECU_LOCAL_DST BIT(1)
	u8 flags;

	priority_t priority;
};

static inline
struct j1939_sk_buff_cb *j1939_skb_to_cb(const struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct j1939_sk_buff_cb) > sizeof(skb->cb));

	return (struct j1939_sk_buff_cb *)skb->cb;
}

int j1939_send_one(struct j1939_priv *priv, struct sk_buff *skb);
void j1939_sk_recv(struct j1939_priv *priv, struct sk_buff *skb);
bool j1939_sk_recv_match(struct j1939_priv *priv,
			 struct j1939_sk_buff_cb *skcb);
void j1939_sk_send_loop_abort(struct sock *sk, int err);
void j1939_sk_errqueue(struct j1939_session *session,
		       enum j1939_sk_errqueue_type type);
void j1939_sk_queue_activate_next(struct j1939_session *session);

 
struct j1939_session *j1939_tp_send(struct j1939_priv *priv,
				    struct sk_buff *skb, size_t size);
int j1939_tp_recv(struct j1939_priv *priv, struct sk_buff *skb);
int j1939_ac_fixup(struct j1939_priv *priv, struct sk_buff *skb);
void j1939_ac_recv(struct j1939_priv *priv, struct sk_buff *skb);
void j1939_simple_recv(struct j1939_priv *priv, struct sk_buff *skb);

 
struct j1939_ecu *j1939_ecu_create_locked(struct j1939_priv *priv, name_t name);

void j1939_ecu_timer_start(struct j1939_ecu *ecu);
void j1939_ecu_timer_cancel(struct j1939_ecu *ecu);
void j1939_ecu_unmap_all(struct j1939_priv *priv);

struct j1939_priv *j1939_netdev_start(struct net_device *ndev);
void j1939_netdev_stop(struct j1939_priv *priv);

void j1939_priv_put(struct j1939_priv *priv);
void j1939_priv_get(struct j1939_priv *priv);

 
void j1939_sk_netdev_event_netdown(struct j1939_priv *priv);
int j1939_cancel_active_session(struct j1939_priv *priv, struct sock *sk);
void j1939_tp_init(struct j1939_priv *priv);

 
void j1939_sock_pending_del(struct sock *sk);

enum j1939_session_state {
	J1939_SESSION_NEW,
	J1939_SESSION_ACTIVE,
	 
	J1939_SESSION_WAITING_ABORT,
	J1939_SESSION_ACTIVE_MAX,
	J1939_SESSION_DONE,
};

struct j1939_session {
	struct j1939_priv *priv;
	struct list_head active_session_list_entry;
	struct list_head sk_session_queue_entry;
	struct kref kref;
	struct sock *sk;

	 
	struct j1939_sk_buff_cb skcb;
	struct sk_buff_head skb_queue;

	 
	u8 last_cmd, last_txcmd;
	bool transmission;
	bool extd;
	 
	unsigned int total_message_size;
	 
	unsigned int total_queued_size;
	unsigned int tx_retry;

	int err;
	u32 tskey;
	enum j1939_session_state state;

	 
	struct {
		 
		unsigned int total;
		 
		unsigned int last;
		 
		unsigned int tx;
		unsigned int tx_acked;
		 
		unsigned int rx;
		 
		unsigned int block;
		 
		unsigned int dpo;
	} pkt;
	struct hrtimer txtimer, rxtimer;
};

struct j1939_sock {
	struct sock sk;  
	struct j1939_priv *priv;
	struct list_head list;

#define J1939_SOCK_BOUND BIT(0)
#define J1939_SOCK_CONNECTED BIT(1)
#define J1939_SOCK_PROMISC BIT(2)
#define J1939_SOCK_ERRQUEUE BIT(3)
	int state;

	int ifindex;
	struct j1939_addr addr;
	struct j1939_filter *filters;
	int nfilters;
	pgn_t pgn_rx_filter;

	 
	atomic_t skb_pending;
	wait_queue_head_t waitq;

	 
	spinlock_t sk_session_queue_lock;
	struct list_head sk_session_queue;
};

static inline struct j1939_sock *j1939_sk(const struct sock *sk)
{
	return container_of(sk, struct j1939_sock, sk);
}

void j1939_session_get(struct j1939_session *session);
void j1939_session_put(struct j1939_session *session);
void j1939_session_skb_queue(struct j1939_session *session,
			     struct sk_buff *skb);
int j1939_session_activate(struct j1939_session *session);
void j1939_tp_schedule_txtimer(struct j1939_session *session, int msec);
void j1939_session_timers_cancel(struct j1939_session *session);

#define J1939_MIN_TP_PACKET_SIZE 9
#define J1939_MAX_TP_PACKET_SIZE (7 * 0xff)
#define J1939_MAX_ETP_PACKET_SIZE (7 * 0x00ffffff)

#define J1939_REGULAR 0
#define J1939_EXTENDED 1

 
extern const struct can_proto j1939_can_proto;

#endif  
