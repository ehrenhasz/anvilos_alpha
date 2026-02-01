 

#include "core.h"
#include "link.h"
#include "node.h"
#include "name_distr.h"
#include "socket.h"
#include "bcast.h"
#include "monitor.h"
#include "discover.h"
#include "netlink.h"
#include "trace.h"
#include "crypto.h"

#define INVALID_NODE_SIG	0x10000
#define NODE_CLEANUP_AFTER	300000

 
enum {
	TIPC_NOTIFY_NODE_DOWN		= (1 << 3),
	TIPC_NOTIFY_NODE_UP		= (1 << 4),
	TIPC_NOTIFY_LINK_UP		= (1 << 6),
	TIPC_NOTIFY_LINK_DOWN		= (1 << 7)
};

struct tipc_link_entry {
	struct tipc_link *link;
	spinlock_t lock;  
	u32 mtu;
	struct sk_buff_head inputq;
	struct tipc_media_addr maddr;
};

struct tipc_bclink_entry {
	struct tipc_link *link;
	struct sk_buff_head inputq1;
	struct sk_buff_head arrvq;
	struct sk_buff_head inputq2;
	struct sk_buff_head namedq;
	u16 named_rcv_nxt;
	bool named_open;
};

 
struct tipc_node {
	u32 addr;
	struct kref kref;
	rwlock_t lock;
	struct net *net;
	struct hlist_node hash;
	int active_links[2];
	struct tipc_link_entry links[MAX_BEARERS];
	struct tipc_bclink_entry bc_entry;
	int action_flags;
	struct list_head list;
	int state;
	bool preliminary;
	bool failover_sent;
	u16 sync_point;
	int link_cnt;
	u16 working_links;
	u16 capabilities;
	u32 signature;
	u32 link_id;
	u8 peer_id[16];
	char peer_id_string[NODE_ID_STR_LEN];
	struct list_head publ_list;
	struct list_head conn_sks;
	unsigned long keepalive_intv;
	struct timer_list timer;
	struct rcu_head rcu;
	unsigned long delete_at;
	struct net *peer_net;
	u32 peer_hash_mix;
#ifdef CONFIG_TIPC_CRYPTO
	struct tipc_crypto *crypto_rx;
#endif
};

 
enum {
	SELF_DOWN_PEER_DOWN    = 0xdd,
	SELF_UP_PEER_UP        = 0xaa,
	SELF_DOWN_PEER_LEAVING = 0xd1,
	SELF_UP_PEER_COMING    = 0xac,
	SELF_COMING_PEER_UP    = 0xca,
	SELF_LEAVING_PEER_DOWN = 0x1d,
	NODE_FAILINGOVER       = 0xf0,
	NODE_SYNCHING          = 0xcc
};

enum {
	SELF_ESTABL_CONTACT_EVT = 0xece,
	SELF_LOST_CONTACT_EVT   = 0x1ce,
	PEER_ESTABL_CONTACT_EVT = 0x9ece,
	PEER_LOST_CONTACT_EVT   = 0x91ce,
	NODE_FAILOVER_BEGIN_EVT = 0xfbe,
	NODE_FAILOVER_END_EVT   = 0xfee,
	NODE_SYNCH_BEGIN_EVT    = 0xcbe,
	NODE_SYNCH_END_EVT      = 0xcee
};

static void __tipc_node_link_down(struct tipc_node *n, int *bearer_id,
				  struct sk_buff_head *xmitq,
				  struct tipc_media_addr **maddr);
static void tipc_node_link_down(struct tipc_node *n, int bearer_id,
				bool delete);
static void node_lost_contact(struct tipc_node *n, struct sk_buff_head *inputq);
static void tipc_node_delete(struct tipc_node *node);
static void tipc_node_timeout(struct timer_list *t);
static void tipc_node_fsm_evt(struct tipc_node *n, int evt);
static struct tipc_node *tipc_node_find(struct net *net, u32 addr);
static struct tipc_node *tipc_node_find_by_id(struct net *net, u8 *id);
static bool node_is_up(struct tipc_node *n);
static void tipc_node_delete_from_list(struct tipc_node *node);

struct tipc_sock_conn {
	u32 port;
	u32 peer_port;
	u32 peer_node;
	struct list_head list;
};

static struct tipc_link *node_active_link(struct tipc_node *n, int sel)
{
	int bearer_id = n->active_links[sel & 1];

	if (unlikely(bearer_id == INVALID_BEARER_ID))
		return NULL;

	return n->links[bearer_id].link;
}

int tipc_node_get_mtu(struct net *net, u32 addr, u32 sel, bool connected)
{
	struct tipc_node *n;
	int bearer_id;
	unsigned int mtu = MAX_MSG_SIZE;

	n = tipc_node_find(net, addr);
	if (unlikely(!n))
		return mtu;

	 
	if (n->peer_net && connected) {
		tipc_node_put(n);
		return mtu;
	}

	bearer_id = n->active_links[sel & 1];
	if (likely(bearer_id != INVALID_BEARER_ID))
		mtu = n->links[bearer_id].mtu;
	tipc_node_put(n);
	return mtu;
}

bool tipc_node_get_id(struct net *net, u32 addr, u8 *id)
{
	u8 *own_id = tipc_own_id(net);
	struct tipc_node *n;

	if (!own_id)
		return true;

	if (addr == tipc_own_addr(net)) {
		memcpy(id, own_id, TIPC_NODEID_LEN);
		return true;
	}
	n = tipc_node_find(net, addr);
	if (!n)
		return false;

	memcpy(id, &n->peer_id, TIPC_NODEID_LEN);
	tipc_node_put(n);
	return true;
}

u16 tipc_node_get_capabilities(struct net *net, u32 addr)
{
	struct tipc_node *n;
	u16 caps;

	n = tipc_node_find(net, addr);
	if (unlikely(!n))
		return TIPC_NODE_CAPABILITIES;
	caps = n->capabilities;
	tipc_node_put(n);
	return caps;
}

u32 tipc_node_get_addr(struct tipc_node *node)
{
	return (node) ? node->addr : 0;
}

char *tipc_node_get_id_str(struct tipc_node *node)
{
	return node->peer_id_string;
}

#ifdef CONFIG_TIPC_CRYPTO
 
struct tipc_crypto *tipc_node_crypto_rx(struct tipc_node *__n)
{
	return (__n) ? __n->crypto_rx : NULL;
}

struct tipc_crypto *tipc_node_crypto_rx_by_list(struct list_head *pos)
{
	return container_of(pos, struct tipc_node, list)->crypto_rx;
}

struct tipc_crypto *tipc_node_crypto_rx_by_addr(struct net *net, u32 addr)
{
	struct tipc_node *n;

	n = tipc_node_find(net, addr);
	return (n) ? n->crypto_rx : NULL;
}
#endif

static void tipc_node_free(struct rcu_head *rp)
{
	struct tipc_node *n = container_of(rp, struct tipc_node, rcu);

#ifdef CONFIG_TIPC_CRYPTO
	tipc_crypto_stop(&n->crypto_rx);
#endif
	kfree(n);
}

static void tipc_node_kref_release(struct kref *kref)
{
	struct tipc_node *n = container_of(kref, struct tipc_node, kref);

	kfree(n->bc_entry.link);
	call_rcu(&n->rcu, tipc_node_free);
}

void tipc_node_put(struct tipc_node *node)
{
	kref_put(&node->kref, tipc_node_kref_release);
}

void tipc_node_get(struct tipc_node *node)
{
	kref_get(&node->kref);
}

 
static struct tipc_node *tipc_node_find(struct net *net, u32 addr)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_node *node;
	unsigned int thash = tipc_hashfn(addr);

	rcu_read_lock();
	hlist_for_each_entry_rcu(node, &tn->node_htable[thash], hash) {
		if (node->addr != addr || node->preliminary)
			continue;
		if (!kref_get_unless_zero(&node->kref))
			node = NULL;
		break;
	}
	rcu_read_unlock();
	return node;
}

 
static struct tipc_node *tipc_node_find_by_id(struct net *net, u8 *id)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_node *n;
	bool found = false;

	rcu_read_lock();
	list_for_each_entry_rcu(n, &tn->node_list, list) {
		read_lock_bh(&n->lock);
		if (!memcmp(id, n->peer_id, 16) &&
		    kref_get_unless_zero(&n->kref))
			found = true;
		read_unlock_bh(&n->lock);
		if (found)
			break;
	}
	rcu_read_unlock();
	return found ? n : NULL;
}

static void tipc_node_read_lock(struct tipc_node *n)
	__acquires(n->lock)
{
	read_lock_bh(&n->lock);
}

static void tipc_node_read_unlock(struct tipc_node *n)
	__releases(n->lock)
{
	read_unlock_bh(&n->lock);
}

static void tipc_node_write_lock(struct tipc_node *n)
	__acquires(n->lock)
{
	write_lock_bh(&n->lock);
}

static void tipc_node_write_unlock_fast(struct tipc_node *n)
	__releases(n->lock)
{
	write_unlock_bh(&n->lock);
}

static void tipc_node_write_unlock(struct tipc_node *n)
	__releases(n->lock)
{
	struct tipc_socket_addr sk;
	struct net *net = n->net;
	u32 flags = n->action_flags;
	struct list_head *publ_list;
	struct tipc_uaddr ua;
	u32 bearer_id, node;

	if (likely(!flags)) {
		write_unlock_bh(&n->lock);
		return;
	}

	tipc_uaddr(&ua, TIPC_SERVICE_RANGE, TIPC_NODE_SCOPE,
		   TIPC_LINK_STATE, n->addr, n->addr);
	sk.ref = n->link_id;
	sk.node = tipc_own_addr(net);
	node = n->addr;
	bearer_id = n->link_id & 0xffff;
	publ_list = &n->publ_list;

	n->action_flags &= ~(TIPC_NOTIFY_NODE_DOWN | TIPC_NOTIFY_NODE_UP |
			     TIPC_NOTIFY_LINK_DOWN | TIPC_NOTIFY_LINK_UP);

	write_unlock_bh(&n->lock);

	if (flags & TIPC_NOTIFY_NODE_DOWN)
		tipc_publ_notify(net, publ_list, node, n->capabilities);

	if (flags & TIPC_NOTIFY_NODE_UP)
		tipc_named_node_up(net, node, n->capabilities);

	if (flags & TIPC_NOTIFY_LINK_UP) {
		tipc_mon_peer_up(net, node, bearer_id);
		tipc_nametbl_publish(net, &ua, &sk, sk.ref);
	}
	if (flags & TIPC_NOTIFY_LINK_DOWN) {
		tipc_mon_peer_down(net, node, bearer_id);
		tipc_nametbl_withdraw(net, &ua, &sk, sk.ref);
	}
}

static void tipc_node_assign_peer_net(struct tipc_node *n, u32 hash_mixes)
{
	int net_id = tipc_netid(n->net);
	struct tipc_net *tn_peer;
	struct net *tmp;
	u32 hash_chk;

	if (n->peer_net)
		return;

	for_each_net_rcu(tmp) {
		tn_peer = tipc_net(tmp);
		if (!tn_peer)
			continue;
		 
		if (tn_peer->net_id != net_id)
			continue;
		if (memcmp(n->peer_id, tn_peer->node_id, NODE_ID_LEN))
			continue;
		hash_chk = tipc_net_hash_mixes(tmp, tn_peer->random);
		if (hash_mixes ^ hash_chk)
			continue;
		n->peer_net = tmp;
		n->peer_hash_mix = hash_mixes;
		break;
	}
}

struct tipc_node *tipc_node_create(struct net *net, u32 addr, u8 *peer_id,
				   u16 capabilities, u32 hash_mixes,
				   bool preliminary)
{
	struct tipc_net *tn = net_generic(net, tipc_net_id);
	struct tipc_link *l, *snd_l = tipc_bc_sndlink(net);
	struct tipc_node *n, *temp_node;
	unsigned long intv;
	int bearer_id;
	int i;

	spin_lock_bh(&tn->node_list_lock);
	n = tipc_node_find(net, addr) ?:
		tipc_node_find_by_id(net, peer_id);
	if (n) {
		if (!n->preliminary)
			goto update;
		if (preliminary)
			goto exit;
		 
		tipc_node_write_lock(n);
		if (!tipc_link_bc_create(net, tipc_own_addr(net), addr, peer_id, U16_MAX,
					 tipc_link_min_win(snd_l), tipc_link_max_win(snd_l),
					 n->capabilities, &n->bc_entry.inputq1,
					 &n->bc_entry.namedq, snd_l, &n->bc_entry.link)) {
			pr_warn("Broadcast rcv link refresh failed, no memory\n");
			tipc_node_write_unlock_fast(n);
			tipc_node_put(n);
			n = NULL;
			goto exit;
		}
		n->preliminary = false;
		n->addr = addr;
		hlist_del_rcu(&n->hash);
		hlist_add_head_rcu(&n->hash,
				   &tn->node_htable[tipc_hashfn(addr)]);
		list_del_rcu(&n->list);
		list_for_each_entry_rcu(temp_node, &tn->node_list, list) {
			if (n->addr < temp_node->addr)
				break;
		}
		list_add_tail_rcu(&n->list, &temp_node->list);
		tipc_node_write_unlock_fast(n);

update:
		if (n->peer_hash_mix ^ hash_mixes)
			tipc_node_assign_peer_net(n, hash_mixes);
		if (n->capabilities == capabilities)
			goto exit;
		 
		tipc_node_write_lock(n);
		n->capabilities = capabilities;
		for (bearer_id = 0; bearer_id < MAX_BEARERS; bearer_id++) {
			l = n->links[bearer_id].link;
			if (l)
				tipc_link_update_caps(l, capabilities);
		}
		tipc_node_write_unlock_fast(n);

		 
		tn->capabilities = TIPC_NODE_CAPABILITIES;
		list_for_each_entry_rcu(temp_node, &tn->node_list, list) {
			tn->capabilities &= temp_node->capabilities;
		}

		tipc_bcast_toggle_rcast(net,
					(tn->capabilities & TIPC_BCAST_RCAST));

		goto exit;
	}
	n = kzalloc(sizeof(*n), GFP_ATOMIC);
	if (!n) {
		pr_warn("Node creation failed, no memory\n");
		goto exit;
	}
	tipc_nodeid2string(n->peer_id_string, peer_id);
#ifdef CONFIG_TIPC_CRYPTO
	if (unlikely(tipc_crypto_start(&n->crypto_rx, net, n))) {
		pr_warn("Failed to start crypto RX(%s)!\n", n->peer_id_string);
		kfree(n);
		n = NULL;
		goto exit;
	}
#endif
	n->addr = addr;
	n->preliminary = preliminary;
	memcpy(&n->peer_id, peer_id, 16);
	n->net = net;
	n->peer_net = NULL;
	n->peer_hash_mix = 0;
	 
	tipc_node_assign_peer_net(n, hash_mixes);
	n->capabilities = capabilities;
	kref_init(&n->kref);
	rwlock_init(&n->lock);
	INIT_HLIST_NODE(&n->hash);
	INIT_LIST_HEAD(&n->list);
	INIT_LIST_HEAD(&n->publ_list);
	INIT_LIST_HEAD(&n->conn_sks);
	skb_queue_head_init(&n->bc_entry.namedq);
	skb_queue_head_init(&n->bc_entry.inputq1);
	__skb_queue_head_init(&n->bc_entry.arrvq);
	skb_queue_head_init(&n->bc_entry.inputq2);
	for (i = 0; i < MAX_BEARERS; i++)
		spin_lock_init(&n->links[i].lock);
	n->state = SELF_DOWN_PEER_LEAVING;
	n->delete_at = jiffies + msecs_to_jiffies(NODE_CLEANUP_AFTER);
	n->signature = INVALID_NODE_SIG;
	n->active_links[0] = INVALID_BEARER_ID;
	n->active_links[1] = INVALID_BEARER_ID;
	if (!preliminary &&
	    !tipc_link_bc_create(net, tipc_own_addr(net), addr, peer_id, U16_MAX,
				 tipc_link_min_win(snd_l), tipc_link_max_win(snd_l),
				 n->capabilities, &n->bc_entry.inputq1,
				 &n->bc_entry.namedq, snd_l, &n->bc_entry.link)) {
		pr_warn("Broadcast rcv link creation failed, no memory\n");
		tipc_node_put(n);
		n = NULL;
		goto exit;
	}
	tipc_node_get(n);
	timer_setup(&n->timer, tipc_node_timeout, 0);
	 
	n->keepalive_intv = 10000;
	intv = jiffies + msecs_to_jiffies(n->keepalive_intv);
	if (!mod_timer(&n->timer, intv))
		tipc_node_get(n);
	hlist_add_head_rcu(&n->hash, &tn->node_htable[tipc_hashfn(addr)]);
	list_for_each_entry_rcu(temp_node, &tn->node_list, list) {
		if (n->addr < temp_node->addr)
			break;
	}
	list_add_tail_rcu(&n->list, &temp_node->list);
	 
	tn->capabilities = TIPC_NODE_CAPABILITIES;
	list_for_each_entry_rcu(temp_node, &tn->node_list, list) {
		tn->capabilities &= temp_node->capabilities;
	}
	tipc_bcast_toggle_rcast(net, (tn->capabilities & TIPC_BCAST_RCAST));
	trace_tipc_node_create(n, true, " ");
exit:
	spin_unlock_bh(&tn->node_list_lock);
	return n;
}

static void tipc_node_calculate_timer(struct tipc_node *n, struct tipc_link *l)
{
	unsigned long tol = tipc_link_tolerance(l);
	unsigned long intv = ((tol / 4) > 500) ? 500 : tol / 4;

	 
	if (intv < n->keepalive_intv)
		n->keepalive_intv = intv;

	 
	tipc_link_set_abort_limit(l, tol / n->keepalive_intv);
}

static void tipc_node_delete_from_list(struct tipc_node *node)
{
#ifdef CONFIG_TIPC_CRYPTO
	tipc_crypto_key_flush(node->crypto_rx);
#endif
	list_del_rcu(&node->list);
	hlist_del_rcu(&node->hash);
	tipc_node_put(node);
}

static void tipc_node_delete(struct tipc_node *node)
{
	trace_tipc_node_delete(node, true, " ");
	tipc_node_delete_from_list(node);

	del_timer_sync(&node->timer);
	tipc_node_put(node);
}

void tipc_node_stop(struct net *net)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_node *node, *t_node;

	spin_lock_bh(&tn->node_list_lock);
	list_for_each_entry_safe(node, t_node, &tn->node_list, list)
		tipc_node_delete(node);
	spin_unlock_bh(&tn->node_list_lock);
}

void tipc_node_subscribe(struct net *net, struct list_head *subscr, u32 addr)
{
	struct tipc_node *n;

	if (in_own_node(net, addr))
		return;

	n = tipc_node_find(net, addr);
	if (!n) {
		pr_warn("Node subscribe rejected, unknown node 0x%x\n", addr);
		return;
	}
	tipc_node_write_lock(n);
	list_add_tail(subscr, &n->publ_list);
	tipc_node_write_unlock_fast(n);
	tipc_node_put(n);
}

void tipc_node_unsubscribe(struct net *net, struct list_head *subscr, u32 addr)
{
	struct tipc_node *n;

	if (in_own_node(net, addr))
		return;

	n = tipc_node_find(net, addr);
	if (!n) {
		pr_warn("Node unsubscribe rejected, unknown node 0x%x\n", addr);
		return;
	}
	tipc_node_write_lock(n);
	list_del_init(subscr);
	tipc_node_write_unlock_fast(n);
	tipc_node_put(n);
}

int tipc_node_add_conn(struct net *net, u32 dnode, u32 port, u32 peer_port)
{
	struct tipc_node *node;
	struct tipc_sock_conn *conn;
	int err = 0;

	if (in_own_node(net, dnode))
		return 0;

	node = tipc_node_find(net, dnode);
	if (!node) {
		pr_warn("Connecting sock to node 0x%x failed\n", dnode);
		return -EHOSTUNREACH;
	}
	conn = kmalloc(sizeof(*conn), GFP_ATOMIC);
	if (!conn) {
		err = -EHOSTUNREACH;
		goto exit;
	}
	conn->peer_node = dnode;
	conn->port = port;
	conn->peer_port = peer_port;

	tipc_node_write_lock(node);
	list_add_tail(&conn->list, &node->conn_sks);
	tipc_node_write_unlock(node);
exit:
	tipc_node_put(node);
	return err;
}

void tipc_node_remove_conn(struct net *net, u32 dnode, u32 port)
{
	struct tipc_node *node;
	struct tipc_sock_conn *conn, *safe;

	if (in_own_node(net, dnode))
		return;

	node = tipc_node_find(net, dnode);
	if (!node)
		return;

	tipc_node_write_lock(node);
	list_for_each_entry_safe(conn, safe, &node->conn_sks, list) {
		if (port != conn->port)
			continue;
		list_del(&conn->list);
		kfree(conn);
	}
	tipc_node_write_unlock(node);
	tipc_node_put(node);
}

static void  tipc_node_clear_links(struct tipc_node *node)
{
	int i;

	for (i = 0; i < MAX_BEARERS; i++) {
		struct tipc_link_entry *le = &node->links[i];

		if (le->link) {
			kfree(le->link);
			le->link = NULL;
			node->link_cnt--;
		}
	}
}

 
static bool tipc_node_cleanup(struct tipc_node *peer)
{
	struct tipc_node *temp_node;
	struct tipc_net *tn = tipc_net(peer->net);
	bool deleted = false;

	 
	if (!spin_trylock_bh(&tn->node_list_lock))
		return false;

	tipc_node_write_lock(peer);

	if (!node_is_up(peer) && time_after(jiffies, peer->delete_at)) {
		tipc_node_clear_links(peer);
		tipc_node_delete_from_list(peer);
		deleted = true;
	}
	tipc_node_write_unlock(peer);

	if (!deleted) {
		spin_unlock_bh(&tn->node_list_lock);
		return deleted;
	}

	 
	tn->capabilities = TIPC_NODE_CAPABILITIES;
	list_for_each_entry_rcu(temp_node, &tn->node_list, list) {
		tn->capabilities &= temp_node->capabilities;
	}
	tipc_bcast_toggle_rcast(peer->net,
				(tn->capabilities & TIPC_BCAST_RCAST));
	spin_unlock_bh(&tn->node_list_lock);
	return deleted;
}

 
static void tipc_node_timeout(struct timer_list *t)
{
	struct tipc_node *n = from_timer(n, t, timer);
	struct tipc_link_entry *le;
	struct sk_buff_head xmitq;
	int remains = n->link_cnt;
	int bearer_id;
	int rc = 0;

	trace_tipc_node_timeout(n, false, " ");
	if (!node_is_up(n) && tipc_node_cleanup(n)) {
		 
		tipc_node_put(n);
		return;
	}

#ifdef CONFIG_TIPC_CRYPTO
	 
	tipc_crypto_timeout(n->crypto_rx);
#endif
	__skb_queue_head_init(&xmitq);

	 
	tipc_node_read_lock(n);
	n->keepalive_intv = 10000;
	tipc_node_read_unlock(n);
	for (bearer_id = 0; remains && (bearer_id < MAX_BEARERS); bearer_id++) {
		tipc_node_read_lock(n);
		le = &n->links[bearer_id];
		if (le->link) {
			spin_lock_bh(&le->lock);
			 
			tipc_node_calculate_timer(n, le->link);
			rc = tipc_link_timeout(le->link, &xmitq);
			spin_unlock_bh(&le->lock);
			remains--;
		}
		tipc_node_read_unlock(n);
		tipc_bearer_xmit(n->net, bearer_id, &xmitq, &le->maddr, n);
		if (rc & TIPC_LINK_DOWN_EVT)
			tipc_node_link_down(n, bearer_id, false);
	}
	mod_timer(&n->timer, jiffies + msecs_to_jiffies(n->keepalive_intv));
}

 
static void __tipc_node_link_up(struct tipc_node *n, int bearer_id,
				struct sk_buff_head *xmitq)
{
	int *slot0 = &n->active_links[0];
	int *slot1 = &n->active_links[1];
	struct tipc_link *ol = node_active_link(n, 0);
	struct tipc_link *nl = n->links[bearer_id].link;

	if (!nl || tipc_link_is_up(nl))
		return;

	tipc_link_fsm_evt(nl, LINK_ESTABLISH_EVT);
	if (!tipc_link_is_up(nl))
		return;

	n->working_links++;
	n->action_flags |= TIPC_NOTIFY_LINK_UP;
	n->link_id = tipc_link_id(nl);

	 
	n->links[bearer_id].mtu = tipc_link_mss(nl);

	tipc_bearer_add_dest(n->net, bearer_id, n->addr);
	tipc_bcast_inc_bearer_dst_cnt(n->net, bearer_id);

	pr_debug("Established link <%s> on network plane %c\n",
		 tipc_link_name(nl), tipc_link_plane(nl));
	trace_tipc_node_link_up(n, true, " ");

	 
	tipc_link_build_state_msg(nl, xmitq);

	 
	if (!ol) {
		*slot0 = bearer_id;
		*slot1 = bearer_id;
		tipc_node_fsm_evt(n, SELF_ESTABL_CONTACT_EVT);
		n->action_flags |= TIPC_NOTIFY_NODE_UP;
		tipc_link_set_active(nl, true);
		tipc_bcast_add_peer(n->net, nl, xmitq);
		return;
	}

	 
	if (tipc_link_prio(nl) > tipc_link_prio(ol)) {
		pr_debug("Old link <%s> becomes standby\n", tipc_link_name(ol));
		*slot0 = bearer_id;
		*slot1 = bearer_id;
		tipc_link_set_active(nl, true);
		tipc_link_set_active(ol, false);
	} else if (tipc_link_prio(nl) == tipc_link_prio(ol)) {
		tipc_link_set_active(nl, true);
		*slot1 = bearer_id;
	} else {
		pr_debug("New link <%s> is standby\n", tipc_link_name(nl));
	}

	 
	tipc_link_tnl_prepare(ol, nl, SYNCH_MSG, xmitq);
}

 
static void tipc_node_link_up(struct tipc_node *n, int bearer_id,
			      struct sk_buff_head *xmitq)
{
	struct tipc_media_addr *maddr;

	tipc_node_write_lock(n);
	__tipc_node_link_up(n, bearer_id, xmitq);
	maddr = &n->links[bearer_id].maddr;
	tipc_bearer_xmit(n->net, bearer_id, xmitq, maddr, n);
	tipc_node_write_unlock(n);
}

 
static void tipc_node_link_failover(struct tipc_node *n, struct tipc_link *l,
				    struct tipc_link *tnl,
				    struct sk_buff_head *xmitq)
{
	 
	if (!tipc_link_is_up(tnl))
		return;

	 
	if (l && !tipc_link_is_reset(l))
		return;

	tipc_link_fsm_evt(tnl, LINK_SYNCH_END_EVT);
	tipc_node_fsm_evt(n, NODE_SYNCH_END_EVT);

	n->sync_point = tipc_link_rcv_nxt(tnl) + (U16_MAX / 2 - 1);
	tipc_link_failover_prepare(l, tnl, xmitq);

	if (l)
		tipc_link_fsm_evt(l, LINK_FAILOVER_BEGIN_EVT);
	tipc_node_fsm_evt(n, NODE_FAILOVER_BEGIN_EVT);
}

 
static void __tipc_node_link_down(struct tipc_node *n, int *bearer_id,
				  struct sk_buff_head *xmitq,
				  struct tipc_media_addr **maddr)
{
	struct tipc_link_entry *le = &n->links[*bearer_id];
	int *slot0 = &n->active_links[0];
	int *slot1 = &n->active_links[1];
	int i, highest = 0, prio;
	struct tipc_link *l, *_l, *tnl;

	l = n->links[*bearer_id].link;
	if (!l || tipc_link_is_reset(l))
		return;

	n->working_links--;
	n->action_flags |= TIPC_NOTIFY_LINK_DOWN;
	n->link_id = tipc_link_id(l);

	tipc_bearer_remove_dest(n->net, *bearer_id, n->addr);

	pr_debug("Lost link <%s> on network plane %c\n",
		 tipc_link_name(l), tipc_link_plane(l));

	 
	*slot0 = INVALID_BEARER_ID;
	*slot1 = INVALID_BEARER_ID;
	for (i = 0; i < MAX_BEARERS; i++) {
		_l = n->links[i].link;
		if (!_l || !tipc_link_is_up(_l))
			continue;
		if (_l == l)
			continue;
		prio = tipc_link_prio(_l);
		if (prio < highest)
			continue;
		if (prio > highest) {
			highest = prio;
			*slot0 = i;
			*slot1 = i;
			continue;
		}
		*slot1 = i;
	}

	if (!node_is_up(n)) {
		if (tipc_link_peer_is_down(l))
			tipc_node_fsm_evt(n, PEER_LOST_CONTACT_EVT);
		tipc_node_fsm_evt(n, SELF_LOST_CONTACT_EVT);
		trace_tipc_link_reset(l, TIPC_DUMP_ALL, "link down!");
		tipc_link_fsm_evt(l, LINK_RESET_EVT);
		tipc_link_reset(l);
		tipc_link_build_reset_msg(l, xmitq);
		*maddr = &n->links[*bearer_id].maddr;
		node_lost_contact(n, &le->inputq);
		tipc_bcast_dec_bearer_dst_cnt(n->net, *bearer_id);
		return;
	}
	tipc_bcast_dec_bearer_dst_cnt(n->net, *bearer_id);

	 
	*bearer_id = n->active_links[0];
	tnl = n->links[*bearer_id].link;
	tipc_link_fsm_evt(tnl, LINK_SYNCH_END_EVT);
	tipc_node_fsm_evt(n, NODE_SYNCH_END_EVT);
	n->sync_point = tipc_link_rcv_nxt(tnl) + (U16_MAX / 2 - 1);
	tipc_link_tnl_prepare(l, tnl, FAILOVER_MSG, xmitq);
	trace_tipc_link_reset(l, TIPC_DUMP_ALL, "link down -> failover!");
	tipc_link_reset(l);
	tipc_link_fsm_evt(l, LINK_RESET_EVT);
	tipc_link_fsm_evt(l, LINK_FAILOVER_BEGIN_EVT);
	tipc_node_fsm_evt(n, NODE_FAILOVER_BEGIN_EVT);
	*maddr = &n->links[*bearer_id].maddr;
}

static void tipc_node_link_down(struct tipc_node *n, int bearer_id, bool delete)
{
	struct tipc_link_entry *le = &n->links[bearer_id];
	struct tipc_media_addr *maddr = NULL;
	struct tipc_link *l = le->link;
	int old_bearer_id = bearer_id;
	struct sk_buff_head xmitq;

	if (!l)
		return;

	__skb_queue_head_init(&xmitq);

	tipc_node_write_lock(n);
	if (!tipc_link_is_establishing(l)) {
		__tipc_node_link_down(n, &bearer_id, &xmitq, &maddr);
	} else {
		 
		tipc_link_reset(l);
		tipc_link_fsm_evt(l, LINK_RESET_EVT);
	}
	if (delete) {
		kfree(l);
		le->link = NULL;
		n->link_cnt--;
	}
	trace_tipc_node_link_down(n, true, "node link down or deleted!");
	tipc_node_write_unlock(n);
	if (delete)
		tipc_mon_remove_peer(n->net, n->addr, old_bearer_id);
	if (!skb_queue_empty(&xmitq))
		tipc_bearer_xmit(n->net, bearer_id, &xmitq, maddr, n);
	tipc_sk_rcv(n->net, &le->inputq);
}

static bool node_is_up(struct tipc_node *n)
{
	return n->active_links[0] != INVALID_BEARER_ID;
}

bool tipc_node_is_up(struct net *net, u32 addr)
{
	struct tipc_node *n;
	bool retval = false;

	if (in_own_node(net, addr))
		return true;

	n = tipc_node_find(net, addr);
	if (!n)
		return false;
	retval = node_is_up(n);
	tipc_node_put(n);
	return retval;
}

static u32 tipc_node_suggest_addr(struct net *net, u32 addr)
{
	struct tipc_node *n;

	addr ^= tipc_net(net)->random;
	while ((n = tipc_node_find(net, addr))) {
		tipc_node_put(n);
		addr++;
	}
	return addr;
}

 
u32 tipc_node_try_addr(struct net *net, u8 *id, u32 addr)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_node *n;
	bool preliminary;
	u32 sugg_addr;

	 
	n = tipc_node_find(net, addr);
	if (n) {
		if (!memcmp(n->peer_id, id, NODE_ID_LEN))
			addr = 0;
		tipc_node_put(n);
		if (!addr)
			return 0;
		return tipc_node_suggest_addr(net, addr);
	}

	 
	n = tipc_node_find_by_id(net, id);
	if (n) {
		sugg_addr = n->addr;
		preliminary = n->preliminary;
		tipc_node_put(n);
		if (!preliminary)
			return sugg_addr;
	}

	 
	if (tn->trial_addr == addr)
		return tipc_node_suggest_addr(net, addr);

	return 0;
}

void tipc_node_check_dest(struct net *net, u32 addr,
			  u8 *peer_id, struct tipc_bearer *b,
			  u16 capabilities, u32 signature, u32 hash_mixes,
			  struct tipc_media_addr *maddr,
			  bool *respond, bool *dupl_addr)
{
	struct tipc_node *n;
	struct tipc_link *l;
	struct tipc_link_entry *le;
	bool addr_match = false;
	bool sign_match = false;
	bool link_up = false;
	bool link_is_reset = false;
	bool accept_addr = false;
	bool reset = false;
	char *if_name;
	unsigned long intv;
	u16 session;

	*dupl_addr = false;
	*respond = false;

	n = tipc_node_create(net, addr, peer_id, capabilities, hash_mixes,
			     false);
	if (!n)
		return;

	tipc_node_write_lock(n);

	le = &n->links[b->identity];

	 
	l = le->link;
	link_up = l && tipc_link_is_up(l);
	link_is_reset = l && tipc_link_is_reset(l);
	addr_match = l && !memcmp(&le->maddr, maddr, sizeof(*maddr));
	sign_match = (signature == n->signature);

	 

	if (sign_match && addr_match && link_up) {
		 
		 
		if (!n->peer_hash_mix)
			n->peer_hash_mix = hash_mixes;
	} else if (sign_match && addr_match && !link_up) {
		 
		*respond = true;
	} else if (sign_match && !addr_match && link_up) {
		 
		*dupl_addr = true;
	} else if (sign_match && !addr_match && !link_up) {
		 
		accept_addr = true;
		*respond = true;
		reset = true;
	} else if (!sign_match && addr_match && link_up) {
		 
		n->signature = signature;
	} else if (!sign_match && addr_match && !link_up) {
		 
		n->signature = signature;
		*respond = true;
	} else if (!sign_match && !addr_match && link_up) {
		 
		*dupl_addr = true;
	} else if (!sign_match && !addr_match && !link_up) {
		 
		n->signature = signature;
		accept_addr = true;
		*respond = true;
		reset = true;
	}

	if (!accept_addr)
		goto exit;

	 
	if (!l) {
		if (n->link_cnt == 2)
			goto exit;

		if_name = strchr(b->name, ':') + 1;
		get_random_bytes(&session, sizeof(u16));
		if (!tipc_link_create(net, if_name, b->identity, b->tolerance,
				      b->net_plane, b->mtu, b->priority,
				      b->min_win, b->max_win, session,
				      tipc_own_addr(net), addr, peer_id,
				      n->capabilities,
				      tipc_bc_sndlink(n->net), n->bc_entry.link,
				      &le->inputq,
				      &n->bc_entry.namedq, &l)) {
			*respond = false;
			goto exit;
		}
		trace_tipc_link_reset(l, TIPC_DUMP_ALL, "link created!");
		tipc_link_reset(l);
		tipc_link_fsm_evt(l, LINK_RESET_EVT);
		if (n->state == NODE_FAILINGOVER)
			tipc_link_fsm_evt(l, LINK_FAILOVER_BEGIN_EVT);
		link_is_reset = tipc_link_is_reset(l);
		le->link = l;
		n->link_cnt++;
		tipc_node_calculate_timer(n, l);
		if (n->link_cnt == 1) {
			intv = jiffies + msecs_to_jiffies(n->keepalive_intv);
			if (!mod_timer(&n->timer, intv))
				tipc_node_get(n);
		}
	}
	memcpy(&le->maddr, maddr, sizeof(*maddr));
exit:
	tipc_node_write_unlock(n);
	if (reset && !link_is_reset)
		tipc_node_link_down(n, b->identity, false);
	tipc_node_put(n);
}

void tipc_node_delete_links(struct net *net, int bearer_id)
{
	struct tipc_net *tn = net_generic(net, tipc_net_id);
	struct tipc_node *n;

	rcu_read_lock();
	list_for_each_entry_rcu(n, &tn->node_list, list) {
		tipc_node_link_down(n, bearer_id, true);
	}
	rcu_read_unlock();
}

static void tipc_node_reset_links(struct tipc_node *n)
{
	int i;

	pr_warn("Resetting all links to %x\n", n->addr);

	trace_tipc_node_reset_links(n, true, " ");
	for (i = 0; i < MAX_BEARERS; i++) {
		tipc_node_link_down(n, i, false);
	}
}

 
static void tipc_node_fsm_evt(struct tipc_node *n, int evt)
{
	int state = n->state;

	switch (state) {
	case SELF_DOWN_PEER_DOWN:
		switch (evt) {
		case SELF_ESTABL_CONTACT_EVT:
			state = SELF_UP_PEER_COMING;
			break;
		case PEER_ESTABL_CONTACT_EVT:
			state = SELF_COMING_PEER_UP;
			break;
		case SELF_LOST_CONTACT_EVT:
		case PEER_LOST_CONTACT_EVT:
			break;
		case NODE_SYNCH_END_EVT:
		case NODE_SYNCH_BEGIN_EVT:
		case NODE_FAILOVER_BEGIN_EVT:
		case NODE_FAILOVER_END_EVT:
		default:
			goto illegal_evt;
		}
		break;
	case SELF_UP_PEER_UP:
		switch (evt) {
		case SELF_LOST_CONTACT_EVT:
			state = SELF_DOWN_PEER_LEAVING;
			break;
		case PEER_LOST_CONTACT_EVT:
			state = SELF_LEAVING_PEER_DOWN;
			break;
		case NODE_SYNCH_BEGIN_EVT:
			state = NODE_SYNCHING;
			break;
		case NODE_FAILOVER_BEGIN_EVT:
			state = NODE_FAILINGOVER;
			break;
		case SELF_ESTABL_CONTACT_EVT:
		case PEER_ESTABL_CONTACT_EVT:
		case NODE_SYNCH_END_EVT:
		case NODE_FAILOVER_END_EVT:
			break;
		default:
			goto illegal_evt;
		}
		break;
	case SELF_DOWN_PEER_LEAVING:
		switch (evt) {
		case PEER_LOST_CONTACT_EVT:
			state = SELF_DOWN_PEER_DOWN;
			break;
		case SELF_ESTABL_CONTACT_EVT:
		case PEER_ESTABL_CONTACT_EVT:
		case SELF_LOST_CONTACT_EVT:
			break;
		case NODE_SYNCH_END_EVT:
		case NODE_SYNCH_BEGIN_EVT:
		case NODE_FAILOVER_BEGIN_EVT:
		case NODE_FAILOVER_END_EVT:
		default:
			goto illegal_evt;
		}
		break;
	case SELF_UP_PEER_COMING:
		switch (evt) {
		case PEER_ESTABL_CONTACT_EVT:
			state = SELF_UP_PEER_UP;
			break;
		case SELF_LOST_CONTACT_EVT:
			state = SELF_DOWN_PEER_DOWN;
			break;
		case SELF_ESTABL_CONTACT_EVT:
		case PEER_LOST_CONTACT_EVT:
		case NODE_SYNCH_END_EVT:
		case NODE_FAILOVER_BEGIN_EVT:
			break;
		case NODE_SYNCH_BEGIN_EVT:
		case NODE_FAILOVER_END_EVT:
		default:
			goto illegal_evt;
		}
		break;
	case SELF_COMING_PEER_UP:
		switch (evt) {
		case SELF_ESTABL_CONTACT_EVT:
			state = SELF_UP_PEER_UP;
			break;
		case PEER_LOST_CONTACT_EVT:
			state = SELF_DOWN_PEER_DOWN;
			break;
		case SELF_LOST_CONTACT_EVT:
		case PEER_ESTABL_CONTACT_EVT:
			break;
		case NODE_SYNCH_END_EVT:
		case NODE_SYNCH_BEGIN_EVT:
		case NODE_FAILOVER_BEGIN_EVT:
		case NODE_FAILOVER_END_EVT:
		default:
			goto illegal_evt;
		}
		break;
	case SELF_LEAVING_PEER_DOWN:
		switch (evt) {
		case SELF_LOST_CONTACT_EVT:
			state = SELF_DOWN_PEER_DOWN;
			break;
		case SELF_ESTABL_CONTACT_EVT:
		case PEER_ESTABL_CONTACT_EVT:
		case PEER_LOST_CONTACT_EVT:
			break;
		case NODE_SYNCH_END_EVT:
		case NODE_SYNCH_BEGIN_EVT:
		case NODE_FAILOVER_BEGIN_EVT:
		case NODE_FAILOVER_END_EVT:
		default:
			goto illegal_evt;
		}
		break;
	case NODE_FAILINGOVER:
		switch (evt) {
		case SELF_LOST_CONTACT_EVT:
			state = SELF_DOWN_PEER_LEAVING;
			break;
		case PEER_LOST_CONTACT_EVT:
			state = SELF_LEAVING_PEER_DOWN;
			break;
		case NODE_FAILOVER_END_EVT:
			state = SELF_UP_PEER_UP;
			break;
		case NODE_FAILOVER_BEGIN_EVT:
		case SELF_ESTABL_CONTACT_EVT:
		case PEER_ESTABL_CONTACT_EVT:
			break;
		case NODE_SYNCH_BEGIN_EVT:
		case NODE_SYNCH_END_EVT:
		default:
			goto illegal_evt;
		}
		break;
	case NODE_SYNCHING:
		switch (evt) {
		case SELF_LOST_CONTACT_EVT:
			state = SELF_DOWN_PEER_LEAVING;
			break;
		case PEER_LOST_CONTACT_EVT:
			state = SELF_LEAVING_PEER_DOWN;
			break;
		case NODE_SYNCH_END_EVT:
			state = SELF_UP_PEER_UP;
			break;
		case NODE_FAILOVER_BEGIN_EVT:
			state = NODE_FAILINGOVER;
			break;
		case NODE_SYNCH_BEGIN_EVT:
		case SELF_ESTABL_CONTACT_EVT:
		case PEER_ESTABL_CONTACT_EVT:
			break;
		case NODE_FAILOVER_END_EVT:
		default:
			goto illegal_evt;
		}
		break;
	default:
		pr_err("Unknown node fsm state %x\n", state);
		break;
	}
	trace_tipc_node_fsm(n->peer_id, n->state, state, evt);
	n->state = state;
	return;

illegal_evt:
	pr_err("Illegal node fsm evt %x in state %x\n", evt, state);
	trace_tipc_node_fsm(n->peer_id, n->state, state, evt);
}

static void node_lost_contact(struct tipc_node *n,
			      struct sk_buff_head *inputq)
{
	struct tipc_sock_conn *conn, *safe;
	struct tipc_link *l;
	struct list_head *conns = &n->conn_sks;
	struct sk_buff *skb;
	uint i;

	pr_debug("Lost contact with %x\n", n->addr);
	n->delete_at = jiffies + msecs_to_jiffies(NODE_CLEANUP_AFTER);
	trace_tipc_node_lost_contact(n, true, " ");

	 
	tipc_bcast_remove_peer(n->net, n->bc_entry.link);
	skb_queue_purge(&n->bc_entry.namedq);

	 
	for (i = 0; i < MAX_BEARERS; i++) {
		l = n->links[i].link;
		if (l)
			tipc_link_fsm_evt(l, LINK_FAILOVER_END_EVT);
	}

	 
	n->action_flags |= TIPC_NOTIFY_NODE_DOWN;
	n->peer_net = NULL;
	n->peer_hash_mix = 0;
	 
	list_for_each_entry_safe(conn, safe, conns, list) {
		skb = tipc_msg_create(TIPC_CRITICAL_IMPORTANCE, TIPC_CONN_MSG,
				      SHORT_H_SIZE, 0, tipc_own_addr(n->net),
				      conn->peer_node, conn->port,
				      conn->peer_port, TIPC_ERR_NO_NODE);
		if (likely(skb))
			skb_queue_tail(inputq, skb);
		list_del(&conn->list);
		kfree(conn);
	}
}

 
int tipc_node_get_linkname(struct net *net, u32 bearer_id, u32 addr,
			   char *linkname, size_t len)
{
	struct tipc_link *link;
	int err = -EINVAL;
	struct tipc_node *node = tipc_node_find(net, addr);

	if (!node)
		return err;

	if (bearer_id >= MAX_BEARERS)
		goto exit;

	tipc_node_read_lock(node);
	link = node->links[bearer_id].link;
	if (link) {
		strncpy(linkname, tipc_link_name(link), len);
		err = 0;
	}
	tipc_node_read_unlock(node);
exit:
	tipc_node_put(node);
	return err;
}

 
static int __tipc_nl_add_node(struct tipc_nl_msg *msg, struct tipc_node *node)
{
	void *hdr;
	struct nlattr *attrs;

	hdr = genlmsg_put(msg->skb, msg->portid, msg->seq, &tipc_genl_family,
			  NLM_F_MULTI, TIPC_NL_NODE_GET);
	if (!hdr)
		return -EMSGSIZE;

	attrs = nla_nest_start_noflag(msg->skb, TIPC_NLA_NODE);
	if (!attrs)
		goto msg_full;

	if (nla_put_u32(msg->skb, TIPC_NLA_NODE_ADDR, node->addr))
		goto attr_msg_full;
	if (node_is_up(node))
		if (nla_put_flag(msg->skb, TIPC_NLA_NODE_UP))
			goto attr_msg_full;

	nla_nest_end(msg->skb, attrs);
	genlmsg_end(msg->skb, hdr);

	return 0;

attr_msg_full:
	nla_nest_cancel(msg->skb, attrs);
msg_full:
	genlmsg_cancel(msg->skb, hdr);

	return -EMSGSIZE;
}

static void tipc_lxc_xmit(struct net *peer_net, struct sk_buff_head *list)
{
	struct tipc_msg *hdr = buf_msg(skb_peek(list));
	struct sk_buff_head inputq;

	switch (msg_user(hdr)) {
	case TIPC_LOW_IMPORTANCE:
	case TIPC_MEDIUM_IMPORTANCE:
	case TIPC_HIGH_IMPORTANCE:
	case TIPC_CRITICAL_IMPORTANCE:
		if (msg_connected(hdr) || msg_named(hdr) ||
		    msg_direct(hdr)) {
			tipc_loopback_trace(peer_net, list);
			spin_lock_init(&list->lock);
			tipc_sk_rcv(peer_net, list);
			return;
		}
		if (msg_mcast(hdr)) {
			tipc_loopback_trace(peer_net, list);
			skb_queue_head_init(&inputq);
			tipc_sk_mcast_rcv(peer_net, list, &inputq);
			__skb_queue_purge(list);
			skb_queue_purge(&inputq);
			return;
		}
		return;
	case MSG_FRAGMENTER:
		if (tipc_msg_assemble(list)) {
			tipc_loopback_trace(peer_net, list);
			skb_queue_head_init(&inputq);
			tipc_sk_mcast_rcv(peer_net, list, &inputq);
			__skb_queue_purge(list);
			skb_queue_purge(&inputq);
		}
		return;
	case GROUP_PROTOCOL:
	case CONN_MANAGER:
		tipc_loopback_trace(peer_net, list);
		spin_lock_init(&list->lock);
		tipc_sk_rcv(peer_net, list);
		return;
	case LINK_PROTOCOL:
	case NAME_DISTRIBUTOR:
	case TUNNEL_PROTOCOL:
	case BCAST_PROTOCOL:
		return;
	default:
		return;
	}
}

 
int tipc_node_xmit(struct net *net, struct sk_buff_head *list,
		   u32 dnode, int selector)
{
	struct tipc_link_entry *le = NULL;
	struct tipc_node *n;
	struct sk_buff_head xmitq;
	bool node_up = false;
	struct net *peer_net;
	int bearer_id;
	int rc;

	if (in_own_node(net, dnode)) {
		tipc_loopback_trace(net, list);
		spin_lock_init(&list->lock);
		tipc_sk_rcv(net, list);
		return 0;
	}

	n = tipc_node_find(net, dnode);
	if (unlikely(!n)) {
		__skb_queue_purge(list);
		return -EHOSTUNREACH;
	}

	rcu_read_lock();
	tipc_node_read_lock(n);
	node_up = node_is_up(n);
	peer_net = n->peer_net;
	tipc_node_read_unlock(n);
	if (node_up && peer_net && check_net(peer_net)) {
		 
		tipc_lxc_xmit(peer_net, list);
		if (likely(skb_queue_empty(list))) {
			rcu_read_unlock();
			tipc_node_put(n);
			return 0;
		}
	}
	rcu_read_unlock();

	tipc_node_read_lock(n);
	bearer_id = n->active_links[selector & 1];
	if (unlikely(bearer_id == INVALID_BEARER_ID)) {
		tipc_node_read_unlock(n);
		tipc_node_put(n);
		__skb_queue_purge(list);
		return -EHOSTUNREACH;
	}

	__skb_queue_head_init(&xmitq);
	le = &n->links[bearer_id];
	spin_lock_bh(&le->lock);
	rc = tipc_link_xmit(le->link, list, &xmitq);
	spin_unlock_bh(&le->lock);
	tipc_node_read_unlock(n);

	if (unlikely(rc == -ENOBUFS))
		tipc_node_link_down(n, bearer_id, false);
	else
		tipc_bearer_xmit(net, bearer_id, &xmitq, &le->maddr, n);

	tipc_node_put(n);

	return rc;
}

 
int tipc_node_xmit_skb(struct net *net, struct sk_buff *skb, u32 dnode,
		       u32 selector)
{
	struct sk_buff_head head;

	__skb_queue_head_init(&head);
	__skb_queue_tail(&head, skb);
	tipc_node_xmit(net, &head, dnode, selector);
	return 0;
}

 
int tipc_node_distr_xmit(struct net *net, struct sk_buff_head *xmitq)
{
	struct sk_buff *skb;
	u32 selector, dnode;

	while ((skb = __skb_dequeue(xmitq))) {
		selector = msg_origport(buf_msg(skb));
		dnode = msg_destnode(buf_msg(skb));
		tipc_node_xmit_skb(net, skb, dnode, selector);
	}
	return 0;
}

void tipc_node_broadcast(struct net *net, struct sk_buff *skb, int rc_dests)
{
	struct sk_buff_head xmitq;
	struct sk_buff *txskb;
	struct tipc_node *n;
	u16 dummy;
	u32 dst;

	 
	if (!rc_dests && tipc_bcast_get_mode(net) != BCLINK_MODE_RCAST) {
		__skb_queue_head_init(&xmitq);
		__skb_queue_tail(&xmitq, skb);
		tipc_bcast_xmit(net, &xmitq, &dummy);
		return;
	}

	 
	rcu_read_lock();
	list_for_each_entry_rcu(n, tipc_nodes(net), list) {
		dst = n->addr;
		if (in_own_node(net, dst))
			continue;
		if (!node_is_up(n))
			continue;
		txskb = pskb_copy(skb, GFP_ATOMIC);
		if (!txskb)
			break;
		msg_set_destnode(buf_msg(txskb), dst);
		tipc_node_xmit_skb(net, txskb, dst, 0);
	}
	rcu_read_unlock();
	kfree_skb(skb);
}

static void tipc_node_mcast_rcv(struct tipc_node *n)
{
	struct tipc_bclink_entry *be = &n->bc_entry;

	 
	spin_lock_bh(&be->inputq2.lock);
	spin_lock_bh(&be->inputq1.lock);
	skb_queue_splice_tail_init(&be->inputq1, &be->arrvq);
	spin_unlock_bh(&be->inputq1.lock);
	spin_unlock_bh(&be->inputq2.lock);
	tipc_sk_mcast_rcv(n->net, &be->arrvq, &be->inputq2);
}

static void tipc_node_bc_sync_rcv(struct tipc_node *n, struct tipc_msg *hdr,
				  int bearer_id, struct sk_buff_head *xmitq)
{
	struct tipc_link *ucl;
	int rc;

	rc = tipc_bcast_sync_rcv(n->net, n->bc_entry.link, hdr, xmitq);

	if (rc & TIPC_LINK_DOWN_EVT) {
		tipc_node_reset_links(n);
		return;
	}

	if (!(rc & TIPC_LINK_SND_STATE))
		return;

	 
	if (msg_probe(hdr))
		return;

	 
	tipc_node_read_lock(n);
	ucl = n->links[bearer_id].link;
	if (ucl)
		tipc_link_build_state_msg(ucl, xmitq);
	tipc_node_read_unlock(n);
}

 
static void tipc_node_bc_rcv(struct net *net, struct sk_buff *skb, int bearer_id)
{
	int rc;
	struct sk_buff_head xmitq;
	struct tipc_bclink_entry *be;
	struct tipc_link_entry *le;
	struct tipc_msg *hdr = buf_msg(skb);
	int usr = msg_user(hdr);
	u32 dnode = msg_destnode(hdr);
	struct tipc_node *n;

	__skb_queue_head_init(&xmitq);

	 
	if ((usr == BCAST_PROTOCOL) && (dnode != tipc_own_addr(net)))
		n = tipc_node_find(net, dnode);
	else
		n = tipc_node_find(net, msg_prevnode(hdr));
	if (!n) {
		kfree_skb(skb);
		return;
	}
	be = &n->bc_entry;
	le = &n->links[bearer_id];

	rc = tipc_bcast_rcv(net, be->link, skb);

	 
	if (rc & TIPC_LINK_SND_STATE) {
		tipc_node_read_lock(n);
		tipc_link_build_state_msg(le->link, &xmitq);
		tipc_node_read_unlock(n);
	}

	if (!skb_queue_empty(&xmitq))
		tipc_bearer_xmit(net, bearer_id, &xmitq, &le->maddr, n);

	if (!skb_queue_empty(&be->inputq1))
		tipc_node_mcast_rcv(n);

	 
	if (!skb_queue_empty(&n->bc_entry.namedq))
		tipc_named_rcv(net, &n->bc_entry.namedq,
			       &n->bc_entry.named_rcv_nxt,
			       &n->bc_entry.named_open);

	 
	if (rc & TIPC_LINK_DOWN_EVT)
		tipc_node_reset_links(n);

	tipc_node_put(n);
}

 
static bool tipc_node_check_state(struct tipc_node *n, struct sk_buff *skb,
				  int bearer_id, struct sk_buff_head *xmitq)
{
	struct tipc_msg *hdr = buf_msg(skb);
	int usr = msg_user(hdr);
	int mtyp = msg_type(hdr);
	u16 oseqno = msg_seqno(hdr);
	u16 exp_pkts = msg_msgcnt(hdr);
	u16 rcv_nxt, syncpt, dlv_nxt, inputq_len;
	int state = n->state;
	struct tipc_link *l, *tnl, *pl = NULL;
	struct tipc_media_addr *maddr;
	int pb_id;

	if (trace_tipc_node_check_state_enabled()) {
		trace_tipc_skb_dump(skb, false, "skb for node state check");
		trace_tipc_node_check_state(n, true, " ");
	}
	l = n->links[bearer_id].link;
	if (!l)
		return false;
	rcv_nxt = tipc_link_rcv_nxt(l);


	if (likely((state == SELF_UP_PEER_UP) && (usr != TUNNEL_PROTOCOL)))
		return true;

	 
	for (pb_id = 0; pb_id < MAX_BEARERS; pb_id++) {
		if ((pb_id != bearer_id) && n->links[pb_id].link) {
			pl = n->links[pb_id].link;
			break;
		}
	}

	if (!tipc_link_validate_msg(l, hdr)) {
		trace_tipc_skb_dump(skb, false, "PROTO invalid (2)!");
		trace_tipc_link_dump(l, TIPC_DUMP_NONE, "PROTO invalid (2)!");
		return false;
	}

	 
	if (state == SELF_UP_PEER_COMING) {
		if (!tipc_link_is_up(l))
			return true;
		if (!msg_peer_link_is_up(hdr))
			return true;
		tipc_node_fsm_evt(n, PEER_ESTABL_CONTACT_EVT);
	}

	if (state == SELF_DOWN_PEER_LEAVING) {
		if (msg_peer_node_is_up(hdr))
			return false;
		tipc_node_fsm_evt(n, PEER_LOST_CONTACT_EVT);
		return true;
	}

	if (state == SELF_LEAVING_PEER_DOWN)
		return false;

	 
	if ((usr != LINK_PROTOCOL) && less(oseqno, rcv_nxt))
		return true;

	 
	if ((usr == TUNNEL_PROTOCOL) && (mtyp == FAILOVER_MSG)) {
		syncpt = oseqno + exp_pkts - 1;
		if (pl && !tipc_link_is_reset(pl)) {
			__tipc_node_link_down(n, &pb_id, xmitq, &maddr);
			trace_tipc_node_link_down(n, true,
						  "node link down <- failover!");
			tipc_skb_queue_splice_tail_init(tipc_link_inputq(pl),
							tipc_link_inputq(l));
		}

		 
		if (n->state != NODE_FAILINGOVER)
			tipc_node_link_failover(n, pl, l, xmitq);

		 
		if (less(syncpt, n->sync_point))
			n->sync_point = syncpt;
	}

	 
	if ((n->state == NODE_FAILINGOVER) && tipc_link_is_up(l)) {
		if (!more(rcv_nxt, n->sync_point))
			return true;
		tipc_node_fsm_evt(n, NODE_FAILOVER_END_EVT);
		if (pl)
			tipc_link_fsm_evt(pl, LINK_FAILOVER_END_EVT);
		return true;
	}

	 
	if (!pl || !tipc_link_is_up(pl))
		return true;

	 
	if ((usr == TUNNEL_PROTOCOL) && (mtyp == SYNCH_MSG) && (oseqno == 1)) {
		if (n->capabilities & TIPC_TUNNEL_ENHANCED)
			syncpt = msg_syncpt(hdr);
		else
			syncpt = msg_seqno(msg_inner_hdr(hdr)) + exp_pkts - 1;
		if (!tipc_link_is_up(l))
			__tipc_node_link_up(n, bearer_id, xmitq);
		if (n->state == SELF_UP_PEER_UP) {
			n->sync_point = syncpt;
			tipc_link_fsm_evt(l, LINK_SYNCH_BEGIN_EVT);
			tipc_node_fsm_evt(n, NODE_SYNCH_BEGIN_EVT);
		}
	}

	 
	if (n->state == NODE_SYNCHING) {
		if (tipc_link_is_synching(l)) {
			tnl = l;
		} else {
			tnl = pl;
			pl = l;
		}
		inputq_len = skb_queue_len(tipc_link_inputq(pl));
		dlv_nxt = tipc_link_rcv_nxt(pl) - inputq_len;
		if (more(dlv_nxt, n->sync_point)) {
			tipc_link_fsm_evt(tnl, LINK_SYNCH_END_EVT);
			tipc_node_fsm_evt(n, NODE_SYNCH_END_EVT);
			return true;
		}
		if (l == pl)
			return true;
		if ((usr == TUNNEL_PROTOCOL) && (mtyp == SYNCH_MSG))
			return true;
		if (usr == LINK_PROTOCOL)
			return true;
		return false;
	}
	return true;
}

 
void tipc_rcv(struct net *net, struct sk_buff *skb, struct tipc_bearer *b)
{
	struct sk_buff_head xmitq;
	struct tipc_link_entry *le;
	struct tipc_msg *hdr;
	struct tipc_node *n;
	int bearer_id = b->identity;
	u32 self = tipc_own_addr(net);
	int usr, rc = 0;
	u16 bc_ack;
#ifdef CONFIG_TIPC_CRYPTO
	struct tipc_ehdr *ehdr;

	 
	if (TIPC_SKB_CB(skb)->decrypted || !tipc_ehdr_validate(skb))
		goto rcv;

	ehdr = (struct tipc_ehdr *)skb->data;
	if (likely(ehdr->user != LINK_CONFIG)) {
		n = tipc_node_find(net, ntohl(ehdr->addr));
		if (unlikely(!n))
			goto discard;
	} else {
		n = tipc_node_find_by_id(net, ehdr->id);
	}
	tipc_crypto_rcv(net, (n) ? n->crypto_rx : NULL, &skb, b);
	if (!skb)
		return;

rcv:
#endif
	 
	if (unlikely(!tipc_msg_validate(&skb)))
		goto discard;
	__skb_queue_head_init(&xmitq);
	hdr = buf_msg(skb);
	usr = msg_user(hdr);
	bc_ack = msg_bcast_ack(hdr);

	 
	if (unlikely(msg_non_seq(hdr))) {
		if (unlikely(usr == LINK_CONFIG))
			return tipc_disc_rcv(net, skb, b);
		else
			return tipc_node_bc_rcv(net, skb, bearer_id);
	}

	 
	if (unlikely(!msg_short(hdr) && (msg_destnode(hdr) != self)))
		goto discard;

	 
	n = tipc_node_find(net, msg_prevnode(hdr));
	if (unlikely(!n))
		goto discard;
	le = &n->links[bearer_id];

	 
	if (unlikely(usr == LINK_PROTOCOL)) {
		if (unlikely(skb_linearize(skb))) {
			tipc_node_put(n);
			goto discard;
		}
		hdr = buf_msg(skb);
		tipc_node_bc_sync_rcv(n, hdr, bearer_id, &xmitq);
	} else if (unlikely(tipc_link_acked(n->bc_entry.link) != bc_ack)) {
		tipc_bcast_ack_rcv(net, n->bc_entry.link, hdr);
	}

	 
	tipc_node_read_lock(n);
	if (likely((n->state == SELF_UP_PEER_UP) && (usr != TUNNEL_PROTOCOL))) {
		spin_lock_bh(&le->lock);
		if (le->link) {
			rc = tipc_link_rcv(le->link, skb, &xmitq);
			skb = NULL;
		}
		spin_unlock_bh(&le->lock);
	}
	tipc_node_read_unlock(n);

	 
	if (unlikely(skb)) {
		if (unlikely(skb_linearize(skb)))
			goto out_node_put;
		tipc_node_write_lock(n);
		if (tipc_node_check_state(n, skb, bearer_id, &xmitq)) {
			if (le->link) {
				rc = tipc_link_rcv(le->link, skb, &xmitq);
				skb = NULL;
			}
		}
		tipc_node_write_unlock(n);
	}

	if (unlikely(rc & TIPC_LINK_UP_EVT))
		tipc_node_link_up(n, bearer_id, &xmitq);

	if (unlikely(rc & TIPC_LINK_DOWN_EVT))
		tipc_node_link_down(n, bearer_id, false);

	if (unlikely(!skb_queue_empty(&n->bc_entry.namedq)))
		tipc_named_rcv(net, &n->bc_entry.namedq,
			       &n->bc_entry.named_rcv_nxt,
			       &n->bc_entry.named_open);

	if (unlikely(!skb_queue_empty(&n->bc_entry.inputq1)))
		tipc_node_mcast_rcv(n);

	if (!skb_queue_empty(&le->inputq))
		tipc_sk_rcv(net, &le->inputq);

	if (!skb_queue_empty(&xmitq))
		tipc_bearer_xmit(net, bearer_id, &xmitq, &le->maddr, n);

out_node_put:
	tipc_node_put(n);
discard:
	kfree_skb(skb);
}

void tipc_node_apply_property(struct net *net, struct tipc_bearer *b,
			      int prop)
{
	struct tipc_net *tn = tipc_net(net);
	int bearer_id = b->identity;
	struct sk_buff_head xmitq;
	struct tipc_link_entry *e;
	struct tipc_node *n;

	__skb_queue_head_init(&xmitq);

	rcu_read_lock();

	list_for_each_entry_rcu(n, &tn->node_list, list) {
		tipc_node_write_lock(n);
		e = &n->links[bearer_id];
		if (e->link) {
			if (prop == TIPC_NLA_PROP_TOL)
				tipc_link_set_tolerance(e->link, b->tolerance,
							&xmitq);
			else if (prop == TIPC_NLA_PROP_MTU)
				tipc_link_set_mtu(e->link, b->mtu);

			 
			e->mtu = tipc_link_mss(e->link);
		}

		tipc_node_write_unlock(n);
		tipc_bearer_xmit(net, bearer_id, &xmitq, &e->maddr, NULL);
	}

	rcu_read_unlock();
}

int tipc_nl_peer_rm(struct sk_buff *skb, struct genl_info *info)
{
	struct net *net = sock_net(skb->sk);
	struct tipc_net *tn = net_generic(net, tipc_net_id);
	struct nlattr *attrs[TIPC_NLA_NET_MAX + 1];
	struct tipc_node *peer, *temp_node;
	u8 node_id[NODE_ID_LEN];
	u64 *w0 = (u64 *)&node_id[0];
	u64 *w1 = (u64 *)&node_id[8];
	u32 addr;
	int err;

	 
	if (!info->attrs[TIPC_NLA_NET])
		return -EINVAL;

	err = nla_parse_nested_deprecated(attrs, TIPC_NLA_NET_MAX,
					  info->attrs[TIPC_NLA_NET],
					  tipc_nl_net_policy, info->extack);
	if (err)
		return err;

	 
	if (attrs[TIPC_NLA_NET_ADDR]) {
		addr = nla_get_u32(attrs[TIPC_NLA_NET_ADDR]);
		if (!addr)
			return -EINVAL;
	}

	if (attrs[TIPC_NLA_NET_NODEID]) {
		if (!attrs[TIPC_NLA_NET_NODEID_W1])
			return -EINVAL;
		*w0 = nla_get_u64(attrs[TIPC_NLA_NET_NODEID]);
		*w1 = nla_get_u64(attrs[TIPC_NLA_NET_NODEID_W1]);
		addr = hash128to32(node_id);
	}

	if (in_own_node(net, addr))
		return -ENOTSUPP;

	spin_lock_bh(&tn->node_list_lock);
	peer = tipc_node_find(net, addr);
	if (!peer) {
		spin_unlock_bh(&tn->node_list_lock);
		return -ENXIO;
	}

	tipc_node_write_lock(peer);
	if (peer->state != SELF_DOWN_PEER_DOWN &&
	    peer->state != SELF_DOWN_PEER_LEAVING) {
		tipc_node_write_unlock(peer);
		err = -EBUSY;
		goto err_out;
	}

	tipc_node_clear_links(peer);
	tipc_node_write_unlock(peer);
	tipc_node_delete(peer);

	 
	tn->capabilities = TIPC_NODE_CAPABILITIES;
	list_for_each_entry_rcu(temp_node, &tn->node_list, list) {
		tn->capabilities &= temp_node->capabilities;
	}
	tipc_bcast_toggle_rcast(net, (tn->capabilities & TIPC_BCAST_RCAST));
	err = 0;
err_out:
	tipc_node_put(peer);
	spin_unlock_bh(&tn->node_list_lock);

	return err;
}

int tipc_nl_node_dump(struct sk_buff *skb, struct netlink_callback *cb)
{
	int err;
	struct net *net = sock_net(skb->sk);
	struct tipc_net *tn = net_generic(net, tipc_net_id);
	int done = cb->args[0];
	int last_addr = cb->args[1];
	struct tipc_node *node;
	struct tipc_nl_msg msg;

	if (done)
		return 0;

	msg.skb = skb;
	msg.portid = NETLINK_CB(cb->skb).portid;
	msg.seq = cb->nlh->nlmsg_seq;

	rcu_read_lock();
	if (last_addr) {
		node = tipc_node_find(net, last_addr);
		if (!node) {
			rcu_read_unlock();
			 
			cb->prev_seq = 1;
			return -EPIPE;
		}
		tipc_node_put(node);
	}

	list_for_each_entry_rcu(node, &tn->node_list, list) {
		if (node->preliminary)
			continue;
		if (last_addr) {
			if (node->addr == last_addr)
				last_addr = 0;
			else
				continue;
		}

		tipc_node_read_lock(node);
		err = __tipc_nl_add_node(&msg, node);
		if (err) {
			last_addr = node->addr;
			tipc_node_read_unlock(node);
			goto out;
		}

		tipc_node_read_unlock(node);
	}
	done = 1;
out:
	cb->args[0] = done;
	cb->args[1] = last_addr;
	rcu_read_unlock();

	return skb->len;
}

 
static struct tipc_node *tipc_node_find_by_name(struct net *net,
						const char *link_name,
						unsigned int *bearer_id)
{
	struct tipc_net *tn = net_generic(net, tipc_net_id);
	struct tipc_link *l;
	struct tipc_node *n;
	struct tipc_node *found_node = NULL;
	int i;

	*bearer_id = 0;
	rcu_read_lock();
	list_for_each_entry_rcu(n, &tn->node_list, list) {
		tipc_node_read_lock(n);
		for (i = 0; i < MAX_BEARERS; i++) {
			l = n->links[i].link;
			if (l && !strcmp(tipc_link_name(l), link_name)) {
				*bearer_id = i;
				found_node = n;
				break;
			}
		}
		tipc_node_read_unlock(n);
		if (found_node)
			break;
	}
	rcu_read_unlock();

	return found_node;
}

int tipc_nl_node_set_link(struct sk_buff *skb, struct genl_info *info)
{
	int err;
	int res = 0;
	int bearer_id;
	char *name;
	struct tipc_link *link;
	struct tipc_node *node;
	struct sk_buff_head xmitq;
	struct nlattr *attrs[TIPC_NLA_LINK_MAX + 1];
	struct net *net = sock_net(skb->sk);

	__skb_queue_head_init(&xmitq);

	if (!info->attrs[TIPC_NLA_LINK])
		return -EINVAL;

	err = nla_parse_nested_deprecated(attrs, TIPC_NLA_LINK_MAX,
					  info->attrs[TIPC_NLA_LINK],
					  tipc_nl_link_policy, info->extack);
	if (err)
		return err;

	if (!attrs[TIPC_NLA_LINK_NAME])
		return -EINVAL;

	name = nla_data(attrs[TIPC_NLA_LINK_NAME]);

	if (strcmp(name, tipc_bclink_name) == 0)
		return tipc_nl_bc_link_set(net, attrs);

	node = tipc_node_find_by_name(net, name, &bearer_id);
	if (!node)
		return -EINVAL;

	tipc_node_read_lock(node);

	link = node->links[bearer_id].link;
	if (!link) {
		res = -EINVAL;
		goto out;
	}

	if (attrs[TIPC_NLA_LINK_PROP]) {
		struct nlattr *props[TIPC_NLA_PROP_MAX + 1];

		err = tipc_nl_parse_link_prop(attrs[TIPC_NLA_LINK_PROP], props);
		if (err) {
			res = err;
			goto out;
		}

		if (props[TIPC_NLA_PROP_TOL]) {
			u32 tol;

			tol = nla_get_u32(props[TIPC_NLA_PROP_TOL]);
			tipc_link_set_tolerance(link, tol, &xmitq);
		}
		if (props[TIPC_NLA_PROP_PRIO]) {
			u32 prio;

			prio = nla_get_u32(props[TIPC_NLA_PROP_PRIO]);
			tipc_link_set_prio(link, prio, &xmitq);
		}
		if (props[TIPC_NLA_PROP_WIN]) {
			u32 max_win;

			max_win = nla_get_u32(props[TIPC_NLA_PROP_WIN]);
			tipc_link_set_queue_limits(link,
						   tipc_link_min_win(link),
						   max_win);
		}
	}

out:
	tipc_node_read_unlock(node);
	tipc_bearer_xmit(net, bearer_id, &xmitq, &node->links[bearer_id].maddr,
			 NULL);
	return res;
}

int tipc_nl_node_get_link(struct sk_buff *skb, struct genl_info *info)
{
	struct net *net = genl_info_net(info);
	struct nlattr *attrs[TIPC_NLA_LINK_MAX + 1];
	struct tipc_nl_msg msg;
	char *name;
	int err;

	msg.portid = info->snd_portid;
	msg.seq = info->snd_seq;

	if (!info->attrs[TIPC_NLA_LINK])
		return -EINVAL;

	err = nla_parse_nested_deprecated(attrs, TIPC_NLA_LINK_MAX,
					  info->attrs[TIPC_NLA_LINK],
					  tipc_nl_link_policy, info->extack);
	if (err)
		return err;

	if (!attrs[TIPC_NLA_LINK_NAME])
		return -EINVAL;

	name = nla_data(attrs[TIPC_NLA_LINK_NAME]);

	msg.skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg.skb)
		return -ENOMEM;

	if (strcmp(name, tipc_bclink_name) == 0) {
		err = tipc_nl_add_bc_link(net, &msg, tipc_net(net)->bcl);
		if (err)
			goto err_free;
	} else {
		int bearer_id;
		struct tipc_node *node;
		struct tipc_link *link;

		node = tipc_node_find_by_name(net, name, &bearer_id);
		if (!node) {
			err = -EINVAL;
			goto err_free;
		}

		tipc_node_read_lock(node);
		link = node->links[bearer_id].link;
		if (!link) {
			tipc_node_read_unlock(node);
			err = -EINVAL;
			goto err_free;
		}

		err = __tipc_nl_add_link(net, &msg, link, 0);
		tipc_node_read_unlock(node);
		if (err)
			goto err_free;
	}

	return genlmsg_reply(msg.skb, info);

err_free:
	nlmsg_free(msg.skb);
	return err;
}

int tipc_nl_node_reset_link_stats(struct sk_buff *skb, struct genl_info *info)
{
	int err;
	char *link_name;
	unsigned int bearer_id;
	struct tipc_link *link;
	struct tipc_node *node;
	struct nlattr *attrs[TIPC_NLA_LINK_MAX + 1];
	struct net *net = sock_net(skb->sk);
	struct tipc_net *tn = tipc_net(net);
	struct tipc_link_entry *le;

	if (!info->attrs[TIPC_NLA_LINK])
		return -EINVAL;

	err = nla_parse_nested_deprecated(attrs, TIPC_NLA_LINK_MAX,
					  info->attrs[TIPC_NLA_LINK],
					  tipc_nl_link_policy, info->extack);
	if (err)
		return err;

	if (!attrs[TIPC_NLA_LINK_NAME])
		return -EINVAL;

	link_name = nla_data(attrs[TIPC_NLA_LINK_NAME]);

	err = -EINVAL;
	if (!strcmp(link_name, tipc_bclink_name)) {
		err = tipc_bclink_reset_stats(net, tipc_bc_sndlink(net));
		if (err)
			return err;
		return 0;
	} else if (strstr(link_name, tipc_bclink_name)) {
		rcu_read_lock();
		list_for_each_entry_rcu(node, &tn->node_list, list) {
			tipc_node_read_lock(node);
			link = node->bc_entry.link;
			if (link && !strcmp(link_name, tipc_link_name(link))) {
				err = tipc_bclink_reset_stats(net, link);
				tipc_node_read_unlock(node);
				break;
			}
			tipc_node_read_unlock(node);
		}
		rcu_read_unlock();
		return err;
	}

	node = tipc_node_find_by_name(net, link_name, &bearer_id);
	if (!node)
		return -EINVAL;

	le = &node->links[bearer_id];
	tipc_node_read_lock(node);
	spin_lock_bh(&le->lock);
	link = node->links[bearer_id].link;
	if (!link) {
		spin_unlock_bh(&le->lock);
		tipc_node_read_unlock(node);
		return -EINVAL;
	}
	tipc_link_reset_stats(link);
	spin_unlock_bh(&le->lock);
	tipc_node_read_unlock(node);
	return 0;
}

 
static int __tipc_nl_add_node_links(struct net *net, struct tipc_nl_msg *msg,
				    struct tipc_node *node, u32 *prev_link,
				    bool bc_link)
{
	u32 i;
	int err;

	for (i = *prev_link; i < MAX_BEARERS; i++) {
		*prev_link = i;

		if (!node->links[i].link)
			continue;

		err = __tipc_nl_add_link(net, msg,
					 node->links[i].link, NLM_F_MULTI);
		if (err)
			return err;
	}

	if (bc_link) {
		*prev_link = i;
		err = tipc_nl_add_bc_link(net, msg, node->bc_entry.link);
		if (err)
			return err;
	}

	*prev_link = 0;

	return 0;
}

int tipc_nl_node_dump_link(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct net *net = sock_net(skb->sk);
	struct nlattr **attrs = genl_dumpit_info(cb)->info.attrs;
	struct nlattr *link[TIPC_NLA_LINK_MAX + 1];
	struct tipc_net *tn = net_generic(net, tipc_net_id);
	struct tipc_node *node;
	struct tipc_nl_msg msg;
	u32 prev_node = cb->args[0];
	u32 prev_link = cb->args[1];
	int done = cb->args[2];
	bool bc_link = cb->args[3];
	int err;

	if (done)
		return 0;

	if (!prev_node) {
		 
		if (attrs && attrs[TIPC_NLA_LINK]) {
			err = nla_parse_nested_deprecated(link,
							  TIPC_NLA_LINK_MAX,
							  attrs[TIPC_NLA_LINK],
							  tipc_nl_link_policy,
							  NULL);
			if (unlikely(err))
				return err;
			if (unlikely(!link[TIPC_NLA_LINK_BROADCAST]))
				return -EINVAL;
			bc_link = true;
		}
	}

	msg.skb = skb;
	msg.portid = NETLINK_CB(cb->skb).portid;
	msg.seq = cb->nlh->nlmsg_seq;

	rcu_read_lock();
	if (prev_node) {
		node = tipc_node_find(net, prev_node);
		if (!node) {
			 
			cb->prev_seq = 1;
			goto out;
		}
		tipc_node_put(node);

		list_for_each_entry_continue_rcu(node, &tn->node_list,
						 list) {
			tipc_node_read_lock(node);
			err = __tipc_nl_add_node_links(net, &msg, node,
						       &prev_link, bc_link);
			tipc_node_read_unlock(node);
			if (err)
				goto out;

			prev_node = node->addr;
		}
	} else {
		err = tipc_nl_add_bc_link(net, &msg, tn->bcl);
		if (err)
			goto out;

		list_for_each_entry_rcu(node, &tn->node_list, list) {
			tipc_node_read_lock(node);
			err = __tipc_nl_add_node_links(net, &msg, node,
						       &prev_link, bc_link);
			tipc_node_read_unlock(node);
			if (err)
				goto out;

			prev_node = node->addr;
		}
	}
	done = 1;
out:
	rcu_read_unlock();

	cb->args[0] = prev_node;
	cb->args[1] = prev_link;
	cb->args[2] = done;
	cb->args[3] = bc_link;

	return skb->len;
}

int tipc_nl_node_set_monitor(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[TIPC_NLA_MON_MAX + 1];
	struct net *net = sock_net(skb->sk);
	int err;

	if (!info->attrs[TIPC_NLA_MON])
		return -EINVAL;

	err = nla_parse_nested_deprecated(attrs, TIPC_NLA_MON_MAX,
					  info->attrs[TIPC_NLA_MON],
					  tipc_nl_monitor_policy,
					  info->extack);
	if (err)
		return err;

	if (attrs[TIPC_NLA_MON_ACTIVATION_THRESHOLD]) {
		u32 val;

		val = nla_get_u32(attrs[TIPC_NLA_MON_ACTIVATION_THRESHOLD]);
		err = tipc_nl_monitor_set_threshold(net, val);
		if (err)
			return err;
	}

	return 0;
}

static int __tipc_nl_add_monitor_prop(struct net *net, struct tipc_nl_msg *msg)
{
	struct nlattr *attrs;
	void *hdr;
	u32 val;

	hdr = genlmsg_put(msg->skb, msg->portid, msg->seq, &tipc_genl_family,
			  0, TIPC_NL_MON_GET);
	if (!hdr)
		return -EMSGSIZE;

	attrs = nla_nest_start_noflag(msg->skb, TIPC_NLA_MON);
	if (!attrs)
		goto msg_full;

	val = tipc_nl_monitor_get_threshold(net);

	if (nla_put_u32(msg->skb, TIPC_NLA_MON_ACTIVATION_THRESHOLD, val))
		goto attr_msg_full;

	nla_nest_end(msg->skb, attrs);
	genlmsg_end(msg->skb, hdr);

	return 0;

attr_msg_full:
	nla_nest_cancel(msg->skb, attrs);
msg_full:
	genlmsg_cancel(msg->skb, hdr);

	return -EMSGSIZE;
}

int tipc_nl_node_get_monitor(struct sk_buff *skb, struct genl_info *info)
{
	struct net *net = sock_net(skb->sk);
	struct tipc_nl_msg msg;
	int err;

	msg.skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg.skb)
		return -ENOMEM;
	msg.portid = info->snd_portid;
	msg.seq = info->snd_seq;

	err = __tipc_nl_add_monitor_prop(net, &msg);
	if (err) {
		nlmsg_free(msg.skb);
		return err;
	}

	return genlmsg_reply(msg.skb, info);
}

int tipc_nl_node_dump_monitor(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct net *net = sock_net(skb->sk);
	u32 prev_bearer = cb->args[0];
	struct tipc_nl_msg msg;
	int bearer_id;
	int err;

	if (prev_bearer == MAX_BEARERS)
		return 0;

	msg.skb = skb;
	msg.portid = NETLINK_CB(cb->skb).portid;
	msg.seq = cb->nlh->nlmsg_seq;

	rtnl_lock();
	for (bearer_id = prev_bearer; bearer_id < MAX_BEARERS; bearer_id++) {
		err = __tipc_nl_add_monitor(net, &msg, bearer_id);
		if (err)
			break;
	}
	rtnl_unlock();
	cb->args[0] = bearer_id;

	return skb->len;
}

int tipc_nl_node_dump_monitor_peer(struct sk_buff *skb,
				   struct netlink_callback *cb)
{
	struct net *net = sock_net(skb->sk);
	u32 prev_node = cb->args[1];
	u32 bearer_id = cb->args[2];
	int done = cb->args[0];
	struct tipc_nl_msg msg;
	int err;

	if (!prev_node) {
		struct nlattr **attrs = genl_dumpit_info(cb)->info.attrs;
		struct nlattr *mon[TIPC_NLA_MON_MAX + 1];

		if (!attrs[TIPC_NLA_MON])
			return -EINVAL;

		err = nla_parse_nested_deprecated(mon, TIPC_NLA_MON_MAX,
						  attrs[TIPC_NLA_MON],
						  tipc_nl_monitor_policy,
						  NULL);
		if (err)
			return err;

		if (!mon[TIPC_NLA_MON_REF])
			return -EINVAL;

		bearer_id = nla_get_u32(mon[TIPC_NLA_MON_REF]);

		if (bearer_id >= MAX_BEARERS)
			return -EINVAL;
	}

	if (done)
		return 0;

	msg.skb = skb;
	msg.portid = NETLINK_CB(cb->skb).portid;
	msg.seq = cb->nlh->nlmsg_seq;

	rtnl_lock();
	err = tipc_nl_add_monitor_peer(net, &msg, bearer_id, &prev_node);
	if (!err)
		done = 1;

	rtnl_unlock();
	cb->args[0] = done;
	cb->args[1] = prev_node;
	cb->args[2] = bearer_id;

	return skb->len;
}

#ifdef CONFIG_TIPC_CRYPTO
static int tipc_nl_retrieve_key(struct nlattr **attrs,
				struct tipc_aead_key **pkey)
{
	struct nlattr *attr = attrs[TIPC_NLA_NODE_KEY];
	struct tipc_aead_key *key;

	if (!attr)
		return -ENODATA;

	if (nla_len(attr) < sizeof(*key))
		return -EINVAL;
	key = (struct tipc_aead_key *)nla_data(attr);
	if (key->keylen > TIPC_AEAD_KEYLEN_MAX ||
	    nla_len(attr) < tipc_aead_key_size(key))
		return -EINVAL;

	*pkey = key;
	return 0;
}

static int tipc_nl_retrieve_nodeid(struct nlattr **attrs, u8 **node_id)
{
	struct nlattr *attr = attrs[TIPC_NLA_NODE_ID];

	if (!attr)
		return -ENODATA;

	if (nla_len(attr) < TIPC_NODEID_LEN)
		return -EINVAL;

	*node_id = (u8 *)nla_data(attr);
	return 0;
}

static int tipc_nl_retrieve_rekeying(struct nlattr **attrs, u32 *intv)
{
	struct nlattr *attr = attrs[TIPC_NLA_NODE_REKEYING];

	if (!attr)
		return -ENODATA;

	*intv = nla_get_u32(attr);
	return 0;
}

static int __tipc_nl_node_set_key(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attrs[TIPC_NLA_NODE_MAX + 1];
	struct net *net = sock_net(skb->sk);
	struct tipc_crypto *tx = tipc_net(net)->crypto_tx, *c = tx;
	struct tipc_node *n = NULL;
	struct tipc_aead_key *ukey;
	bool rekeying = true, master_key = false;
	u8 *id, *own_id, mode;
	u32 intv = 0;
	int rc = 0;

	if (!info->attrs[TIPC_NLA_NODE])
		return -EINVAL;

	rc = nla_parse_nested(attrs, TIPC_NLA_NODE_MAX,
			      info->attrs[TIPC_NLA_NODE],
			      tipc_nl_node_policy, info->extack);
	if (rc)
		return rc;

	own_id = tipc_own_id(net);
	if (!own_id) {
		GENL_SET_ERR_MSG(info, "not found own node identity (set id?)");
		return -EPERM;
	}

	rc = tipc_nl_retrieve_rekeying(attrs, &intv);
	if (rc == -ENODATA)
		rekeying = false;

	rc = tipc_nl_retrieve_key(attrs, &ukey);
	if (rc == -ENODATA && rekeying)
		goto rekeying;
	else if (rc)
		return rc;

	rc = tipc_aead_key_validate(ukey, info);
	if (rc)
		return rc;

	rc = tipc_nl_retrieve_nodeid(attrs, &id);
	switch (rc) {
	case -ENODATA:
		mode = CLUSTER_KEY;
		master_key = !!(attrs[TIPC_NLA_NODE_KEY_MASTER]);
		break;
	case 0:
		mode = PER_NODE_KEY;
		if (memcmp(id, own_id, NODE_ID_LEN)) {
			n = tipc_node_find_by_id(net, id) ?:
				tipc_node_create(net, 0, id, 0xffffu, 0, true);
			if (unlikely(!n))
				return -ENOMEM;
			c = n->crypto_rx;
		}
		break;
	default:
		return rc;
	}

	 
	rc = tipc_crypto_key_init(c, ukey, mode, master_key);
	if (n)
		tipc_node_put(n);

	if (unlikely(rc < 0)) {
		GENL_SET_ERR_MSG(info, "unable to initiate or attach new key");
		return rc;
	} else if (c == tx) {
		 
		if (!master_key && tipc_crypto_key_distr(tx, rc, NULL))
			GENL_SET_ERR_MSG(info, "failed to replicate new key");
rekeying:
		 
		tipc_crypto_rekeying_sched(tx, rekeying, intv);
	}

	return 0;
}

int tipc_nl_node_set_key(struct sk_buff *skb, struct genl_info *info)
{
	int err;

	rtnl_lock();
	err = __tipc_nl_node_set_key(skb, info);
	rtnl_unlock();

	return err;
}

static int __tipc_nl_node_flush_key(struct sk_buff *skb,
				    struct genl_info *info)
{
	struct net *net = sock_net(skb->sk);
	struct tipc_net *tn = tipc_net(net);
	struct tipc_node *n;

	tipc_crypto_key_flush(tn->crypto_tx);
	rcu_read_lock();
	list_for_each_entry_rcu(n, &tn->node_list, list)
		tipc_crypto_key_flush(n->crypto_rx);
	rcu_read_unlock();

	return 0;
}

int tipc_nl_node_flush_key(struct sk_buff *skb, struct genl_info *info)
{
	int err;

	rtnl_lock();
	err = __tipc_nl_node_flush_key(skb, info);
	rtnl_unlock();

	return err;
}
#endif

 
int tipc_node_dump(struct tipc_node *n, bool more, char *buf)
{
	int i = 0;
	size_t sz = (more) ? NODE_LMAX : NODE_LMIN;

	if (!n) {
		i += scnprintf(buf, sz, "node data: (null)\n");
		return i;
	}

	i += scnprintf(buf, sz, "node data: %x", n->addr);
	i += scnprintf(buf + i, sz - i, " %x", n->state);
	i += scnprintf(buf + i, sz - i, " %d", n->active_links[0]);
	i += scnprintf(buf + i, sz - i, " %d", n->active_links[1]);
	i += scnprintf(buf + i, sz - i, " %x", n->action_flags);
	i += scnprintf(buf + i, sz - i, " %u", n->failover_sent);
	i += scnprintf(buf + i, sz - i, " %u", n->sync_point);
	i += scnprintf(buf + i, sz - i, " %d", n->link_cnt);
	i += scnprintf(buf + i, sz - i, " %u", n->working_links);
	i += scnprintf(buf + i, sz - i, " %x", n->capabilities);
	i += scnprintf(buf + i, sz - i, " %lu\n", n->keepalive_intv);

	if (!more)
		return i;

	i += scnprintf(buf + i, sz - i, "link_entry[0]:\n");
	i += scnprintf(buf + i, sz - i, " mtu: %u\n", n->links[0].mtu);
	i += scnprintf(buf + i, sz - i, " media: ");
	i += tipc_media_addr_printf(buf + i, sz - i, &n->links[0].maddr);
	i += scnprintf(buf + i, sz - i, "\n");
	i += tipc_link_dump(n->links[0].link, TIPC_DUMP_NONE, buf + i);
	i += scnprintf(buf + i, sz - i, " inputq: ");
	i += tipc_list_dump(&n->links[0].inputq, false, buf + i);

	i += scnprintf(buf + i, sz - i, "link_entry[1]:\n");
	i += scnprintf(buf + i, sz - i, " mtu: %u\n", n->links[1].mtu);
	i += scnprintf(buf + i, sz - i, " media: ");
	i += tipc_media_addr_printf(buf + i, sz - i, &n->links[1].maddr);
	i += scnprintf(buf + i, sz - i, "\n");
	i += tipc_link_dump(n->links[1].link, TIPC_DUMP_NONE, buf + i);
	i += scnprintf(buf + i, sz - i, " inputq: ");
	i += tipc_list_dump(&n->links[1].inputq, false, buf + i);

	i += scnprintf(buf + i, sz - i, "bclink:\n ");
	i += tipc_link_dump(n->bc_entry.link, TIPC_DUMP_NONE, buf + i);

	return i;
}

void tipc_node_pre_cleanup_net(struct net *exit_net)
{
	struct tipc_node *n;
	struct tipc_net *tn;
	struct net *tmp;

	rcu_read_lock();
	for_each_net_rcu(tmp) {
		if (tmp == exit_net)
			continue;
		tn = tipc_net(tmp);
		if (!tn)
			continue;
		spin_lock_bh(&tn->node_list_lock);
		list_for_each_entry_rcu(n, &tn->node_list, list) {
			if (!n->peer_net)
				continue;
			if (n->peer_net != exit_net)
				continue;
			tipc_node_write_lock(n);
			n->peer_net = NULL;
			n->peer_hash_mix = 0;
			tipc_node_write_unlock_fast(n);
			break;
		}
		spin_unlock_bh(&tn->node_list_lock);
	}
	rcu_read_unlock();
}
