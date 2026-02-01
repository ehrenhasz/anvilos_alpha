
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/uaccess.h>

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/jiffies.h>

#include <linux/netdevice.h>
#include <linux/net.h>
#include <linux/inetdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/l2tp.h>
#include <linux/hash.h>
#include <linux/sort.h>
#include <linux/file.h>
#include <linux/nsproxy.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/dst.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/udp_tunnel.h>
#include <net/inet_common.h>
#include <net/xfrm.h>
#include <net/protocol.h>
#include <net/inet6_connection_sock.h>
#include <net/inet_ecn.h>
#include <net/ip6_route.h>
#include <net/ip6_checksum.h>

#include <asm/byteorder.h>
#include <linux/atomic.h>

#include "l2tp_core.h"
#include "trace.h"

#define CREATE_TRACE_POINTS
#include "trace.h"

#define L2TP_DRV_VERSION	"V2.0"

 
#define L2TP_HDRFLAG_T	   0x8000
#define L2TP_HDRFLAG_L	   0x4000
#define L2TP_HDRFLAG_S	   0x0800
#define L2TP_HDRFLAG_O	   0x0200
#define L2TP_HDRFLAG_P	   0x0100

#define L2TP_HDR_VER_MASK  0x000F
#define L2TP_HDR_VER_2	   0x0002
#define L2TP_HDR_VER_3	   0x0003

 
#define L2TP_SLFLAG_S	   0x40000000
#define L2TP_SL_SEQ_MASK   0x00ffffff

#define L2TP_HDR_SIZE_MAX		14

 
#define L2TP_DEFAULT_DEBUG_FLAGS	0

 
struct l2tp_skb_cb {
	u32			ns;
	u16			has_seq;
	u16			length;
	unsigned long		expires;
};

#define L2TP_SKB_CB(skb)	((struct l2tp_skb_cb *)&(skb)->cb[sizeof(struct inet_skb_parm)])

static struct workqueue_struct *l2tp_wq;

 
static unsigned int l2tp_net_id;
struct l2tp_net {
	 
	spinlock_t l2tp_tunnel_idr_lock;
	struct idr l2tp_tunnel_idr;
	struct hlist_head l2tp_session_hlist[L2TP_HASH_SIZE_2];
	 
	spinlock_t l2tp_session_hlist_lock;
};

#if IS_ENABLED(CONFIG_IPV6)
static bool l2tp_sk_is_v6(struct sock *sk)
{
	return sk->sk_family == PF_INET6 &&
	       !ipv6_addr_v4mapped(&sk->sk_v6_daddr);
}
#endif

static inline struct l2tp_net *l2tp_pernet(const struct net *net)
{
	return net_generic(net, l2tp_net_id);
}

 
static inline struct hlist_head *
l2tp_session_id_hash_2(struct l2tp_net *pn, u32 session_id)
{
	return &pn->l2tp_session_hlist[hash_32(session_id, L2TP_HASH_BITS_2)];
}

 
static inline struct hlist_head *
l2tp_session_id_hash(struct l2tp_tunnel *tunnel, u32 session_id)
{
	return &tunnel->session_hlist[hash_32(session_id, L2TP_HASH_BITS)];
}

static void l2tp_tunnel_free(struct l2tp_tunnel *tunnel)
{
	trace_free_tunnel(tunnel);
	sock_put(tunnel->sock);
	 
}

static void l2tp_session_free(struct l2tp_session *session)
{
	trace_free_session(session);
	if (session->tunnel)
		l2tp_tunnel_dec_refcount(session->tunnel);
	kfree(session);
}

struct l2tp_tunnel *l2tp_sk_to_tunnel(struct sock *sk)
{
	struct l2tp_tunnel *tunnel = sk->sk_user_data;

	if (tunnel)
		if (WARN_ON(tunnel->magic != L2TP_TUNNEL_MAGIC))
			return NULL;

	return tunnel;
}
EXPORT_SYMBOL_GPL(l2tp_sk_to_tunnel);

void l2tp_tunnel_inc_refcount(struct l2tp_tunnel *tunnel)
{
	refcount_inc(&tunnel->ref_count);
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_inc_refcount);

void l2tp_tunnel_dec_refcount(struct l2tp_tunnel *tunnel)
{
	if (refcount_dec_and_test(&tunnel->ref_count))
		l2tp_tunnel_free(tunnel);
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_dec_refcount);

void l2tp_session_inc_refcount(struct l2tp_session *session)
{
	refcount_inc(&session->ref_count);
}
EXPORT_SYMBOL_GPL(l2tp_session_inc_refcount);

void l2tp_session_dec_refcount(struct l2tp_session *session)
{
	if (refcount_dec_and_test(&session->ref_count))
		l2tp_session_free(session);
}
EXPORT_SYMBOL_GPL(l2tp_session_dec_refcount);

 
struct l2tp_tunnel *l2tp_tunnel_get(const struct net *net, u32 tunnel_id)
{
	const struct l2tp_net *pn = l2tp_pernet(net);
	struct l2tp_tunnel *tunnel;

	rcu_read_lock_bh();
	tunnel = idr_find(&pn->l2tp_tunnel_idr, tunnel_id);
	if (tunnel && refcount_inc_not_zero(&tunnel->ref_count)) {
		rcu_read_unlock_bh();
		return tunnel;
	}
	rcu_read_unlock_bh();

	return NULL;
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_get);

struct l2tp_tunnel *l2tp_tunnel_get_nth(const struct net *net, int nth)
{
	struct l2tp_net *pn = l2tp_pernet(net);
	unsigned long tunnel_id, tmp;
	struct l2tp_tunnel *tunnel;
	int count = 0;

	rcu_read_lock_bh();
	idr_for_each_entry_ul(&pn->l2tp_tunnel_idr, tunnel, tmp, tunnel_id) {
		if (tunnel && ++count > nth &&
		    refcount_inc_not_zero(&tunnel->ref_count)) {
			rcu_read_unlock_bh();
			return tunnel;
		}
	}
	rcu_read_unlock_bh();

	return NULL;
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_get_nth);

struct l2tp_session *l2tp_tunnel_get_session(struct l2tp_tunnel *tunnel,
					     u32 session_id)
{
	struct hlist_head *session_list;
	struct l2tp_session *session;

	session_list = l2tp_session_id_hash(tunnel, session_id);

	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(session, session_list, hlist)
		if (session->session_id == session_id) {
			l2tp_session_inc_refcount(session);
			rcu_read_unlock_bh();

			return session;
		}
	rcu_read_unlock_bh();

	return NULL;
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_get_session);

struct l2tp_session *l2tp_session_get(const struct net *net, u32 session_id)
{
	struct hlist_head *session_list;
	struct l2tp_session *session;

	session_list = l2tp_session_id_hash_2(l2tp_pernet(net), session_id);

	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(session, session_list, global_hlist)
		if (session->session_id == session_id) {
			l2tp_session_inc_refcount(session);
			rcu_read_unlock_bh();

			return session;
		}
	rcu_read_unlock_bh();

	return NULL;
}
EXPORT_SYMBOL_GPL(l2tp_session_get);

struct l2tp_session *l2tp_session_get_nth(struct l2tp_tunnel *tunnel, int nth)
{
	int hash;
	struct l2tp_session *session;
	int count = 0;

	rcu_read_lock_bh();
	for (hash = 0; hash < L2TP_HASH_SIZE; hash++) {
		hlist_for_each_entry_rcu(session, &tunnel->session_hlist[hash], hlist) {
			if (++count > nth) {
				l2tp_session_inc_refcount(session);
				rcu_read_unlock_bh();
				return session;
			}
		}
	}

	rcu_read_unlock_bh();

	return NULL;
}
EXPORT_SYMBOL_GPL(l2tp_session_get_nth);

 
struct l2tp_session *l2tp_session_get_by_ifname(const struct net *net,
						const char *ifname)
{
	struct l2tp_net *pn = l2tp_pernet(net);
	int hash;
	struct l2tp_session *session;

	rcu_read_lock_bh();
	for (hash = 0; hash < L2TP_HASH_SIZE_2; hash++) {
		hlist_for_each_entry_rcu(session, &pn->l2tp_session_hlist[hash], global_hlist) {
			if (!strcmp(session->ifname, ifname)) {
				l2tp_session_inc_refcount(session);
				rcu_read_unlock_bh();

				return session;
			}
		}
	}

	rcu_read_unlock_bh();

	return NULL;
}
EXPORT_SYMBOL_GPL(l2tp_session_get_by_ifname);

int l2tp_session_register(struct l2tp_session *session,
			  struct l2tp_tunnel *tunnel)
{
	struct l2tp_session *session_walk;
	struct hlist_head *g_head;
	struct hlist_head *head;
	struct l2tp_net *pn;
	int err;

	head = l2tp_session_id_hash(tunnel, session->session_id);

	spin_lock_bh(&tunnel->hlist_lock);
	if (!tunnel->acpt_newsess) {
		err = -ENODEV;
		goto err_tlock;
	}

	hlist_for_each_entry(session_walk, head, hlist)
		if (session_walk->session_id == session->session_id) {
			err = -EEXIST;
			goto err_tlock;
		}

	if (tunnel->version == L2TP_HDR_VER_3) {
		pn = l2tp_pernet(tunnel->l2tp_net);
		g_head = l2tp_session_id_hash_2(pn, session->session_id);

		spin_lock_bh(&pn->l2tp_session_hlist_lock);

		 
		hlist_for_each_entry(session_walk, g_head, global_hlist)
			if (session_walk->session_id == session->session_id &&
			    (session_walk->tunnel->encap == L2TP_ENCAPTYPE_IP ||
			     tunnel->encap == L2TP_ENCAPTYPE_IP)) {
				err = -EEXIST;
				goto err_tlock_pnlock;
			}

		l2tp_tunnel_inc_refcount(tunnel);
		hlist_add_head_rcu(&session->global_hlist, g_head);

		spin_unlock_bh(&pn->l2tp_session_hlist_lock);
	} else {
		l2tp_tunnel_inc_refcount(tunnel);
	}

	hlist_add_head_rcu(&session->hlist, head);
	spin_unlock_bh(&tunnel->hlist_lock);

	trace_register_session(session);

	return 0;

err_tlock_pnlock:
	spin_unlock_bh(&pn->l2tp_session_hlist_lock);
err_tlock:
	spin_unlock_bh(&tunnel->hlist_lock);

	return err;
}
EXPORT_SYMBOL_GPL(l2tp_session_register);

 

 
static void l2tp_recv_queue_skb(struct l2tp_session *session, struct sk_buff *skb)
{
	struct sk_buff *skbp;
	struct sk_buff *tmp;
	u32 ns = L2TP_SKB_CB(skb)->ns;

	spin_lock_bh(&session->reorder_q.lock);
	skb_queue_walk_safe(&session->reorder_q, skbp, tmp) {
		if (L2TP_SKB_CB(skbp)->ns > ns) {
			__skb_queue_before(&session->reorder_q, skbp, skb);
			atomic_long_inc(&session->stats.rx_oos_packets);
			goto out;
		}
	}

	__skb_queue_tail(&session->reorder_q, skb);

out:
	spin_unlock_bh(&session->reorder_q.lock);
}

 
static void l2tp_recv_dequeue_skb(struct l2tp_session *session, struct sk_buff *skb)
{
	struct l2tp_tunnel *tunnel = session->tunnel;
	int length = L2TP_SKB_CB(skb)->length;

	 
	skb_orphan(skb);

	atomic_long_inc(&tunnel->stats.rx_packets);
	atomic_long_add(length, &tunnel->stats.rx_bytes);
	atomic_long_inc(&session->stats.rx_packets);
	atomic_long_add(length, &session->stats.rx_bytes);

	if (L2TP_SKB_CB(skb)->has_seq) {
		 
		session->nr++;
		session->nr &= session->nr_max;
		trace_session_seqnum_update(session);
	}

	 
	if (session->recv_skb)
		(*session->recv_skb)(session, skb, L2TP_SKB_CB(skb)->length);
	else
		kfree_skb(skb);
}

 
static void l2tp_recv_dequeue(struct l2tp_session *session)
{
	struct sk_buff *skb;
	struct sk_buff *tmp;

	 
start:
	spin_lock_bh(&session->reorder_q.lock);
	skb_queue_walk_safe(&session->reorder_q, skb, tmp) {
		struct l2tp_skb_cb *cb = L2TP_SKB_CB(skb);

		 
		if (time_after(jiffies, cb->expires)) {
			atomic_long_inc(&session->stats.rx_seq_discards);
			atomic_long_inc(&session->stats.rx_errors);
			trace_session_pkt_expired(session, cb->ns);
			session->reorder_skip = 1;
			__skb_unlink(skb, &session->reorder_q);
			kfree_skb(skb);
			continue;
		}

		if (cb->has_seq) {
			if (session->reorder_skip) {
				session->reorder_skip = 0;
				session->nr = cb->ns;
				trace_session_seqnum_reset(session);
			}
			if (cb->ns != session->nr)
				goto out;
		}
		__skb_unlink(skb, &session->reorder_q);

		 
		spin_unlock_bh(&session->reorder_q.lock);
		l2tp_recv_dequeue_skb(session, skb);
		goto start;
	}

out:
	spin_unlock_bh(&session->reorder_q.lock);
}

static int l2tp_seq_check_rx_window(struct l2tp_session *session, u32 nr)
{
	u32 nws;

	if (nr >= session->nr)
		nws = nr - session->nr;
	else
		nws = (session->nr_max + 1) - (session->nr - nr);

	return nws < session->nr_window_size;
}

 
static int l2tp_recv_data_seq(struct l2tp_session *session, struct sk_buff *skb)
{
	struct l2tp_skb_cb *cb = L2TP_SKB_CB(skb);

	if (!l2tp_seq_check_rx_window(session, cb->ns)) {
		 
		trace_session_pkt_outside_rx_window(session, cb->ns);
		goto discard;
	}

	if (session->reorder_timeout != 0) {
		 
		l2tp_recv_queue_skb(session, skb);
		goto out;
	}

	 
	if (cb->ns == session->nr) {
		skb_queue_tail(&session->reorder_q, skb);
	} else {
		u32 nr_oos = cb->ns;
		u32 nr_next = (session->nr_oos + 1) & session->nr_max;

		if (nr_oos == nr_next)
			session->nr_oos_count++;
		else
			session->nr_oos_count = 0;

		session->nr_oos = nr_oos;
		if (session->nr_oos_count > session->nr_oos_count_max) {
			session->reorder_skip = 1;
		}
		if (!session->reorder_skip) {
			atomic_long_inc(&session->stats.rx_seq_discards);
			trace_session_pkt_oos(session, cb->ns);
			goto discard;
		}
		skb_queue_tail(&session->reorder_q, skb);
	}

out:
	return 0;

discard:
	return 1;
}

 
void l2tp_recv_common(struct l2tp_session *session, struct sk_buff *skb,
		      unsigned char *ptr, unsigned char *optr, u16 hdrflags,
		      int length)
{
	struct l2tp_tunnel *tunnel = session->tunnel;
	int offset;

	 
	if (session->peer_cookie_len > 0) {
		if (memcmp(ptr, &session->peer_cookie[0], session->peer_cookie_len)) {
			pr_debug_ratelimited("%s: cookie mismatch (%u/%u). Discarding.\n",
					     tunnel->name, tunnel->tunnel_id,
					     session->session_id);
			atomic_long_inc(&session->stats.rx_cookie_discards);
			goto discard;
		}
		ptr += session->peer_cookie_len;
	}

	 
	L2TP_SKB_CB(skb)->has_seq = 0;
	if (tunnel->version == L2TP_HDR_VER_2) {
		if (hdrflags & L2TP_HDRFLAG_S) {
			 
			L2TP_SKB_CB(skb)->ns = ntohs(*(__be16 *)ptr);
			L2TP_SKB_CB(skb)->has_seq = 1;
			ptr += 2;
			 
			ptr += 2;

		}
	} else if (session->l2specific_type == L2TP_L2SPECTYPE_DEFAULT) {
		u32 l2h = ntohl(*(__be32 *)ptr);

		if (l2h & 0x40000000) {
			 
			L2TP_SKB_CB(skb)->ns = l2h & 0x00ffffff;
			L2TP_SKB_CB(skb)->has_seq = 1;
		}
		ptr += 4;
	}

	if (L2TP_SKB_CB(skb)->has_seq) {
		 
		if (!session->lns_mode && !session->send_seq) {
			trace_session_seqnum_lns_enable(session);
			session->send_seq = 1;
			l2tp_session_set_header_len(session, tunnel->version);
		}
	} else {
		 
		if (session->recv_seq) {
			pr_debug_ratelimited("%s: recv data has no seq numbers when required. Discarding.\n",
					     session->name);
			atomic_long_inc(&session->stats.rx_seq_discards);
			goto discard;
		}

		 
		if (!session->lns_mode && session->send_seq) {
			trace_session_seqnum_lns_disable(session);
			session->send_seq = 0;
			l2tp_session_set_header_len(session, tunnel->version);
		} else if (session->send_seq) {
			pr_debug_ratelimited("%s: recv data has no seq numbers when required. Discarding.\n",
					     session->name);
			atomic_long_inc(&session->stats.rx_seq_discards);
			goto discard;
		}
	}

	 
	if (tunnel->version == L2TP_HDR_VER_2) {
		 
		if (hdrflags & L2TP_HDRFLAG_O) {
			offset = ntohs(*(__be16 *)ptr);
			ptr += 2 + offset;
		}
	}

	offset = ptr - optr;
	if (!pskb_may_pull(skb, offset))
		goto discard;

	__skb_pull(skb, offset);

	 
	L2TP_SKB_CB(skb)->length = length;
	L2TP_SKB_CB(skb)->expires = jiffies +
		(session->reorder_timeout ? session->reorder_timeout : HZ);

	 
	if (L2TP_SKB_CB(skb)->has_seq) {
		if (l2tp_recv_data_seq(session, skb))
			goto discard;
	} else {
		 
		skb_queue_tail(&session->reorder_q, skb);
	}

	 
	l2tp_recv_dequeue(session);

	return;

discard:
	atomic_long_inc(&session->stats.rx_errors);
	kfree_skb(skb);
}
EXPORT_SYMBOL_GPL(l2tp_recv_common);

 
static void l2tp_session_queue_purge(struct l2tp_session *session)
{
	struct sk_buff *skb = NULL;

	while ((skb = skb_dequeue(&session->reorder_q))) {
		atomic_long_inc(&session->stats.rx_errors);
		kfree_skb(skb);
	}
}

 
static int l2tp_udp_recv_core(struct l2tp_tunnel *tunnel, struct sk_buff *skb)
{
	struct l2tp_session *session = NULL;
	unsigned char *ptr, *optr;
	u16 hdrflags;
	u32 tunnel_id, session_id;
	u16 version;
	int length;

	 

	 
	__skb_pull(skb, sizeof(struct udphdr));

	 
	if (!pskb_may_pull(skb, L2TP_HDR_SIZE_MAX)) {
		pr_debug_ratelimited("%s: recv short packet (len=%d)\n",
				     tunnel->name, skb->len);
		goto invalid;
	}

	 
	optr = skb->data;
	ptr = skb->data;

	 
	hdrflags = ntohs(*(__be16 *)ptr);

	 
	version = hdrflags & L2TP_HDR_VER_MASK;
	if (version != tunnel->version) {
		pr_debug_ratelimited("%s: recv protocol version mismatch: got %d expected %d\n",
				     tunnel->name, version, tunnel->version);
		goto invalid;
	}

	 
	length = skb->len;

	 
	if (hdrflags & L2TP_HDRFLAG_T)
		goto pass;

	 
	ptr += 2;

	if (tunnel->version == L2TP_HDR_VER_2) {
		 
		if (hdrflags & L2TP_HDRFLAG_L)
			ptr += 2;

		 
		tunnel_id = ntohs(*(__be16 *)ptr);
		ptr += 2;
		session_id = ntohs(*(__be16 *)ptr);
		ptr += 2;
	} else {
		ptr += 2;	 
		tunnel_id = tunnel->tunnel_id;
		session_id = ntohl(*(__be32 *)ptr);
		ptr += 4;
	}

	 
	session = l2tp_tunnel_get_session(tunnel, session_id);
	if (!session || !session->recv_skb) {
		if (session)
			l2tp_session_dec_refcount(session);

		 
		pr_debug_ratelimited("%s: no session found (%u/%u). Passing up.\n",
				     tunnel->name, tunnel_id, session_id);
		goto pass;
	}

	if (tunnel->version == L2TP_HDR_VER_3 &&
	    l2tp_v3_ensure_opt_in_linear(session, skb, &ptr, &optr)) {
		l2tp_session_dec_refcount(session);
		goto invalid;
	}

	l2tp_recv_common(session, skb, ptr, optr, hdrflags, length);
	l2tp_session_dec_refcount(session);

	return 0;

invalid:
	atomic_long_inc(&tunnel->stats.rx_invalid);

pass:
	 
	__skb_push(skb, sizeof(struct udphdr));

	return 1;
}

 
int l2tp_udp_encap_recv(struct sock *sk, struct sk_buff *skb)
{
	struct l2tp_tunnel *tunnel;

	 
	tunnel = rcu_dereference_sk_user_data(sk);
	if (!tunnel)
		goto pass_up;
	if (WARN_ON(tunnel->magic != L2TP_TUNNEL_MAGIC))
		goto pass_up;

	if (l2tp_udp_recv_core(tunnel, skb))
		goto pass_up;

	return 0;

pass_up:
	return 1;
}
EXPORT_SYMBOL_GPL(l2tp_udp_encap_recv);

 

 
static int l2tp_build_l2tpv2_header(struct l2tp_session *session, void *buf)
{
	struct l2tp_tunnel *tunnel = session->tunnel;
	__be16 *bufp = buf;
	__be16 *optr = buf;
	u16 flags = L2TP_HDR_VER_2;
	u32 tunnel_id = tunnel->peer_tunnel_id;
	u32 session_id = session->peer_session_id;

	if (session->send_seq)
		flags |= L2TP_HDRFLAG_S;

	 
	*bufp++ = htons(flags);
	*bufp++ = htons(tunnel_id);
	*bufp++ = htons(session_id);
	if (session->send_seq) {
		*bufp++ = htons(session->ns);
		*bufp++ = 0;
		session->ns++;
		session->ns &= 0xffff;
		trace_session_seqnum_update(session);
	}

	return bufp - optr;
}

static int l2tp_build_l2tpv3_header(struct l2tp_session *session, void *buf)
{
	struct l2tp_tunnel *tunnel = session->tunnel;
	char *bufp = buf;
	char *optr = bufp;

	 
	if (tunnel->encap == L2TP_ENCAPTYPE_UDP) {
		u16 flags = L2TP_HDR_VER_3;
		*((__be16 *)bufp) = htons(flags);
		bufp += 2;
		*((__be16 *)bufp) = 0;
		bufp += 2;
	}

	*((__be32 *)bufp) = htonl(session->peer_session_id);
	bufp += 4;
	if (session->cookie_len) {
		memcpy(bufp, &session->cookie[0], session->cookie_len);
		bufp += session->cookie_len;
	}
	if (session->l2specific_type == L2TP_L2SPECTYPE_DEFAULT) {
		u32 l2h = 0;

		if (session->send_seq) {
			l2h = 0x40000000 | session->ns;
			session->ns++;
			session->ns &= 0xffffff;
			trace_session_seqnum_update(session);
		}

		*((__be32 *)bufp) = htonl(l2h);
		bufp += 4;
	}

	return bufp - optr;
}

 
static int l2tp_xmit_queue(struct l2tp_tunnel *tunnel, struct sk_buff *skb, struct flowi *fl)
{
	int err;

	skb->ignore_df = 1;
	skb_dst_drop(skb);
#if IS_ENABLED(CONFIG_IPV6)
	if (l2tp_sk_is_v6(tunnel->sock))
		err = inet6_csk_xmit(tunnel->sock, skb, NULL);
	else
#endif
		err = ip_queue_xmit(tunnel->sock, skb, fl);

	return err >= 0 ? NET_XMIT_SUCCESS : NET_XMIT_DROP;
}

static int l2tp_xmit_core(struct l2tp_session *session, struct sk_buff *skb, unsigned int *len)
{
	struct l2tp_tunnel *tunnel = session->tunnel;
	unsigned int data_len = skb->len;
	struct sock *sk = tunnel->sock;
	int headroom, uhlen, udp_len;
	int ret = NET_XMIT_SUCCESS;
	struct inet_sock *inet;
	struct udphdr *uh;

	 
	uhlen = (tunnel->encap == L2TP_ENCAPTYPE_UDP) ? sizeof(*uh) : 0;
	headroom = NET_SKB_PAD + sizeof(struct iphdr) + uhlen + session->hdr_len;
	if (skb_cow_head(skb, headroom)) {
		kfree_skb(skb);
		return NET_XMIT_DROP;
	}

	 
	if (tunnel->version == L2TP_HDR_VER_2)
		l2tp_build_l2tpv2_header(session, __skb_push(skb, session->hdr_len));
	else
		l2tp_build_l2tpv3_header(session, __skb_push(skb, session->hdr_len));

	 
	memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
	IPCB(skb)->flags &= ~(IPSKB_XFRM_TUNNEL_SIZE | IPSKB_XFRM_TRANSFORMED | IPSKB_REROUTED);
	nf_reset_ct(skb);

	bh_lock_sock_nested(sk);
	if (sock_owned_by_user(sk)) {
		kfree_skb(skb);
		ret = NET_XMIT_DROP;
		goto out_unlock;
	}

	 
	if (tunnel->fd >= 0 && sk->sk_state != TCP_ESTABLISHED) {
		kfree_skb(skb);
		ret = NET_XMIT_DROP;
		goto out_unlock;
	}

	 
	*len = skb->len;

	inet = inet_sk(sk);
	switch (tunnel->encap) {
	case L2TP_ENCAPTYPE_UDP:
		 
		__skb_push(skb, sizeof(*uh));
		skb_reset_transport_header(skb);
		uh = udp_hdr(skb);
		uh->source = inet->inet_sport;
		uh->dest = inet->inet_dport;
		udp_len = uhlen + session->hdr_len + data_len;
		uh->len = htons(udp_len);

		 
#if IS_ENABLED(CONFIG_IPV6)
		if (l2tp_sk_is_v6(sk))
			udp6_set_csum(udp_get_no_check6_tx(sk),
				      skb, &inet6_sk(sk)->saddr,
				      &sk->sk_v6_daddr, udp_len);
		else
#endif
			udp_set_csum(sk->sk_no_check_tx, skb, inet->inet_saddr,
				     inet->inet_daddr, udp_len);
		break;

	case L2TP_ENCAPTYPE_IP:
		break;
	}

	ret = l2tp_xmit_queue(tunnel, skb, &inet->cork.fl);

out_unlock:
	bh_unlock_sock(sk);

	return ret;
}

 
int l2tp_xmit_skb(struct l2tp_session *session, struct sk_buff *skb)
{
	unsigned int len = 0;
	int ret;

	ret = l2tp_xmit_core(session, skb, &len);
	if (ret == NET_XMIT_SUCCESS) {
		atomic_long_inc(&session->tunnel->stats.tx_packets);
		atomic_long_add(len, &session->tunnel->stats.tx_bytes);
		atomic_long_inc(&session->stats.tx_packets);
		atomic_long_add(len, &session->stats.tx_bytes);
	} else {
		atomic_long_inc(&session->tunnel->stats.tx_errors);
		atomic_long_inc(&session->stats.tx_errors);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(l2tp_xmit_skb);

 

 
static void l2tp_tunnel_destruct(struct sock *sk)
{
	struct l2tp_tunnel *tunnel = l2tp_sk_to_tunnel(sk);

	if (!tunnel)
		goto end;

	 
	switch (tunnel->encap) {
	case L2TP_ENCAPTYPE_UDP:
		 
		WRITE_ONCE(udp_sk(sk)->encap_type, 0);
		udp_sk(sk)->encap_rcv = NULL;
		udp_sk(sk)->encap_destroy = NULL;
		break;
	case L2TP_ENCAPTYPE_IP:
		break;
	}

	 
	write_lock_bh(&sk->sk_callback_lock);
	sk->sk_destruct = tunnel->old_sk_destruct;
	sk->sk_user_data = NULL;
	write_unlock_bh(&sk->sk_callback_lock);

	 
	if (sk->sk_destruct)
		(*sk->sk_destruct)(sk);

	kfree_rcu(tunnel, rcu);
end:
	return;
}

 
static void l2tp_session_unhash(struct l2tp_session *session)
{
	struct l2tp_tunnel *tunnel = session->tunnel;

	 
	if (tunnel) {
		 
		spin_lock_bh(&tunnel->hlist_lock);
		hlist_del_init_rcu(&session->hlist);
		spin_unlock_bh(&tunnel->hlist_lock);

		 
		if (tunnel->version != L2TP_HDR_VER_2) {
			struct l2tp_net *pn = l2tp_pernet(tunnel->l2tp_net);

			spin_lock_bh(&pn->l2tp_session_hlist_lock);
			hlist_del_init_rcu(&session->global_hlist);
			spin_unlock_bh(&pn->l2tp_session_hlist_lock);
		}

		synchronize_rcu();
	}
}

 
static void l2tp_tunnel_closeall(struct l2tp_tunnel *tunnel)
{
	struct l2tp_session *session;
	int hash;

	spin_lock_bh(&tunnel->hlist_lock);
	tunnel->acpt_newsess = false;
	for (hash = 0; hash < L2TP_HASH_SIZE; hash++) {
again:
		hlist_for_each_entry_rcu(session, &tunnel->session_hlist[hash], hlist) {
			hlist_del_init_rcu(&session->hlist);

			spin_unlock_bh(&tunnel->hlist_lock);
			l2tp_session_delete(session);
			spin_lock_bh(&tunnel->hlist_lock);

			 
			goto again;
		}
	}
	spin_unlock_bh(&tunnel->hlist_lock);
}

 
static void l2tp_udp_encap_destroy(struct sock *sk)
{
	struct l2tp_tunnel *tunnel = l2tp_sk_to_tunnel(sk);

	if (tunnel)
		l2tp_tunnel_delete(tunnel);
}

static void l2tp_tunnel_remove(struct net *net, struct l2tp_tunnel *tunnel)
{
	struct l2tp_net *pn = l2tp_pernet(net);

	spin_lock_bh(&pn->l2tp_tunnel_idr_lock);
	idr_remove(&pn->l2tp_tunnel_idr, tunnel->tunnel_id);
	spin_unlock_bh(&pn->l2tp_tunnel_idr_lock);
}

 
static void l2tp_tunnel_del_work(struct work_struct *work)
{
	struct l2tp_tunnel *tunnel = container_of(work, struct l2tp_tunnel,
						  del_work);
	struct sock *sk = tunnel->sock;
	struct socket *sock = sk->sk_socket;

	l2tp_tunnel_closeall(tunnel);

	 
	if (tunnel->fd < 0) {
		if (sock) {
			kernel_sock_shutdown(sock, SHUT_RDWR);
			sock_release(sock);
		}
	}

	l2tp_tunnel_remove(tunnel->l2tp_net, tunnel);
	 
	l2tp_tunnel_dec_refcount(tunnel);

	 
	l2tp_tunnel_dec_refcount(tunnel);
}

 
static int l2tp_tunnel_sock_create(struct net *net,
				   u32 tunnel_id,
				   u32 peer_tunnel_id,
				   struct l2tp_tunnel_cfg *cfg,
				   struct socket **sockp)
{
	int err = -EINVAL;
	struct socket *sock = NULL;
	struct udp_port_cfg udp_conf;

	switch (cfg->encap) {
	case L2TP_ENCAPTYPE_UDP:
		memset(&udp_conf, 0, sizeof(udp_conf));

#if IS_ENABLED(CONFIG_IPV6)
		if (cfg->local_ip6 && cfg->peer_ip6) {
			udp_conf.family = AF_INET6;
			memcpy(&udp_conf.local_ip6, cfg->local_ip6,
			       sizeof(udp_conf.local_ip6));
			memcpy(&udp_conf.peer_ip6, cfg->peer_ip6,
			       sizeof(udp_conf.peer_ip6));
			udp_conf.use_udp6_tx_checksums =
			  !cfg->udp6_zero_tx_checksums;
			udp_conf.use_udp6_rx_checksums =
			  !cfg->udp6_zero_rx_checksums;
		} else
#endif
		{
			udp_conf.family = AF_INET;
			udp_conf.local_ip = cfg->local_ip;
			udp_conf.peer_ip = cfg->peer_ip;
			udp_conf.use_udp_checksums = cfg->use_udp_checksums;
		}

		udp_conf.local_udp_port = htons(cfg->local_udp_port);
		udp_conf.peer_udp_port = htons(cfg->peer_udp_port);

		err = udp_sock_create(net, &udp_conf, &sock);
		if (err < 0)
			goto out;

		break;

	case L2TP_ENCAPTYPE_IP:
#if IS_ENABLED(CONFIG_IPV6)
		if (cfg->local_ip6 && cfg->peer_ip6) {
			struct sockaddr_l2tpip6 ip6_addr = {0};

			err = sock_create_kern(net, AF_INET6, SOCK_DGRAM,
					       IPPROTO_L2TP, &sock);
			if (err < 0)
				goto out;

			ip6_addr.l2tp_family = AF_INET6;
			memcpy(&ip6_addr.l2tp_addr, cfg->local_ip6,
			       sizeof(ip6_addr.l2tp_addr));
			ip6_addr.l2tp_conn_id = tunnel_id;
			err = kernel_bind(sock, (struct sockaddr *)&ip6_addr,
					  sizeof(ip6_addr));
			if (err < 0)
				goto out;

			ip6_addr.l2tp_family = AF_INET6;
			memcpy(&ip6_addr.l2tp_addr, cfg->peer_ip6,
			       sizeof(ip6_addr.l2tp_addr));
			ip6_addr.l2tp_conn_id = peer_tunnel_id;
			err = kernel_connect(sock,
					     (struct sockaddr *)&ip6_addr,
					     sizeof(ip6_addr), 0);
			if (err < 0)
				goto out;
		} else
#endif
		{
			struct sockaddr_l2tpip ip_addr = {0};

			err = sock_create_kern(net, AF_INET, SOCK_DGRAM,
					       IPPROTO_L2TP, &sock);
			if (err < 0)
				goto out;

			ip_addr.l2tp_family = AF_INET;
			ip_addr.l2tp_addr = cfg->local_ip;
			ip_addr.l2tp_conn_id = tunnel_id;
			err = kernel_bind(sock, (struct sockaddr *)&ip_addr,
					  sizeof(ip_addr));
			if (err < 0)
				goto out;

			ip_addr.l2tp_family = AF_INET;
			ip_addr.l2tp_addr = cfg->peer_ip;
			ip_addr.l2tp_conn_id = peer_tunnel_id;
			err = kernel_connect(sock, (struct sockaddr *)&ip_addr,
					     sizeof(ip_addr), 0);
			if (err < 0)
				goto out;
		}
		break;

	default:
		goto out;
	}

out:
	*sockp = sock;
	if (err < 0 && sock) {
		kernel_sock_shutdown(sock, SHUT_RDWR);
		sock_release(sock);
		*sockp = NULL;
	}

	return err;
}

int l2tp_tunnel_create(int fd, int version, u32 tunnel_id, u32 peer_tunnel_id,
		       struct l2tp_tunnel_cfg *cfg, struct l2tp_tunnel **tunnelp)
{
	struct l2tp_tunnel *tunnel = NULL;
	int err;
	enum l2tp_encap_type encap = L2TP_ENCAPTYPE_UDP;

	if (cfg)
		encap = cfg->encap;

	tunnel = kzalloc(sizeof(*tunnel), GFP_KERNEL);
	if (!tunnel) {
		err = -ENOMEM;
		goto err;
	}

	tunnel->version = version;
	tunnel->tunnel_id = tunnel_id;
	tunnel->peer_tunnel_id = peer_tunnel_id;

	tunnel->magic = L2TP_TUNNEL_MAGIC;
	sprintf(&tunnel->name[0], "tunl %u", tunnel_id);
	spin_lock_init(&tunnel->hlist_lock);
	tunnel->acpt_newsess = true;

	tunnel->encap = encap;

	refcount_set(&tunnel->ref_count, 1);
	tunnel->fd = fd;

	 
	INIT_WORK(&tunnel->del_work, l2tp_tunnel_del_work);

	INIT_LIST_HEAD(&tunnel->list);

	err = 0;
err:
	if (tunnelp)
		*tunnelp = tunnel;

	return err;
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_create);

static int l2tp_validate_socket(const struct sock *sk, const struct net *net,
				enum l2tp_encap_type encap)
{
	if (!net_eq(sock_net(sk), net))
		return -EINVAL;

	if (sk->sk_type != SOCK_DGRAM)
		return -EPROTONOSUPPORT;

	if (sk->sk_family != PF_INET && sk->sk_family != PF_INET6)
		return -EPROTONOSUPPORT;

	if ((encap == L2TP_ENCAPTYPE_UDP && sk->sk_protocol != IPPROTO_UDP) ||
	    (encap == L2TP_ENCAPTYPE_IP && sk->sk_protocol != IPPROTO_L2TP))
		return -EPROTONOSUPPORT;

	if (sk->sk_user_data)
		return -EBUSY;

	return 0;
}

int l2tp_tunnel_register(struct l2tp_tunnel *tunnel, struct net *net,
			 struct l2tp_tunnel_cfg *cfg)
{
	struct l2tp_net *pn = l2tp_pernet(net);
	u32 tunnel_id = tunnel->tunnel_id;
	struct socket *sock;
	struct sock *sk;
	int ret;

	spin_lock_bh(&pn->l2tp_tunnel_idr_lock);
	ret = idr_alloc_u32(&pn->l2tp_tunnel_idr, NULL, &tunnel_id, tunnel_id,
			    GFP_ATOMIC);
	spin_unlock_bh(&pn->l2tp_tunnel_idr_lock);
	if (ret)
		return ret == -ENOSPC ? -EEXIST : ret;

	if (tunnel->fd < 0) {
		ret = l2tp_tunnel_sock_create(net, tunnel->tunnel_id,
					      tunnel->peer_tunnel_id, cfg,
					      &sock);
		if (ret < 0)
			goto err;
	} else {
		sock = sockfd_lookup(tunnel->fd, &ret);
		if (!sock)
			goto err;
	}

	sk = sock->sk;
	lock_sock(sk);
	write_lock_bh(&sk->sk_callback_lock);
	ret = l2tp_validate_socket(sk, net, tunnel->encap);
	if (ret < 0)
		goto err_inval_sock;
	rcu_assign_sk_user_data(sk, tunnel);
	write_unlock_bh(&sk->sk_callback_lock);

	if (tunnel->encap == L2TP_ENCAPTYPE_UDP) {
		struct udp_tunnel_sock_cfg udp_cfg = {
			.sk_user_data = tunnel,
			.encap_type = UDP_ENCAP_L2TPINUDP,
			.encap_rcv = l2tp_udp_encap_recv,
			.encap_destroy = l2tp_udp_encap_destroy,
		};

		setup_udp_tunnel_sock(net, sock, &udp_cfg);
	}

	tunnel->old_sk_destruct = sk->sk_destruct;
	sk->sk_destruct = &l2tp_tunnel_destruct;
	sk->sk_allocation = GFP_ATOMIC;
	release_sock(sk);

	sock_hold(sk);
	tunnel->sock = sk;
	tunnel->l2tp_net = net;

	spin_lock_bh(&pn->l2tp_tunnel_idr_lock);
	idr_replace(&pn->l2tp_tunnel_idr, tunnel, tunnel->tunnel_id);
	spin_unlock_bh(&pn->l2tp_tunnel_idr_lock);

	trace_register_tunnel(tunnel);

	if (tunnel->fd >= 0)
		sockfd_put(sock);

	return 0;

err_inval_sock:
	write_unlock_bh(&sk->sk_callback_lock);
	release_sock(sk);

	if (tunnel->fd < 0)
		sock_release(sock);
	else
		sockfd_put(sock);
err:
	l2tp_tunnel_remove(net, tunnel);
	return ret;
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_register);

 
void l2tp_tunnel_delete(struct l2tp_tunnel *tunnel)
{
	if (!test_and_set_bit(0, &tunnel->dead)) {
		trace_delete_tunnel(tunnel);
		l2tp_tunnel_inc_refcount(tunnel);
		queue_work(l2tp_wq, &tunnel->del_work);
	}
}
EXPORT_SYMBOL_GPL(l2tp_tunnel_delete);

void l2tp_session_delete(struct l2tp_session *session)
{
	if (test_and_set_bit(0, &session->dead))
		return;

	trace_delete_session(session);
	l2tp_session_unhash(session);
	l2tp_session_queue_purge(session);
	if (session->session_close)
		(*session->session_close)(session);

	l2tp_session_dec_refcount(session);
}
EXPORT_SYMBOL_GPL(l2tp_session_delete);

 
void l2tp_session_set_header_len(struct l2tp_session *session, int version)
{
	if (version == L2TP_HDR_VER_2) {
		session->hdr_len = 6;
		if (session->send_seq)
			session->hdr_len += 4;
	} else {
		session->hdr_len = 4 + session->cookie_len;
		session->hdr_len += l2tp_get_l2specific_len(session);
		if (session->tunnel->encap == L2TP_ENCAPTYPE_UDP)
			session->hdr_len += 4;
	}
}
EXPORT_SYMBOL_GPL(l2tp_session_set_header_len);

struct l2tp_session *l2tp_session_create(int priv_size, struct l2tp_tunnel *tunnel, u32 session_id,
					 u32 peer_session_id, struct l2tp_session_cfg *cfg)
{
	struct l2tp_session *session;

	session = kzalloc(sizeof(*session) + priv_size, GFP_KERNEL);
	if (session) {
		session->magic = L2TP_SESSION_MAGIC;
		session->tunnel = tunnel;

		session->session_id = session_id;
		session->peer_session_id = peer_session_id;
		session->nr = 0;
		if (tunnel->version == L2TP_HDR_VER_2)
			session->nr_max = 0xffff;
		else
			session->nr_max = 0xffffff;
		session->nr_window_size = session->nr_max / 2;
		session->nr_oos_count_max = 4;

		 
		session->reorder_skip = 1;

		sprintf(&session->name[0], "sess %u/%u",
			tunnel->tunnel_id, session->session_id);

		skb_queue_head_init(&session->reorder_q);

		INIT_HLIST_NODE(&session->hlist);
		INIT_HLIST_NODE(&session->global_hlist);

		if (cfg) {
			session->pwtype = cfg->pw_type;
			session->send_seq = cfg->send_seq;
			session->recv_seq = cfg->recv_seq;
			session->lns_mode = cfg->lns_mode;
			session->reorder_timeout = cfg->reorder_timeout;
			session->l2specific_type = cfg->l2specific_type;
			session->cookie_len = cfg->cookie_len;
			memcpy(&session->cookie[0], &cfg->cookie[0], cfg->cookie_len);
			session->peer_cookie_len = cfg->peer_cookie_len;
			memcpy(&session->peer_cookie[0], &cfg->peer_cookie[0], cfg->peer_cookie_len);
		}

		l2tp_session_set_header_len(session, tunnel->version);

		refcount_set(&session->ref_count, 1);

		return session;
	}

	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL_GPL(l2tp_session_create);

 

static __net_init int l2tp_init_net(struct net *net)
{
	struct l2tp_net *pn = net_generic(net, l2tp_net_id);
	int hash;

	idr_init(&pn->l2tp_tunnel_idr);
	spin_lock_init(&pn->l2tp_tunnel_idr_lock);

	for (hash = 0; hash < L2TP_HASH_SIZE_2; hash++)
		INIT_HLIST_HEAD(&pn->l2tp_session_hlist[hash]);

	spin_lock_init(&pn->l2tp_session_hlist_lock);

	return 0;
}

static __net_exit void l2tp_exit_net(struct net *net)
{
	struct l2tp_net *pn = l2tp_pernet(net);
	struct l2tp_tunnel *tunnel = NULL;
	unsigned long tunnel_id, tmp;
	int hash;

	rcu_read_lock_bh();
	idr_for_each_entry_ul(&pn->l2tp_tunnel_idr, tunnel, tmp, tunnel_id) {
		if (tunnel)
			l2tp_tunnel_delete(tunnel);
	}
	rcu_read_unlock_bh();

	if (l2tp_wq)
		flush_workqueue(l2tp_wq);
	rcu_barrier();

	for (hash = 0; hash < L2TP_HASH_SIZE_2; hash++)
		WARN_ON_ONCE(!hlist_empty(&pn->l2tp_session_hlist[hash]));
	idr_destroy(&pn->l2tp_tunnel_idr);
}

static struct pernet_operations l2tp_net_ops = {
	.init = l2tp_init_net,
	.exit = l2tp_exit_net,
	.id   = &l2tp_net_id,
	.size = sizeof(struct l2tp_net),
};

static int __init l2tp_init(void)
{
	int rc = 0;

	rc = register_pernet_device(&l2tp_net_ops);
	if (rc)
		goto out;

	l2tp_wq = alloc_workqueue("l2tp", WQ_UNBOUND, 0);
	if (!l2tp_wq) {
		pr_err("alloc_workqueue failed\n");
		unregister_pernet_device(&l2tp_net_ops);
		rc = -ENOMEM;
		goto out;
	}

	pr_info("L2TP core driver, %s\n", L2TP_DRV_VERSION);

out:
	return rc;
}

static void __exit l2tp_exit(void)
{
	unregister_pernet_device(&l2tp_net_ops);
	if (l2tp_wq) {
		destroy_workqueue(l2tp_wq);
		l2tp_wq = NULL;
	}
}

module_init(l2tp_init);
module_exit(l2tp_exit);

MODULE_AUTHOR("James Chapman <jchapman@katalix.com>");
MODULE_DESCRIPTION("L2TP core");
MODULE_LICENSE("GPL");
MODULE_VERSION(L2TP_DRV_VERSION);
