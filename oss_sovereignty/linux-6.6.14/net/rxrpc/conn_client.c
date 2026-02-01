
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/timer.h>
#include <linux/sched/signal.h>

#include "ar-internal.h"

__read_mostly unsigned int rxrpc_reap_client_connections = 900;
__read_mostly unsigned long rxrpc_conn_idle_client_expiry = 2 * 60 * HZ;
__read_mostly unsigned long rxrpc_conn_idle_client_fast_expiry = 2 * HZ;

static void rxrpc_activate_bundle(struct rxrpc_bundle *bundle)
{
	atomic_inc(&bundle->active);
}

 
static void rxrpc_put_client_connection_id(struct rxrpc_local *local,
					   struct rxrpc_connection *conn)
{
	idr_remove(&local->conn_ids, conn->proto.cid >> RXRPC_CIDSHIFT);
}

 
static void rxrpc_destroy_client_conn_ids(struct rxrpc_local *local)
{
	struct rxrpc_connection *conn;
	int id;

	if (!idr_is_empty(&local->conn_ids)) {
		idr_for_each_entry(&local->conn_ids, conn, id) {
			pr_err("AF_RXRPC: Leaked client conn %p {%d}\n",
			       conn, refcount_read(&conn->ref));
		}
		BUG();
	}

	idr_destroy(&local->conn_ids);
}

 
static struct rxrpc_bundle *rxrpc_alloc_bundle(struct rxrpc_call *call,
					       gfp_t gfp)
{
	static atomic_t rxrpc_bundle_id;
	struct rxrpc_bundle *bundle;

	bundle = kzalloc(sizeof(*bundle), gfp);
	if (bundle) {
		bundle->local		= call->local;
		bundle->peer		= rxrpc_get_peer(call->peer, rxrpc_peer_get_bundle);
		bundle->key		= key_get(call->key);
		bundle->security	= call->security;
		bundle->exclusive	= test_bit(RXRPC_CALL_EXCLUSIVE, &call->flags);
		bundle->upgrade		= test_bit(RXRPC_CALL_UPGRADE, &call->flags);
		bundle->service_id	= call->dest_srx.srx_service;
		bundle->security_level	= call->security_level;
		bundle->debug_id	= atomic_inc_return(&rxrpc_bundle_id);
		refcount_set(&bundle->ref, 1);
		atomic_set(&bundle->active, 1);
		INIT_LIST_HEAD(&bundle->waiting_calls);
		trace_rxrpc_bundle(bundle->debug_id, 1, rxrpc_bundle_new);
	}
	return bundle;
}

struct rxrpc_bundle *rxrpc_get_bundle(struct rxrpc_bundle *bundle,
				      enum rxrpc_bundle_trace why)
{
	int r;

	__refcount_inc(&bundle->ref, &r);
	trace_rxrpc_bundle(bundle->debug_id, r + 1, why);
	return bundle;
}

static void rxrpc_free_bundle(struct rxrpc_bundle *bundle)
{
	trace_rxrpc_bundle(bundle->debug_id, refcount_read(&bundle->ref),
			   rxrpc_bundle_free);
	rxrpc_put_peer(bundle->peer, rxrpc_peer_put_bundle);
	key_put(bundle->key);
	kfree(bundle);
}

void rxrpc_put_bundle(struct rxrpc_bundle *bundle, enum rxrpc_bundle_trace why)
{
	unsigned int id;
	bool dead;
	int r;

	if (bundle) {
		id = bundle->debug_id;
		dead = __refcount_dec_and_test(&bundle->ref, &r);
		trace_rxrpc_bundle(id, r - 1, why);
		if (dead)
			rxrpc_free_bundle(bundle);
	}
}

 
void rxrpc_purge_client_connections(struct rxrpc_local *local)
{
	rxrpc_destroy_client_conn_ids(local);
}

 
static struct rxrpc_connection *
rxrpc_alloc_client_connection(struct rxrpc_bundle *bundle)
{
	struct rxrpc_connection *conn;
	struct rxrpc_local *local = bundle->local;
	struct rxrpc_net *rxnet = local->rxnet;
	int id;

	_enter("");

	conn = rxrpc_alloc_connection(rxnet, GFP_ATOMIC | __GFP_NOWARN);
	if (!conn)
		return ERR_PTR(-ENOMEM);

	id = idr_alloc_cyclic(&local->conn_ids, conn, 1, 0x40000000,
			      GFP_ATOMIC | __GFP_NOWARN);
	if (id < 0) {
		kfree(conn);
		return ERR_PTR(id);
	}

	refcount_set(&conn->ref, 1);
	conn->proto.cid		= id << RXRPC_CIDSHIFT;
	conn->proto.epoch	= local->rxnet->epoch;
	conn->out_clientflag	= RXRPC_CLIENT_INITIATED;
	conn->bundle		= rxrpc_get_bundle(bundle, rxrpc_bundle_get_client_conn);
	conn->local		= rxrpc_get_local(bundle->local, rxrpc_local_get_client_conn);
	conn->peer		= rxrpc_get_peer(bundle->peer, rxrpc_peer_get_client_conn);
	conn->key		= key_get(bundle->key);
	conn->security		= bundle->security;
	conn->exclusive		= bundle->exclusive;
	conn->upgrade		= bundle->upgrade;
	conn->orig_service_id	= bundle->service_id;
	conn->security_level	= bundle->security_level;
	conn->state		= RXRPC_CONN_CLIENT_UNSECURED;
	conn->service_id	= conn->orig_service_id;

	if (conn->security == &rxrpc_no_security)
		conn->state	= RXRPC_CONN_CLIENT;

	atomic_inc(&rxnet->nr_conns);
	write_lock(&rxnet->conn_lock);
	list_add_tail(&conn->proc_link, &rxnet->conn_proc_list);
	write_unlock(&rxnet->conn_lock);

	rxrpc_see_connection(conn, rxrpc_conn_new_client);

	atomic_inc(&rxnet->nr_client_conns);
	trace_rxrpc_client(conn, -1, rxrpc_client_alloc);
	return conn;
}

 
static bool rxrpc_may_reuse_conn(struct rxrpc_connection *conn)
{
	struct rxrpc_net *rxnet;
	int id_cursor, id, distance, limit;

	if (!conn)
		goto dont_reuse;

	rxnet = conn->rxnet;
	if (test_bit(RXRPC_CONN_DONT_REUSE, &conn->flags))
		goto dont_reuse;

	if ((conn->state != RXRPC_CONN_CLIENT_UNSECURED &&
	     conn->state != RXRPC_CONN_CLIENT) ||
	    conn->proto.epoch != rxnet->epoch)
		goto mark_dont_reuse;

	 
	id_cursor = idr_get_cursor(&conn->local->conn_ids);
	id = conn->proto.cid >> RXRPC_CIDSHIFT;
	distance = id - id_cursor;
	if (distance < 0)
		distance = -distance;
	limit = max_t(unsigned long, atomic_read(&rxnet->nr_conns) * 4, 1024);
	if (distance > limit)
		goto mark_dont_reuse;

	return true;

mark_dont_reuse:
	set_bit(RXRPC_CONN_DONT_REUSE, &conn->flags);
dont_reuse:
	return false;
}

 
int rxrpc_look_up_bundle(struct rxrpc_call *call, gfp_t gfp)
{
	struct rxrpc_bundle *bundle, *candidate;
	struct rxrpc_local *local = call->local;
	struct rb_node *p, **pp, *parent;
	long diff;
	bool upgrade = test_bit(RXRPC_CALL_UPGRADE, &call->flags);

	_enter("{%px,%x,%u,%u}",
	       call->peer, key_serial(call->key), call->security_level,
	       upgrade);

	if (test_bit(RXRPC_CALL_EXCLUSIVE, &call->flags)) {
		call->bundle = rxrpc_alloc_bundle(call, gfp);
		return call->bundle ? 0 : -ENOMEM;
	}

	 
	_debug("search 1");
	spin_lock(&local->client_bundles_lock);
	p = local->client_bundles.rb_node;
	while (p) {
		bundle = rb_entry(p, struct rxrpc_bundle, local_node);

#define cmp(X, Y) ((long)(X) - (long)(Y))
		diff = (cmp(bundle->peer, call->peer) ?:
			cmp(bundle->key, call->key) ?:
			cmp(bundle->security_level, call->security_level) ?:
			cmp(bundle->upgrade, upgrade));
#undef cmp
		if (diff < 0)
			p = p->rb_left;
		else if (diff > 0)
			p = p->rb_right;
		else
			goto found_bundle;
	}
	spin_unlock(&local->client_bundles_lock);
	_debug("not found");

	 
	candidate = rxrpc_alloc_bundle(call, gfp);
	if (!candidate)
		return -ENOMEM;

	_debug("search 2");
	spin_lock(&local->client_bundles_lock);
	pp = &local->client_bundles.rb_node;
	parent = NULL;
	while (*pp) {
		parent = *pp;
		bundle = rb_entry(parent, struct rxrpc_bundle, local_node);

#define cmp(X, Y) ((long)(X) - (long)(Y))
		diff = (cmp(bundle->peer, call->peer) ?:
			cmp(bundle->key, call->key) ?:
			cmp(bundle->security_level, call->security_level) ?:
			cmp(bundle->upgrade, upgrade));
#undef cmp
		if (diff < 0)
			pp = &(*pp)->rb_left;
		else if (diff > 0)
			pp = &(*pp)->rb_right;
		else
			goto found_bundle_free;
	}

	_debug("new bundle");
	rb_link_node(&candidate->local_node, parent, pp);
	rb_insert_color(&candidate->local_node, &local->client_bundles);
	call->bundle = rxrpc_get_bundle(candidate, rxrpc_bundle_get_client_call);
	spin_unlock(&local->client_bundles_lock);
	_leave(" = B=%u [new]", call->bundle->debug_id);
	return 0;

found_bundle_free:
	rxrpc_free_bundle(candidate);
found_bundle:
	call->bundle = rxrpc_get_bundle(bundle, rxrpc_bundle_get_client_call);
	rxrpc_activate_bundle(bundle);
	spin_unlock(&local->client_bundles_lock);
	_leave(" = B=%u [found]", call->bundle->debug_id);
	return 0;
}

 
static bool rxrpc_add_conn_to_bundle(struct rxrpc_bundle *bundle,
				     unsigned int slot)
{
	struct rxrpc_connection *conn, *old;
	unsigned int shift = slot * RXRPC_MAXCALLS;
	unsigned int i;

	old = bundle->conns[slot];
	if (old) {
		bundle->conns[slot] = NULL;
		trace_rxrpc_client(old, -1, rxrpc_client_replace);
		rxrpc_put_connection(old, rxrpc_conn_put_noreuse);
	}

	conn = rxrpc_alloc_client_connection(bundle);
	if (IS_ERR(conn)) {
		bundle->alloc_error = PTR_ERR(conn);
		return false;
	}

	rxrpc_activate_bundle(bundle);
	conn->bundle_shift = shift;
	bundle->conns[slot] = conn;
	for (i = 0; i < RXRPC_MAXCALLS; i++)
		set_bit(shift + i, &bundle->avail_chans);
	return true;
}

 
static bool rxrpc_bundle_has_space(struct rxrpc_bundle *bundle)
{
	int slot = -1, i, usable;

	_enter("");

	bundle->alloc_error = 0;

	 
	usable = 0;
	for (i = 0; i < ARRAY_SIZE(bundle->conns); i++) {
		if (rxrpc_may_reuse_conn(bundle->conns[i]))
			usable++;
		else if (slot == -1)
			slot = i;
	}

	if (!usable && bundle->upgrade)
		bundle->try_upgrade = true;

	if (!usable)
		goto alloc_conn;

	if (!bundle->avail_chans &&
	    !bundle->try_upgrade &&
	    usable < ARRAY_SIZE(bundle->conns))
		goto alloc_conn;

	_leave("");
	return usable;

alloc_conn:
	return slot >= 0 ? rxrpc_add_conn_to_bundle(bundle, slot) : false;
}

 
static void rxrpc_activate_one_channel(struct rxrpc_connection *conn,
				       unsigned int channel)
{
	struct rxrpc_channel *chan = &conn->channels[channel];
	struct rxrpc_bundle *bundle = conn->bundle;
	struct rxrpc_call *call = list_entry(bundle->waiting_calls.next,
					     struct rxrpc_call, wait_link);
	u32 call_id = chan->call_counter + 1;

	_enter("C=%x,%u", conn->debug_id, channel);

	list_del_init(&call->wait_link);

	trace_rxrpc_client(conn, channel, rxrpc_client_chan_activate);

	 
	clear_bit(RXRPC_CONN_FINAL_ACK_0 + channel, &conn->flags);
	clear_bit(conn->bundle_shift + channel, &bundle->avail_chans);

	rxrpc_see_call(call, rxrpc_call_see_activate_client);
	call->conn	= rxrpc_get_connection(conn, rxrpc_conn_get_activate_call);
	call->cid	= conn->proto.cid | channel;
	call->call_id	= call_id;
	call->dest_srx.srx_service = conn->service_id;
	call->cong_ssthresh = call->peer->cong_ssthresh;
	if (call->cong_cwnd >= call->cong_ssthresh)
		call->cong_mode = RXRPC_CALL_CONGEST_AVOIDANCE;
	else
		call->cong_mode = RXRPC_CALL_SLOW_START;

	chan->call_id		= call_id;
	chan->call_debug_id	= call->debug_id;
	chan->call		= call;

	rxrpc_see_call(call, rxrpc_call_see_connected);
	trace_rxrpc_connect_call(call);
	call->tx_last_sent = ktime_get_real();
	rxrpc_start_call_timer(call);
	rxrpc_set_call_state(call, RXRPC_CALL_CLIENT_SEND_REQUEST);
	wake_up(&call->waitq);
}

 
static void rxrpc_unidle_conn(struct rxrpc_connection *conn)
{
	if (!list_empty(&conn->cache_link)) {
		list_del_init(&conn->cache_link);
		rxrpc_put_connection(conn, rxrpc_conn_put_unidle);
	}
}

 
static void rxrpc_activate_channels(struct rxrpc_bundle *bundle)
{
	struct rxrpc_connection *conn;
	unsigned long avail, mask;
	unsigned int channel, slot;

	trace_rxrpc_client(NULL, -1, rxrpc_client_activate_chans);

	if (bundle->try_upgrade)
		mask = 1;
	else
		mask = ULONG_MAX;

	while (!list_empty(&bundle->waiting_calls)) {
		avail = bundle->avail_chans & mask;
		if (!avail)
			break;
		channel = __ffs(avail);
		clear_bit(channel, &bundle->avail_chans);

		slot = channel / RXRPC_MAXCALLS;
		conn = bundle->conns[slot];
		if (!conn)
			break;

		if (bundle->try_upgrade)
			set_bit(RXRPC_CONN_PROBING_FOR_UPGRADE, &conn->flags);
		rxrpc_unidle_conn(conn);

		channel &= (RXRPC_MAXCALLS - 1);
		conn->act_chans	|= 1 << channel;
		rxrpc_activate_one_channel(conn, channel);
	}
}

 
void rxrpc_connect_client_calls(struct rxrpc_local *local)
{
	struct rxrpc_call *call;

	while ((call = list_first_entry_or_null(&local->new_client_calls,
						struct rxrpc_call, wait_link))
	       ) {
		struct rxrpc_bundle *bundle = call->bundle;

		spin_lock(&local->client_call_lock);
		list_move_tail(&call->wait_link, &bundle->waiting_calls);
		spin_unlock(&local->client_call_lock);

		if (rxrpc_bundle_has_space(bundle))
			rxrpc_activate_channels(bundle);
	}
}

 
void rxrpc_expose_client_call(struct rxrpc_call *call)
{
	unsigned int channel = call->cid & RXRPC_CHANNELMASK;
	struct rxrpc_connection *conn = call->conn;
	struct rxrpc_channel *chan = &conn->channels[channel];

	if (!test_and_set_bit(RXRPC_CALL_EXPOSED, &call->flags)) {
		 
		chan->call_counter++;
		if (chan->call_counter >= INT_MAX)
			set_bit(RXRPC_CONN_DONT_REUSE, &conn->flags);
		trace_rxrpc_client(conn, channel, rxrpc_client_exposed);

		spin_lock(&call->peer->lock);
		hlist_add_head(&call->error_link, &call->peer->error_targets);
		spin_unlock(&call->peer->lock);
	}
}

 
static void rxrpc_set_client_reap_timer(struct rxrpc_local *local)
{
	if (!local->kill_all_client_conns) {
		unsigned long now = jiffies;
		unsigned long reap_at = now + rxrpc_conn_idle_client_expiry;

		if (local->rxnet->live)
			timer_reduce(&local->client_conn_reap_timer, reap_at);
	}
}

 
void rxrpc_disconnect_client_call(struct rxrpc_bundle *bundle, struct rxrpc_call *call)
{
	struct rxrpc_connection *conn;
	struct rxrpc_channel *chan = NULL;
	struct rxrpc_local *local = bundle->local;
	unsigned int channel;
	bool may_reuse;
	u32 cid;

	_enter("c=%x", call->debug_id);

	 
	conn = call->conn;
	if (!conn) {
		_debug("call is waiting");
		ASSERTCMP(call->call_id, ==, 0);
		ASSERT(!test_bit(RXRPC_CALL_EXPOSED, &call->flags));
		list_del_init(&call->wait_link);
		return;
	}

	cid = call->cid;
	channel = cid & RXRPC_CHANNELMASK;
	chan = &conn->channels[channel];
	trace_rxrpc_client(conn, channel, rxrpc_client_chan_disconnect);

	if (WARN_ON(chan->call != call))
		return;

	may_reuse = rxrpc_may_reuse_conn(conn);

	 
	if (test_bit(RXRPC_CALL_EXPOSED, &call->flags)) {
		_debug("exposed %u,%u", call->call_id, call->abort_code);
		__rxrpc_disconnect_call(conn, call);

		if (test_and_clear_bit(RXRPC_CONN_PROBING_FOR_UPGRADE, &conn->flags)) {
			trace_rxrpc_client(conn, channel, rxrpc_client_to_active);
			bundle->try_upgrade = false;
			if (may_reuse)
				rxrpc_activate_channels(bundle);
		}
	}

	 
	if (may_reuse && !list_empty(&bundle->waiting_calls)) {
		trace_rxrpc_client(conn, channel, rxrpc_client_chan_pass);
		rxrpc_activate_one_channel(conn, channel);
		return;
	}

	 
	if (call->completion == RXRPC_CALL_SUCCEEDED &&
	    test_bit(RXRPC_CALL_EXPOSED, &call->flags)) {
		unsigned long final_ack_at = jiffies + 2;

		WRITE_ONCE(chan->final_ack_at, final_ack_at);
		smp_wmb();  
		set_bit(RXRPC_CONN_FINAL_ACK_0 + channel, &conn->flags);
		rxrpc_reduce_conn_timer(conn, final_ack_at);
	}

	 
	chan->call = NULL;
	set_bit(conn->bundle_shift + channel, &conn->bundle->avail_chans);
	conn->act_chans	&= ~(1 << channel);

	 
	if (!conn->act_chans) {
		trace_rxrpc_client(conn, channel, rxrpc_client_to_idle);
		conn->idle_timestamp = jiffies;

		rxrpc_get_connection(conn, rxrpc_conn_get_idle);
		list_move_tail(&conn->cache_link, &local->idle_client_conns);

		rxrpc_set_client_reap_timer(local);
	}
}

 
static void rxrpc_unbundle_conn(struct rxrpc_connection *conn)
{
	struct rxrpc_bundle *bundle = conn->bundle;
	unsigned int bindex;
	int i;

	_enter("C=%x", conn->debug_id);

	if (conn->flags & RXRPC_CONN_FINAL_ACK_MASK)
		rxrpc_process_delayed_final_acks(conn, true);

	bindex = conn->bundle_shift / RXRPC_MAXCALLS;
	if (bundle->conns[bindex] == conn) {
		_debug("clear slot %u", bindex);
		bundle->conns[bindex] = NULL;
		for (i = 0; i < RXRPC_MAXCALLS; i++)
			clear_bit(conn->bundle_shift + i, &bundle->avail_chans);
		rxrpc_put_client_connection_id(bundle->local, conn);
		rxrpc_deactivate_bundle(bundle);
		rxrpc_put_connection(conn, rxrpc_conn_put_unbundle);
	}
}

 
void rxrpc_deactivate_bundle(struct rxrpc_bundle *bundle)
{
	struct rxrpc_local *local;
	bool need_put = false;

	if (!bundle)
		return;

	local = bundle->local;
	if (atomic_dec_and_lock(&bundle->active, &local->client_bundles_lock)) {
		if (!bundle->exclusive) {
			_debug("erase bundle");
			rb_erase(&bundle->local_node, &local->client_bundles);
			need_put = true;
		}

		spin_unlock(&local->client_bundles_lock);
		if (need_put)
			rxrpc_put_bundle(bundle, rxrpc_bundle_put_discard);
	}
}

 
void rxrpc_kill_client_conn(struct rxrpc_connection *conn)
{
	struct rxrpc_local *local = conn->local;
	struct rxrpc_net *rxnet = local->rxnet;

	_enter("C=%x", conn->debug_id);

	trace_rxrpc_client(conn, -1, rxrpc_client_cleanup);
	atomic_dec(&rxnet->nr_client_conns);

	rxrpc_put_client_connection_id(local, conn);
}

 
void rxrpc_discard_expired_client_conns(struct rxrpc_local *local)
{
	struct rxrpc_connection *conn;
	unsigned long expiry, conn_expires_at, now;
	unsigned int nr_conns;

	_enter("");

	 
	nr_conns = atomic_read(&local->rxnet->nr_client_conns);

next:
	conn = list_first_entry_or_null(&local->idle_client_conns,
					struct rxrpc_connection, cache_link);
	if (!conn)
		return;

	if (!local->kill_all_client_conns) {
		 
		expiry = rxrpc_conn_idle_client_expiry;
		if (nr_conns > rxrpc_reap_client_connections)
			expiry = rxrpc_conn_idle_client_fast_expiry;
		if (conn->local->service_closed)
			expiry = rxrpc_closed_conn_expiry * HZ;

		conn_expires_at = conn->idle_timestamp + expiry;

		now = READ_ONCE(jiffies);
		if (time_after(conn_expires_at, now))
			goto not_yet_expired;
	}

	atomic_dec(&conn->active);
	trace_rxrpc_client(conn, -1, rxrpc_client_discard);
	list_del_init(&conn->cache_link);

	rxrpc_unbundle_conn(conn);
	 
	rxrpc_put_connection(conn, rxrpc_conn_put_discard_idle);

	nr_conns--;
	goto next;

not_yet_expired:
	 
	_debug("not yet");
	if (!local->kill_all_client_conns)
		timer_reduce(&local->client_conn_reap_timer, conn_expires_at);

	_leave("");
}

 
void rxrpc_clean_up_local_conns(struct rxrpc_local *local)
{
	struct rxrpc_connection *conn;

	_enter("");

	local->kill_all_client_conns = true;

	del_timer_sync(&local->client_conn_reap_timer);

	while ((conn = list_first_entry_or_null(&local->idle_client_conns,
						struct rxrpc_connection, cache_link))) {
		list_del_init(&conn->cache_link);
		atomic_dec(&conn->active);
		trace_rxrpc_client(conn, -1, rxrpc_client_discard);
		rxrpc_unbundle_conn(conn);
		rxrpc_put_connection(conn, rxrpc_conn_put_local_dead);
	}

	_leave(" [culled]");
}
