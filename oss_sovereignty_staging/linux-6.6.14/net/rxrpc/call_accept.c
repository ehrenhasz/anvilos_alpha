
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/errqueue.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/icmp.h>
#include <linux/gfp.h>
#include <linux/circ_buf.h>
#include <net/sock.h>
#include <net/af_rxrpc.h>
#include <net/ip.h>
#include "ar-internal.h"

static void rxrpc_dummy_notify(struct sock *sk, struct rxrpc_call *call,
			       unsigned long user_call_ID)
{
}

 
static int rxrpc_service_prealloc_one(struct rxrpc_sock *rx,
				      struct rxrpc_backlog *b,
				      rxrpc_notify_rx_t notify_rx,
				      rxrpc_user_attach_call_t user_attach_call,
				      unsigned long user_call_ID, gfp_t gfp,
				      unsigned int debug_id)
{
	struct rxrpc_call *call, *xcall;
	struct rxrpc_net *rxnet = rxrpc_net(sock_net(&rx->sk));
	struct rb_node *parent, **pp;
	int max, tmp;
	unsigned int size = RXRPC_BACKLOG_MAX;
	unsigned int head, tail, call_head, call_tail;

	max = rx->sk.sk_max_ack_backlog;
	tmp = rx->sk.sk_ack_backlog;
	if (tmp >= max) {
		_leave(" = -ENOBUFS [full %u]", max);
		return -ENOBUFS;
	}
	max -= tmp;

	 
	call_head = b->call_backlog_head;
	call_tail = READ_ONCE(b->call_backlog_tail);
	tmp = CIRC_CNT(call_head, call_tail, size);
	if (tmp >= max) {
		_leave(" = -ENOBUFS [enough %u]", tmp);
		return -ENOBUFS;
	}
	max = tmp + 1;

	head = b->peer_backlog_head;
	tail = READ_ONCE(b->peer_backlog_tail);
	if (CIRC_CNT(head, tail, size) < max) {
		struct rxrpc_peer *peer;

		peer = rxrpc_alloc_peer(rx->local, gfp, rxrpc_peer_new_prealloc);
		if (!peer)
			return -ENOMEM;
		b->peer_backlog[head] = peer;
		smp_store_release(&b->peer_backlog_head,
				  (head + 1) & (size - 1));
	}

	head = b->conn_backlog_head;
	tail = READ_ONCE(b->conn_backlog_tail);
	if (CIRC_CNT(head, tail, size) < max) {
		struct rxrpc_connection *conn;

		conn = rxrpc_prealloc_service_connection(rxnet, gfp);
		if (!conn)
			return -ENOMEM;
		b->conn_backlog[head] = conn;
		smp_store_release(&b->conn_backlog_head,
				  (head + 1) & (size - 1));
	}

	 
	call = rxrpc_alloc_call(rx, gfp, debug_id);
	if (!call)
		return -ENOMEM;
	call->flags |= (1 << RXRPC_CALL_IS_SERVICE);
	rxrpc_set_call_state(call, RXRPC_CALL_SERVER_PREALLOC);
	__set_bit(RXRPC_CALL_EV_INITIAL_PING, &call->events);

	trace_rxrpc_call(call->debug_id, refcount_read(&call->ref),
			 user_call_ID, rxrpc_call_new_prealloc_service);

	write_lock(&rx->call_lock);

	 
	pp = &rx->calls.rb_node;
	parent = NULL;
	while (*pp) {
		parent = *pp;
		xcall = rb_entry(parent, struct rxrpc_call, sock_node);
		if (user_call_ID < xcall->user_call_ID)
			pp = &(*pp)->rb_left;
		else if (user_call_ID > xcall->user_call_ID)
			pp = &(*pp)->rb_right;
		else
			goto id_in_use;
	}

	call->user_call_ID = user_call_ID;
	call->notify_rx = notify_rx;
	if (user_attach_call) {
		rxrpc_get_call(call, rxrpc_call_get_kernel_service);
		user_attach_call(call, user_call_ID);
	}

	rxrpc_get_call(call, rxrpc_call_get_userid);
	rb_link_node(&call->sock_node, parent, pp);
	rb_insert_color(&call->sock_node, &rx->calls);
	set_bit(RXRPC_CALL_HAS_USERID, &call->flags);

	list_add(&call->sock_link, &rx->sock_calls);

	write_unlock(&rx->call_lock);

	rxnet = call->rxnet;
	spin_lock(&rxnet->call_lock);
	list_add_tail_rcu(&call->link, &rxnet->calls);
	spin_unlock(&rxnet->call_lock);

	b->call_backlog[call_head] = call;
	smp_store_release(&b->call_backlog_head, (call_head + 1) & (size - 1));
	_leave(" = 0 [%d -> %lx]", call->debug_id, user_call_ID);
	return 0;

id_in_use:
	write_unlock(&rx->call_lock);
	rxrpc_cleanup_call(call);
	_leave(" = -EBADSLT");
	return -EBADSLT;
}

 
int rxrpc_service_prealloc(struct rxrpc_sock *rx, gfp_t gfp)
{
	struct rxrpc_backlog *b = rx->backlog;

	if (!b) {
		b = kzalloc(sizeof(struct rxrpc_backlog), gfp);
		if (!b)
			return -ENOMEM;
		rx->backlog = b;
	}

	return 0;
}

 
void rxrpc_discard_prealloc(struct rxrpc_sock *rx)
{
	struct rxrpc_backlog *b = rx->backlog;
	struct rxrpc_net *rxnet = rxrpc_net(sock_net(&rx->sk));
	unsigned int size = RXRPC_BACKLOG_MAX, head, tail;

	if (!b)
		return;
	rx->backlog = NULL;

	 
	spin_lock(&rx->incoming_lock);
	spin_unlock(&rx->incoming_lock);

	head = b->peer_backlog_head;
	tail = b->peer_backlog_tail;
	while (CIRC_CNT(head, tail, size) > 0) {
		struct rxrpc_peer *peer = b->peer_backlog[tail];
		rxrpc_put_local(peer->local, rxrpc_local_put_prealloc_peer);
		kfree(peer);
		tail = (tail + 1) & (size - 1);
	}

	head = b->conn_backlog_head;
	tail = b->conn_backlog_tail;
	while (CIRC_CNT(head, tail, size) > 0) {
		struct rxrpc_connection *conn = b->conn_backlog[tail];
		write_lock(&rxnet->conn_lock);
		list_del(&conn->link);
		list_del(&conn->proc_link);
		write_unlock(&rxnet->conn_lock);
		kfree(conn);
		if (atomic_dec_and_test(&rxnet->nr_conns))
			wake_up_var(&rxnet->nr_conns);
		tail = (tail + 1) & (size - 1);
	}

	head = b->call_backlog_head;
	tail = b->call_backlog_tail;
	while (CIRC_CNT(head, tail, size) > 0) {
		struct rxrpc_call *call = b->call_backlog[tail];
		rcu_assign_pointer(call->socket, rx);
		if (rx->discard_new_call) {
			_debug("discard %lx", call->user_call_ID);
			rx->discard_new_call(call, call->user_call_ID);
			if (call->notify_rx)
				call->notify_rx = rxrpc_dummy_notify;
			rxrpc_put_call(call, rxrpc_call_put_kernel);
		}
		rxrpc_call_completed(call);
		rxrpc_release_call(rx, call);
		rxrpc_put_call(call, rxrpc_call_put_discard_prealloc);
		tail = (tail + 1) & (size - 1);
	}

	kfree(b);
}

 
static struct rxrpc_call *rxrpc_alloc_incoming_call(struct rxrpc_sock *rx,
						    struct rxrpc_local *local,
						    struct rxrpc_peer *peer,
						    struct rxrpc_connection *conn,
						    const struct rxrpc_security *sec,
						    struct sockaddr_rxrpc *peer_srx,
						    struct sk_buff *skb)
{
	struct rxrpc_backlog *b = rx->backlog;
	struct rxrpc_call *call;
	unsigned short call_head, conn_head, peer_head;
	unsigned short call_tail, conn_tail, peer_tail;
	unsigned short call_count, conn_count;

	 
	call_head = smp_load_acquire(&b->call_backlog_head);
	call_tail = b->call_backlog_tail;
	call_count = CIRC_CNT(call_head, call_tail, RXRPC_BACKLOG_MAX);
	conn_head = smp_load_acquire(&b->conn_backlog_head);
	conn_tail = b->conn_backlog_tail;
	conn_count = CIRC_CNT(conn_head, conn_tail, RXRPC_BACKLOG_MAX);
	ASSERTCMP(conn_count, >=, call_count);
	peer_head = smp_load_acquire(&b->peer_backlog_head);
	peer_tail = b->peer_backlog_tail;
	ASSERTCMP(CIRC_CNT(peer_head, peer_tail, RXRPC_BACKLOG_MAX), >=,
		  conn_count);

	if (call_count == 0)
		return NULL;

	if (!conn) {
		if (peer && !rxrpc_get_peer_maybe(peer, rxrpc_peer_get_service_conn))
			peer = NULL;
		if (!peer) {
			peer = b->peer_backlog[peer_tail];
			peer->srx = *peer_srx;
			b->peer_backlog[peer_tail] = NULL;
			smp_store_release(&b->peer_backlog_tail,
					  (peer_tail + 1) &
					  (RXRPC_BACKLOG_MAX - 1));

			rxrpc_new_incoming_peer(local, peer);
		}

		 
		conn = b->conn_backlog[conn_tail];
		b->conn_backlog[conn_tail] = NULL;
		smp_store_release(&b->conn_backlog_tail,
				  (conn_tail + 1) & (RXRPC_BACKLOG_MAX - 1));
		conn->local = rxrpc_get_local(local, rxrpc_local_get_prealloc_conn);
		conn->peer = peer;
		rxrpc_see_connection(conn, rxrpc_conn_see_new_service_conn);
		rxrpc_new_incoming_connection(rx, conn, sec, skb);
	} else {
		rxrpc_get_connection(conn, rxrpc_conn_get_service_conn);
		atomic_inc(&conn->active);
	}

	 
	call = b->call_backlog[call_tail];
	b->call_backlog[call_tail] = NULL;
	smp_store_release(&b->call_backlog_tail,
			  (call_tail + 1) & (RXRPC_BACKLOG_MAX - 1));

	rxrpc_see_call(call, rxrpc_call_see_accept);
	call->local = rxrpc_get_local(conn->local, rxrpc_local_get_call);
	call->conn = conn;
	call->security = conn->security;
	call->security_ix = conn->security_ix;
	call->peer = rxrpc_get_peer(conn->peer, rxrpc_peer_get_accept);
	call->dest_srx = peer->srx;
	call->cong_ssthresh = call->peer->cong_ssthresh;
	call->tx_last_sent = ktime_get_real();
	return call;
}

 
bool rxrpc_new_incoming_call(struct rxrpc_local *local,
			     struct rxrpc_peer *peer,
			     struct rxrpc_connection *conn,
			     struct sockaddr_rxrpc *peer_srx,
			     struct sk_buff *skb)
{
	const struct rxrpc_security *sec = NULL;
	struct rxrpc_skb_priv *sp = rxrpc_skb(skb);
	struct rxrpc_call *call = NULL;
	struct rxrpc_sock *rx;

	_enter("");

	 
	if (sp->hdr.type != RXRPC_PACKET_TYPE_DATA)
		return rxrpc_protocol_error(skb, rxrpc_eproto_no_service_call);

	read_lock(&local->services_lock);

	 
	rx = local->service;
	if (!rx || (sp->hdr.serviceId != rx->srx.srx_service &&
		    sp->hdr.serviceId != rx->second_service)
	    ) {
		if (sp->hdr.type == RXRPC_PACKET_TYPE_DATA &&
		    sp->hdr.seq == 1)
			goto unsupported_service;
		goto discard;
	}

	if (!conn) {
		sec = rxrpc_get_incoming_security(rx, skb);
		if (!sec)
			goto unsupported_security;
	}

	spin_lock(&rx->incoming_lock);
	if (rx->sk.sk_state == RXRPC_SERVER_LISTEN_DISABLED ||
	    rx->sk.sk_state == RXRPC_CLOSE) {
		rxrpc_direct_abort(skb, rxrpc_abort_shut_down,
				   RX_INVALID_OPERATION, -ESHUTDOWN);
		goto no_call;
	}

	call = rxrpc_alloc_incoming_call(rx, local, peer, conn, sec, peer_srx,
					 skb);
	if (!call) {
		skb->mark = RXRPC_SKB_MARK_REJECT_BUSY;
		goto no_call;
	}

	trace_rxrpc_receive(call, rxrpc_receive_incoming,
			    sp->hdr.serial, sp->hdr.seq);

	 
	rxrpc_incoming_call(rx, call, skb);
	conn = call->conn;

	if (rx->notify_new_call)
		rx->notify_new_call(&rx->sk, call, call->user_call_ID);

	spin_lock(&conn->state_lock);
	if (conn->state == RXRPC_CONN_SERVICE_UNSECURED) {
		conn->state = RXRPC_CONN_SERVICE_CHALLENGING;
		set_bit(RXRPC_CONN_EV_CHALLENGE, &call->conn->events);
		rxrpc_queue_conn(call->conn, rxrpc_conn_queue_challenge);
	}
	spin_unlock(&conn->state_lock);

	spin_unlock(&rx->incoming_lock);
	read_unlock(&local->services_lock);

	if (hlist_unhashed(&call->error_link)) {
		spin_lock(&call->peer->lock);
		hlist_add_head(&call->error_link, &call->peer->error_targets);
		spin_unlock(&call->peer->lock);
	}

	_leave(" = %p{%d}", call, call->debug_id);
	rxrpc_input_call_event(call, skb);
	rxrpc_put_call(call, rxrpc_call_put_input);
	return true;

unsupported_service:
	read_unlock(&local->services_lock);
	return rxrpc_direct_abort(skb, rxrpc_abort_service_not_offered,
				  RX_INVALID_OPERATION, -EOPNOTSUPP);
unsupported_security:
	read_unlock(&local->services_lock);
	return rxrpc_direct_abort(skb, rxrpc_abort_service_not_offered,
				  RX_INVALID_OPERATION, -EKEYREJECTED);
no_call:
	spin_unlock(&rx->incoming_lock);
	read_unlock(&local->services_lock);
	_leave(" = f [%u]", skb->mark);
	return false;
discard:
	read_unlock(&local->services_lock);
	return true;
}

 
int rxrpc_user_charge_accept(struct rxrpc_sock *rx, unsigned long user_call_ID)
{
	struct rxrpc_backlog *b = rx->backlog;

	if (rx->sk.sk_state == RXRPC_CLOSE)
		return -ESHUTDOWN;

	return rxrpc_service_prealloc_one(rx, b, NULL, NULL, user_call_ID,
					  GFP_KERNEL,
					  atomic_inc_return(&rxrpc_debug_id));
}

 
int rxrpc_kernel_charge_accept(struct socket *sock,
			       rxrpc_notify_rx_t notify_rx,
			       rxrpc_user_attach_call_t user_attach_call,
			       unsigned long user_call_ID, gfp_t gfp,
			       unsigned int debug_id)
{
	struct rxrpc_sock *rx = rxrpc_sk(sock->sk);
	struct rxrpc_backlog *b = rx->backlog;

	if (sock->sk->sk_state == RXRPC_CLOSE)
		return -ESHUTDOWN;

	return rxrpc_service_prealloc_one(rx, b, notify_rx,
					  user_attach_call, user_call_ID,
					  gfp, debug_id);
}
EXPORT_SYMBOL(rxrpc_kernel_charge_accept);
