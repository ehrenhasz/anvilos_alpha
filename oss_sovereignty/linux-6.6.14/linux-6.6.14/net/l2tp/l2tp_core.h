#include <linux/refcount.h>
#ifndef _L2TP_CORE_H_
#define _L2TP_CORE_H_
#include <net/dst.h>
#include <net/sock.h>
#ifdef CONFIG_XFRM
#include <net/xfrm.h>
#endif
#define L2TP_TUNNEL_MAGIC	0x42114DDA
#define L2TP_SESSION_MAGIC	0x0C04EB7D
#define L2TP_HASH_BITS	4
#define L2TP_HASH_SIZE	BIT(L2TP_HASH_BITS)
#define L2TP_HASH_BITS_2	8
#define L2TP_HASH_SIZE_2	BIT(L2TP_HASH_BITS_2)
struct sk_buff;
struct l2tp_stats {
	atomic_long_t		tx_packets;
	atomic_long_t		tx_bytes;
	atomic_long_t		tx_errors;
	atomic_long_t		rx_packets;
	atomic_long_t		rx_bytes;
	atomic_long_t		rx_seq_discards;
	atomic_long_t		rx_oos_packets;
	atomic_long_t		rx_errors;
	atomic_long_t		rx_cookie_discards;
	atomic_long_t		rx_invalid;
};
struct l2tp_tunnel;
struct l2tp_session_cfg {
	enum l2tp_pwtype	pw_type;
	unsigned int		recv_seq:1;	 
	unsigned int		send_seq:1;	 
	unsigned int		lns_mode:1;	 
	u16			l2specific_type;  
	u8			cookie[8];	 
	int			cookie_len;	 
	u8			peer_cookie[8];	 
	int			peer_cookie_len;  
	int			reorder_timeout;  
	char			*ifname;
};
#define L2TP_SESSION_NAME_MAX 32
struct l2tp_session {
	int			magic;		 
	long			dead;
	struct l2tp_tunnel	*tunnel;	 
	u32			session_id;
	u32			peer_session_id;
	u8			cookie[8];
	int			cookie_len;
	u8			peer_cookie[8];
	int			peer_cookie_len;
	u16			l2specific_type;
	u16			hdr_len;
	u32			nr;		 
	u32			ns;		 
	struct sk_buff_head	reorder_q;	 
	u32			nr_max;		 
	u32			nr_window_size;	 
	u32			nr_oos;		 
	int			nr_oos_count;	 
	int			nr_oos_count_max;
	struct hlist_node	hlist;		 
	refcount_t		ref_count;
	char			name[L2TP_SESSION_NAME_MAX];  
	char			ifname[IFNAMSIZ];
	unsigned int		recv_seq:1;	 
	unsigned int		send_seq:1;	 
	unsigned int		lns_mode:1;	 
	int			reorder_timeout;  
	int			reorder_skip;	 
	enum l2tp_pwtype	pwtype;
	struct l2tp_stats	stats;
	struct hlist_node	global_hlist;	 
	void (*recv_skb)(struct l2tp_session *session, struct sk_buff *skb, int data_len);
	void (*session_close)(struct l2tp_session *session);
	void (*show)(struct seq_file *m, void *priv);
	u8			priv[];		 
};
struct l2tp_tunnel_cfg {
	enum l2tp_encap_type	encap;
	struct in_addr		local_ip;
	struct in_addr		peer_ip;
#if IS_ENABLED(CONFIG_IPV6)
	struct in6_addr		*local_ip6;
	struct in6_addr		*peer_ip6;
#endif
	u16			local_udp_port;
	u16			peer_udp_port;
	unsigned int		use_udp_checksums:1,
				udp6_zero_tx_checksums:1,
				udp6_zero_rx_checksums:1;
};
#define L2TP_TUNNEL_NAME_MAX 20
struct l2tp_tunnel {
	int			magic;		 
	unsigned long		dead;
	struct rcu_head rcu;
	spinlock_t		hlist_lock;	 
	bool			acpt_newsess;	 
	struct hlist_head	session_hlist[L2TP_HASH_SIZE];
	u32			tunnel_id;
	u32			peer_tunnel_id;
	int			version;	 
	char			name[L2TP_TUNNEL_NAME_MAX];  
	enum l2tp_encap_type	encap;
	struct l2tp_stats	stats;
	struct list_head	list;		 
	struct net		*l2tp_net;	 
	refcount_t		ref_count;
	void (*old_sk_destruct)(struct sock *sk);
	struct sock		*sock;		 
	int			fd;		 
	struct work_struct	del_work;
};
struct l2tp_nl_cmd_ops {
	int (*session_create)(struct net *net, struct l2tp_tunnel *tunnel,
			      u32 session_id, u32 peer_session_id,
			      struct l2tp_session_cfg *cfg);
	void (*session_delete)(struct l2tp_session *session);
};
static inline void *l2tp_session_priv(struct l2tp_session *session)
{
	return &session->priv[0];
}
void l2tp_tunnel_inc_refcount(struct l2tp_tunnel *tunnel);
void l2tp_tunnel_dec_refcount(struct l2tp_tunnel *tunnel);
void l2tp_session_inc_refcount(struct l2tp_session *session);
void l2tp_session_dec_refcount(struct l2tp_session *session);
struct l2tp_tunnel *l2tp_tunnel_get(const struct net *net, u32 tunnel_id);
struct l2tp_tunnel *l2tp_tunnel_get_nth(const struct net *net, int nth);
struct l2tp_session *l2tp_tunnel_get_session(struct l2tp_tunnel *tunnel,
					     u32 session_id);
struct l2tp_session *l2tp_session_get(const struct net *net, u32 session_id);
struct l2tp_session *l2tp_session_get_nth(struct l2tp_tunnel *tunnel, int nth);
struct l2tp_session *l2tp_session_get_by_ifname(const struct net *net,
						const char *ifname);
int l2tp_tunnel_create(int fd, int version, u32 tunnel_id,
		       u32 peer_tunnel_id, struct l2tp_tunnel_cfg *cfg,
		       struct l2tp_tunnel **tunnelp);
int l2tp_tunnel_register(struct l2tp_tunnel *tunnel, struct net *net,
			 struct l2tp_tunnel_cfg *cfg);
void l2tp_tunnel_delete(struct l2tp_tunnel *tunnel);
struct l2tp_session *l2tp_session_create(int priv_size,
					 struct l2tp_tunnel *tunnel,
					 u32 session_id, u32 peer_session_id,
					 struct l2tp_session_cfg *cfg);
int l2tp_session_register(struct l2tp_session *session,
			  struct l2tp_tunnel *tunnel);
void l2tp_session_delete(struct l2tp_session *session);
void l2tp_recv_common(struct l2tp_session *session, struct sk_buff *skb,
		      unsigned char *ptr, unsigned char *optr, u16 hdrflags,
		      int length);
int l2tp_udp_encap_recv(struct sock *sk, struct sk_buff *skb);
void l2tp_session_set_header_len(struct l2tp_session *session, int version);
int l2tp_xmit_skb(struct l2tp_session *session, struct sk_buff *skb);
int l2tp_nl_register_ops(enum l2tp_pwtype pw_type, const struct l2tp_nl_cmd_ops *ops);
void l2tp_nl_unregister_ops(enum l2tp_pwtype pw_type);
int l2tp_ioctl(struct sock *sk, int cmd, int *karg);
struct l2tp_tunnel *l2tp_sk_to_tunnel(struct sock *sk);
static inline int l2tp_get_l2specific_len(struct l2tp_session *session)
{
	switch (session->l2specific_type) {
	case L2TP_L2SPECTYPE_DEFAULT:
		return 4;
	case L2TP_L2SPECTYPE_NONE:
	default:
		return 0;
	}
}
static inline u32 l2tp_tunnel_dst_mtu(const struct l2tp_tunnel *tunnel)
{
	struct dst_entry *dst;
	u32 mtu;
	dst = sk_dst_get(tunnel->sock);
	if (!dst)
		return 0;
	mtu = dst_mtu(dst);
	dst_release(dst);
	return mtu;
}
#ifdef CONFIG_XFRM
static inline bool l2tp_tunnel_uses_xfrm(const struct l2tp_tunnel *tunnel)
{
	struct sock *sk = tunnel->sock;
	return sk && (rcu_access_pointer(sk->sk_policy[0]) ||
		      rcu_access_pointer(sk->sk_policy[1]));
}
#else
static inline bool l2tp_tunnel_uses_xfrm(const struct l2tp_tunnel *tunnel)
{
	return false;
}
#endif
static inline int l2tp_v3_ensure_opt_in_linear(struct l2tp_session *session, struct sk_buff *skb,
					       unsigned char **ptr, unsigned char **optr)
{
	int opt_len = session->peer_cookie_len + l2tp_get_l2specific_len(session);
	if (opt_len > 0) {
		int off = *ptr - *optr;
		if (!pskb_may_pull(skb, off + opt_len))
			return -1;
		if (skb->data != *optr) {
			*optr = skb->data;
			*ptr = skb->data + off;
		}
	}
	return 0;
}
#define MODULE_ALIAS_L2TP_PWTYPE(type) \
	MODULE_ALIAS("net-l2tp-type-" __stringify(type))
#endif  
