
#include <linux/kernel.h>
#include <linux/tcp.h>
#include <linux/rcupdate.h>
#include <net/tcp.h>

void tcp_fastopen_init_key_once(struct net *net)
{
	u8 key[TCP_FASTOPEN_KEY_LENGTH];
	struct tcp_fastopen_context *ctxt;

	rcu_read_lock();
	ctxt = rcu_dereference(net->ipv4.tcp_fastopen_ctx);
	if (ctxt) {
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	 
	get_random_bytes(key, sizeof(key));
	tcp_fastopen_reset_cipher(net, NULL, key, NULL);
}

static void tcp_fastopen_ctx_free(struct rcu_head *head)
{
	struct tcp_fastopen_context *ctx =
	    container_of(head, struct tcp_fastopen_context, rcu);

	kfree_sensitive(ctx);
}

void tcp_fastopen_destroy_cipher(struct sock *sk)
{
	struct tcp_fastopen_context *ctx;

	ctx = rcu_dereference_protected(
			inet_csk(sk)->icsk_accept_queue.fastopenq.ctx, 1);
	if (ctx)
		call_rcu(&ctx->rcu, tcp_fastopen_ctx_free);
}

void tcp_fastopen_ctx_destroy(struct net *net)
{
	struct tcp_fastopen_context *ctxt;

	ctxt = xchg((__force struct tcp_fastopen_context **)&net->ipv4.tcp_fastopen_ctx, NULL);

	if (ctxt)
		call_rcu(&ctxt->rcu, tcp_fastopen_ctx_free);
}

int tcp_fastopen_reset_cipher(struct net *net, struct sock *sk,
			      void *primary_key, void *backup_key)
{
	struct tcp_fastopen_context *ctx, *octx;
	struct fastopen_queue *q;
	int err = 0;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		err = -ENOMEM;
		goto out;
	}

	ctx->key[0].key[0] = get_unaligned_le64(primary_key);
	ctx->key[0].key[1] = get_unaligned_le64(primary_key + 8);
	if (backup_key) {
		ctx->key[1].key[0] = get_unaligned_le64(backup_key);
		ctx->key[1].key[1] = get_unaligned_le64(backup_key + 8);
		ctx->num = 2;
	} else {
		ctx->num = 1;
	}

	if (sk) {
		q = &inet_csk(sk)->icsk_accept_queue.fastopenq;
		octx = xchg((__force struct tcp_fastopen_context **)&q->ctx, ctx);
	} else {
		octx = xchg((__force struct tcp_fastopen_context **)&net->ipv4.tcp_fastopen_ctx, ctx);
	}

	if (octx)
		call_rcu(&octx->rcu, tcp_fastopen_ctx_free);
out:
	return err;
}

int tcp_fastopen_get_cipher(struct net *net, struct inet_connection_sock *icsk,
			    u64 *key)
{
	struct tcp_fastopen_context *ctx;
	int n_keys = 0, i;

	rcu_read_lock();
	if (icsk)
		ctx = rcu_dereference(icsk->icsk_accept_queue.fastopenq.ctx);
	else
		ctx = rcu_dereference(net->ipv4.tcp_fastopen_ctx);
	if (ctx) {
		n_keys = tcp_fastopen_context_len(ctx);
		for (i = 0; i < n_keys; i++) {
			put_unaligned_le64(ctx->key[i].key[0], key + (i * 2));
			put_unaligned_le64(ctx->key[i].key[1], key + (i * 2) + 1);
		}
	}
	rcu_read_unlock();

	return n_keys;
}

static bool __tcp_fastopen_cookie_gen_cipher(struct request_sock *req,
					     struct sk_buff *syn,
					     const siphash_key_t *key,
					     struct tcp_fastopen_cookie *foc)
{
	BUILD_BUG_ON(TCP_FASTOPEN_COOKIE_SIZE != sizeof(u64));

	if (req->rsk_ops->family == AF_INET) {
		const struct iphdr *iph = ip_hdr(syn);

		foc->val[0] = cpu_to_le64(siphash(&iph->saddr,
					  sizeof(iph->saddr) +
					  sizeof(iph->daddr),
					  key));
		foc->len = TCP_FASTOPEN_COOKIE_SIZE;
		return true;
	}
#if IS_ENABLED(CONFIG_IPV6)
	if (req->rsk_ops->family == AF_INET6) {
		const struct ipv6hdr *ip6h = ipv6_hdr(syn);

		foc->val[0] = cpu_to_le64(siphash(&ip6h->saddr,
					  sizeof(ip6h->saddr) +
					  sizeof(ip6h->daddr),
					  key));
		foc->len = TCP_FASTOPEN_COOKIE_SIZE;
		return true;
	}
#endif
	return false;
}

 
static void tcp_fastopen_cookie_gen(struct sock *sk,
				    struct request_sock *req,
				    struct sk_buff *syn,
				    struct tcp_fastopen_cookie *foc)
{
	struct tcp_fastopen_context *ctx;

	rcu_read_lock();
	ctx = tcp_fastopen_get_ctx(sk);
	if (ctx)
		__tcp_fastopen_cookie_gen_cipher(req, syn, &ctx->key[0], foc);
	rcu_read_unlock();
}

 
void tcp_fastopen_add_skb(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (TCP_SKB_CB(skb)->end_seq == tp->rcv_nxt)
		return;

	skb = skb_clone(skb, GFP_ATOMIC);
	if (!skb)
		return;

	skb_dst_drop(skb);
	 
	tp->segs_in = 0;
	tcp_segs_in(tp, skb);
	__skb_pull(skb, tcp_hdrlen(skb));
	sk_forced_mem_schedule(sk, skb->truesize);
	skb_set_owner_r(skb, sk);

	TCP_SKB_CB(skb)->seq++;
	TCP_SKB_CB(skb)->tcp_flags &= ~TCPHDR_SYN;

	tp->rcv_nxt = TCP_SKB_CB(skb)->end_seq;
	__skb_queue_tail(&sk->sk_receive_queue, skb);
	tp->syn_data_acked = 1;

	 
	tp->bytes_received = skb->len;

	if (TCP_SKB_CB(skb)->tcp_flags & TCPHDR_FIN)
		tcp_fin(sk);
}

 
static int tcp_fastopen_cookie_gen_check(struct sock *sk,
					 struct request_sock *req,
					 struct sk_buff *syn,
					 struct tcp_fastopen_cookie *orig,
					 struct tcp_fastopen_cookie *valid_foc)
{
	struct tcp_fastopen_cookie search_foc = { .len = -1 };
	struct tcp_fastopen_cookie *foc = valid_foc;
	struct tcp_fastopen_context *ctx;
	int i, ret = 0;

	rcu_read_lock();
	ctx = tcp_fastopen_get_ctx(sk);
	if (!ctx)
		goto out;
	for (i = 0; i < tcp_fastopen_context_len(ctx); i++) {
		__tcp_fastopen_cookie_gen_cipher(req, syn, &ctx->key[i], foc);
		if (tcp_fastopen_cookie_match(foc, orig)) {
			ret = i + 1;
			goto out;
		}
		foc = &search_foc;
	}
out:
	rcu_read_unlock();
	return ret;
}

static struct sock *tcp_fastopen_create_child(struct sock *sk,
					      struct sk_buff *skb,
					      struct request_sock *req)
{
	struct tcp_sock *tp;
	struct request_sock_queue *queue = &inet_csk(sk)->icsk_accept_queue;
	struct sock *child;
	bool own_req;

	child = inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, NULL,
							 NULL, &own_req);
	if (!child)
		return NULL;

	spin_lock(&queue->fastopenq.lock);
	queue->fastopenq.qlen++;
	spin_unlock(&queue->fastopenq.lock);

	 
	tp = tcp_sk(child);

	rcu_assign_pointer(tp->fastopen_rsk, req);
	tcp_rsk(req)->tfo_listener = true;

	 
	tp->snd_wnd = ntohs(tcp_hdr(skb)->window);
	tp->max_window = tp->snd_wnd;

	 
	req->timeout = tcp_timeout_init(child);
	inet_csk_reset_xmit_timer(child, ICSK_TIME_RETRANS,
				  req->timeout, TCP_RTO_MAX);

	refcount_set(&req->rsk_refcnt, 2);

	 
	tcp_init_transfer(child, BPF_SOCK_OPS_PASSIVE_ESTABLISHED_CB, skb);

	tp->rcv_nxt = TCP_SKB_CB(skb)->seq + 1;

	tcp_fastopen_add_skb(child, skb);

	tcp_rsk(req)->rcv_nxt = tp->rcv_nxt;
	tp->rcv_wup = tp->rcv_nxt;
	 
	return child;
}

static bool tcp_fastopen_queue_check(struct sock *sk)
{
	struct fastopen_queue *fastopenq;
	int max_qlen;

	 
	fastopenq = &inet_csk(sk)->icsk_accept_queue.fastopenq;
	max_qlen = READ_ONCE(fastopenq->max_qlen);
	if (max_qlen == 0)
		return false;

	if (fastopenq->qlen >= max_qlen) {
		struct request_sock *req1;
		spin_lock(&fastopenq->lock);
		req1 = fastopenq->rskq_rst_head;
		if (!req1 || time_after(req1->rsk_timer.expires, jiffies)) {
			__NET_INC_STATS(sock_net(sk),
					LINUX_MIB_TCPFASTOPENLISTENOVERFLOW);
			spin_unlock(&fastopenq->lock);
			return false;
		}
		fastopenq->rskq_rst_head = req1->dl_next;
		fastopenq->qlen--;
		spin_unlock(&fastopenq->lock);
		reqsk_put(req1);
	}
	return true;
}

static bool tcp_fastopen_no_cookie(const struct sock *sk,
				   const struct dst_entry *dst,
				   int flag)
{
	return (READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_fastopen) & flag) ||
	       tcp_sk(sk)->fastopen_no_cookie ||
	       (dst && dst_metric(dst, RTAX_FASTOPEN_NO_COOKIE));
}

 
struct sock *tcp_try_fastopen(struct sock *sk, struct sk_buff *skb,
			      struct request_sock *req,
			      struct tcp_fastopen_cookie *foc,
			      const struct dst_entry *dst)
{
	bool syn_data = TCP_SKB_CB(skb)->end_seq != TCP_SKB_CB(skb)->seq + 1;
	int tcp_fastopen = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_fastopen);
	struct tcp_fastopen_cookie valid_foc = { .len = -1 };
	struct sock *child;
	int ret = 0;

	if (foc->len == 0)  
		NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPFASTOPENCOOKIEREQD);

	if (!((tcp_fastopen & TFO_SERVER_ENABLE) &&
	      (syn_data || foc->len >= 0) &&
	      tcp_fastopen_queue_check(sk))) {
		foc->len = -1;
		return NULL;
	}

	if (tcp_fastopen_no_cookie(sk, dst, TFO_SERVER_COOKIE_NOT_REQD))
		goto fastopen;

	if (foc->len == 0) {
		 
		tcp_fastopen_cookie_gen(sk, req, skb, &valid_foc);
	} else if (foc->len > 0) {
		ret = tcp_fastopen_cookie_gen_check(sk, req, skb, foc,
						    &valid_foc);
		if (!ret) {
			NET_INC_STATS(sock_net(sk),
				      LINUX_MIB_TCPFASTOPENPASSIVEFAIL);
		} else {
			 
fastopen:
			child = tcp_fastopen_create_child(sk, skb, req);
			if (child) {
				if (ret == 2) {
					valid_foc.exp = foc->exp;
					*foc = valid_foc;
					NET_INC_STATS(sock_net(sk),
						      LINUX_MIB_TCPFASTOPENPASSIVEALTKEY);
				} else {
					foc->len = -1;
				}
				NET_INC_STATS(sock_net(sk),
					      LINUX_MIB_TCPFASTOPENPASSIVE);
				return child;
			}
			NET_INC_STATS(sock_net(sk),
				      LINUX_MIB_TCPFASTOPENPASSIVEFAIL);
		}
	}
	valid_foc.exp = foc->exp;
	*foc = valid_foc;
	return NULL;
}

bool tcp_fastopen_cookie_check(struct sock *sk, u16 *mss,
			       struct tcp_fastopen_cookie *cookie)
{
	const struct dst_entry *dst;

	tcp_fastopen_cache_get(sk, mss, cookie);

	 
	if (tcp_fastopen_active_should_disable(sk)) {
		cookie->len = -1;
		return false;
	}

	dst = __sk_dst_get(sk);

	if (tcp_fastopen_no_cookie(sk, dst, TFO_CLIENT_NO_COOKIE)) {
		cookie->len = -1;
		return true;
	}
	if (cookie->len > 0)
		return true;
	tcp_sk(sk)->fastopen_client_fail = TFO_COOKIE_UNAVAILABLE;
	return false;
}

 
bool tcp_fastopen_defer_connect(struct sock *sk, int *err)
{
	struct tcp_fastopen_cookie cookie = { .len = 0 };
	struct tcp_sock *tp = tcp_sk(sk);
	u16 mss;

	if (tp->fastopen_connect && !tp->fastopen_req) {
		if (tcp_fastopen_cookie_check(sk, &mss, &cookie)) {
			inet_set_bit(DEFER_CONNECT, sk);
			return true;
		}

		 
		tp->fastopen_req = kzalloc(sizeof(*tp->fastopen_req),
					   sk->sk_allocation);
		if (tp->fastopen_req)
			tp->fastopen_req->cookie = cookie;
		else
			*err = -ENOBUFS;
	}
	return false;
}
EXPORT_SYMBOL(tcp_fastopen_defer_connect);

 

 
void tcp_fastopen_active_disable(struct sock *sk)
{
	struct net *net = sock_net(sk);

	if (!READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_fastopen_blackhole_timeout))
		return;

	 
	WRITE_ONCE(net->ipv4.tfo_active_disable_stamp, jiffies);

	 
	smp_mb__before_atomic();
	atomic_inc(&net->ipv4.tfo_active_disable_times);

	NET_INC_STATS(net, LINUX_MIB_TCPFASTOPENBLACKHOLE);
}

 
bool tcp_fastopen_active_should_disable(struct sock *sk)
{
	unsigned int tfo_bh_timeout =
		READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_fastopen_blackhole_timeout);
	unsigned long timeout;
	int tfo_da_times;
	int multiplier;

	if (!tfo_bh_timeout)
		return false;

	tfo_da_times = atomic_read(&sock_net(sk)->ipv4.tfo_active_disable_times);
	if (!tfo_da_times)
		return false;

	 
	smp_rmb();

	 
	multiplier = 1 << min(tfo_da_times - 1, 6);

	 
	timeout = READ_ONCE(sock_net(sk)->ipv4.tfo_active_disable_stamp) +
		  multiplier * tfo_bh_timeout * HZ;
	if (time_before(jiffies, timeout))
		return true;

	 
	tcp_sk(sk)->syn_fastopen_ch = 1;
	return false;
}

 
void tcp_fastopen_active_disable_ofo_check(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct dst_entry *dst;
	struct sk_buff *skb;

	if (!tp->syn_fastopen)
		return;

	if (!tp->data_segs_in) {
		skb = skb_rb_first(&tp->out_of_order_queue);
		if (skb && !skb_rb_next(skb)) {
			if (TCP_SKB_CB(skb)->tcp_flags & TCPHDR_FIN) {
				tcp_fastopen_active_disable(sk);
				return;
			}
		}
	} else if (tp->syn_fastopen_ch &&
		   atomic_read(&sock_net(sk)->ipv4.tfo_active_disable_times)) {
		dst = sk_dst_get(sk);
		if (!(dst && dst->dev && (dst->dev->flags & IFF_LOOPBACK)))
			atomic_set(&sock_net(sk)->ipv4.tfo_active_disable_times, 0);
		dst_release(dst);
	}
}

void tcp_fastopen_active_detect_blackhole(struct sock *sk, bool expired)
{
	u32 timeouts = inet_csk(sk)->icsk_retransmits;
	struct tcp_sock *tp = tcp_sk(sk);

	 
	if ((tp->syn_fastopen || tp->syn_data || tp->syn_data_acked) &&
	    (timeouts == 2 || (timeouts < 2 && expired))) {
		tcp_fastopen_active_disable(sk);
		NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPFASTOPENACTIVEFAIL);
	}
}
