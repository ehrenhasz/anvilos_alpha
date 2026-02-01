
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <crypto/hash.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/sched/signal.h>
#include <linux/ip.h>
#include <linux/capability.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/compat.h>
#include <linux/rhashtable.h>

#include <net/ip.h>
#include <net/icmp.h>
#include <net/route.h>
#include <net/ipv6.h>
#include <net/inet_common.h>
#include <net/busy_poll.h>
#include <trace/events/sock.h>

#include <linux/socket.h>  
#include <linux/export.h>
#include <net/sock.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>
#include <net/sctp/stream_sched.h>

 
static bool sctp_writeable(const struct sock *sk);
static void sctp_wfree(struct sk_buff *skb);
static int sctp_wait_for_sndbuf(struct sctp_association *asoc, long *timeo_p,
				size_t msg_len);
static int sctp_wait_for_packet(struct sock *sk, int *err, long *timeo_p);
static int sctp_wait_for_connect(struct sctp_association *, long *timeo_p);
static int sctp_wait_for_accept(struct sock *sk, long timeo);
static void sctp_wait_for_close(struct sock *sk, long timeo);
static void sctp_destruct_sock(struct sock *sk);
static struct sctp_af *sctp_sockaddr_af(struct sctp_sock *opt,
					union sctp_addr *addr, int len);
static int sctp_bindx_add(struct sock *, struct sockaddr *, int);
static int sctp_bindx_rem(struct sock *, struct sockaddr *, int);
static int sctp_send_asconf_add_ip(struct sock *, struct sockaddr *, int);
static int sctp_send_asconf_del_ip(struct sock *, struct sockaddr *, int);
static int sctp_send_asconf(struct sctp_association *asoc,
			    struct sctp_chunk *chunk);
static int sctp_do_bind(struct sock *, union sctp_addr *, int);
static int sctp_autobind(struct sock *sk);
static int sctp_sock_migrate(struct sock *oldsk, struct sock *newsk,
			     struct sctp_association *assoc,
			     enum sctp_socket_type type);

static unsigned long sctp_memory_pressure;
static atomic_long_t sctp_memory_allocated;
static DEFINE_PER_CPU(int, sctp_memory_per_cpu_fw_alloc);
struct percpu_counter sctp_sockets_allocated;

static void sctp_enter_memory_pressure(struct sock *sk)
{
	WRITE_ONCE(sctp_memory_pressure, 1);
}


 
static inline int sctp_wspace(struct sctp_association *asoc)
{
	struct sock *sk = asoc->base.sk;

	return asoc->ep->sndbuf_policy ? sk->sk_sndbuf - asoc->sndbuf_used
				       : sk_stream_wspace(sk);
}

 
static inline void sctp_set_owner_w(struct sctp_chunk *chunk)
{
	struct sctp_association *asoc = chunk->asoc;
	struct sock *sk = asoc->base.sk;

	 
	sctp_association_hold(asoc);

	if (chunk->shkey)
		sctp_auth_shkey_hold(chunk->shkey);

	skb_set_owner_w(chunk->skb, sk);

	chunk->skb->destructor = sctp_wfree;
	 
	skb_shinfo(chunk->skb)->destructor_arg = chunk;

	refcount_add(sizeof(struct sctp_chunk), &sk->sk_wmem_alloc);
	asoc->sndbuf_used += chunk->skb->truesize + sizeof(struct sctp_chunk);
	sk_wmem_queued_add(sk, chunk->skb->truesize + sizeof(struct sctp_chunk));
	sk_mem_charge(sk, chunk->skb->truesize);
}

static void sctp_clear_owner_w(struct sctp_chunk *chunk)
{
	skb_orphan(chunk->skb);
}

#define traverse_and_process()	\
do {				\
	msg = chunk->msg;	\
	if (msg == prev_msg)	\
		continue;	\
	list_for_each_entry(c, &msg->chunks, frag_list) {	\
		if ((clear && asoc->base.sk == c->skb->sk) ||	\
		    (!clear && asoc->base.sk != c->skb->sk))	\
			cb(c);	\
	}			\
	prev_msg = msg;		\
} while (0)

static void sctp_for_each_tx_datachunk(struct sctp_association *asoc,
				       bool clear,
				       void (*cb)(struct sctp_chunk *))

{
	struct sctp_datamsg *msg, *prev_msg = NULL;
	struct sctp_outq *q = &asoc->outqueue;
	struct sctp_chunk *chunk, *c;
	struct sctp_transport *t;

	list_for_each_entry(t, &asoc->peer.transport_addr_list, transports)
		list_for_each_entry(chunk, &t->transmitted, transmitted_list)
			traverse_and_process();

	list_for_each_entry(chunk, &q->retransmit, transmitted_list)
		traverse_and_process();

	list_for_each_entry(chunk, &q->sacked, transmitted_list)
		traverse_and_process();

	list_for_each_entry(chunk, &q->abandoned, transmitted_list)
		traverse_and_process();

	list_for_each_entry(chunk, &q->out_chunk_list, list)
		traverse_and_process();
}

static void sctp_for_each_rx_skb(struct sctp_association *asoc, struct sock *sk,
				 void (*cb)(struct sk_buff *, struct sock *))

{
	struct sk_buff *skb, *tmp;

	sctp_skb_for_each(skb, &asoc->ulpq.lobby, tmp)
		cb(skb, sk);

	sctp_skb_for_each(skb, &asoc->ulpq.reasm, tmp)
		cb(skb, sk);

	sctp_skb_for_each(skb, &asoc->ulpq.reasm_uo, tmp)
		cb(skb, sk);
}

 
static inline int sctp_verify_addr(struct sock *sk, union sctp_addr *addr,
				   int len)
{
	struct sctp_af *af;

	 
	af = sctp_sockaddr_af(sctp_sk(sk), addr, len);
	if (!af)
		return -EINVAL;

	 
	if (!af->addr_valid(addr, sctp_sk(sk), NULL))
		return -EINVAL;

	if (!sctp_sk(sk)->pf->send_verify(sctp_sk(sk), (addr)))
		return -EINVAL;

	return 0;
}

 
struct sctp_association *sctp_id2assoc(struct sock *sk, sctp_assoc_t id)
{
	struct sctp_association *asoc = NULL;

	 
	if (!sctp_style(sk, UDP)) {
		 
		if (!sctp_sstate(sk, ESTABLISHED) && !sctp_sstate(sk, CLOSING))
			return NULL;

		 
		if (!list_empty(&sctp_sk(sk)->ep->asocs))
			asoc = list_entry(sctp_sk(sk)->ep->asocs.next,
					  struct sctp_association, asocs);
		return asoc;
	}

	 
	if (id <= SCTP_ALL_ASSOC)
		return NULL;

	spin_lock_bh(&sctp_assocs_id_lock);
	asoc = (struct sctp_association *)idr_find(&sctp_assocs_id, (int)id);
	if (asoc && (asoc->base.sk != sk || asoc->base.dead))
		asoc = NULL;
	spin_unlock_bh(&sctp_assocs_id_lock);

	return asoc;
}

 
static struct sctp_transport *sctp_addr_id2transport(struct sock *sk,
					      struct sockaddr_storage *addr,
					      sctp_assoc_t id)
{
	struct sctp_association *addr_asoc = NULL, *id_asoc = NULL;
	struct sctp_af *af = sctp_get_af_specific(addr->ss_family);
	union sctp_addr *laddr = (union sctp_addr *)addr;
	struct sctp_transport *transport;

	if (!af || sctp_verify_addr(sk, laddr, af->sockaddr_len))
		return NULL;

	addr_asoc = sctp_endpoint_lookup_assoc(sctp_sk(sk)->ep,
					       laddr,
					       &transport);

	if (!addr_asoc)
		return NULL;

	id_asoc = sctp_id2assoc(sk, id);
	if (id_asoc && (id_asoc != addr_asoc))
		return NULL;

	sctp_get_pf_specific(sk->sk_family)->addr_to_user(sctp_sk(sk),
						(union sctp_addr *)addr);

	return transport;
}

 
static int sctp_bind(struct sock *sk, struct sockaddr *addr, int addr_len)
{
	int retval = 0;

	lock_sock(sk);

	pr_debug("%s: sk:%p, addr:%p, addr_len:%d\n", __func__, sk,
		 addr, addr_len);

	 
	if (!sctp_sk(sk)->ep->base.bind_addr.port)
		retval = sctp_do_bind(sk, (union sctp_addr *)addr,
				      addr_len);
	else
		retval = -EINVAL;

	release_sock(sk);

	return retval;
}

static int sctp_get_port_local(struct sock *, union sctp_addr *);

 
static struct sctp_af *sctp_sockaddr_af(struct sctp_sock *opt,
					union sctp_addr *addr, int len)
{
	struct sctp_af *af;

	 
	if (len < sizeof (struct sockaddr))
		return NULL;

	if (!opt->pf->af_supported(addr->sa.sa_family, opt))
		return NULL;

	if (addr->sa.sa_family == AF_INET6) {
		if (len < SIN6_LEN_RFC2133)
			return NULL;
		 
		if (ipv6_addr_v4mapped(&addr->v6.sin6_addr) &&
		    !opt->pf->af_supported(AF_INET, opt))
			return NULL;
	}

	 
	af = sctp_get_af_specific(addr->sa.sa_family);

	if (len < af->sockaddr_len)
		return NULL;

	return af;
}

static void sctp_auto_asconf_init(struct sctp_sock *sp)
{
	struct net *net = sock_net(&sp->inet.sk);

	if (net->sctp.default_auto_asconf) {
		spin_lock_bh(&net->sctp.addr_wq_lock);
		list_add_tail(&sp->auto_asconf_list, &net->sctp.auto_asconf_splist);
		spin_unlock_bh(&net->sctp.addr_wq_lock);
		sp->do_auto_asconf = 1;
	}
}

 
static int sctp_do_bind(struct sock *sk, union sctp_addr *addr, int len)
{
	struct net *net = sock_net(sk);
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_endpoint *ep = sp->ep;
	struct sctp_bind_addr *bp = &ep->base.bind_addr;
	struct sctp_af *af;
	unsigned short snum;
	int ret = 0;

	 
	af = sctp_sockaddr_af(sp, addr, len);
	if (!af) {
		pr_debug("%s: sk:%p, newaddr:%p, len:%d EINVAL\n",
			 __func__, sk, addr, len);
		return -EINVAL;
	}

	snum = ntohs(addr->v4.sin_port);

	pr_debug("%s: sk:%p, new addr:%pISc, port:%d, new port:%d, len:%d\n",
		 __func__, sk, &addr->sa, bp->port, snum, len);

	 
	if (!sp->pf->bind_verify(sp, addr))
		return -EADDRNOTAVAIL;

	 
	if (bp->port) {
		if (!snum)
			snum = bp->port;
		else if (snum != bp->port) {
			pr_debug("%s: new port %d doesn't match existing port "
				 "%d\n", __func__, snum, bp->port);
			return -EINVAL;
		}
	}

	if (snum && inet_port_requires_bind_service(net, snum) &&
	    !ns_capable(net->user_ns, CAP_NET_BIND_SERVICE))
		return -EACCES;

	 
	if (sctp_bind_addr_match(bp, addr, sp))
		return -EINVAL;

	 
	addr->v4.sin_port = htons(snum);
	if (sctp_get_port_local(sk, addr))
		return -EADDRINUSE;

	 
	if (!bp->port) {
		bp->port = inet_sk(sk)->inet_num;
		sctp_auto_asconf_init(sp);
	}

	 
	ret = sctp_add_bind_addr(bp, addr, af->sockaddr_len,
				 SCTP_ADDR_SRC, GFP_ATOMIC);

	if (ret) {
		sctp_put_port(sk);
		return ret;
	}
	 
	inet_sk(sk)->inet_sport = htons(inet_sk(sk)->inet_num);
	sp->pf->to_sk_saddr(addr, sk);

	return ret;
}

  
static int sctp_send_asconf(struct sctp_association *asoc,
			    struct sctp_chunk *chunk)
{
	int retval = 0;

	 
	if (asoc->addip_last_asconf) {
		list_add_tail(&chunk->list, &asoc->addip_chunk_list);
		goto out;
	}

	 
	sctp_chunk_hold(chunk);
	retval = sctp_primitive_ASCONF(asoc->base.net, asoc, chunk);
	if (retval)
		sctp_chunk_free(chunk);
	else
		asoc->addip_last_asconf = chunk;

out:
	return retval;
}

 
static int sctp_bindx_add(struct sock *sk, struct sockaddr *addrs, int addrcnt)
{
	int cnt;
	int retval = 0;
	void *addr_buf;
	struct sockaddr *sa_addr;
	struct sctp_af *af;

	pr_debug("%s: sk:%p, addrs:%p, addrcnt:%d\n", __func__, sk,
		 addrs, addrcnt);

	addr_buf = addrs;
	for (cnt = 0; cnt < addrcnt; cnt++) {
		 
		sa_addr = addr_buf;
		af = sctp_get_af_specific(sa_addr->sa_family);
		if (!af) {
			retval = -EINVAL;
			goto err_bindx_add;
		}

		retval = sctp_do_bind(sk, (union sctp_addr *)sa_addr,
				      af->sockaddr_len);

		addr_buf += af->sockaddr_len;

err_bindx_add:
		if (retval < 0) {
			 
			if (cnt > 0)
				sctp_bindx_rem(sk, addrs, cnt);
			return retval;
		}
	}

	return retval;
}

 
static int sctp_send_asconf_add_ip(struct sock		*sk,
				   struct sockaddr	*addrs,
				   int 			addrcnt)
{
	struct sctp_sock		*sp;
	struct sctp_endpoint		*ep;
	struct sctp_association		*asoc;
	struct sctp_bind_addr		*bp;
	struct sctp_chunk		*chunk;
	struct sctp_sockaddr_entry	*laddr;
	union sctp_addr			*addr;
	union sctp_addr			saveaddr;
	void				*addr_buf;
	struct sctp_af			*af;
	struct list_head		*p;
	int 				i;
	int 				retval = 0;

	sp = sctp_sk(sk);
	ep = sp->ep;

	if (!ep->asconf_enable)
		return retval;

	pr_debug("%s: sk:%p, addrs:%p, addrcnt:%d\n",
		 __func__, sk, addrs, addrcnt);

	list_for_each_entry(asoc, &ep->asocs, asocs) {
		if (!asoc->peer.asconf_capable)
			continue;

		if (asoc->peer.addip_disabled_mask & SCTP_PARAM_ADD_IP)
			continue;

		if (!sctp_state(asoc, ESTABLISHED))
			continue;

		 
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			addr = addr_buf;
			af = sctp_get_af_specific(addr->v4.sin_family);
			if (!af) {
				retval = -EINVAL;
				goto out;
			}

			if (sctp_assoc_lookup_laddr(asoc, addr))
				break;

			addr_buf += af->sockaddr_len;
		}
		if (i < addrcnt)
			continue;

		 
		bp = &asoc->base.bind_addr;
		p = bp->address_list.next;
		laddr = list_entry(p, struct sctp_sockaddr_entry, list);
		chunk = sctp_make_asconf_update_ip(asoc, &laddr->a, addrs,
						   addrcnt, SCTP_PARAM_ADD_IP);
		if (!chunk) {
			retval = -ENOMEM;
			goto out;
		}

		 
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			addr = addr_buf;
			af = sctp_get_af_specific(addr->v4.sin_family);
			memcpy(&saveaddr, addr, af->sockaddr_len);
			retval = sctp_add_bind_addr(bp, &saveaddr,
						    sizeof(saveaddr),
						    SCTP_ADDR_NEW, GFP_ATOMIC);
			addr_buf += af->sockaddr_len;
		}
		if (asoc->src_out_of_asoc_ok) {
			struct sctp_transport *trans;

			list_for_each_entry(trans,
			    &asoc->peer.transport_addr_list, transports) {
				trans->cwnd = min(4*asoc->pathmtu, max_t(__u32,
				    2*asoc->pathmtu, 4380));
				trans->ssthresh = asoc->peer.i.a_rwnd;
				trans->rto = asoc->rto_initial;
				sctp_max_rto(asoc, trans);
				trans->rtt = trans->srtt = trans->rttvar = 0;
				 
				sctp_transport_route(trans, NULL,
						     sctp_sk(asoc->base.sk));
			}
		}
		retval = sctp_send_asconf(asoc, chunk);
	}

out:
	return retval;
}

 
static int sctp_bindx_rem(struct sock *sk, struct sockaddr *addrs, int addrcnt)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_endpoint *ep = sp->ep;
	int cnt;
	struct sctp_bind_addr *bp = &ep->base.bind_addr;
	int retval = 0;
	void *addr_buf;
	union sctp_addr *sa_addr;
	struct sctp_af *af;

	pr_debug("%s: sk:%p, addrs:%p, addrcnt:%d\n",
		 __func__, sk, addrs, addrcnt);

	addr_buf = addrs;
	for (cnt = 0; cnt < addrcnt; cnt++) {
		 
		if (list_empty(&bp->address_list) ||
		    (sctp_list_single_entry(&bp->address_list))) {
			retval = -EBUSY;
			goto err_bindx_rem;
		}

		sa_addr = addr_buf;
		af = sctp_get_af_specific(sa_addr->sa.sa_family);
		if (!af) {
			retval = -EINVAL;
			goto err_bindx_rem;
		}

		if (!af->addr_valid(sa_addr, sp, NULL)) {
			retval = -EADDRNOTAVAIL;
			goto err_bindx_rem;
		}

		if (sa_addr->v4.sin_port &&
		    sa_addr->v4.sin_port != htons(bp->port)) {
			retval = -EINVAL;
			goto err_bindx_rem;
		}

		if (!sa_addr->v4.sin_port)
			sa_addr->v4.sin_port = htons(bp->port);

		 
		retval = sctp_del_bind_addr(bp, sa_addr);

		addr_buf += af->sockaddr_len;
err_bindx_rem:
		if (retval < 0) {
			 
			if (cnt > 0)
				sctp_bindx_add(sk, addrs, cnt);
			return retval;
		}
	}

	return retval;
}

 
static int sctp_send_asconf_del_ip(struct sock		*sk,
				   struct sockaddr	*addrs,
				   int			addrcnt)
{
	struct sctp_sock	*sp;
	struct sctp_endpoint	*ep;
	struct sctp_association	*asoc;
	struct sctp_transport	*transport;
	struct sctp_bind_addr	*bp;
	struct sctp_chunk	*chunk;
	union sctp_addr		*laddr;
	void			*addr_buf;
	struct sctp_af		*af;
	struct sctp_sockaddr_entry *saddr;
	int 			i;
	int 			retval = 0;
	int			stored = 0;

	chunk = NULL;
	sp = sctp_sk(sk);
	ep = sp->ep;

	if (!ep->asconf_enable)
		return retval;

	pr_debug("%s: sk:%p, addrs:%p, addrcnt:%d\n",
		 __func__, sk, addrs, addrcnt);

	list_for_each_entry(asoc, &ep->asocs, asocs) {

		if (!asoc->peer.asconf_capable)
			continue;

		if (asoc->peer.addip_disabled_mask & SCTP_PARAM_DEL_IP)
			continue;

		if (!sctp_state(asoc, ESTABLISHED))
			continue;

		 
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			laddr = addr_buf;
			af = sctp_get_af_specific(laddr->v4.sin_family);
			if (!af) {
				retval = -EINVAL;
				goto out;
			}

			if (!sctp_assoc_lookup_laddr(asoc, laddr))
				break;

			addr_buf += af->sockaddr_len;
		}
		if (i < addrcnt)
			continue;

		 
		bp = &asoc->base.bind_addr;
		laddr = sctp_find_unmatch_addr(bp, (union sctp_addr *)addrs,
					       addrcnt, sp);
		if ((laddr == NULL) && (addrcnt == 1)) {
			if (asoc->asconf_addr_del_pending)
				continue;
			asoc->asconf_addr_del_pending =
			    kzalloc(sizeof(union sctp_addr), GFP_ATOMIC);
			if (asoc->asconf_addr_del_pending == NULL) {
				retval = -ENOMEM;
				goto out;
			}
			asoc->asconf_addr_del_pending->sa.sa_family =
				    addrs->sa_family;
			asoc->asconf_addr_del_pending->v4.sin_port =
				    htons(bp->port);
			if (addrs->sa_family == AF_INET) {
				struct sockaddr_in *sin;

				sin = (struct sockaddr_in *)addrs;
				asoc->asconf_addr_del_pending->v4.sin_addr.s_addr = sin->sin_addr.s_addr;
			} else if (addrs->sa_family == AF_INET6) {
				struct sockaddr_in6 *sin6;

				sin6 = (struct sockaddr_in6 *)addrs;
				asoc->asconf_addr_del_pending->v6.sin6_addr = sin6->sin6_addr;
			}

			pr_debug("%s: keep the last address asoc:%p %pISc at %p\n",
				 __func__, asoc, &asoc->asconf_addr_del_pending->sa,
				 asoc->asconf_addr_del_pending);

			asoc->src_out_of_asoc_ok = 1;
			stored = 1;
			goto skip_mkasconf;
		}

		if (laddr == NULL)
			return -EINVAL;

		 
		chunk = sctp_make_asconf_update_ip(asoc, laddr, addrs, addrcnt,
						   SCTP_PARAM_DEL_IP);
		if (!chunk) {
			retval = -ENOMEM;
			goto out;
		}

skip_mkasconf:
		 
		addr_buf = addrs;
		for (i = 0; i < addrcnt; i++) {
			laddr = addr_buf;
			af = sctp_get_af_specific(laddr->v4.sin_family);
			list_for_each_entry(saddr, &bp->address_list, list) {
				if (sctp_cmp_addr_exact(&saddr->a, laddr))
					saddr->state = SCTP_ADDR_DEL;
			}
			addr_buf += af->sockaddr_len;
		}

		 
		list_for_each_entry(transport, &asoc->peer.transport_addr_list,
					transports) {
			sctp_transport_route(transport, NULL,
					     sctp_sk(asoc->base.sk));
		}

		if (stored)
			 
			continue;
		retval = sctp_send_asconf(asoc, chunk);
	}
out:
	return retval;
}

 
int sctp_asconf_mgmt(struct sctp_sock *sp, struct sctp_sockaddr_entry *addrw)
{
	struct sock *sk = sctp_opt2sk(sp);
	union sctp_addr *addr;
	struct sctp_af *af;

	 
	addr = &addrw->a;
	addr->v4.sin_port = htons(sp->ep->base.bind_addr.port);
	af = sctp_get_af_specific(addr->sa.sa_family);
	if (!af)
		return -EINVAL;
	if (sctp_verify_addr(sk, addr, af->sockaddr_len))
		return -EINVAL;

	if (addrw->state == SCTP_ADDR_NEW)
		return sctp_send_asconf_add_ip(sk, (struct sockaddr *)addr, 1);
	else
		return sctp_send_asconf_del_ip(sk, (struct sockaddr *)addr, 1);
}

 
static int sctp_setsockopt_bindx(struct sock *sk, struct sockaddr *addrs,
				 int addrs_size, int op)
{
	int err;
	int addrcnt = 0;
	int walk_size = 0;
	struct sockaddr *sa_addr;
	void *addr_buf = addrs;
	struct sctp_af *af;

	pr_debug("%s: sk:%p addrs:%p addrs_size:%d opt:%d\n",
		 __func__, sk, addr_buf, addrs_size, op);

	if (unlikely(addrs_size <= 0))
		return -EINVAL;

	 
	while (walk_size < addrs_size) {
		if (walk_size + sizeof(sa_family_t) > addrs_size)
			return -EINVAL;

		sa_addr = addr_buf;
		af = sctp_get_af_specific(sa_addr->sa_family);

		 
		if (!af || (walk_size + af->sockaddr_len) > addrs_size)
			return -EINVAL;
		addrcnt++;
		addr_buf += af->sockaddr_len;
		walk_size += af->sockaddr_len;
	}

	 
	switch (op) {
	case SCTP_BINDX_ADD_ADDR:
		 
		err = security_sctp_bind_connect(sk, SCTP_SOCKOPT_BINDX_ADD,
						 addrs, addrs_size);
		if (err)
			return err;
		err = sctp_bindx_add(sk, addrs, addrcnt);
		if (err)
			return err;
		return sctp_send_asconf_add_ip(sk, addrs, addrcnt);
	case SCTP_BINDX_REM_ADDR:
		err = sctp_bindx_rem(sk, addrs, addrcnt);
		if (err)
			return err;
		return sctp_send_asconf_del_ip(sk, addrs, addrcnt);

	default:
		return -EINVAL;
	}
}

static int sctp_bind_add(struct sock *sk, struct sockaddr *addrs,
		int addrlen)
{
	int err;

	lock_sock(sk);
	err = sctp_setsockopt_bindx(sk, addrs, addrlen, SCTP_BINDX_ADD_ADDR);
	release_sock(sk);
	return err;
}

static int sctp_connect_new_asoc(struct sctp_endpoint *ep,
				 const union sctp_addr *daddr,
				 const struct sctp_initmsg *init,
				 struct sctp_transport **tp)
{
	struct sctp_association *asoc;
	struct sock *sk = ep->base.sk;
	struct net *net = sock_net(sk);
	enum sctp_scope scope;
	int err;

	if (sctp_endpoint_is_peeled_off(ep, daddr))
		return -EADDRNOTAVAIL;

	if (!ep->base.bind_addr.port) {
		if (sctp_autobind(sk))
			return -EAGAIN;
	} else {
		if (inet_port_requires_bind_service(net, ep->base.bind_addr.port) &&
		    !ns_capable(net->user_ns, CAP_NET_BIND_SERVICE))
			return -EACCES;
	}

	scope = sctp_scope(daddr);
	asoc = sctp_association_new(ep, sk, scope, GFP_KERNEL);
	if (!asoc)
		return -ENOMEM;

	err = sctp_assoc_set_bind_addr_from_ep(asoc, scope, GFP_KERNEL);
	if (err < 0)
		goto free;

	*tp = sctp_assoc_add_peer(asoc, daddr, GFP_KERNEL, SCTP_UNKNOWN);
	if (!*tp) {
		err = -ENOMEM;
		goto free;
	}

	if (!init)
		return 0;

	if (init->sinit_num_ostreams) {
		__u16 outcnt = init->sinit_num_ostreams;

		asoc->c.sinit_num_ostreams = outcnt;
		 
		err = sctp_stream_init(&asoc->stream, outcnt, 0, GFP_KERNEL);
		if (err)
			goto free;
	}

	if (init->sinit_max_instreams)
		asoc->c.sinit_max_instreams = init->sinit_max_instreams;

	if (init->sinit_max_attempts)
		asoc->max_init_attempts = init->sinit_max_attempts;

	if (init->sinit_max_init_timeo)
		asoc->max_init_timeo =
			msecs_to_jiffies(init->sinit_max_init_timeo);

	return 0;
free:
	sctp_association_free(asoc);
	return err;
}

static int sctp_connect_add_peer(struct sctp_association *asoc,
				 union sctp_addr *daddr, int addr_len)
{
	struct sctp_endpoint *ep = asoc->ep;
	struct sctp_association *old;
	struct sctp_transport *t;
	int err;

	err = sctp_verify_addr(ep->base.sk, daddr, addr_len);
	if (err)
		return err;

	old = sctp_endpoint_lookup_assoc(ep, daddr, &t);
	if (old && old != asoc)
		return old->state >= SCTP_STATE_ESTABLISHED ? -EISCONN
							    : -EALREADY;

	if (sctp_endpoint_is_peeled_off(ep, daddr))
		return -EADDRNOTAVAIL;

	t = sctp_assoc_add_peer(asoc, daddr, GFP_KERNEL, SCTP_UNKNOWN);
	if (!t)
		return -ENOMEM;

	return 0;
}

 
static int __sctp_connect(struct sock *sk, struct sockaddr *kaddrs,
			  int addrs_size, int flags, sctp_assoc_t *assoc_id)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_endpoint *ep = sp->ep;
	struct sctp_transport *transport;
	struct sctp_association *asoc;
	void *addr_buf = kaddrs;
	union sctp_addr *daddr;
	struct sctp_af *af;
	int walk_size, err;
	long timeo;

	if (sctp_sstate(sk, ESTABLISHED) || sctp_sstate(sk, CLOSING) ||
	    (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING)))
		return -EISCONN;

	daddr = addr_buf;
	af = sctp_get_af_specific(daddr->sa.sa_family);
	if (!af || af->sockaddr_len > addrs_size)
		return -EINVAL;

	err = sctp_verify_addr(sk, daddr, af->sockaddr_len);
	if (err)
		return err;

	asoc = sctp_endpoint_lookup_assoc(ep, daddr, &transport);
	if (asoc)
		return asoc->state >= SCTP_STATE_ESTABLISHED ? -EISCONN
							     : -EALREADY;

	err = sctp_connect_new_asoc(ep, daddr, NULL, &transport);
	if (err)
		return err;
	asoc = transport->asoc;

	addr_buf += af->sockaddr_len;
	walk_size = af->sockaddr_len;
	while (walk_size < addrs_size) {
		err = -EINVAL;
		if (walk_size + sizeof(sa_family_t) > addrs_size)
			goto out_free;

		daddr = addr_buf;
		af = sctp_get_af_specific(daddr->sa.sa_family);
		if (!af || af->sockaddr_len + walk_size > addrs_size)
			goto out_free;

		if (asoc->peer.port != ntohs(daddr->v4.sin_port))
			goto out_free;

		err = sctp_connect_add_peer(asoc, daddr, af->sockaddr_len);
		if (err)
			goto out_free;

		addr_buf  += af->sockaddr_len;
		walk_size += af->sockaddr_len;
	}

	 
	if (assoc_id) {
		err = sctp_assoc_set_id(asoc, GFP_KERNEL);
		if (err < 0)
			goto out_free;
	}

	err = sctp_primitive_ASSOCIATE(sock_net(sk), asoc, NULL);
	if (err < 0)
		goto out_free;

	 
	inet_sk(sk)->inet_dport = htons(asoc->peer.port);
	sp->pf->to_sk_daddr(daddr, sk);
	sk->sk_err = 0;

	if (assoc_id)
		*assoc_id = asoc->assoc_id;

	timeo = sock_sndtimeo(sk, flags & O_NONBLOCK);
	return sctp_wait_for_connect(asoc, &timeo);

out_free:
	pr_debug("%s: took out_free path with asoc:%p kaddrs:%p err:%d\n",
		 __func__, asoc, kaddrs, err);
	sctp_association_free(asoc);
	return err;
}

 
static int __sctp_setsockopt_connectx(struct sock *sk, struct sockaddr *kaddrs,
				      int addrs_size, sctp_assoc_t *assoc_id)
{
	int err = 0, flags = 0;

	pr_debug("%s: sk:%p addrs:%p addrs_size:%d\n",
		 __func__, sk, kaddrs, addrs_size);

	 
	if (unlikely(addrs_size < sizeof(sa_family_t)))
		return -EINVAL;

	 
	err = security_sctp_bind_connect(sk, SCTP_SOCKOPT_CONNECTX,
					 (struct sockaddr *)kaddrs,
					  addrs_size);
	if (err)
		return err;

	 
	if (sk->sk_socket->file)
		flags = sk->sk_socket->file->f_flags;

	return __sctp_connect(sk, kaddrs, addrs_size, flags, assoc_id);
}

 
static int sctp_setsockopt_connectx_old(struct sock *sk,
					struct sockaddr *kaddrs,
					int addrs_size)
{
	return __sctp_setsockopt_connectx(sk, kaddrs, addrs_size, NULL);
}

 
static int sctp_setsockopt_connectx(struct sock *sk,
				    struct sockaddr *kaddrs,
				    int addrs_size)
{
	sctp_assoc_t assoc_id = 0;
	int err = 0;

	err = __sctp_setsockopt_connectx(sk, kaddrs, addrs_size, &assoc_id);

	if (err)
		return err;
	else
		return assoc_id;
}

 
#ifdef CONFIG_COMPAT
struct compat_sctp_getaddrs_old {
	sctp_assoc_t	assoc_id;
	s32		addr_num;
	compat_uptr_t	addrs;		 
};
#endif

static int sctp_getsockopt_connectx3(struct sock *sk, int len,
				     char __user *optval,
				     int __user *optlen)
{
	struct sctp_getaddrs_old param;
	sctp_assoc_t assoc_id = 0;
	struct sockaddr *kaddrs;
	int err = 0;

#ifdef CONFIG_COMPAT
	if (in_compat_syscall()) {
		struct compat_sctp_getaddrs_old param32;

		if (len < sizeof(param32))
			return -EINVAL;
		if (copy_from_user(&param32, optval, sizeof(param32)))
			return -EFAULT;

		param.assoc_id = param32.assoc_id;
		param.addr_num = param32.addr_num;
		param.addrs = compat_ptr(param32.addrs);
	} else
#endif
	{
		if (len < sizeof(param))
			return -EINVAL;
		if (copy_from_user(&param, optval, sizeof(param)))
			return -EFAULT;
	}

	kaddrs = memdup_user(param.addrs, param.addr_num);
	if (IS_ERR(kaddrs))
		return PTR_ERR(kaddrs);

	err = __sctp_setsockopt_connectx(sk, kaddrs, param.addr_num, &assoc_id);
	kfree(kaddrs);
	if (err == 0 || err == -EINPROGRESS) {
		if (copy_to_user(optval, &assoc_id, sizeof(assoc_id)))
			return -EFAULT;
		if (put_user(sizeof(assoc_id), optlen))
			return -EFAULT;
	}

	return err;
}

 
static void sctp_close(struct sock *sk, long timeout)
{
	struct net *net = sock_net(sk);
	struct sctp_endpoint *ep;
	struct sctp_association *asoc;
	struct list_head *pos, *temp;
	unsigned int data_was_unread;

	pr_debug("%s: sk:%p, timeout:%ld\n", __func__, sk, timeout);

	lock_sock_nested(sk, SINGLE_DEPTH_NESTING);
	sk->sk_shutdown = SHUTDOWN_MASK;
	inet_sk_set_state(sk, SCTP_SS_CLOSING);

	ep = sctp_sk(sk)->ep;

	 
	data_was_unread = sctp_queue_purge_ulpevents(&sk->sk_receive_queue);
	data_was_unread += sctp_queue_purge_ulpevents(&sctp_sk(sk)->pd_lobby);

	 
	list_for_each_safe(pos, temp, &ep->asocs) {
		asoc = list_entry(pos, struct sctp_association, asocs);

		if (sctp_style(sk, TCP)) {
			 
			if (sctp_state(asoc, CLOSED)) {
				sctp_association_free(asoc);
				continue;
			}
		}

		if (data_was_unread || !skb_queue_empty(&asoc->ulpq.lobby) ||
		    !skb_queue_empty(&asoc->ulpq.reasm) ||
		    !skb_queue_empty(&asoc->ulpq.reasm_uo) ||
		    (sock_flag(sk, SOCK_LINGER) && !sk->sk_lingertime)) {
			struct sctp_chunk *chunk;

			chunk = sctp_make_abort_user(asoc, NULL, 0);
			sctp_primitive_ABORT(net, asoc, chunk);
		} else
			sctp_primitive_SHUTDOWN(net, asoc, NULL);
	}

	 
	if (sctp_style(sk, TCP) && timeout)
		sctp_wait_for_close(sk, timeout);

	 
	release_sock(sk);

	 
	spin_lock_bh(&net->sctp.addr_wq_lock);
	bh_lock_sock_nested(sk);

	 
	sock_hold(sk);
	sk_common_release(sk);

	bh_unlock_sock(sk);
	spin_unlock_bh(&net->sctp.addr_wq_lock);

	sock_put(sk);

	SCTP_DBG_OBJCNT_DEC(sock);
}

 
static int sctp_error(struct sock *sk, int flags, int err)
{
	if (err == -EPIPE)
		err = sock_error(sk) ? : -EPIPE;
	if (err == -EPIPE && !(flags & MSG_NOSIGNAL))
		send_sig(SIGPIPE, current, 0);
	return err;
}

 
 

static int sctp_msghdr_parse(const struct msghdr *msg,
			     struct sctp_cmsgs *cmsgs);

static int sctp_sendmsg_parse(struct sock *sk, struct sctp_cmsgs *cmsgs,
			      struct sctp_sndrcvinfo *srinfo,
			      const struct msghdr *msg, size_t msg_len)
{
	__u16 sflags;
	int err;

	if (sctp_sstate(sk, LISTENING) && sctp_style(sk, TCP))
		return -EPIPE;

	if (msg_len > sk->sk_sndbuf)
		return -EMSGSIZE;

	memset(cmsgs, 0, sizeof(*cmsgs));
	err = sctp_msghdr_parse(msg, cmsgs);
	if (err) {
		pr_debug("%s: msghdr parse err:%x\n", __func__, err);
		return err;
	}

	memset(srinfo, 0, sizeof(*srinfo));
	if (cmsgs->srinfo) {
		srinfo->sinfo_stream = cmsgs->srinfo->sinfo_stream;
		srinfo->sinfo_flags = cmsgs->srinfo->sinfo_flags;
		srinfo->sinfo_ppid = cmsgs->srinfo->sinfo_ppid;
		srinfo->sinfo_context = cmsgs->srinfo->sinfo_context;
		srinfo->sinfo_assoc_id = cmsgs->srinfo->sinfo_assoc_id;
		srinfo->sinfo_timetolive = cmsgs->srinfo->sinfo_timetolive;
	}

	if (cmsgs->sinfo) {
		srinfo->sinfo_stream = cmsgs->sinfo->snd_sid;
		srinfo->sinfo_flags = cmsgs->sinfo->snd_flags;
		srinfo->sinfo_ppid = cmsgs->sinfo->snd_ppid;
		srinfo->sinfo_context = cmsgs->sinfo->snd_context;
		srinfo->sinfo_assoc_id = cmsgs->sinfo->snd_assoc_id;
	}

	if (cmsgs->prinfo) {
		srinfo->sinfo_timetolive = cmsgs->prinfo->pr_value;
		SCTP_PR_SET_POLICY(srinfo->sinfo_flags,
				   cmsgs->prinfo->pr_policy);
	}

	sflags = srinfo->sinfo_flags;
	if (!sflags && msg_len)
		return 0;

	if (sctp_style(sk, TCP) && (sflags & (SCTP_EOF | SCTP_ABORT)))
		return -EINVAL;

	if (((sflags & SCTP_EOF) && msg_len > 0) ||
	    (!(sflags & (SCTP_EOF | SCTP_ABORT)) && msg_len == 0))
		return -EINVAL;

	if ((sflags & SCTP_ADDR_OVER) && !msg->msg_name)
		return -EINVAL;

	return 0;
}

static int sctp_sendmsg_new_asoc(struct sock *sk, __u16 sflags,
				 struct sctp_cmsgs *cmsgs,
				 union sctp_addr *daddr,
				 struct sctp_transport **tp)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_association *asoc;
	struct cmsghdr *cmsg;
	__be32 flowinfo = 0;
	struct sctp_af *af;
	int err;

	*tp = NULL;

	if (sflags & (SCTP_EOF | SCTP_ABORT))
		return -EINVAL;

	if (sctp_style(sk, TCP) && (sctp_sstate(sk, ESTABLISHED) ||
				    sctp_sstate(sk, CLOSING)))
		return -EADDRNOTAVAIL;

	 
	af = sctp_get_af_specific(daddr->sa.sa_family);
	if (!af)
		return -EINVAL;
	err = security_sctp_bind_connect(sk, SCTP_SENDMSG_CONNECT,
					 (struct sockaddr *)daddr,
					 af->sockaddr_len);
	if (err < 0)
		return err;

	err = sctp_connect_new_asoc(ep, daddr, cmsgs->init, tp);
	if (err)
		return err;
	asoc = (*tp)->asoc;

	if (!cmsgs->addrs_msg)
		return 0;

	if (daddr->sa.sa_family == AF_INET6)
		flowinfo = daddr->v6.sin6_flowinfo;

	 
	for_each_cmsghdr(cmsg, cmsgs->addrs_msg) {
		union sctp_addr _daddr;
		int dlen;

		if (cmsg->cmsg_level != IPPROTO_SCTP ||
		    (cmsg->cmsg_type != SCTP_DSTADDRV4 &&
		     cmsg->cmsg_type != SCTP_DSTADDRV6))
			continue;

		daddr = &_daddr;
		memset(daddr, 0, sizeof(*daddr));
		dlen = cmsg->cmsg_len - sizeof(struct cmsghdr);
		if (cmsg->cmsg_type == SCTP_DSTADDRV4) {
			if (dlen < sizeof(struct in_addr)) {
				err = -EINVAL;
				goto free;
			}

			dlen = sizeof(struct in_addr);
			daddr->v4.sin_family = AF_INET;
			daddr->v4.sin_port = htons(asoc->peer.port);
			memcpy(&daddr->v4.sin_addr, CMSG_DATA(cmsg), dlen);
		} else {
			if (dlen < sizeof(struct in6_addr)) {
				err = -EINVAL;
				goto free;
			}

			dlen = sizeof(struct in6_addr);
			daddr->v6.sin6_flowinfo = flowinfo;
			daddr->v6.sin6_family = AF_INET6;
			daddr->v6.sin6_port = htons(asoc->peer.port);
			memcpy(&daddr->v6.sin6_addr, CMSG_DATA(cmsg), dlen);
		}

		err = sctp_connect_add_peer(asoc, daddr, sizeof(*daddr));
		if (err)
			goto free;
	}

	return 0;

free:
	sctp_association_free(asoc);
	return err;
}

static int sctp_sendmsg_check_sflags(struct sctp_association *asoc,
				     __u16 sflags, struct msghdr *msg,
				     size_t msg_len)
{
	struct sock *sk = asoc->base.sk;
	struct net *net = sock_net(sk);

	if (sctp_state(asoc, CLOSED) && sctp_style(sk, TCP))
		return -EPIPE;

	if ((sflags & SCTP_SENDALL) && sctp_style(sk, UDP) &&
	    !sctp_state(asoc, ESTABLISHED))
		return 0;

	if (sflags & SCTP_EOF) {
		pr_debug("%s: shutting down association:%p\n", __func__, asoc);
		sctp_primitive_SHUTDOWN(net, asoc, NULL);

		return 0;
	}

	if (sflags & SCTP_ABORT) {
		struct sctp_chunk *chunk;

		chunk = sctp_make_abort_user(asoc, msg, msg_len);
		if (!chunk)
			return -ENOMEM;

		pr_debug("%s: aborting association:%p\n", __func__, asoc);
		sctp_primitive_ABORT(net, asoc, chunk);
		iov_iter_revert(&msg->msg_iter, msg_len);

		return 0;
	}

	return 1;
}

static int sctp_sendmsg_to_asoc(struct sctp_association *asoc,
				struct msghdr *msg, size_t msg_len,
				struct sctp_transport *transport,
				struct sctp_sndrcvinfo *sinfo)
{
	struct sock *sk = asoc->base.sk;
	struct sctp_sock *sp = sctp_sk(sk);
	struct net *net = sock_net(sk);
	struct sctp_datamsg *datamsg;
	bool wait_connect = false;
	struct sctp_chunk *chunk;
	long timeo;
	int err;

	if (sinfo->sinfo_stream >= asoc->stream.outcnt) {
		err = -EINVAL;
		goto err;
	}

	if (unlikely(!SCTP_SO(&asoc->stream, sinfo->sinfo_stream)->ext)) {
		err = sctp_stream_init_ext(&asoc->stream, sinfo->sinfo_stream);
		if (err)
			goto err;
	}

	if (sp->disable_fragments && msg_len > asoc->frag_point) {
		err = -EMSGSIZE;
		goto err;
	}

	if (asoc->pmtu_pending) {
		if (sp->param_flags & SPP_PMTUD_ENABLE)
			sctp_assoc_sync_pmtu(asoc);
		asoc->pmtu_pending = 0;
	}

	if (sctp_wspace(asoc) < (int)msg_len)
		sctp_prsctp_prune(asoc, sinfo, msg_len - sctp_wspace(asoc));

	if (sctp_wspace(asoc) <= 0 || !sk_wmem_schedule(sk, msg_len)) {
		timeo = sock_sndtimeo(sk, msg->msg_flags & MSG_DONTWAIT);
		err = sctp_wait_for_sndbuf(asoc, &timeo, msg_len);
		if (err)
			goto err;
		if (unlikely(sinfo->sinfo_stream >= asoc->stream.outcnt)) {
			err = -EINVAL;
			goto err;
		}
	}

	if (sctp_state(asoc, CLOSED)) {
		err = sctp_primitive_ASSOCIATE(net, asoc, NULL);
		if (err)
			goto err;

		if (asoc->ep->intl_enable) {
			timeo = sock_sndtimeo(sk, 0);
			err = sctp_wait_for_connect(asoc, &timeo);
			if (err) {
				err = -ESRCH;
				goto err;
			}
		} else {
			wait_connect = true;
		}

		pr_debug("%s: we associated primitively\n", __func__);
	}

	datamsg = sctp_datamsg_from_user(asoc, sinfo, &msg->msg_iter);
	if (IS_ERR(datamsg)) {
		err = PTR_ERR(datamsg);
		goto err;
	}

	asoc->force_delay = !!(msg->msg_flags & MSG_MORE);

	list_for_each_entry(chunk, &datamsg->chunks, frag_list) {
		sctp_chunk_hold(chunk);
		sctp_set_owner_w(chunk);
		chunk->transport = transport;
	}

	err = sctp_primitive_SEND(net, asoc, datamsg);
	if (err) {
		sctp_datamsg_free(datamsg);
		goto err;
	}

	pr_debug("%s: we sent primitively\n", __func__);

	sctp_datamsg_put(datamsg);

	if (unlikely(wait_connect)) {
		timeo = sock_sndtimeo(sk, msg->msg_flags & MSG_DONTWAIT);
		sctp_wait_for_connect(asoc, &timeo);
	}

	err = msg_len;

err:
	return err;
}

static union sctp_addr *sctp_sendmsg_get_daddr(struct sock *sk,
					       const struct msghdr *msg,
					       struct sctp_cmsgs *cmsgs)
{
	union sctp_addr *daddr = NULL;
	int err;

	if (!sctp_style(sk, UDP_HIGH_BANDWIDTH) && msg->msg_name) {
		int len = msg->msg_namelen;

		if (len > sizeof(*daddr))
			len = sizeof(*daddr);

		daddr = (union sctp_addr *)msg->msg_name;

		err = sctp_verify_addr(sk, daddr, len);
		if (err)
			return ERR_PTR(err);
	}

	return daddr;
}

static void sctp_sendmsg_update_sinfo(struct sctp_association *asoc,
				      struct sctp_sndrcvinfo *sinfo,
				      struct sctp_cmsgs *cmsgs)
{
	if (!cmsgs->srinfo && !cmsgs->sinfo) {
		sinfo->sinfo_stream = asoc->default_stream;
		sinfo->sinfo_ppid = asoc->default_ppid;
		sinfo->sinfo_context = asoc->default_context;
		sinfo->sinfo_assoc_id = sctp_assoc2id(asoc);

		if (!cmsgs->prinfo)
			sinfo->sinfo_flags = asoc->default_flags;
	}

	if (!cmsgs->srinfo && !cmsgs->prinfo)
		sinfo->sinfo_timetolive = asoc->default_timetolive;

	if (cmsgs->authinfo) {
		 
		sinfo->sinfo_tsn = 1;
		sinfo->sinfo_ssn = cmsgs->authinfo->auth_keynumber;
	}
}

static int sctp_sendmsg(struct sock *sk, struct msghdr *msg, size_t msg_len)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_transport *transport = NULL;
	struct sctp_sndrcvinfo _sinfo, *sinfo;
	struct sctp_association *asoc, *tmp;
	struct sctp_cmsgs cmsgs;
	union sctp_addr *daddr;
	bool new = false;
	__u16 sflags;
	int err;

	 
	err = sctp_sendmsg_parse(sk, &cmsgs, &_sinfo, msg, msg_len);
	if (err)
		goto out;

	sinfo  = &_sinfo;
	sflags = sinfo->sinfo_flags;

	 
	daddr = sctp_sendmsg_get_daddr(sk, msg, &cmsgs);
	if (IS_ERR(daddr)) {
		err = PTR_ERR(daddr);
		goto out;
	}

	lock_sock(sk);

	 
	if ((sflags & SCTP_SENDALL) && sctp_style(sk, UDP)) {
		list_for_each_entry_safe(asoc, tmp, &ep->asocs, asocs) {
			err = sctp_sendmsg_check_sflags(asoc, sflags, msg,
							msg_len);
			if (err == 0)
				continue;
			if (err < 0)
				goto out_unlock;

			sctp_sendmsg_update_sinfo(asoc, sinfo, &cmsgs);

			err = sctp_sendmsg_to_asoc(asoc, msg, msg_len,
						   NULL, sinfo);
			if (err < 0)
				goto out_unlock;

			iov_iter_revert(&msg->msg_iter, err);
		}

		goto out_unlock;
	}

	 
	if (daddr) {
		asoc = sctp_endpoint_lookup_assoc(ep, daddr, &transport);
		if (asoc) {
			err = sctp_sendmsg_check_sflags(asoc, sflags, msg,
							msg_len);
			if (err <= 0)
				goto out_unlock;
		} else {
			err = sctp_sendmsg_new_asoc(sk, sflags, &cmsgs, daddr,
						    &transport);
			if (err)
				goto out_unlock;

			asoc = transport->asoc;
			new = true;
		}

		if (!sctp_style(sk, TCP) && !(sflags & SCTP_ADDR_OVER))
			transport = NULL;
	} else {
		asoc = sctp_id2assoc(sk, sinfo->sinfo_assoc_id);
		if (!asoc) {
			err = -EPIPE;
			goto out_unlock;
		}

		err = sctp_sendmsg_check_sflags(asoc, sflags, msg, msg_len);
		if (err <= 0)
			goto out_unlock;
	}

	 
	sctp_sendmsg_update_sinfo(asoc, sinfo, &cmsgs);

	 
	err = sctp_sendmsg_to_asoc(asoc, msg, msg_len, transport, sinfo);
	if (err < 0 && err != -ESRCH && new)
		sctp_association_free(asoc);

out_unlock:
	release_sock(sk);
out:
	return sctp_error(sk, msg->msg_flags, err);
}

 
static int sctp_skb_pull(struct sk_buff *skb, int len)
{
	struct sk_buff *list;
	int skb_len = skb_headlen(skb);
	int rlen;

	if (len <= skb_len) {
		__skb_pull(skb, len);
		return 0;
	}
	len -= skb_len;
	__skb_pull(skb, skb_len);

	skb_walk_frags(skb, list) {
		rlen = sctp_skb_pull(list, len);
		skb->len -= (len-rlen);
		skb->data_len -= (len-rlen);

		if (!rlen)
			return 0;

		len = rlen;
	}

	return len;
}

 
static int sctp_recvmsg(struct sock *sk, struct msghdr *msg, size_t len,
			int flags, int *addr_len)
{
	struct sctp_ulpevent *event = NULL;
	struct sctp_sock *sp = sctp_sk(sk);
	struct sk_buff *skb, *head_skb;
	int copied;
	int err = 0;
	int skb_len;

	pr_debug("%s: sk:%p, msghdr:%p, len:%zd, flags:0x%x, addr_len:%p)\n",
		 __func__, sk, msg, len, flags, addr_len);

	if (unlikely(flags & MSG_ERRQUEUE))
		return inet_recv_error(sk, msg, len, addr_len);

	if (sk_can_busy_loop(sk) &&
	    skb_queue_empty_lockless(&sk->sk_receive_queue))
		sk_busy_loop(sk, flags & MSG_DONTWAIT);

	lock_sock(sk);

	if (sctp_style(sk, TCP) && !sctp_sstate(sk, ESTABLISHED) &&
	    !sctp_sstate(sk, CLOSING) && !sctp_sstate(sk, CLOSED)) {
		err = -ENOTCONN;
		goto out;
	}

	skb = sctp_skb_recv_datagram(sk, flags, &err);
	if (!skb)
		goto out;

	 
	skb_len = skb->len;

	copied = skb_len;
	if (copied > len)
		copied = len;

	err = skb_copy_datagram_msg(skb, 0, msg, copied);

	event = sctp_skb2event(skb);

	if (err)
		goto out_free;

	if (event->chunk && event->chunk->head_skb)
		head_skb = event->chunk->head_skb;
	else
		head_skb = skb;
	sock_recv_cmsgs(msg, sk, head_skb);
	if (sctp_ulpevent_is_notification(event)) {
		msg->msg_flags |= MSG_NOTIFICATION;
		sp->pf->event_msgname(event, msg->msg_name, addr_len);
	} else {
		sp->pf->skb_msgname(head_skb, msg->msg_name, addr_len);
	}

	 
	if (sp->recvnxtinfo)
		sctp_ulpevent_read_nxtinfo(event, msg, sk);
	 
	if (sp->recvrcvinfo)
		sctp_ulpevent_read_rcvinfo(event, msg);
	 
	if (sctp_ulpevent_type_enabled(sp->subscribe, SCTP_DATA_IO_EVENT))
		sctp_ulpevent_read_sndrcvinfo(event, msg);

	err = copied;

	 
	if (skb_len > copied) {
		msg->msg_flags &= ~MSG_EOR;
		if (flags & MSG_PEEK)
			goto out_free;
		sctp_skb_pull(skb, copied);
		skb_queue_head(&sk->sk_receive_queue, skb);

		 
		if (!sctp_ulpevent_is_notification(event))
			sctp_assoc_rwnd_increase(event->asoc, copied);
		goto out;
	} else if ((event->msg_flags & MSG_NOTIFICATION) ||
		   (event->msg_flags & MSG_EOR))
		msg->msg_flags |= MSG_EOR;
	else
		msg->msg_flags &= ~MSG_EOR;

out_free:
	if (flags & MSG_PEEK) {
		 
		kfree_skb(skb);
	} else {
		 
		sctp_ulpevent_free(event);
	}
out:
	release_sock(sk);
	return err;
}

 
static int sctp_setsockopt_disable_fragments(struct sock *sk, int *val,
					     unsigned int optlen)
{
	if (optlen < sizeof(int))
		return -EINVAL;
	sctp_sk(sk)->disable_fragments = (*val == 0) ? 0 : 1;
	return 0;
}

static int sctp_setsockopt_events(struct sock *sk, __u8 *sn_type,
				  unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	int i;

	if (optlen > sizeof(struct sctp_event_subscribe))
		return -EINVAL;

	for (i = 0; i < optlen; i++)
		sctp_ulpevent_type_set(&sp->subscribe, SCTP_SN_TYPE_BASE + i,
				       sn_type[i]);

	list_for_each_entry(asoc, &sp->ep->asocs, asocs)
		asoc->subscribe = sctp_sk(sk)->subscribe;

	 
	if (sctp_ulpevent_type_enabled(sp->subscribe, SCTP_SENDER_DRY_EVENT)) {
		struct sctp_ulpevent *event;

		asoc = sctp_id2assoc(sk, 0);
		if (asoc && sctp_outq_is_empty(&asoc->outqueue)) {
			event = sctp_ulpevent_make_sender_dry_event(asoc,
					GFP_USER | __GFP_NOWARN);
			if (!event)
				return -ENOMEM;

			asoc->stream.si->enqueue_event(&asoc->ulpq, event);
		}
	}

	return 0;
}

 
static int sctp_setsockopt_autoclose(struct sock *sk, u32 *optval,
				     unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct net *net = sock_net(sk);

	 
	if (sctp_style(sk, TCP))
		return -EOPNOTSUPP;
	if (optlen != sizeof(int))
		return -EINVAL;

	sp->autoclose = *optval;
	if (sp->autoclose > net->sctp.max_autoclose)
		sp->autoclose = net->sctp.max_autoclose;

	return 0;
}

 
static int sctp_apply_peer_addr_params(struct sctp_paddrparams *params,
				       struct sctp_transport   *trans,
				       struct sctp_association *asoc,
				       struct sctp_sock        *sp,
				       int                      hb_change,
				       int                      pmtud_change,
				       int                      sackdelay_change)
{
	int error;

	if (params->spp_flags & SPP_HB_DEMAND && trans) {
		error = sctp_primitive_REQUESTHEARTBEAT(trans->asoc->base.net,
							trans->asoc, trans);
		if (error)
			return error;
	}

	 
	if (params->spp_flags & SPP_HB_ENABLE) {

		 
		if (params->spp_flags & SPP_HB_TIME_IS_ZERO)
			params->spp_hbinterval = 0;

		if (params->spp_hbinterval ||
		    (params->spp_flags & SPP_HB_TIME_IS_ZERO)) {
			if (trans) {
				trans->hbinterval =
				    msecs_to_jiffies(params->spp_hbinterval);
				sctp_transport_reset_hb_timer(trans);
			} else if (asoc) {
				asoc->hbinterval =
				    msecs_to_jiffies(params->spp_hbinterval);
			} else {
				sp->hbinterval = params->spp_hbinterval;
			}
		}
	}

	if (hb_change) {
		if (trans) {
			trans->param_flags =
				(trans->param_flags & ~SPP_HB) | hb_change;
		} else if (asoc) {
			asoc->param_flags =
				(asoc->param_flags & ~SPP_HB) | hb_change;
		} else {
			sp->param_flags =
				(sp->param_flags & ~SPP_HB) | hb_change;
		}
	}

	 
	if ((params->spp_flags & SPP_PMTUD_DISABLE) && params->spp_pathmtu) {
		if (trans) {
			trans->pathmtu = params->spp_pathmtu;
			sctp_assoc_sync_pmtu(asoc);
		} else if (asoc) {
			sctp_assoc_set_pmtu(asoc, params->spp_pathmtu);
		} else {
			sp->pathmtu = params->spp_pathmtu;
		}
	}

	if (pmtud_change) {
		if (trans) {
			int update = (trans->param_flags & SPP_PMTUD_DISABLE) &&
				(params->spp_flags & SPP_PMTUD_ENABLE);
			trans->param_flags =
				(trans->param_flags & ~SPP_PMTUD) | pmtud_change;
			if (update) {
				sctp_transport_pmtu(trans, sctp_opt2sk(sp));
				sctp_assoc_sync_pmtu(asoc);
			}
			sctp_transport_pl_reset(trans);
		} else if (asoc) {
			asoc->param_flags =
				(asoc->param_flags & ~SPP_PMTUD) | pmtud_change;
		} else {
			sp->param_flags =
				(sp->param_flags & ~SPP_PMTUD) | pmtud_change;
		}
	}

	 
	if ((params->spp_flags & SPP_SACKDELAY_ENABLE) && params->spp_sackdelay) {
		if (trans) {
			trans->sackdelay =
				msecs_to_jiffies(params->spp_sackdelay);
		} else if (asoc) {
			asoc->sackdelay =
				msecs_to_jiffies(params->spp_sackdelay);
		} else {
			sp->sackdelay = params->spp_sackdelay;
		}
	}

	if (sackdelay_change) {
		if (trans) {
			trans->param_flags =
				(trans->param_flags & ~SPP_SACKDELAY) |
				sackdelay_change;
		} else if (asoc) {
			asoc->param_flags =
				(asoc->param_flags & ~SPP_SACKDELAY) |
				sackdelay_change;
		} else {
			sp->param_flags =
				(sp->param_flags & ~SPP_SACKDELAY) |
				sackdelay_change;
		}
	}

	 
	if (params->spp_pathmaxrxt) {
		if (trans) {
			trans->pathmaxrxt = params->spp_pathmaxrxt;
		} else if (asoc) {
			asoc->pathmaxrxt = params->spp_pathmaxrxt;
		} else {
			sp->pathmaxrxt = params->spp_pathmaxrxt;
		}
	}

	if (params->spp_flags & SPP_IPV6_FLOWLABEL) {
		if (trans) {
			if (trans->ipaddr.sa.sa_family == AF_INET6) {
				trans->flowlabel = params->spp_ipv6_flowlabel &
						   SCTP_FLOWLABEL_VAL_MASK;
				trans->flowlabel |= SCTP_FLOWLABEL_SET_MASK;
			}
		} else if (asoc) {
			struct sctp_transport *t;

			list_for_each_entry(t, &asoc->peer.transport_addr_list,
					    transports) {
				if (t->ipaddr.sa.sa_family != AF_INET6)
					continue;
				t->flowlabel = params->spp_ipv6_flowlabel &
					       SCTP_FLOWLABEL_VAL_MASK;
				t->flowlabel |= SCTP_FLOWLABEL_SET_MASK;
			}
			asoc->flowlabel = params->spp_ipv6_flowlabel &
					  SCTP_FLOWLABEL_VAL_MASK;
			asoc->flowlabel |= SCTP_FLOWLABEL_SET_MASK;
		} else if (sctp_opt2sk(sp)->sk_family == AF_INET6) {
			sp->flowlabel = params->spp_ipv6_flowlabel &
					SCTP_FLOWLABEL_VAL_MASK;
			sp->flowlabel |= SCTP_FLOWLABEL_SET_MASK;
		}
	}

	if (params->spp_flags & SPP_DSCP) {
		if (trans) {
			trans->dscp = params->spp_dscp & SCTP_DSCP_VAL_MASK;
			trans->dscp |= SCTP_DSCP_SET_MASK;
		} else if (asoc) {
			struct sctp_transport *t;

			list_for_each_entry(t, &asoc->peer.transport_addr_list,
					    transports) {
				t->dscp = params->spp_dscp &
					  SCTP_DSCP_VAL_MASK;
				t->dscp |= SCTP_DSCP_SET_MASK;
			}
			asoc->dscp = params->spp_dscp & SCTP_DSCP_VAL_MASK;
			asoc->dscp |= SCTP_DSCP_SET_MASK;
		} else {
			sp->dscp = params->spp_dscp & SCTP_DSCP_VAL_MASK;
			sp->dscp |= SCTP_DSCP_SET_MASK;
		}
	}

	return 0;
}

static int sctp_setsockopt_peer_addr_params(struct sock *sk,
					    struct sctp_paddrparams *params,
					    unsigned int optlen)
{
	struct sctp_transport   *trans = NULL;
	struct sctp_association *asoc = NULL;
	struct sctp_sock        *sp = sctp_sk(sk);
	int error;
	int hb_change, pmtud_change, sackdelay_change;

	if (optlen == ALIGN(offsetof(struct sctp_paddrparams,
					    spp_ipv6_flowlabel), 4)) {
		if (params->spp_flags & (SPP_DSCP | SPP_IPV6_FLOWLABEL))
			return -EINVAL;
	} else if (optlen != sizeof(*params)) {
		return -EINVAL;
	}

	 
	hb_change        = params->spp_flags & SPP_HB;
	pmtud_change     = params->spp_flags & SPP_PMTUD;
	sackdelay_change = params->spp_flags & SPP_SACKDELAY;

	if (hb_change        == SPP_HB ||
	    pmtud_change     == SPP_PMTUD ||
	    sackdelay_change == SPP_SACKDELAY ||
	    params->spp_sackdelay > 500 ||
	    (params->spp_pathmtu &&
	     params->spp_pathmtu < SCTP_DEFAULT_MINSEGMENT))
		return -EINVAL;

	 
	if (!sctp_is_any(sk, (union sctp_addr *)&params->spp_address)) {
		trans = sctp_addr_id2transport(sk, &params->spp_address,
					       params->spp_assoc_id);
		if (!trans)
			return -EINVAL;
	}

	 
	asoc = sctp_id2assoc(sk, params->spp_assoc_id);
	if (!asoc && params->spp_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	 
	if (params->spp_flags & SPP_HB_DEMAND && !trans && !asoc)
		return -EINVAL;

	 
	error = sctp_apply_peer_addr_params(params, trans, asoc, sp,
					    hb_change, pmtud_change,
					    sackdelay_change);

	if (error)
		return error;

	 
	if (!trans && asoc) {
		list_for_each_entry(trans, &asoc->peer.transport_addr_list,
				transports) {
			sctp_apply_peer_addr_params(params, trans, asoc, sp,
						    hb_change, pmtud_change,
						    sackdelay_change);
		}
	}

	return 0;
}

static inline __u32 sctp_spp_sackdelay_enable(__u32 param_flags)
{
	return (param_flags & ~SPP_SACKDELAY) | SPP_SACKDELAY_ENABLE;
}

static inline __u32 sctp_spp_sackdelay_disable(__u32 param_flags)
{
	return (param_flags & ~SPP_SACKDELAY) | SPP_SACKDELAY_DISABLE;
}

static void sctp_apply_asoc_delayed_ack(struct sctp_sack_info *params,
					struct sctp_association *asoc)
{
	struct sctp_transport *trans;

	if (params->sack_delay) {
		asoc->sackdelay = msecs_to_jiffies(params->sack_delay);
		asoc->param_flags =
			sctp_spp_sackdelay_enable(asoc->param_flags);
	}
	if (params->sack_freq == 1) {
		asoc->param_flags =
			sctp_spp_sackdelay_disable(asoc->param_flags);
	} else if (params->sack_freq > 1) {
		asoc->sackfreq = params->sack_freq;
		asoc->param_flags =
			sctp_spp_sackdelay_enable(asoc->param_flags);
	}

	list_for_each_entry(trans, &asoc->peer.transport_addr_list,
			    transports) {
		if (params->sack_delay) {
			trans->sackdelay = msecs_to_jiffies(params->sack_delay);
			trans->param_flags =
				sctp_spp_sackdelay_enable(trans->param_flags);
		}
		if (params->sack_freq == 1) {
			trans->param_flags =
				sctp_spp_sackdelay_disable(trans->param_flags);
		} else if (params->sack_freq > 1) {
			trans->sackfreq = params->sack_freq;
			trans->param_flags =
				sctp_spp_sackdelay_enable(trans->param_flags);
		}
	}
}

 
static int __sctp_setsockopt_delayed_ack(struct sock *sk,
					 struct sctp_sack_info *params)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;

	 
	if (params->sack_delay > 500)
		return -EINVAL;

	 
	asoc = sctp_id2assoc(sk, params->sack_assoc_id);
	if (!asoc && params->sack_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		sctp_apply_asoc_delayed_ack(params, asoc);

		return 0;
	}

	if (sctp_style(sk, TCP))
		params->sack_assoc_id = SCTP_FUTURE_ASSOC;

	if (params->sack_assoc_id == SCTP_FUTURE_ASSOC ||
	    params->sack_assoc_id == SCTP_ALL_ASSOC) {
		if (params->sack_delay) {
			sp->sackdelay = params->sack_delay;
			sp->param_flags =
				sctp_spp_sackdelay_enable(sp->param_flags);
		}
		if (params->sack_freq == 1) {
			sp->param_flags =
				sctp_spp_sackdelay_disable(sp->param_flags);
		} else if (params->sack_freq > 1) {
			sp->sackfreq = params->sack_freq;
			sp->param_flags =
				sctp_spp_sackdelay_enable(sp->param_flags);
		}
	}

	if (params->sack_assoc_id == SCTP_CURRENT_ASSOC ||
	    params->sack_assoc_id == SCTP_ALL_ASSOC)
		list_for_each_entry(asoc, &sp->ep->asocs, asocs)
			sctp_apply_asoc_delayed_ack(params, asoc);

	return 0;
}

static int sctp_setsockopt_delayed_ack(struct sock *sk,
				       struct sctp_sack_info *params,
				       unsigned int optlen)
{
	if (optlen == sizeof(struct sctp_assoc_value)) {
		struct sctp_assoc_value *v = (struct sctp_assoc_value *)params;
		struct sctp_sack_info p;

		pr_warn_ratelimited(DEPRECATED
				    "%s (pid %d) "
				    "Use of struct sctp_assoc_value in delayed_ack socket option.\n"
				    "Use struct sctp_sack_info instead\n",
				    current->comm, task_pid_nr(current));

		p.sack_assoc_id = v->assoc_id;
		p.sack_delay = v->assoc_value;
		p.sack_freq = v->assoc_value ? 0 : 1;
		return __sctp_setsockopt_delayed_ack(sk, &p);
	}

	if (optlen != sizeof(struct sctp_sack_info))
		return -EINVAL;
	if (params->sack_delay == 0 && params->sack_freq == 0)
		return 0;
	return __sctp_setsockopt_delayed_ack(sk, params);
}

 
static int sctp_setsockopt_initmsg(struct sock *sk, struct sctp_initmsg *sinit,
				   unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);

	if (optlen != sizeof(struct sctp_initmsg))
		return -EINVAL;

	if (sinit->sinit_num_ostreams)
		sp->initmsg.sinit_num_ostreams = sinit->sinit_num_ostreams;
	if (sinit->sinit_max_instreams)
		sp->initmsg.sinit_max_instreams = sinit->sinit_max_instreams;
	if (sinit->sinit_max_attempts)
		sp->initmsg.sinit_max_attempts = sinit->sinit_max_attempts;
	if (sinit->sinit_max_init_timeo)
		sp->initmsg.sinit_max_init_timeo = sinit->sinit_max_init_timeo;

	return 0;
}

 
static int sctp_setsockopt_default_send_param(struct sock *sk,
					      struct sctp_sndrcvinfo *info,
					      unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;

	if (optlen != sizeof(*info))
		return -EINVAL;
	if (info->sinfo_flags &
	    ~(SCTP_UNORDERED | SCTP_ADDR_OVER |
	      SCTP_ABORT | SCTP_EOF))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, info->sinfo_assoc_id);
	if (!asoc && info->sinfo_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		asoc->default_stream = info->sinfo_stream;
		asoc->default_flags = info->sinfo_flags;
		asoc->default_ppid = info->sinfo_ppid;
		asoc->default_context = info->sinfo_context;
		asoc->default_timetolive = info->sinfo_timetolive;

		return 0;
	}

	if (sctp_style(sk, TCP))
		info->sinfo_assoc_id = SCTP_FUTURE_ASSOC;

	if (info->sinfo_assoc_id == SCTP_FUTURE_ASSOC ||
	    info->sinfo_assoc_id == SCTP_ALL_ASSOC) {
		sp->default_stream = info->sinfo_stream;
		sp->default_flags = info->sinfo_flags;
		sp->default_ppid = info->sinfo_ppid;
		sp->default_context = info->sinfo_context;
		sp->default_timetolive = info->sinfo_timetolive;
	}

	if (info->sinfo_assoc_id == SCTP_CURRENT_ASSOC ||
	    info->sinfo_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &sp->ep->asocs, asocs) {
			asoc->default_stream = info->sinfo_stream;
			asoc->default_flags = info->sinfo_flags;
			asoc->default_ppid = info->sinfo_ppid;
			asoc->default_context = info->sinfo_context;
			asoc->default_timetolive = info->sinfo_timetolive;
		}
	}

	return 0;
}

 
static int sctp_setsockopt_default_sndinfo(struct sock *sk,
					   struct sctp_sndinfo *info,
					   unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;

	if (optlen != sizeof(*info))
		return -EINVAL;
	if (info->snd_flags &
	    ~(SCTP_UNORDERED | SCTP_ADDR_OVER |
	      SCTP_ABORT | SCTP_EOF))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, info->snd_assoc_id);
	if (!asoc && info->snd_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		asoc->default_stream = info->snd_sid;
		asoc->default_flags = info->snd_flags;
		asoc->default_ppid = info->snd_ppid;
		asoc->default_context = info->snd_context;

		return 0;
	}

	if (sctp_style(sk, TCP))
		info->snd_assoc_id = SCTP_FUTURE_ASSOC;

	if (info->snd_assoc_id == SCTP_FUTURE_ASSOC ||
	    info->snd_assoc_id == SCTP_ALL_ASSOC) {
		sp->default_stream = info->snd_sid;
		sp->default_flags = info->snd_flags;
		sp->default_ppid = info->snd_ppid;
		sp->default_context = info->snd_context;
	}

	if (info->snd_assoc_id == SCTP_CURRENT_ASSOC ||
	    info->snd_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &sp->ep->asocs, asocs) {
			asoc->default_stream = info->snd_sid;
			asoc->default_flags = info->snd_flags;
			asoc->default_ppid = info->snd_ppid;
			asoc->default_context = info->snd_context;
		}
	}

	return 0;
}

 
static int sctp_setsockopt_primary_addr(struct sock *sk, struct sctp_prim *prim,
					unsigned int optlen)
{
	struct sctp_transport *trans;
	struct sctp_af *af;
	int err;

	if (optlen != sizeof(struct sctp_prim))
		return -EINVAL;

	 
	af = sctp_get_af_specific(prim->ssp_addr.ss_family);
	if (!af)
		return -EINVAL;

	err = security_sctp_bind_connect(sk, SCTP_PRIMARY_ADDR,
					 (struct sockaddr *)&prim->ssp_addr,
					 af->sockaddr_len);
	if (err)
		return err;

	trans = sctp_addr_id2transport(sk, &prim->ssp_addr, prim->ssp_assoc_id);
	if (!trans)
		return -EINVAL;

	sctp_assoc_set_primary(trans->asoc, trans);

	return 0;
}

 
static int sctp_setsockopt_nodelay(struct sock *sk, int *val,
				   unsigned int optlen)
{
	if (optlen < sizeof(int))
		return -EINVAL;
	sctp_sk(sk)->nodelay = (*val == 0) ? 0 : 1;
	return 0;
}

 
static int sctp_setsockopt_rtoinfo(struct sock *sk,
				   struct sctp_rtoinfo *rtoinfo,
				   unsigned int optlen)
{
	struct sctp_association *asoc;
	unsigned long rto_min, rto_max;
	struct sctp_sock *sp = sctp_sk(sk);

	if (optlen != sizeof (struct sctp_rtoinfo))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, rtoinfo->srto_assoc_id);

	 
	if (!asoc && rtoinfo->srto_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	rto_max = rtoinfo->srto_max;
	rto_min = rtoinfo->srto_min;

	if (rto_max)
		rto_max = asoc ? msecs_to_jiffies(rto_max) : rto_max;
	else
		rto_max = asoc ? asoc->rto_max : sp->rtoinfo.srto_max;

	if (rto_min)
		rto_min = asoc ? msecs_to_jiffies(rto_min) : rto_min;
	else
		rto_min = asoc ? asoc->rto_min : sp->rtoinfo.srto_min;

	if (rto_min > rto_max)
		return -EINVAL;

	if (asoc) {
		if (rtoinfo->srto_initial != 0)
			asoc->rto_initial =
				msecs_to_jiffies(rtoinfo->srto_initial);
		asoc->rto_max = rto_max;
		asoc->rto_min = rto_min;
	} else {
		 
		if (rtoinfo->srto_initial != 0)
			sp->rtoinfo.srto_initial = rtoinfo->srto_initial;
		sp->rtoinfo.srto_max = rto_max;
		sp->rtoinfo.srto_min = rto_min;
	}

	return 0;
}

 
static int sctp_setsockopt_associnfo(struct sock *sk,
				     struct sctp_assocparams *assocparams,
				     unsigned int optlen)
{

	struct sctp_association *asoc;

	if (optlen != sizeof(struct sctp_assocparams))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, assocparams->sasoc_assoc_id);

	if (!asoc && assocparams->sasoc_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	 
	if (asoc) {
		if (assocparams->sasoc_asocmaxrxt != 0) {
			__u32 path_sum = 0;
			int   paths = 0;
			struct sctp_transport *peer_addr;

			list_for_each_entry(peer_addr, &asoc->peer.transport_addr_list,
					transports) {
				path_sum += peer_addr->pathmaxrxt;
				paths++;
			}

			 
			if (paths > 1 &&
			    assocparams->sasoc_asocmaxrxt > path_sum)
				return -EINVAL;

			asoc->max_retrans = assocparams->sasoc_asocmaxrxt;
		}

		if (assocparams->sasoc_cookie_life != 0)
			asoc->cookie_life =
				ms_to_ktime(assocparams->sasoc_cookie_life);
	} else {
		 
		struct sctp_sock *sp = sctp_sk(sk);

		if (assocparams->sasoc_asocmaxrxt != 0)
			sp->assocparams.sasoc_asocmaxrxt =
						assocparams->sasoc_asocmaxrxt;
		if (assocparams->sasoc_cookie_life != 0)
			sp->assocparams.sasoc_cookie_life =
						assocparams->sasoc_cookie_life;
	}
	return 0;
}

 
static int sctp_setsockopt_mappedv4(struct sock *sk, int *val,
				    unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);

	if (optlen < sizeof(int))
		return -EINVAL;
	if (*val)
		sp->v4mapped = 1;
	else
		sp->v4mapped = 0;

	return 0;
}

 
static int sctp_setsockopt_maxseg(struct sock *sk,
				  struct sctp_assoc_value *params,
				  unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	sctp_assoc_t assoc_id;
	int val;

	if (optlen == sizeof(int)) {
		pr_warn_ratelimited(DEPRECATED
				    "%s (pid %d) "
				    "Use of int in maxseg socket option.\n"
				    "Use struct sctp_assoc_value instead\n",
				    current->comm, task_pid_nr(current));
		assoc_id = SCTP_FUTURE_ASSOC;
		val = *(int *)params;
	} else if (optlen == sizeof(struct sctp_assoc_value)) {
		assoc_id = params->assoc_id;
		val = params->assoc_value;
	} else {
		return -EINVAL;
	}

	asoc = sctp_id2assoc(sk, assoc_id);
	if (!asoc && assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (val) {
		int min_len, max_len;
		__u16 datasize = asoc ? sctp_datachk_len(&asoc->stream) :
				 sizeof(struct sctp_data_chunk);

		min_len = sctp_min_frag_point(sp, datasize);
		max_len = SCTP_MAX_CHUNK_LEN - datasize;

		if (val < min_len || val > max_len)
			return -EINVAL;
	}

	if (asoc) {
		asoc->user_frag = val;
		sctp_assoc_update_frag_point(asoc);
	} else {
		sp->user_frag = val;
	}

	return 0;
}


 
static int sctp_setsockopt_peer_primary_addr(struct sock *sk,
					     struct sctp_setpeerprim *prim,
					     unsigned int optlen)
{
	struct sctp_sock	*sp;
	struct sctp_association	*asoc = NULL;
	struct sctp_chunk	*chunk;
	struct sctp_af		*af;
	int 			err;

	sp = sctp_sk(sk);

	if (!sp->ep->asconf_enable)
		return -EPERM;

	if (optlen != sizeof(struct sctp_setpeerprim))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, prim->sspp_assoc_id);
	if (!asoc)
		return -EINVAL;

	if (!asoc->peer.asconf_capable)
		return -EPERM;

	if (asoc->peer.addip_disabled_mask & SCTP_PARAM_SET_PRIMARY)
		return -EPERM;

	if (!sctp_state(asoc, ESTABLISHED))
		return -ENOTCONN;

	af = sctp_get_af_specific(prim->sspp_addr.ss_family);
	if (!af)
		return -EINVAL;

	if (!af->addr_valid((union sctp_addr *)&prim->sspp_addr, sp, NULL))
		return -EADDRNOTAVAIL;

	if (!sctp_assoc_lookup_laddr(asoc, (union sctp_addr *)&prim->sspp_addr))
		return -EADDRNOTAVAIL;

	 
	err = security_sctp_bind_connect(sk, SCTP_SET_PEER_PRIMARY_ADDR,
					 (struct sockaddr *)&prim->sspp_addr,
					 af->sockaddr_len);
	if (err)
		return err;

	 
	chunk = sctp_make_asconf_set_prim(asoc,
					  (union sctp_addr *)&prim->sspp_addr);
	if (!chunk)
		return -ENOMEM;

	err = sctp_send_asconf(asoc, chunk);

	pr_debug("%s: we set peer primary addr primitively\n", __func__);

	return err;
}

static int sctp_setsockopt_adaptation_layer(struct sock *sk,
					    struct sctp_setadaptation *adapt,
					    unsigned int optlen)
{
	if (optlen != sizeof(struct sctp_setadaptation))
		return -EINVAL;

	sctp_sk(sk)->adaptation_ind = adapt->ssb_adaptation_ind;

	return 0;
}

 
static int sctp_setsockopt_context(struct sock *sk,
				   struct sctp_assoc_value *params,
				   unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;

	if (optlen != sizeof(struct sctp_assoc_value))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		asoc->default_rcv_context = params->assoc_value;

		return 0;
	}

	if (sctp_style(sk, TCP))
		params->assoc_id = SCTP_FUTURE_ASSOC;

	if (params->assoc_id == SCTP_FUTURE_ASSOC ||
	    params->assoc_id == SCTP_ALL_ASSOC)
		sp->default_rcv_context = params->assoc_value;

	if (params->assoc_id == SCTP_CURRENT_ASSOC ||
	    params->assoc_id == SCTP_ALL_ASSOC)
		list_for_each_entry(asoc, &sp->ep->asocs, asocs)
			asoc->default_rcv_context = params->assoc_value;

	return 0;
}

 
static int sctp_setsockopt_fragment_interleave(struct sock *sk, int *val,
					       unsigned int optlen)
{
	if (optlen != sizeof(int))
		return -EINVAL;

	sctp_sk(sk)->frag_interleave = !!*val;

	if (!sctp_sk(sk)->frag_interleave)
		sctp_sk(sk)->ep->intl_enable = 0;

	return 0;
}

 
static int sctp_setsockopt_partial_delivery_point(struct sock *sk, u32 *val,
						  unsigned int optlen)
{
	if (optlen != sizeof(u32))
		return -EINVAL;

	 
	if (*val > (sk->sk_rcvbuf >> 1))
		return -EINVAL;

	sctp_sk(sk)->pd_point = *val;

	return 0;  
}

 
static int sctp_setsockopt_maxburst(struct sock *sk,
				    struct sctp_assoc_value *params,
				    unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	sctp_assoc_t assoc_id;
	u32 assoc_value;

	if (optlen == sizeof(int)) {
		pr_warn_ratelimited(DEPRECATED
				    "%s (pid %d) "
				    "Use of int in max_burst socket option deprecated.\n"
				    "Use struct sctp_assoc_value instead\n",
				    current->comm, task_pid_nr(current));
		assoc_id = SCTP_FUTURE_ASSOC;
		assoc_value = *((int *)params);
	} else if (optlen == sizeof(struct sctp_assoc_value)) {
		assoc_id = params->assoc_id;
		assoc_value = params->assoc_value;
	} else
		return -EINVAL;

	asoc = sctp_id2assoc(sk, assoc_id);
	if (!asoc && assoc_id > SCTP_ALL_ASSOC && sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		asoc->max_burst = assoc_value;

		return 0;
	}

	if (sctp_style(sk, TCP))
		assoc_id = SCTP_FUTURE_ASSOC;

	if (assoc_id == SCTP_FUTURE_ASSOC || assoc_id == SCTP_ALL_ASSOC)
		sp->max_burst = assoc_value;

	if (assoc_id == SCTP_CURRENT_ASSOC || assoc_id == SCTP_ALL_ASSOC)
		list_for_each_entry(asoc, &sp->ep->asocs, asocs)
			asoc->max_burst = assoc_value;

	return 0;
}

 
static int sctp_setsockopt_auth_chunk(struct sock *sk,
				      struct sctp_authchunk *val,
				      unsigned int optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;

	if (!ep->auth_enable)
		return -EACCES;

	if (optlen != sizeof(struct sctp_authchunk))
		return -EINVAL;

	switch (val->sauth_chunk) {
	case SCTP_CID_INIT:
	case SCTP_CID_INIT_ACK:
	case SCTP_CID_SHUTDOWN_COMPLETE:
	case SCTP_CID_AUTH:
		return -EINVAL;
	}

	 
	return sctp_auth_ep_add_chunkid(ep, val->sauth_chunk);
}

 
static int sctp_setsockopt_hmac_ident(struct sock *sk,
				      struct sctp_hmacalgo *hmacs,
				      unsigned int optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	u32 idents;

	if (!ep->auth_enable)
		return -EACCES;

	if (optlen < sizeof(struct sctp_hmacalgo))
		return -EINVAL;
	optlen = min_t(unsigned int, optlen, sizeof(struct sctp_hmacalgo) +
					     SCTP_AUTH_NUM_HMACS * sizeof(u16));

	idents = hmacs->shmac_num_idents;
	if (idents == 0 || idents > SCTP_AUTH_NUM_HMACS ||
	    (idents * sizeof(u16)) > (optlen - sizeof(struct sctp_hmacalgo)))
		return -EINVAL;

	return sctp_auth_ep_set_hmacs(ep, hmacs);
}

 
static int sctp_setsockopt_auth_key(struct sock *sk,
				    struct sctp_authkey *authkey,
				    unsigned int optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_association *asoc;
	int ret = -EINVAL;

	if (optlen <= sizeof(struct sctp_authkey))
		return -EINVAL;
	 
	optlen = min_t(unsigned int, optlen, USHRT_MAX + sizeof(*authkey));

	if (authkey->sca_keylength > optlen - sizeof(*authkey))
		goto out;

	asoc = sctp_id2assoc(sk, authkey->sca_assoc_id);
	if (!asoc && authkey->sca_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	if (asoc) {
		ret = sctp_auth_set_key(ep, asoc, authkey);
		goto out;
	}

	if (sctp_style(sk, TCP))
		authkey->sca_assoc_id = SCTP_FUTURE_ASSOC;

	if (authkey->sca_assoc_id == SCTP_FUTURE_ASSOC ||
	    authkey->sca_assoc_id == SCTP_ALL_ASSOC) {
		ret = sctp_auth_set_key(ep, asoc, authkey);
		if (ret)
			goto out;
	}

	ret = 0;

	if (authkey->sca_assoc_id == SCTP_CURRENT_ASSOC ||
	    authkey->sca_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &ep->asocs, asocs) {
			int res = sctp_auth_set_key(ep, asoc, authkey);

			if (res && !ret)
				ret = res;
		}
	}

out:
	memzero_explicit(authkey, optlen);
	return ret;
}

 
static int sctp_setsockopt_active_key(struct sock *sk,
				      struct sctp_authkeyid *val,
				      unsigned int optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_association *asoc;
	int ret = 0;

	if (optlen != sizeof(struct sctp_authkeyid))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, val->scact_assoc_id);
	if (!asoc && val->scact_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc)
		return sctp_auth_set_active_key(ep, asoc, val->scact_keynumber);

	if (sctp_style(sk, TCP))
		val->scact_assoc_id = SCTP_FUTURE_ASSOC;

	if (val->scact_assoc_id == SCTP_FUTURE_ASSOC ||
	    val->scact_assoc_id == SCTP_ALL_ASSOC) {
		ret = sctp_auth_set_active_key(ep, asoc, val->scact_keynumber);
		if (ret)
			return ret;
	}

	if (val->scact_assoc_id == SCTP_CURRENT_ASSOC ||
	    val->scact_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &ep->asocs, asocs) {
			int res = sctp_auth_set_active_key(ep, asoc,
							   val->scact_keynumber);

			if (res && !ret)
				ret = res;
		}
	}

	return ret;
}

 
static int sctp_setsockopt_del_key(struct sock *sk,
				   struct sctp_authkeyid *val,
				   unsigned int optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_association *asoc;
	int ret = 0;

	if (optlen != sizeof(struct sctp_authkeyid))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, val->scact_assoc_id);
	if (!asoc && val->scact_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc)
		return sctp_auth_del_key_id(ep, asoc, val->scact_keynumber);

	if (sctp_style(sk, TCP))
		val->scact_assoc_id = SCTP_FUTURE_ASSOC;

	if (val->scact_assoc_id == SCTP_FUTURE_ASSOC ||
	    val->scact_assoc_id == SCTP_ALL_ASSOC) {
		ret = sctp_auth_del_key_id(ep, asoc, val->scact_keynumber);
		if (ret)
			return ret;
	}

	if (val->scact_assoc_id == SCTP_CURRENT_ASSOC ||
	    val->scact_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &ep->asocs, asocs) {
			int res = sctp_auth_del_key_id(ep, asoc,
						       val->scact_keynumber);

			if (res && !ret)
				ret = res;
		}
	}

	return ret;
}

 
static int sctp_setsockopt_deactivate_key(struct sock *sk,
					  struct sctp_authkeyid *val,
					  unsigned int optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_association *asoc;
	int ret = 0;

	if (optlen != sizeof(struct sctp_authkeyid))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, val->scact_assoc_id);
	if (!asoc && val->scact_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc)
		return sctp_auth_deact_key_id(ep, asoc, val->scact_keynumber);

	if (sctp_style(sk, TCP))
		val->scact_assoc_id = SCTP_FUTURE_ASSOC;

	if (val->scact_assoc_id == SCTP_FUTURE_ASSOC ||
	    val->scact_assoc_id == SCTP_ALL_ASSOC) {
		ret = sctp_auth_deact_key_id(ep, asoc, val->scact_keynumber);
		if (ret)
			return ret;
	}

	if (val->scact_assoc_id == SCTP_CURRENT_ASSOC ||
	    val->scact_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &ep->asocs, asocs) {
			int res = sctp_auth_deact_key_id(ep, asoc,
							 val->scact_keynumber);

			if (res && !ret)
				ret = res;
		}
	}

	return ret;
}

 
static int sctp_setsockopt_auto_asconf(struct sock *sk, int *val,
					unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);

	if (optlen < sizeof(int))
		return -EINVAL;
	if (!sctp_is_ep_boundall(sk) && *val)
		return -EINVAL;
	if ((*val && sp->do_auto_asconf) || (!*val && !sp->do_auto_asconf))
		return 0;

	spin_lock_bh(&sock_net(sk)->sctp.addr_wq_lock);
	if (*val == 0 && sp->do_auto_asconf) {
		list_del(&sp->auto_asconf_list);
		sp->do_auto_asconf = 0;
	} else if (*val && !sp->do_auto_asconf) {
		list_add_tail(&sp->auto_asconf_list,
		    &sock_net(sk)->sctp.auto_asconf_splist);
		sp->do_auto_asconf = 1;
	}
	spin_unlock_bh(&sock_net(sk)->sctp.addr_wq_lock);
	return 0;
}

 
static int sctp_setsockopt_paddr_thresholds(struct sock *sk,
					    struct sctp_paddrthlds_v2 *val,
					    unsigned int optlen, bool v2)
{
	struct sctp_transport *trans;
	struct sctp_association *asoc;
	int len;

	len = v2 ? sizeof(*val) : sizeof(struct sctp_paddrthlds);
	if (optlen < len)
		return -EINVAL;

	if (v2 && val->spt_pathpfthld > val->spt_pathcpthld)
		return -EINVAL;

	if (!sctp_is_any(sk, (const union sctp_addr *)&val->spt_address)) {
		trans = sctp_addr_id2transport(sk, &val->spt_address,
					       val->spt_assoc_id);
		if (!trans)
			return -ENOENT;

		if (val->spt_pathmaxrxt)
			trans->pathmaxrxt = val->spt_pathmaxrxt;
		if (v2)
			trans->ps_retrans = val->spt_pathcpthld;
		trans->pf_retrans = val->spt_pathpfthld;

		return 0;
	}

	asoc = sctp_id2assoc(sk, val->spt_assoc_id);
	if (!asoc && val->spt_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		list_for_each_entry(trans, &asoc->peer.transport_addr_list,
				    transports) {
			if (val->spt_pathmaxrxt)
				trans->pathmaxrxt = val->spt_pathmaxrxt;
			if (v2)
				trans->ps_retrans = val->spt_pathcpthld;
			trans->pf_retrans = val->spt_pathpfthld;
		}

		if (val->spt_pathmaxrxt)
			asoc->pathmaxrxt = val->spt_pathmaxrxt;
		if (v2)
			asoc->ps_retrans = val->spt_pathcpthld;
		asoc->pf_retrans = val->spt_pathpfthld;
	} else {
		struct sctp_sock *sp = sctp_sk(sk);

		if (val->spt_pathmaxrxt)
			sp->pathmaxrxt = val->spt_pathmaxrxt;
		if (v2)
			sp->ps_retrans = val->spt_pathcpthld;
		sp->pf_retrans = val->spt_pathpfthld;
	}

	return 0;
}

static int sctp_setsockopt_recvrcvinfo(struct sock *sk, int *val,
				       unsigned int optlen)
{
	if (optlen < sizeof(int))
		return -EINVAL;

	sctp_sk(sk)->recvrcvinfo = (*val == 0) ? 0 : 1;

	return 0;
}

static int sctp_setsockopt_recvnxtinfo(struct sock *sk, int *val,
				       unsigned int optlen)
{
	if (optlen < sizeof(int))
		return -EINVAL;

	sctp_sk(sk)->recvnxtinfo = (*val == 0) ? 0 : 1;

	return 0;
}

static int sctp_setsockopt_pr_supported(struct sock *sk,
					struct sctp_assoc_value *params,
					unsigned int optlen)
{
	struct sctp_association *asoc;

	if (optlen != sizeof(*params))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	sctp_sk(sk)->ep->prsctp_enable = !!params->assoc_value;

	return 0;
}

static int sctp_setsockopt_default_prinfo(struct sock *sk,
					  struct sctp_default_prinfo *info,
					  unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	int retval = -EINVAL;

	if (optlen != sizeof(*info))
		goto out;

	if (info->pr_policy & ~SCTP_PR_SCTP_MASK)
		goto out;

	if (info->pr_policy == SCTP_PR_SCTP_NONE)
		info->pr_value = 0;

	asoc = sctp_id2assoc(sk, info->pr_assoc_id);
	if (!asoc && info->pr_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	retval = 0;

	if (asoc) {
		SCTP_PR_SET_POLICY(asoc->default_flags, info->pr_policy);
		asoc->default_timetolive = info->pr_value;
		goto out;
	}

	if (sctp_style(sk, TCP))
		info->pr_assoc_id = SCTP_FUTURE_ASSOC;

	if (info->pr_assoc_id == SCTP_FUTURE_ASSOC ||
	    info->pr_assoc_id == SCTP_ALL_ASSOC) {
		SCTP_PR_SET_POLICY(sp->default_flags, info->pr_policy);
		sp->default_timetolive = info->pr_value;
	}

	if (info->pr_assoc_id == SCTP_CURRENT_ASSOC ||
	    info->pr_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &sp->ep->asocs, asocs) {
			SCTP_PR_SET_POLICY(asoc->default_flags,
					   info->pr_policy);
			asoc->default_timetolive = info->pr_value;
		}
	}

out:
	return retval;
}

static int sctp_setsockopt_reconfig_supported(struct sock *sk,
					      struct sctp_assoc_value *params,
					      unsigned int optlen)
{
	struct sctp_association *asoc;
	int retval = -EINVAL;

	if (optlen != sizeof(*params))
		goto out;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	sctp_sk(sk)->ep->reconf_enable = !!params->assoc_value;

	retval = 0;

out:
	return retval;
}

static int sctp_setsockopt_enable_strreset(struct sock *sk,
					   struct sctp_assoc_value *params,
					   unsigned int optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_association *asoc;
	int retval = -EINVAL;

	if (optlen != sizeof(*params))
		goto out;

	if (params->assoc_value & (~SCTP_ENABLE_STRRESET_MASK))
		goto out;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	retval = 0;

	if (asoc) {
		asoc->strreset_enable = params->assoc_value;
		goto out;
	}

	if (sctp_style(sk, TCP))
		params->assoc_id = SCTP_FUTURE_ASSOC;

	if (params->assoc_id == SCTP_FUTURE_ASSOC ||
	    params->assoc_id == SCTP_ALL_ASSOC)
		ep->strreset_enable = params->assoc_value;

	if (params->assoc_id == SCTP_CURRENT_ASSOC ||
	    params->assoc_id == SCTP_ALL_ASSOC)
		list_for_each_entry(asoc, &ep->asocs, asocs)
			asoc->strreset_enable = params->assoc_value;

out:
	return retval;
}

static int sctp_setsockopt_reset_streams(struct sock *sk,
					 struct sctp_reset_streams *params,
					 unsigned int optlen)
{
	struct sctp_association *asoc;

	if (optlen < sizeof(*params))
		return -EINVAL;
	 
	optlen = min_t(unsigned int, optlen, USHRT_MAX +
					     sizeof(__u16) * sizeof(*params));

	if (params->srs_number_streams * sizeof(__u16) >
	    optlen - sizeof(*params))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params->srs_assoc_id);
	if (!asoc)
		return -EINVAL;

	return sctp_send_reset_streams(asoc, params);
}

static int sctp_setsockopt_reset_assoc(struct sock *sk, sctp_assoc_t *associd,
				       unsigned int optlen)
{
	struct sctp_association *asoc;

	if (optlen != sizeof(*associd))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, *associd);
	if (!asoc)
		return -EINVAL;

	return sctp_send_reset_assoc(asoc);
}

static int sctp_setsockopt_add_streams(struct sock *sk,
				       struct sctp_add_streams *params,
				       unsigned int optlen)
{
	struct sctp_association *asoc;

	if (optlen != sizeof(*params))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params->sas_assoc_id);
	if (!asoc)
		return -EINVAL;

	return sctp_send_add_streams(asoc, params);
}

static int sctp_setsockopt_scheduler(struct sock *sk,
				     struct sctp_assoc_value *params,
				     unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	int retval = 0;

	if (optlen < sizeof(*params))
		return -EINVAL;

	if (params->assoc_value > SCTP_SS_MAX)
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc)
		return sctp_sched_set_sched(asoc, params->assoc_value);

	if (sctp_style(sk, TCP))
		params->assoc_id = SCTP_FUTURE_ASSOC;

	if (params->assoc_id == SCTP_FUTURE_ASSOC ||
	    params->assoc_id == SCTP_ALL_ASSOC)
		sp->default_ss = params->assoc_value;

	if (params->assoc_id == SCTP_CURRENT_ASSOC ||
	    params->assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &sp->ep->asocs, asocs) {
			int ret = sctp_sched_set_sched(asoc,
						       params->assoc_value);

			if (ret && !retval)
				retval = ret;
		}
	}

	return retval;
}

static int sctp_setsockopt_scheduler_value(struct sock *sk,
					   struct sctp_stream_value *params,
					   unsigned int optlen)
{
	struct sctp_association *asoc;
	int retval = -EINVAL;

	if (optlen < sizeof(*params))
		goto out;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id != SCTP_CURRENT_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	if (asoc) {
		retval = sctp_sched_set_value(asoc, params->stream_id,
					      params->stream_value, GFP_KERNEL);
		goto out;
	}

	retval = 0;

	list_for_each_entry(asoc, &sctp_sk(sk)->ep->asocs, asocs) {
		int ret = sctp_sched_set_value(asoc, params->stream_id,
					       params->stream_value,
					       GFP_KERNEL);
		if (ret && !retval)  
			retval = ret;
	}

out:
	return retval;
}

static int sctp_setsockopt_interleaving_supported(struct sock *sk,
						  struct sctp_assoc_value *p,
						  unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;

	if (optlen < sizeof(*p))
		return -EINVAL;

	asoc = sctp_id2assoc(sk, p->assoc_id);
	if (!asoc && p->assoc_id != SCTP_FUTURE_ASSOC && sctp_style(sk, UDP))
		return -EINVAL;

	if (!sock_net(sk)->sctp.intl_enable || !sp->frag_interleave) {
		return -EPERM;
	}

	sp->ep->intl_enable = !!p->assoc_value;
	return 0;
}

static int sctp_setsockopt_reuse_port(struct sock *sk, int *val,
				      unsigned int optlen)
{
	if (!sctp_style(sk, TCP))
		return -EOPNOTSUPP;

	if (sctp_sk(sk)->ep->base.bind_addr.port)
		return -EFAULT;

	if (optlen < sizeof(int))
		return -EINVAL;

	sctp_sk(sk)->reuse = !!*val;

	return 0;
}

static int sctp_assoc_ulpevent_type_set(struct sctp_event *param,
					struct sctp_association *asoc)
{
	struct sctp_ulpevent *event;

	sctp_ulpevent_type_set(&asoc->subscribe, param->se_type, param->se_on);

	if (param->se_type == SCTP_SENDER_DRY_EVENT && param->se_on) {
		if (sctp_outq_is_empty(&asoc->outqueue)) {
			event = sctp_ulpevent_make_sender_dry_event(asoc,
					GFP_USER | __GFP_NOWARN);
			if (!event)
				return -ENOMEM;

			asoc->stream.si->enqueue_event(&asoc->ulpq, event);
		}
	}

	return 0;
}

static int sctp_setsockopt_event(struct sock *sk, struct sctp_event *param,
				 unsigned int optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	int retval = 0;

	if (optlen < sizeof(*param))
		return -EINVAL;

	if (param->se_type < SCTP_SN_TYPE_BASE ||
	    param->se_type > SCTP_SN_TYPE_MAX)
		return -EINVAL;

	asoc = sctp_id2assoc(sk, param->se_assoc_id);
	if (!asoc && param->se_assoc_id > SCTP_ALL_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc)
		return sctp_assoc_ulpevent_type_set(param, asoc);

	if (sctp_style(sk, TCP))
		param->se_assoc_id = SCTP_FUTURE_ASSOC;

	if (param->se_assoc_id == SCTP_FUTURE_ASSOC ||
	    param->se_assoc_id == SCTP_ALL_ASSOC)
		sctp_ulpevent_type_set(&sp->subscribe,
				       param->se_type, param->se_on);

	if (param->se_assoc_id == SCTP_CURRENT_ASSOC ||
	    param->se_assoc_id == SCTP_ALL_ASSOC) {
		list_for_each_entry(asoc, &sp->ep->asocs, asocs) {
			int ret = sctp_assoc_ulpevent_type_set(param, asoc);

			if (ret && !retval)
				retval = ret;
		}
	}

	return retval;
}

static int sctp_setsockopt_asconf_supported(struct sock *sk,
					    struct sctp_assoc_value *params,
					    unsigned int optlen)
{
	struct sctp_association *asoc;
	struct sctp_endpoint *ep;
	int retval = -EINVAL;

	if (optlen != sizeof(*params))
		goto out;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	ep = sctp_sk(sk)->ep;
	ep->asconf_enable = !!params->assoc_value;

	if (ep->asconf_enable && ep->auth_enable) {
		sctp_auth_ep_add_chunkid(ep, SCTP_CID_ASCONF);
		sctp_auth_ep_add_chunkid(ep, SCTP_CID_ASCONF_ACK);
	}

	retval = 0;

out:
	return retval;
}

static int sctp_setsockopt_auth_supported(struct sock *sk,
					  struct sctp_assoc_value *params,
					  unsigned int optlen)
{
	struct sctp_association *asoc;
	struct sctp_endpoint *ep;
	int retval = -EINVAL;

	if (optlen != sizeof(*params))
		goto out;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	ep = sctp_sk(sk)->ep;
	if (params->assoc_value) {
		retval = sctp_auth_init(ep, GFP_KERNEL);
		if (retval)
			goto out;
		if (ep->asconf_enable) {
			sctp_auth_ep_add_chunkid(ep, SCTP_CID_ASCONF);
			sctp_auth_ep_add_chunkid(ep, SCTP_CID_ASCONF_ACK);
		}
	}

	ep->auth_enable = !!params->assoc_value;
	retval = 0;

out:
	return retval;
}

static int sctp_setsockopt_ecn_supported(struct sock *sk,
					 struct sctp_assoc_value *params,
					 unsigned int optlen)
{
	struct sctp_association *asoc;
	int retval = -EINVAL;

	if (optlen != sizeof(*params))
		goto out;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	sctp_sk(sk)->ep->ecn_enable = !!params->assoc_value;
	retval = 0;

out:
	return retval;
}

static int sctp_setsockopt_pf_expose(struct sock *sk,
				     struct sctp_assoc_value *params,
				     unsigned int optlen)
{
	struct sctp_association *asoc;
	int retval = -EINVAL;

	if (optlen != sizeof(*params))
		goto out;

	if (params->assoc_value > SCTP_PF_EXPOSE_MAX)
		goto out;

	asoc = sctp_id2assoc(sk, params->assoc_id);
	if (!asoc && params->assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		goto out;

	if (asoc)
		asoc->pf_expose = params->assoc_value;
	else
		sctp_sk(sk)->pf_expose = params->assoc_value;
	retval = 0;

out:
	return retval;
}

static int sctp_setsockopt_encap_port(struct sock *sk,
				      struct sctp_udpencaps *encap,
				      unsigned int optlen)
{
	struct sctp_association *asoc;
	struct sctp_transport *t;
	__be16 encap_port;

	if (optlen != sizeof(*encap))
		return -EINVAL;

	 
	encap_port = (__force __be16)encap->sue_port;
	if (!sctp_is_any(sk, (union sctp_addr *)&encap->sue_address)) {
		t = sctp_addr_id2transport(sk, &encap->sue_address,
					   encap->sue_assoc_id);
		if (!t)
			return -EINVAL;

		t->encap_port = encap_port;
		return 0;
	}

	 
	asoc = sctp_id2assoc(sk, encap->sue_assoc_id);
	if (!asoc && encap->sue_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	 
	if (asoc) {
		list_for_each_entry(t, &asoc->peer.transport_addr_list,
				    transports)
			t->encap_port = encap_port;

		asoc->encap_port = encap_port;
		return 0;
	}

	sctp_sk(sk)->encap_port = encap_port;
	return 0;
}

static int sctp_setsockopt_probe_interval(struct sock *sk,
					  struct sctp_probeinterval *params,
					  unsigned int optlen)
{
	struct sctp_association *asoc;
	struct sctp_transport *t;
	__u32 probe_interval;

	if (optlen != sizeof(*params))
		return -EINVAL;

	probe_interval = params->spi_interval;
	if (probe_interval && probe_interval < SCTP_PROBE_TIMER_MIN)
		return -EINVAL;

	 
	if (!sctp_is_any(sk, (union sctp_addr *)&params->spi_address)) {
		t = sctp_addr_id2transport(sk, &params->spi_address,
					   params->spi_assoc_id);
		if (!t)
			return -EINVAL;

		t->probe_interval = msecs_to_jiffies(probe_interval);
		sctp_transport_pl_reset(t);
		return 0;
	}

	 
	asoc = sctp_id2assoc(sk, params->spi_assoc_id);
	if (!asoc && params->spi_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	 
	if (asoc) {
		list_for_each_entry(t, &asoc->peer.transport_addr_list, transports) {
			t->probe_interval = msecs_to_jiffies(probe_interval);
			sctp_transport_pl_reset(t);
		}

		asoc->probe_interval = msecs_to_jiffies(probe_interval);
		return 0;
	}

	sctp_sk(sk)->probe_interval = probe_interval;
	return 0;
}

 
static int sctp_setsockopt(struct sock *sk, int level, int optname,
			   sockptr_t optval, unsigned int optlen)
{
	void *kopt = NULL;
	int retval = 0;

	pr_debug("%s: sk:%p, optname:%d\n", __func__, sk, optname);

	 
	if (level != SOL_SCTP) {
		struct sctp_af *af = sctp_sk(sk)->pf->af;

		return af->setsockopt(sk, level, optname, optval, optlen);
	}

	if (optlen > 0) {
		 
		optlen = min_t(unsigned int, optlen,
			       PAGE_ALIGN(USHRT_MAX +
					  sizeof(__u16) * sizeof(struct sctp_reset_streams)));
		kopt = memdup_sockptr(optval, optlen);
		if (IS_ERR(kopt))
			return PTR_ERR(kopt);
	}

	lock_sock(sk);

	switch (optname) {
	case SCTP_SOCKOPT_BINDX_ADD:
		 
		retval = sctp_setsockopt_bindx(sk, kopt, optlen,
					       SCTP_BINDX_ADD_ADDR);
		break;

	case SCTP_SOCKOPT_BINDX_REM:
		 
		retval = sctp_setsockopt_bindx(sk, kopt, optlen,
					       SCTP_BINDX_REM_ADDR);
		break;

	case SCTP_SOCKOPT_CONNECTX_OLD:
		 
		retval = sctp_setsockopt_connectx_old(sk, kopt, optlen);
		break;

	case SCTP_SOCKOPT_CONNECTX:
		 
		retval = sctp_setsockopt_connectx(sk, kopt, optlen);
		break;

	case SCTP_DISABLE_FRAGMENTS:
		retval = sctp_setsockopt_disable_fragments(sk, kopt, optlen);
		break;

	case SCTP_EVENTS:
		retval = sctp_setsockopt_events(sk, kopt, optlen);
		break;

	case SCTP_AUTOCLOSE:
		retval = sctp_setsockopt_autoclose(sk, kopt, optlen);
		break;

	case SCTP_PEER_ADDR_PARAMS:
		retval = sctp_setsockopt_peer_addr_params(sk, kopt, optlen);
		break;

	case SCTP_DELAYED_SACK:
		retval = sctp_setsockopt_delayed_ack(sk, kopt, optlen);
		break;
	case SCTP_PARTIAL_DELIVERY_POINT:
		retval = sctp_setsockopt_partial_delivery_point(sk, kopt, optlen);
		break;

	case SCTP_INITMSG:
		retval = sctp_setsockopt_initmsg(sk, kopt, optlen);
		break;
	case SCTP_DEFAULT_SEND_PARAM:
		retval = sctp_setsockopt_default_send_param(sk, kopt, optlen);
		break;
	case SCTP_DEFAULT_SNDINFO:
		retval = sctp_setsockopt_default_sndinfo(sk, kopt, optlen);
		break;
	case SCTP_PRIMARY_ADDR:
		retval = sctp_setsockopt_primary_addr(sk, kopt, optlen);
		break;
	case SCTP_SET_PEER_PRIMARY_ADDR:
		retval = sctp_setsockopt_peer_primary_addr(sk, kopt, optlen);
		break;
	case SCTP_NODELAY:
		retval = sctp_setsockopt_nodelay(sk, kopt, optlen);
		break;
	case SCTP_RTOINFO:
		retval = sctp_setsockopt_rtoinfo(sk, kopt, optlen);
		break;
	case SCTP_ASSOCINFO:
		retval = sctp_setsockopt_associnfo(sk, kopt, optlen);
		break;
	case SCTP_I_WANT_MAPPED_V4_ADDR:
		retval = sctp_setsockopt_mappedv4(sk, kopt, optlen);
		break;
	case SCTP_MAXSEG:
		retval = sctp_setsockopt_maxseg(sk, kopt, optlen);
		break;
	case SCTP_ADAPTATION_LAYER:
		retval = sctp_setsockopt_adaptation_layer(sk, kopt, optlen);
		break;
	case SCTP_CONTEXT:
		retval = sctp_setsockopt_context(sk, kopt, optlen);
		break;
	case SCTP_FRAGMENT_INTERLEAVE:
		retval = sctp_setsockopt_fragment_interleave(sk, kopt, optlen);
		break;
	case SCTP_MAX_BURST:
		retval = sctp_setsockopt_maxburst(sk, kopt, optlen);
		break;
	case SCTP_AUTH_CHUNK:
		retval = sctp_setsockopt_auth_chunk(sk, kopt, optlen);
		break;
	case SCTP_HMAC_IDENT:
		retval = sctp_setsockopt_hmac_ident(sk, kopt, optlen);
		break;
	case SCTP_AUTH_KEY:
		retval = sctp_setsockopt_auth_key(sk, kopt, optlen);
		break;
	case SCTP_AUTH_ACTIVE_KEY:
		retval = sctp_setsockopt_active_key(sk, kopt, optlen);
		break;
	case SCTP_AUTH_DELETE_KEY:
		retval = sctp_setsockopt_del_key(sk, kopt, optlen);
		break;
	case SCTP_AUTH_DEACTIVATE_KEY:
		retval = sctp_setsockopt_deactivate_key(sk, kopt, optlen);
		break;
	case SCTP_AUTO_ASCONF:
		retval = sctp_setsockopt_auto_asconf(sk, kopt, optlen);
		break;
	case SCTP_PEER_ADDR_THLDS:
		retval = sctp_setsockopt_paddr_thresholds(sk, kopt, optlen,
							  false);
		break;
	case SCTP_PEER_ADDR_THLDS_V2:
		retval = sctp_setsockopt_paddr_thresholds(sk, kopt, optlen,
							  true);
		break;
	case SCTP_RECVRCVINFO:
		retval = sctp_setsockopt_recvrcvinfo(sk, kopt, optlen);
		break;
	case SCTP_RECVNXTINFO:
		retval = sctp_setsockopt_recvnxtinfo(sk, kopt, optlen);
		break;
	case SCTP_PR_SUPPORTED:
		retval = sctp_setsockopt_pr_supported(sk, kopt, optlen);
		break;
	case SCTP_DEFAULT_PRINFO:
		retval = sctp_setsockopt_default_prinfo(sk, kopt, optlen);
		break;
	case SCTP_RECONFIG_SUPPORTED:
		retval = sctp_setsockopt_reconfig_supported(sk, kopt, optlen);
		break;
	case SCTP_ENABLE_STREAM_RESET:
		retval = sctp_setsockopt_enable_strreset(sk, kopt, optlen);
		break;
	case SCTP_RESET_STREAMS:
		retval = sctp_setsockopt_reset_streams(sk, kopt, optlen);
		break;
	case SCTP_RESET_ASSOC:
		retval = sctp_setsockopt_reset_assoc(sk, kopt, optlen);
		break;
	case SCTP_ADD_STREAMS:
		retval = sctp_setsockopt_add_streams(sk, kopt, optlen);
		break;
	case SCTP_STREAM_SCHEDULER:
		retval = sctp_setsockopt_scheduler(sk, kopt, optlen);
		break;
	case SCTP_STREAM_SCHEDULER_VALUE:
		retval = sctp_setsockopt_scheduler_value(sk, kopt, optlen);
		break;
	case SCTP_INTERLEAVING_SUPPORTED:
		retval = sctp_setsockopt_interleaving_supported(sk, kopt,
								optlen);
		break;
	case SCTP_REUSE_PORT:
		retval = sctp_setsockopt_reuse_port(sk, kopt, optlen);
		break;
	case SCTP_EVENT:
		retval = sctp_setsockopt_event(sk, kopt, optlen);
		break;
	case SCTP_ASCONF_SUPPORTED:
		retval = sctp_setsockopt_asconf_supported(sk, kopt, optlen);
		break;
	case SCTP_AUTH_SUPPORTED:
		retval = sctp_setsockopt_auth_supported(sk, kopt, optlen);
		break;
	case SCTP_ECN_SUPPORTED:
		retval = sctp_setsockopt_ecn_supported(sk, kopt, optlen);
		break;
	case SCTP_EXPOSE_POTENTIALLY_FAILED_STATE:
		retval = sctp_setsockopt_pf_expose(sk, kopt, optlen);
		break;
	case SCTP_REMOTE_UDP_ENCAPS_PORT:
		retval = sctp_setsockopt_encap_port(sk, kopt, optlen);
		break;
	case SCTP_PLPMTUD_PROBE_INTERVAL:
		retval = sctp_setsockopt_probe_interval(sk, kopt, optlen);
		break;
	default:
		retval = -ENOPROTOOPT;
		break;
	}

	release_sock(sk);
	kfree(kopt);
	return retval;
}

 
static int sctp_connect(struct sock *sk, struct sockaddr *addr,
			int addr_len, int flags)
{
	struct sctp_af *af;
	int err = -EINVAL;

	lock_sock(sk);
	pr_debug("%s: sk:%p, sockaddr:%p, addr_len:%d\n", __func__, sk,
		 addr, addr_len);

	 
	af = sctp_get_af_specific(addr->sa_family);
	if (af && addr_len >= af->sockaddr_len)
		err = __sctp_connect(sk, addr, af->sockaddr_len, flags, NULL);

	release_sock(sk);
	return err;
}

int sctp_inet_connect(struct socket *sock, struct sockaddr *uaddr,
		      int addr_len, int flags)
{
	if (addr_len < sizeof(uaddr->sa_family))
		return -EINVAL;

	if (uaddr->sa_family == AF_UNSPEC)
		return -EOPNOTSUPP;

	return sctp_connect(sock->sk, uaddr, addr_len, flags);
}

 
static int sctp_disconnect(struct sock *sk, int flags)
{
	return -EOPNOTSUPP;  
}

 
static struct sock *sctp_accept(struct sock *sk, int flags, int *err, bool kern)
{
	struct sctp_sock *sp;
	struct sctp_endpoint *ep;
	struct sock *newsk = NULL;
	struct sctp_association *asoc;
	long timeo;
	int error = 0;

	lock_sock(sk);

	sp = sctp_sk(sk);
	ep = sp->ep;

	if (!sctp_style(sk, TCP)) {
		error = -EOPNOTSUPP;
		goto out;
	}

	if (!sctp_sstate(sk, LISTENING)) {
		error = -EINVAL;
		goto out;
	}

	timeo = sock_rcvtimeo(sk, flags & O_NONBLOCK);

	error = sctp_wait_for_accept(sk, timeo);
	if (error)
		goto out;

	 
	asoc = list_entry(ep->asocs.next, struct sctp_association, asocs);

	newsk = sp->pf->create_accept_sk(sk, asoc, kern);
	if (!newsk) {
		error = -ENOMEM;
		goto out;
	}

	 
	error = sctp_sock_migrate(sk, newsk, asoc, SCTP_SOCKET_TCP);
	if (error) {
		sk_common_release(newsk);
		newsk = NULL;
	}

out:
	release_sock(sk);
	*err = error;
	return newsk;
}

 
static int sctp_ioctl(struct sock *sk, int cmd, int *karg)
{
	int rc = -ENOTCONN;

	lock_sock(sk);

	 
	if (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING))
		goto out;

	switch (cmd) {
	case SIOCINQ: {
		struct sk_buff *skb;
		*karg = 0;

		skb = skb_peek(&sk->sk_receive_queue);
		if (skb != NULL) {
			 
			*karg = skb->len;
		}
		rc = 0;
		break;
	}
	default:
		rc = -ENOIOCTLCMD;
		break;
	}
out:
	release_sock(sk);
	return rc;
}

 
static int sctp_init_sock(struct sock *sk)
{
	struct net *net = sock_net(sk);
	struct sctp_sock *sp;

	pr_debug("%s: sk:%p\n", __func__, sk);

	sp = sctp_sk(sk);

	 
	switch (sk->sk_type) {
	case SOCK_SEQPACKET:
		sp->type = SCTP_SOCKET_UDP;
		break;
	case SOCK_STREAM:
		sp->type = SCTP_SOCKET_TCP;
		break;
	default:
		return -ESOCKTNOSUPPORT;
	}

	sk->sk_gso_type = SKB_GSO_SCTP;

	 
	sp->default_stream = 0;
	sp->default_ppid = 0;
	sp->default_flags = 0;
	sp->default_context = 0;
	sp->default_timetolive = 0;

	sp->default_rcv_context = 0;
	sp->max_burst = net->sctp.max_burst;

	sp->sctp_hmac_alg = net->sctp.sctp_hmac_alg;

	 
	sp->initmsg.sinit_num_ostreams   = sctp_max_outstreams;
	sp->initmsg.sinit_max_instreams  = sctp_max_instreams;
	sp->initmsg.sinit_max_attempts   = net->sctp.max_retrans_init;
	sp->initmsg.sinit_max_init_timeo = net->sctp.rto_max;

	 
	sp->rtoinfo.srto_initial = net->sctp.rto_initial;
	sp->rtoinfo.srto_max     = net->sctp.rto_max;
	sp->rtoinfo.srto_min     = net->sctp.rto_min;

	 
	sp->assocparams.sasoc_asocmaxrxt = net->sctp.max_retrans_association;
	sp->assocparams.sasoc_number_peer_destinations = 0;
	sp->assocparams.sasoc_peer_rwnd = 0;
	sp->assocparams.sasoc_local_rwnd = 0;
	sp->assocparams.sasoc_cookie_life = net->sctp.valid_cookie_life;

	 
	sp->subscribe = 0;

	 
	sp->hbinterval  = net->sctp.hb_interval;
	sp->udp_port    = htons(net->sctp.udp_port);
	sp->encap_port  = htons(net->sctp.encap_port);
	sp->pathmaxrxt  = net->sctp.max_retrans_path;
	sp->pf_retrans  = net->sctp.pf_retrans;
	sp->ps_retrans  = net->sctp.ps_retrans;
	sp->pf_expose   = net->sctp.pf_expose;
	sp->pathmtu     = 0;  
	sp->sackdelay   = net->sctp.sack_timeout;
	sp->sackfreq	= 2;
	sp->param_flags = SPP_HB_ENABLE |
			  SPP_PMTUD_ENABLE |
			  SPP_SACKDELAY_ENABLE;
	sp->default_ss = SCTP_SS_DEFAULT;

	 
	sp->disable_fragments = 0;

	 
	sp->nodelay           = 0;

	sp->recvrcvinfo = 0;
	sp->recvnxtinfo = 0;

	 
	sp->v4mapped          = 1;

	 
	sp->autoclose         = 0;

	 
	sp->user_frag         = 0;

	sp->adaptation_ind = 0;

	sp->pf = sctp_get_pf_specific(sk->sk_family);

	 
	atomic_set(&sp->pd_mode, 0);
	skb_queue_head_init(&sp->pd_lobby);
	sp->frag_interleave = 0;
	sp->probe_interval = net->sctp.probe_interval;

	 
	sp->ep = sctp_endpoint_new(sk, GFP_KERNEL);
	if (!sp->ep)
		return -ENOMEM;

	sp->hmac = NULL;

	sk->sk_destruct = sctp_destruct_sock;

	SCTP_DBG_OBJCNT_INC(sock);

	sk_sockets_allocated_inc(sk);
	sock_prot_inuse_add(net, sk->sk_prot, 1);

	return 0;
}

 
static void sctp_destroy_sock(struct sock *sk)
{
	struct sctp_sock *sp;

	pr_debug("%s: sk:%p\n", __func__, sk);

	 
	sp = sctp_sk(sk);
	 
	if (sp->ep == NULL)
		return;

	if (sp->do_auto_asconf) {
		sp->do_auto_asconf = 0;
		list_del(&sp->auto_asconf_list);
	}
	sctp_endpoint_free(sp->ep);
	sk_sockets_allocated_dec(sk);
	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, -1);
}

 
static void sctp_destruct_common(struct sock *sk)
{
	struct sctp_sock *sp = sctp_sk(sk);

	 
	crypto_free_shash(sp->hmac);
}

static void sctp_destruct_sock(struct sock *sk)
{
	sctp_destruct_common(sk);
	inet_sock_destruct(sk);
}

 
static void sctp_shutdown(struct sock *sk, int how)
{
	struct net *net = sock_net(sk);
	struct sctp_endpoint *ep;

	if (!sctp_style(sk, TCP))
		return;

	ep = sctp_sk(sk)->ep;
	if (how & SEND_SHUTDOWN && !list_empty(&ep->asocs)) {
		struct sctp_association *asoc;

		inet_sk_set_state(sk, SCTP_SS_CLOSING);
		asoc = list_entry(ep->asocs.next,
				  struct sctp_association, asocs);
		sctp_primitive_SHUTDOWN(net, asoc, NULL);
	}
}

int sctp_get_sctp_info(struct sock *sk, struct sctp_association *asoc,
		       struct sctp_info *info)
{
	struct sctp_transport *prim;
	struct list_head *pos;
	int mask;

	memset(info, 0, sizeof(*info));
	if (!asoc) {
		struct sctp_sock *sp = sctp_sk(sk);

		info->sctpi_s_autoclose = sp->autoclose;
		info->sctpi_s_adaptation_ind = sp->adaptation_ind;
		info->sctpi_s_pd_point = sp->pd_point;
		info->sctpi_s_nodelay = sp->nodelay;
		info->sctpi_s_disable_fragments = sp->disable_fragments;
		info->sctpi_s_v4mapped = sp->v4mapped;
		info->sctpi_s_frag_interleave = sp->frag_interleave;
		info->sctpi_s_type = sp->type;

		return 0;
	}

	info->sctpi_tag = asoc->c.my_vtag;
	info->sctpi_state = asoc->state;
	info->sctpi_rwnd = asoc->a_rwnd;
	info->sctpi_unackdata = asoc->unack_data;
	info->sctpi_penddata = sctp_tsnmap_pending(&asoc->peer.tsn_map);
	info->sctpi_instrms = asoc->stream.incnt;
	info->sctpi_outstrms = asoc->stream.outcnt;
	list_for_each(pos, &asoc->base.inqueue.in_chunk_list)
		info->sctpi_inqueue++;
	list_for_each(pos, &asoc->outqueue.out_chunk_list)
		info->sctpi_outqueue++;
	info->sctpi_overall_error = asoc->overall_error_count;
	info->sctpi_max_burst = asoc->max_burst;
	info->sctpi_maxseg = asoc->frag_point;
	info->sctpi_peer_rwnd = asoc->peer.rwnd;
	info->sctpi_peer_tag = asoc->c.peer_vtag;

	mask = asoc->peer.intl_capable << 1;
	mask = (mask | asoc->peer.ecn_capable) << 1;
	mask = (mask | asoc->peer.ipv4_address) << 1;
	mask = (mask | asoc->peer.ipv6_address) << 1;
	mask = (mask | asoc->peer.reconf_capable) << 1;
	mask = (mask | asoc->peer.asconf_capable) << 1;
	mask = (mask | asoc->peer.prsctp_capable) << 1;
	mask = (mask | asoc->peer.auth_capable);
	info->sctpi_peer_capable = mask;
	mask = asoc->peer.sack_needed << 1;
	mask = (mask | asoc->peer.sack_generation) << 1;
	mask = (mask | asoc->peer.zero_window_announced);
	info->sctpi_peer_sack = mask;

	info->sctpi_isacks = asoc->stats.isacks;
	info->sctpi_osacks = asoc->stats.osacks;
	info->sctpi_opackets = asoc->stats.opackets;
	info->sctpi_ipackets = asoc->stats.ipackets;
	info->sctpi_rtxchunks = asoc->stats.rtxchunks;
	info->sctpi_outofseqtsns = asoc->stats.outofseqtsns;
	info->sctpi_idupchunks = asoc->stats.idupchunks;
	info->sctpi_gapcnt = asoc->stats.gapcnt;
	info->sctpi_ouodchunks = asoc->stats.ouodchunks;
	info->sctpi_iuodchunks = asoc->stats.iuodchunks;
	info->sctpi_oodchunks = asoc->stats.oodchunks;
	info->sctpi_iodchunks = asoc->stats.iodchunks;
	info->sctpi_octrlchunks = asoc->stats.octrlchunks;
	info->sctpi_ictrlchunks = asoc->stats.ictrlchunks;

	prim = asoc->peer.primary_path;
	memcpy(&info->sctpi_p_address, &prim->ipaddr, sizeof(prim->ipaddr));
	info->sctpi_p_state = prim->state;
	info->sctpi_p_cwnd = prim->cwnd;
	info->sctpi_p_srtt = prim->srtt;
	info->sctpi_p_rto = jiffies_to_msecs(prim->rto);
	info->sctpi_p_hbinterval = prim->hbinterval;
	info->sctpi_p_pathmaxrxt = prim->pathmaxrxt;
	info->sctpi_p_sackdelay = jiffies_to_msecs(prim->sackdelay);
	info->sctpi_p_ssthresh = prim->ssthresh;
	info->sctpi_p_partial_bytes_acked = prim->partial_bytes_acked;
	info->sctpi_p_flight_size = prim->flight_size;
	info->sctpi_p_error = prim->error_count;

	return 0;
}
EXPORT_SYMBOL_GPL(sctp_get_sctp_info);

 
void sctp_transport_walk_start(struct rhashtable_iter *iter) __acquires(RCU)
{
	rhltable_walk_enter(&sctp_transport_hashtable, iter);

	rhashtable_walk_start(iter);
}

void sctp_transport_walk_stop(struct rhashtable_iter *iter) __releases(RCU)
{
	rhashtable_walk_stop(iter);
	rhashtable_walk_exit(iter);
}

struct sctp_transport *sctp_transport_get_next(struct net *net,
					       struct rhashtable_iter *iter)
{
	struct sctp_transport *t;

	t = rhashtable_walk_next(iter);
	for (; t; t = rhashtable_walk_next(iter)) {
		if (IS_ERR(t)) {
			if (PTR_ERR(t) == -EAGAIN)
				continue;
			break;
		}

		if (!sctp_transport_hold(t))
			continue;

		if (net_eq(t->asoc->base.net, net) &&
		    t->asoc->peer.primary_path == t)
			break;

		sctp_transport_put(t);
	}

	return t;
}

struct sctp_transport *sctp_transport_get_idx(struct net *net,
					      struct rhashtable_iter *iter,
					      int pos)
{
	struct sctp_transport *t;

	if (!pos)
		return SEQ_START_TOKEN;

	while ((t = sctp_transport_get_next(net, iter)) && !IS_ERR(t)) {
		if (!--pos)
			break;
		sctp_transport_put(t);
	}

	return t;
}

int sctp_for_each_endpoint(int (*cb)(struct sctp_endpoint *, void *),
			   void *p) {
	int err = 0;
	int hash = 0;
	struct sctp_endpoint *ep;
	struct sctp_hashbucket *head;

	for (head = sctp_ep_hashtable; hash < sctp_ep_hashsize;
	     hash++, head++) {
		read_lock_bh(&head->lock);
		sctp_for_each_hentry(ep, &head->chain) {
			err = cb(ep, p);
			if (err)
				break;
		}
		read_unlock_bh(&head->lock);
	}

	return err;
}
EXPORT_SYMBOL_GPL(sctp_for_each_endpoint);

int sctp_transport_lookup_process(sctp_callback_t cb, struct net *net,
				  const union sctp_addr *laddr,
				  const union sctp_addr *paddr, void *p, int dif)
{
	struct sctp_transport *transport;
	struct sctp_endpoint *ep;
	int err = -ENOENT;

	rcu_read_lock();
	transport = sctp_addrs_lookup_transport(net, laddr, paddr, dif, dif);
	if (!transport) {
		rcu_read_unlock();
		return err;
	}
	ep = transport->asoc->ep;
	if (!sctp_endpoint_hold(ep)) {  
		sctp_transport_put(transport);
		rcu_read_unlock();
		return err;
	}
	rcu_read_unlock();

	err = cb(ep, transport, p);
	sctp_endpoint_put(ep);
	sctp_transport_put(transport);
	return err;
}
EXPORT_SYMBOL_GPL(sctp_transport_lookup_process);

int sctp_transport_traverse_process(sctp_callback_t cb, sctp_callback_t cb_done,
				    struct net *net, int *pos, void *p)
{
	struct rhashtable_iter hti;
	struct sctp_transport *tsp;
	struct sctp_endpoint *ep;
	int ret;

again:
	ret = 0;
	sctp_transport_walk_start(&hti);

	tsp = sctp_transport_get_idx(net, &hti, *pos + 1);
	for (; !IS_ERR_OR_NULL(tsp); tsp = sctp_transport_get_next(net, &hti)) {
		ep = tsp->asoc->ep;
		if (sctp_endpoint_hold(ep)) {  
			ret = cb(ep, tsp, p);
			if (ret)
				break;
			sctp_endpoint_put(ep);
		}
		(*pos)++;
		sctp_transport_put(tsp);
	}
	sctp_transport_walk_stop(&hti);

	if (ret) {
		if (cb_done && !cb_done(ep, tsp, p)) {
			(*pos)++;
			sctp_endpoint_put(ep);
			sctp_transport_put(tsp);
			goto again;
		}
		sctp_endpoint_put(ep);
		sctp_transport_put(tsp);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sctp_transport_traverse_process);

 
static int sctp_getsockopt_sctp_status(struct sock *sk, int len,
				       char __user *optval,
				       int __user *optlen)
{
	struct sctp_status status;
	struct sctp_association *asoc = NULL;
	struct sctp_transport *transport;
	sctp_assoc_t associd;
	int retval = 0;

	if (len < sizeof(status)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(status);
	if (copy_from_user(&status, optval, len)) {
		retval = -EFAULT;
		goto out;
	}

	associd = status.sstat_assoc_id;
	asoc = sctp_id2assoc(sk, associd);
	if (!asoc) {
		retval = -EINVAL;
		goto out;
	}

	transport = asoc->peer.primary_path;

	status.sstat_assoc_id = sctp_assoc2id(asoc);
	status.sstat_state = sctp_assoc_to_state(asoc);
	status.sstat_rwnd =  asoc->peer.rwnd;
	status.sstat_unackdata = asoc->unack_data;

	status.sstat_penddata = sctp_tsnmap_pending(&asoc->peer.tsn_map);
	status.sstat_instrms = asoc->stream.incnt;
	status.sstat_outstrms = asoc->stream.outcnt;
	status.sstat_fragmentation_point = asoc->frag_point;
	status.sstat_primary.spinfo_assoc_id = sctp_assoc2id(transport->asoc);
	memcpy(&status.sstat_primary.spinfo_address, &transport->ipaddr,
			transport->af_specific->sockaddr_len);
	 
	sctp_get_pf_specific(sk->sk_family)->addr_to_user(sctp_sk(sk),
		(union sctp_addr *)&status.sstat_primary.spinfo_address);
	status.sstat_primary.spinfo_state = transport->state;
	status.sstat_primary.spinfo_cwnd = transport->cwnd;
	status.sstat_primary.spinfo_srtt = transport->srtt;
	status.sstat_primary.spinfo_rto = jiffies_to_msecs(transport->rto);
	status.sstat_primary.spinfo_mtu = transport->pathmtu;

	if (status.sstat_primary.spinfo_state == SCTP_UNKNOWN)
		status.sstat_primary.spinfo_state = SCTP_ACTIVE;

	if (put_user(len, optlen)) {
		retval = -EFAULT;
		goto out;
	}

	pr_debug("%s: len:%d, state:%d, rwnd:%d, assoc_id:%d\n",
		 __func__, len, status.sstat_state, status.sstat_rwnd,
		 status.sstat_assoc_id);

	if (copy_to_user(optval, &status, len)) {
		retval = -EFAULT;
		goto out;
	}

out:
	return retval;
}


 
static int sctp_getsockopt_peer_addr_info(struct sock *sk, int len,
					  char __user *optval,
					  int __user *optlen)
{
	struct sctp_paddrinfo pinfo;
	struct sctp_transport *transport;
	int retval = 0;

	if (len < sizeof(pinfo)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(pinfo);
	if (copy_from_user(&pinfo, optval, len)) {
		retval = -EFAULT;
		goto out;
	}

	transport = sctp_addr_id2transport(sk, &pinfo.spinfo_address,
					   pinfo.spinfo_assoc_id);
	if (!transport) {
		retval = -EINVAL;
		goto out;
	}

	if (transport->state == SCTP_PF &&
	    transport->asoc->pf_expose == SCTP_PF_EXPOSE_DISABLE) {
		retval = -EACCES;
		goto out;
	}

	pinfo.spinfo_assoc_id = sctp_assoc2id(transport->asoc);
	pinfo.spinfo_state = transport->state;
	pinfo.spinfo_cwnd = transport->cwnd;
	pinfo.spinfo_srtt = transport->srtt;
	pinfo.spinfo_rto = jiffies_to_msecs(transport->rto);
	pinfo.spinfo_mtu = transport->pathmtu;

	if (pinfo.spinfo_state == SCTP_UNKNOWN)
		pinfo.spinfo_state = SCTP_ACTIVE;

	if (put_user(len, optlen)) {
		retval = -EFAULT;
		goto out;
	}

	if (copy_to_user(optval, &pinfo, len)) {
		retval = -EFAULT;
		goto out;
	}

out:
	return retval;
}

 
static int sctp_getsockopt_disable_fragments(struct sock *sk, int len,
					char __user *optval, int __user *optlen)
{
	int val;

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);
	val = (sctp_sk(sk)->disable_fragments == 1);
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;
	return 0;
}

 
static int sctp_getsockopt_events(struct sock *sk, int len, char __user *optval,
				  int __user *optlen)
{
	struct sctp_event_subscribe subscribe;
	__u8 *sn_type = (__u8 *)&subscribe;
	int i;

	if (len == 0)
		return -EINVAL;
	if (len > sizeof(struct sctp_event_subscribe))
		len = sizeof(struct sctp_event_subscribe);
	if (put_user(len, optlen))
		return -EFAULT;

	for (i = 0; i < len; i++)
		sn_type[i] = sctp_ulpevent_type_enabled(sctp_sk(sk)->subscribe,
							SCTP_SN_TYPE_BASE + i);

	if (copy_to_user(optval, &subscribe, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_autoclose(struct sock *sk, int len, char __user *optval, int __user *optlen)
{
	 
	if (sctp_style(sk, TCP))
		return -EOPNOTSUPP;
	if (len < sizeof(int))
		return -EINVAL;
	len = sizeof(int);
	if (put_user(len, optlen))
		return -EFAULT;
	if (put_user(sctp_sk(sk)->autoclose, (int __user *)optval))
		return -EFAULT;
	return 0;
}

 
int sctp_do_peeloff(struct sock *sk, sctp_assoc_t id, struct socket **sockp)
{
	struct sctp_association *asoc = sctp_id2assoc(sk, id);
	struct sctp_sock *sp = sctp_sk(sk);
	struct socket *sock;
	int err = 0;

	 
	if (!net_eq(current->nsproxy->net_ns, sock_net(sk)))
		return -EINVAL;

	if (!asoc)
		return -EINVAL;

	 
	if (!sctp_style(sk, UDP))
		return -EINVAL;

	 
	err = sock_create(sk->sk_family, SOCK_SEQPACKET, IPPROTO_SCTP, &sock);
	if (err < 0)
		return err;

	sctp_copy_sock(sock->sk, sk, asoc);

	 
	sp->pf->to_sk_daddr(&asoc->peer.primary_addr, sock->sk);
	sp->pf->copy_ip_options(sk, sock->sk);

	 
	err = sctp_sock_migrate(sk, sock->sk, asoc,
				SCTP_SOCKET_UDP_HIGH_BANDWIDTH);
	if (err) {
		sock_release(sock);
		sock = NULL;
	}

	*sockp = sock;

	return err;
}
EXPORT_SYMBOL(sctp_do_peeloff);

static int sctp_getsockopt_peeloff_common(struct sock *sk, sctp_peeloff_arg_t *peeloff,
					  struct file **newfile, unsigned flags)
{
	struct socket *newsock;
	int retval;

	retval = sctp_do_peeloff(sk, peeloff->associd, &newsock);
	if (retval < 0)
		goto out;

	 
	retval = get_unused_fd_flags(flags & SOCK_CLOEXEC);
	if (retval < 0) {
		sock_release(newsock);
		goto out;
	}

	*newfile = sock_alloc_file(newsock, 0, NULL);
	if (IS_ERR(*newfile)) {
		put_unused_fd(retval);
		retval = PTR_ERR(*newfile);
		*newfile = NULL;
		return retval;
	}

	pr_debug("%s: sk:%p, newsk:%p, sd:%d\n", __func__, sk, newsock->sk,
		 retval);

	peeloff->sd = retval;

	if (flags & SOCK_NONBLOCK)
		(*newfile)->f_flags |= O_NONBLOCK;
out:
	return retval;
}

static int sctp_getsockopt_peeloff(struct sock *sk, int len, char __user *optval, int __user *optlen)
{
	sctp_peeloff_arg_t peeloff;
	struct file *newfile = NULL;
	int retval = 0;

	if (len < sizeof(sctp_peeloff_arg_t))
		return -EINVAL;
	len = sizeof(sctp_peeloff_arg_t);
	if (copy_from_user(&peeloff, optval, len))
		return -EFAULT;

	retval = sctp_getsockopt_peeloff_common(sk, &peeloff, &newfile, 0);
	if (retval < 0)
		goto out;

	 
	if (put_user(len, optlen)) {
		fput(newfile);
		put_unused_fd(retval);
		return -EFAULT;
	}

	if (copy_to_user(optval, &peeloff, len)) {
		fput(newfile);
		put_unused_fd(retval);
		return -EFAULT;
	}
	fd_install(retval, newfile);
out:
	return retval;
}

static int sctp_getsockopt_peeloff_flags(struct sock *sk, int len,
					 char __user *optval, int __user *optlen)
{
	sctp_peeloff_flags_arg_t peeloff;
	struct file *newfile = NULL;
	int retval = 0;

	if (len < sizeof(sctp_peeloff_flags_arg_t))
		return -EINVAL;
	len = sizeof(sctp_peeloff_flags_arg_t);
	if (copy_from_user(&peeloff, optval, len))
		return -EFAULT;

	retval = sctp_getsockopt_peeloff_common(sk, &peeloff.p_arg,
						&newfile, peeloff.flags);
	if (retval < 0)
		goto out;

	 
	if (put_user(len, optlen)) {
		fput(newfile);
		put_unused_fd(retval);
		return -EFAULT;
	}

	if (copy_to_user(optval, &peeloff, len)) {
		fput(newfile);
		put_unused_fd(retval);
		return -EFAULT;
	}
	fd_install(retval, newfile);
out:
	return retval;
}

 
static int sctp_getsockopt_peer_addr_params(struct sock *sk, int len,
					    char __user *optval, int __user *optlen)
{
	struct sctp_paddrparams  params;
	struct sctp_transport   *trans = NULL;
	struct sctp_association *asoc = NULL;
	struct sctp_sock        *sp = sctp_sk(sk);

	if (len >= sizeof(params))
		len = sizeof(params);
	else if (len >= ALIGN(offsetof(struct sctp_paddrparams,
				       spp_ipv6_flowlabel), 4))
		len = ALIGN(offsetof(struct sctp_paddrparams,
				     spp_ipv6_flowlabel), 4);
	else
		return -EINVAL;

	if (copy_from_user(&params, optval, len))
		return -EFAULT;

	 
	if (!sctp_is_any(sk, (union sctp_addr *)&params.spp_address)) {
		trans = sctp_addr_id2transport(sk, &params.spp_address,
					       params.spp_assoc_id);
		if (!trans) {
			pr_debug("%s: failed no transport\n", __func__);
			return -EINVAL;
		}
	}

	 
	asoc = sctp_id2assoc(sk, params.spp_assoc_id);
	if (!asoc && params.spp_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		pr_debug("%s: failed no association\n", __func__);
		return -EINVAL;
	}

	if (trans) {
		 
		params.spp_hbinterval = jiffies_to_msecs(trans->hbinterval);
		params.spp_pathmtu    = trans->pathmtu;
		params.spp_pathmaxrxt = trans->pathmaxrxt;
		params.spp_sackdelay  = jiffies_to_msecs(trans->sackdelay);

		 
		params.spp_flags      = trans->param_flags;
		if (trans->flowlabel & SCTP_FLOWLABEL_SET_MASK) {
			params.spp_ipv6_flowlabel = trans->flowlabel &
						    SCTP_FLOWLABEL_VAL_MASK;
			params.spp_flags |= SPP_IPV6_FLOWLABEL;
		}
		if (trans->dscp & SCTP_DSCP_SET_MASK) {
			params.spp_dscp	= trans->dscp & SCTP_DSCP_VAL_MASK;
			params.spp_flags |= SPP_DSCP;
		}
	} else if (asoc) {
		 
		params.spp_hbinterval = jiffies_to_msecs(asoc->hbinterval);
		params.spp_pathmtu    = asoc->pathmtu;
		params.spp_pathmaxrxt = asoc->pathmaxrxt;
		params.spp_sackdelay  = jiffies_to_msecs(asoc->sackdelay);

		 
		params.spp_flags      = asoc->param_flags;
		if (asoc->flowlabel & SCTP_FLOWLABEL_SET_MASK) {
			params.spp_ipv6_flowlabel = asoc->flowlabel &
						    SCTP_FLOWLABEL_VAL_MASK;
			params.spp_flags |= SPP_IPV6_FLOWLABEL;
		}
		if (asoc->dscp & SCTP_DSCP_SET_MASK) {
			params.spp_dscp	= asoc->dscp & SCTP_DSCP_VAL_MASK;
			params.spp_flags |= SPP_DSCP;
		}
	} else {
		 
		params.spp_hbinterval = sp->hbinterval;
		params.spp_pathmtu    = sp->pathmtu;
		params.spp_sackdelay  = sp->sackdelay;
		params.spp_pathmaxrxt = sp->pathmaxrxt;

		 
		params.spp_flags      = sp->param_flags;
		if (sp->flowlabel & SCTP_FLOWLABEL_SET_MASK) {
			params.spp_ipv6_flowlabel = sp->flowlabel &
						    SCTP_FLOWLABEL_VAL_MASK;
			params.spp_flags |= SPP_IPV6_FLOWLABEL;
		}
		if (sp->dscp & SCTP_DSCP_SET_MASK) {
			params.spp_dscp	= sp->dscp & SCTP_DSCP_VAL_MASK;
			params.spp_flags |= SPP_DSCP;
		}
	}

	if (copy_to_user(optval, &params, len))
		return -EFAULT;

	if (put_user(len, optlen))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_delayed_ack(struct sock *sk, int len,
					    char __user *optval,
					    int __user *optlen)
{
	struct sctp_sack_info    params;
	struct sctp_association *asoc = NULL;
	struct sctp_sock        *sp = sctp_sk(sk);

	if (len >= sizeof(struct sctp_sack_info)) {
		len = sizeof(struct sctp_sack_info);

		if (copy_from_user(&params, optval, len))
			return -EFAULT;
	} else if (len == sizeof(struct sctp_assoc_value)) {
		pr_warn_ratelimited(DEPRECATED
				    "%s (pid %d) "
				    "Use of struct sctp_assoc_value in delayed_ack socket option.\n"
				    "Use struct sctp_sack_info instead\n",
				    current->comm, task_pid_nr(current));
		if (copy_from_user(&params, optval, len))
			return -EFAULT;
	} else
		return -EINVAL;

	 
	asoc = sctp_id2assoc(sk, params.sack_assoc_id);
	if (!asoc && params.sack_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		 
		if (asoc->param_flags & SPP_SACKDELAY_ENABLE) {
			params.sack_delay = jiffies_to_msecs(asoc->sackdelay);
			params.sack_freq = asoc->sackfreq;

		} else {
			params.sack_delay = 0;
			params.sack_freq = 1;
		}
	} else {
		 
		if (sp->param_flags & SPP_SACKDELAY_ENABLE) {
			params.sack_delay  = sp->sackdelay;
			params.sack_freq = sp->sackfreq;
		} else {
			params.sack_delay  = 0;
			params.sack_freq = 1;
		}
	}

	if (copy_to_user(optval, &params, len))
		return -EFAULT;

	if (put_user(len, optlen))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_initmsg(struct sock *sk, int len, char __user *optval, int __user *optlen)
{
	if (len < sizeof(struct sctp_initmsg))
		return -EINVAL;
	len = sizeof(struct sctp_initmsg);
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &sctp_sk(sk)->initmsg, len))
		return -EFAULT;
	return 0;
}


static int sctp_getsockopt_peer_addrs(struct sock *sk, int len,
				      char __user *optval, int __user *optlen)
{
	struct sctp_association *asoc;
	int cnt = 0;
	struct sctp_getaddrs getaddrs;
	struct sctp_transport *from;
	void __user *to;
	union sctp_addr temp;
	struct sctp_sock *sp = sctp_sk(sk);
	int addrlen;
	size_t space_left;
	int bytes_copied;

	if (len < sizeof(struct sctp_getaddrs))
		return -EINVAL;

	if (copy_from_user(&getaddrs, optval, sizeof(struct sctp_getaddrs)))
		return -EFAULT;

	 
	asoc = sctp_id2assoc(sk, getaddrs.assoc_id);
	if (!asoc)
		return -EINVAL;

	to = optval + offsetof(struct sctp_getaddrs, addrs);
	space_left = len - offsetof(struct sctp_getaddrs, addrs);

	list_for_each_entry(from, &asoc->peer.transport_addr_list,
				transports) {
		memcpy(&temp, &from->ipaddr, sizeof(temp));
		addrlen = sctp_get_pf_specific(sk->sk_family)
			      ->addr_to_user(sp, &temp);
		if (space_left < addrlen)
			return -ENOMEM;
		if (copy_to_user(to, &temp, addrlen))
			return -EFAULT;
		to += addrlen;
		cnt++;
		space_left -= addrlen;
	}

	if (put_user(cnt, &((struct sctp_getaddrs __user *)optval)->addr_num))
		return -EFAULT;
	bytes_copied = ((char __user *)to) - optval;
	if (put_user(bytes_copied, optlen))
		return -EFAULT;

	return 0;
}

static int sctp_copy_laddrs(struct sock *sk, __u16 port, void *to,
			    size_t space_left, int *bytes_copied)
{
	struct sctp_sockaddr_entry *addr;
	union sctp_addr temp;
	int cnt = 0;
	int addrlen;
	struct net *net = sock_net(sk);

	rcu_read_lock();
	list_for_each_entry_rcu(addr, &net->sctp.local_addr_list, list) {
		if (!addr->valid)
			continue;

		if ((PF_INET == sk->sk_family) &&
		    (AF_INET6 == addr->a.sa.sa_family))
			continue;
		if ((PF_INET6 == sk->sk_family) &&
		    inet_v6_ipv6only(sk) &&
		    (AF_INET == addr->a.sa.sa_family))
			continue;
		memcpy(&temp, &addr->a, sizeof(temp));
		if (!temp.v4.sin_port)
			temp.v4.sin_port = htons(port);

		addrlen = sctp_get_pf_specific(sk->sk_family)
			      ->addr_to_user(sctp_sk(sk), &temp);

		if (space_left < addrlen) {
			cnt =  -ENOMEM;
			break;
		}
		memcpy(to, &temp, addrlen);

		to += addrlen;
		cnt++;
		space_left -= addrlen;
		*bytes_copied += addrlen;
	}
	rcu_read_unlock();

	return cnt;
}


static int sctp_getsockopt_local_addrs(struct sock *sk, int len,
				       char __user *optval, int __user *optlen)
{
	struct sctp_bind_addr *bp;
	struct sctp_association *asoc;
	int cnt = 0;
	struct sctp_getaddrs getaddrs;
	struct sctp_sockaddr_entry *addr;
	void __user *to;
	union sctp_addr temp;
	struct sctp_sock *sp = sctp_sk(sk);
	int addrlen;
	int err = 0;
	size_t space_left;
	int bytes_copied = 0;
	void *addrs;
	void *buf;

	if (len < sizeof(struct sctp_getaddrs))
		return -EINVAL;

	if (copy_from_user(&getaddrs, optval, sizeof(struct sctp_getaddrs)))
		return -EFAULT;

	 
	if (0 == getaddrs.assoc_id) {
		bp = &sctp_sk(sk)->ep->base.bind_addr;
	} else {
		asoc = sctp_id2assoc(sk, getaddrs.assoc_id);
		if (!asoc)
			return -EINVAL;
		bp = &asoc->base.bind_addr;
	}

	to = optval + offsetof(struct sctp_getaddrs, addrs);
	space_left = len - offsetof(struct sctp_getaddrs, addrs);

	addrs = kmalloc(space_left, GFP_USER | __GFP_NOWARN);
	if (!addrs)
		return -ENOMEM;

	 
	if (sctp_list_single_entry(&bp->address_list)) {
		addr = list_entry(bp->address_list.next,
				  struct sctp_sockaddr_entry, list);
		if (sctp_is_any(sk, &addr->a)) {
			cnt = sctp_copy_laddrs(sk, bp->port, addrs,
						space_left, &bytes_copied);
			if (cnt < 0) {
				err = cnt;
				goto out;
			}
			goto copy_getaddrs;
		}
	}

	buf = addrs;
	 
	list_for_each_entry(addr, &bp->address_list, list) {
		memcpy(&temp, &addr->a, sizeof(temp));
		addrlen = sctp_get_pf_specific(sk->sk_family)
			      ->addr_to_user(sp, &temp);
		if (space_left < addrlen) {
			err =  -ENOMEM;  
			goto out;
		}
		memcpy(buf, &temp, addrlen);
		buf += addrlen;
		bytes_copied += addrlen;
		cnt++;
		space_left -= addrlen;
	}

copy_getaddrs:
	if (copy_to_user(to, addrs, bytes_copied)) {
		err = -EFAULT;
		goto out;
	}
	if (put_user(cnt, &((struct sctp_getaddrs __user *)optval)->addr_num)) {
		err = -EFAULT;
		goto out;
	}
	 
	if (put_user(bytes_copied, optlen))
		err = -EFAULT;
out:
	kfree(addrs);
	return err;
}

 
static int sctp_getsockopt_primary_addr(struct sock *sk, int len,
					char __user *optval, int __user *optlen)
{
	struct sctp_prim prim;
	struct sctp_association *asoc;
	struct sctp_sock *sp = sctp_sk(sk);

	if (len < sizeof(struct sctp_prim))
		return -EINVAL;

	len = sizeof(struct sctp_prim);

	if (copy_from_user(&prim, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, prim.ssp_assoc_id);
	if (!asoc)
		return -EINVAL;

	if (!asoc->peer.primary_path)
		return -ENOTCONN;

	memcpy(&prim.ssp_addr, &asoc->peer.primary_path->ipaddr,
		asoc->peer.primary_path->af_specific->sockaddr_len);

	sctp_get_pf_specific(sk->sk_family)->addr_to_user(sp,
			(union sctp_addr *)&prim.ssp_addr);

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &prim, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_adaptation_layer(struct sock *sk, int len,
				  char __user *optval, int __user *optlen)
{
	struct sctp_setadaptation adaptation;

	if (len < sizeof(struct sctp_setadaptation))
		return -EINVAL;

	len = sizeof(struct sctp_setadaptation);

	adaptation.ssb_adaptation_ind = sctp_sk(sk)->adaptation_ind;

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &adaptation, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_default_send_param(struct sock *sk,
					int len, char __user *optval,
					int __user *optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	struct sctp_sndrcvinfo info;

	if (len < sizeof(info))
		return -EINVAL;

	len = sizeof(info);

	if (copy_from_user(&info, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, info.sinfo_assoc_id);
	if (!asoc && info.sinfo_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		info.sinfo_stream = asoc->default_stream;
		info.sinfo_flags = asoc->default_flags;
		info.sinfo_ppid = asoc->default_ppid;
		info.sinfo_context = asoc->default_context;
		info.sinfo_timetolive = asoc->default_timetolive;
	} else {
		info.sinfo_stream = sp->default_stream;
		info.sinfo_flags = sp->default_flags;
		info.sinfo_ppid = sp->default_ppid;
		info.sinfo_context = sp->default_context;
		info.sinfo_timetolive = sp->default_timetolive;
	}

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &info, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_default_sndinfo(struct sock *sk, int len,
					   char __user *optval,
					   int __user *optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	struct sctp_sndinfo info;

	if (len < sizeof(info))
		return -EINVAL;

	len = sizeof(info);

	if (copy_from_user(&info, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, info.snd_assoc_id);
	if (!asoc && info.snd_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		info.snd_sid = asoc->default_stream;
		info.snd_flags = asoc->default_flags;
		info.snd_ppid = asoc->default_ppid;
		info.snd_context = asoc->default_context;
	} else {
		info.snd_sid = sp->default_stream;
		info.snd_flags = sp->default_flags;
		info.snd_ppid = sp->default_ppid;
		info.snd_context = sp->default_context;
	}

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &info, len))
		return -EFAULT;

	return 0;
}

 

static int sctp_getsockopt_nodelay(struct sock *sk, int len,
				   char __user *optval, int __user *optlen)
{
	int val;

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);
	val = (sctp_sk(sk)->nodelay == 1);
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;
	return 0;
}

 
static int sctp_getsockopt_rtoinfo(struct sock *sk, int len,
				char __user *optval,
				int __user *optlen) {
	struct sctp_rtoinfo rtoinfo;
	struct sctp_association *asoc;

	if (len < sizeof (struct sctp_rtoinfo))
		return -EINVAL;

	len = sizeof(struct sctp_rtoinfo);

	if (copy_from_user(&rtoinfo, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, rtoinfo.srto_assoc_id);

	if (!asoc && rtoinfo.srto_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	 
	if (asoc) {
		rtoinfo.srto_initial = jiffies_to_msecs(asoc->rto_initial);
		rtoinfo.srto_max = jiffies_to_msecs(asoc->rto_max);
		rtoinfo.srto_min = jiffies_to_msecs(asoc->rto_min);
	} else {
		 
		struct sctp_sock *sp = sctp_sk(sk);

		rtoinfo.srto_initial = sp->rtoinfo.srto_initial;
		rtoinfo.srto_max = sp->rtoinfo.srto_max;
		rtoinfo.srto_min = sp->rtoinfo.srto_min;
	}

	if (put_user(len, optlen))
		return -EFAULT;

	if (copy_to_user(optval, &rtoinfo, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_associnfo(struct sock *sk, int len,
				     char __user *optval,
				     int __user *optlen)
{

	struct sctp_assocparams assocparams;
	struct sctp_association *asoc;
	struct list_head *pos;
	int cnt = 0;

	if (len < sizeof (struct sctp_assocparams))
		return -EINVAL;

	len = sizeof(struct sctp_assocparams);

	if (copy_from_user(&assocparams, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, assocparams.sasoc_assoc_id);

	if (!asoc && assocparams.sasoc_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	 
	if (asoc) {
		assocparams.sasoc_asocmaxrxt = asoc->max_retrans;
		assocparams.sasoc_peer_rwnd = asoc->peer.rwnd;
		assocparams.sasoc_local_rwnd = asoc->a_rwnd;
		assocparams.sasoc_cookie_life = ktime_to_ms(asoc->cookie_life);

		list_for_each(pos, &asoc->peer.transport_addr_list) {
			cnt++;
		}

		assocparams.sasoc_number_peer_destinations = cnt;
	} else {
		 
		struct sctp_sock *sp = sctp_sk(sk);

		assocparams.sasoc_asocmaxrxt = sp->assocparams.sasoc_asocmaxrxt;
		assocparams.sasoc_peer_rwnd = sp->assocparams.sasoc_peer_rwnd;
		assocparams.sasoc_local_rwnd = sp->assocparams.sasoc_local_rwnd;
		assocparams.sasoc_cookie_life =
					sp->assocparams.sasoc_cookie_life;
		assocparams.sasoc_number_peer_destinations =
					sp->assocparams.
					sasoc_number_peer_destinations;
	}

	if (put_user(len, optlen))
		return -EFAULT;

	if (copy_to_user(optval, &assocparams, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_mappedv4(struct sock *sk, int len,
				    char __user *optval, int __user *optlen)
{
	int val;
	struct sctp_sock *sp = sctp_sk(sk);

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);
	val = sp->v4mapped;
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_context(struct sock *sk, int len,
				   char __user *optval, int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;

	if (len < sizeof(struct sctp_assoc_value))
		return -EINVAL;

	len = sizeof(struct sctp_assoc_value);

	if (copy_from_user(&params, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	params.assoc_value = asoc ? asoc->default_rcv_context
				  : sctp_sk(sk)->default_rcv_context;

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &params, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_maxseg(struct sock *sk, int len,
				  char __user *optval, int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;

	if (len == sizeof(int)) {
		pr_warn_ratelimited(DEPRECATED
				    "%s (pid %d) "
				    "Use of int in maxseg socket option.\n"
				    "Use struct sctp_assoc_value instead\n",
				    current->comm, task_pid_nr(current));
		params.assoc_id = SCTP_FUTURE_ASSOC;
	} else if (len >= sizeof(struct sctp_assoc_value)) {
		len = sizeof(struct sctp_assoc_value);
		if (copy_from_user(&params, optval, len))
			return -EFAULT;
	} else
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc)
		params.assoc_value = asoc->frag_point;
	else
		params.assoc_value = sctp_sk(sk)->user_frag;

	if (put_user(len, optlen))
		return -EFAULT;
	if (len == sizeof(int)) {
		if (copy_to_user(optval, &params.assoc_value, len))
			return -EFAULT;
	} else {
		if (copy_to_user(optval, &params, len))
			return -EFAULT;
	}

	return 0;
}

 
static int sctp_getsockopt_fragment_interleave(struct sock *sk, int len,
					       char __user *optval, int __user *optlen)
{
	int val;

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);

	val = sctp_sk(sk)->frag_interleave;
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_partial_delivery_point(struct sock *sk, int len,
						  char __user *optval,
						  int __user *optlen)
{
	u32 val;

	if (len < sizeof(u32))
		return -EINVAL;

	len = sizeof(u32);

	val = sctp_sk(sk)->pd_point;
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_maxburst(struct sock *sk, int len,
				    char __user *optval,
				    int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;

	if (len == sizeof(int)) {
		pr_warn_ratelimited(DEPRECATED
				    "%s (pid %d) "
				    "Use of int in max_burst socket option.\n"
				    "Use struct sctp_assoc_value instead\n",
				    current->comm, task_pid_nr(current));
		params.assoc_id = SCTP_FUTURE_ASSOC;
	} else if (len >= sizeof(struct sctp_assoc_value)) {
		len = sizeof(struct sctp_assoc_value);
		if (copy_from_user(&params, optval, len))
			return -EFAULT;
	} else
		return -EINVAL;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	params.assoc_value = asoc ? asoc->max_burst : sctp_sk(sk)->max_burst;

	if (len == sizeof(int)) {
		if (copy_to_user(optval, &params.assoc_value, len))
			return -EFAULT;
	} else {
		if (copy_to_user(optval, &params, len))
			return -EFAULT;
	}

	return 0;

}

static int sctp_getsockopt_hmac_ident(struct sock *sk, int len,
				    char __user *optval, int __user *optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_hmacalgo  __user *p = (void __user *)optval;
	struct sctp_hmac_algo_param *hmacs;
	__u16 data_len = 0;
	u32 num_idents;
	int i;

	if (!ep->auth_enable)
		return -EACCES;

	hmacs = ep->auth_hmacs_list;
	data_len = ntohs(hmacs->param_hdr.length) -
		   sizeof(struct sctp_paramhdr);

	if (len < sizeof(struct sctp_hmacalgo) + data_len)
		return -EINVAL;

	len = sizeof(struct sctp_hmacalgo) + data_len;
	num_idents = data_len / sizeof(u16);

	if (put_user(len, optlen))
		return -EFAULT;
	if (put_user(num_idents, &p->shmac_num_idents))
		return -EFAULT;
	for (i = 0; i < num_idents; i++) {
		__u16 hmacid = ntohs(hmacs->hmac_ids[i]);

		if (copy_to_user(&p->shmac_idents[i], &hmacid, sizeof(__u16)))
			return -EFAULT;
	}
	return 0;
}

static int sctp_getsockopt_active_key(struct sock *sk, int len,
				    char __user *optval, int __user *optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_authkeyid val;
	struct sctp_association *asoc;

	if (len < sizeof(struct sctp_authkeyid))
		return -EINVAL;

	len = sizeof(struct sctp_authkeyid);
	if (copy_from_user(&val, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, val.scact_assoc_id);
	if (!asoc && val.scact_assoc_id && sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		if (!asoc->peer.auth_capable)
			return -EACCES;
		val.scact_keynumber = asoc->active_key_id;
	} else {
		if (!ep->auth_enable)
			return -EACCES;
		val.scact_keynumber = ep->active_key_id;
	}

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt_peer_auth_chunks(struct sock *sk, int len,
				    char __user *optval, int __user *optlen)
{
	struct sctp_authchunks __user *p = (void __user *)optval;
	struct sctp_authchunks val;
	struct sctp_association *asoc;
	struct sctp_chunks_param *ch;
	u32    num_chunks = 0;
	char __user *to;

	if (len < sizeof(struct sctp_authchunks))
		return -EINVAL;

	if (copy_from_user(&val, optval, sizeof(val)))
		return -EFAULT;

	to = p->gauth_chunks;
	asoc = sctp_id2assoc(sk, val.gauth_assoc_id);
	if (!asoc)
		return -EINVAL;

	if (!asoc->peer.auth_capable)
		return -EACCES;

	ch = asoc->peer.peer_chunks;
	if (!ch)
		goto num;

	 
	num_chunks = ntohs(ch->param_hdr.length) - sizeof(struct sctp_paramhdr);
	if (len < num_chunks)
		return -EINVAL;

	if (copy_to_user(to, ch->chunks, num_chunks))
		return -EFAULT;
num:
	len = sizeof(struct sctp_authchunks) + num_chunks;
	if (put_user(len, optlen))
		return -EFAULT;
	if (put_user(num_chunks, &p->gauth_number_of_chunks))
		return -EFAULT;
	return 0;
}

static int sctp_getsockopt_local_auth_chunks(struct sock *sk, int len,
				    char __user *optval, int __user *optlen)
{
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	struct sctp_authchunks __user *p = (void __user *)optval;
	struct sctp_authchunks val;
	struct sctp_association *asoc;
	struct sctp_chunks_param *ch;
	u32    num_chunks = 0;
	char __user *to;

	if (len < sizeof(struct sctp_authchunks))
		return -EINVAL;

	if (copy_from_user(&val, optval, sizeof(val)))
		return -EFAULT;

	to = p->gauth_chunks;
	asoc = sctp_id2assoc(sk, val.gauth_assoc_id);
	if (!asoc && val.gauth_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		if (!asoc->peer.auth_capable)
			return -EACCES;
		ch = (struct sctp_chunks_param *)asoc->c.auth_chunks;
	} else {
		if (!ep->auth_enable)
			return -EACCES;
		ch = ep->auth_chunk_list;
	}
	if (!ch)
		goto num;

	num_chunks = ntohs(ch->param_hdr.length) - sizeof(struct sctp_paramhdr);
	if (len < sizeof(struct sctp_authchunks) + num_chunks)
		return -EINVAL;

	if (copy_to_user(to, ch->chunks, num_chunks))
		return -EFAULT;
num:
	len = sizeof(struct sctp_authchunks) + num_chunks;
	if (put_user(len, optlen))
		return -EFAULT;
	if (put_user(num_chunks, &p->gauth_number_of_chunks))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_assoc_number(struct sock *sk, int len,
				    char __user *optval, int __user *optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	u32 val = 0;

	if (sctp_style(sk, TCP))
		return -EOPNOTSUPP;

	if (len < sizeof(u32))
		return -EINVAL;

	len = sizeof(u32);

	list_for_each_entry(asoc, &(sp->ep->asocs), asocs) {
		val++;
	}

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_auto_asconf(struct sock *sk, int len,
				   char __user *optval, int __user *optlen)
{
	int val = 0;

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);
	if (sctp_sk(sk)->do_auto_asconf && sctp_is_ep_boundall(sk))
		val = 1;
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;
	return 0;
}

 
static int sctp_getsockopt_assoc_ids(struct sock *sk, int len,
				    char __user *optval, int __user *optlen)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_association *asoc;
	struct sctp_assoc_ids *ids;
	u32 num = 0;

	if (sctp_style(sk, TCP))
		return -EOPNOTSUPP;

	if (len < sizeof(struct sctp_assoc_ids))
		return -EINVAL;

	list_for_each_entry(asoc, &(sp->ep->asocs), asocs) {
		num++;
	}

	if (len < sizeof(struct sctp_assoc_ids) + sizeof(sctp_assoc_t) * num)
		return -EINVAL;

	len = sizeof(struct sctp_assoc_ids) + sizeof(sctp_assoc_t) * num;

	ids = kmalloc(len, GFP_USER | __GFP_NOWARN);
	if (unlikely(!ids))
		return -ENOMEM;

	ids->gaids_number_of_ids = num;
	num = 0;
	list_for_each_entry(asoc, &(sp->ep->asocs), asocs) {
		ids->gaids_assoc_id[num++] = asoc->assoc_id;
	}

	if (put_user(len, optlen) || copy_to_user(optval, ids, len)) {
		kfree(ids);
		return -EFAULT;
	}

	kfree(ids);
	return 0;
}

 
static int sctp_getsockopt_paddr_thresholds(struct sock *sk,
					    char __user *optval, int len,
					    int __user *optlen, bool v2)
{
	struct sctp_paddrthlds_v2 val;
	struct sctp_transport *trans;
	struct sctp_association *asoc;
	int min;

	min = v2 ? sizeof(val) : sizeof(struct sctp_paddrthlds);
	if (len < min)
		return -EINVAL;
	len = min;
	if (copy_from_user(&val, optval, len))
		return -EFAULT;

	if (!sctp_is_any(sk, (const union sctp_addr *)&val.spt_address)) {
		trans = sctp_addr_id2transport(sk, &val.spt_address,
					       val.spt_assoc_id);
		if (!trans)
			return -ENOENT;

		val.spt_pathmaxrxt = trans->pathmaxrxt;
		val.spt_pathpfthld = trans->pf_retrans;
		val.spt_pathcpthld = trans->ps_retrans;

		goto out;
	}

	asoc = sctp_id2assoc(sk, val.spt_assoc_id);
	if (!asoc && val.spt_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	if (asoc) {
		val.spt_pathpfthld = asoc->pf_retrans;
		val.spt_pathmaxrxt = asoc->pathmaxrxt;
		val.spt_pathcpthld = asoc->ps_retrans;
	} else {
		struct sctp_sock *sp = sctp_sk(sk);

		val.spt_pathpfthld = sp->pf_retrans;
		val.spt_pathmaxrxt = sp->pathmaxrxt;
		val.spt_pathcpthld = sp->ps_retrans;
	}

out:
	if (put_user(len, optlen) || copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

 
static int sctp_getsockopt_assoc_stats(struct sock *sk, int len,
				       char __user *optval,
				       int __user *optlen)
{
	struct sctp_assoc_stats sas;
	struct sctp_association *asoc = NULL;

	 
	if (len < sizeof(sctp_assoc_t))
		return -EINVAL;

	 
	len = min_t(size_t, len, sizeof(sas));

	if (copy_from_user(&sas, optval, len))
		return -EFAULT;

	asoc = sctp_id2assoc(sk, sas.sas_assoc_id);
	if (!asoc)
		return -EINVAL;

	sas.sas_rtxchunks = asoc->stats.rtxchunks;
	sas.sas_gapcnt = asoc->stats.gapcnt;
	sas.sas_outofseqtsns = asoc->stats.outofseqtsns;
	sas.sas_osacks = asoc->stats.osacks;
	sas.sas_isacks = asoc->stats.isacks;
	sas.sas_octrlchunks = asoc->stats.octrlchunks;
	sas.sas_ictrlchunks = asoc->stats.ictrlchunks;
	sas.sas_oodchunks = asoc->stats.oodchunks;
	sas.sas_iodchunks = asoc->stats.iodchunks;
	sas.sas_ouodchunks = asoc->stats.ouodchunks;
	sas.sas_iuodchunks = asoc->stats.iuodchunks;
	sas.sas_idupchunks = asoc->stats.idupchunks;
	sas.sas_opackets = asoc->stats.opackets;
	sas.sas_ipackets = asoc->stats.ipackets;

	 
	sas.sas_maxrto = asoc->stats.max_obs_rto;
	memcpy(&sas.sas_obs_rto_ipaddr, &asoc->stats.obs_rto_ipaddr,
		sizeof(struct sockaddr_storage));

	 
	asoc->stats.max_obs_rto = asoc->rto_min;

	if (put_user(len, optlen))
		return -EFAULT;

	pr_debug("%s: len:%d, assoc_id:%d\n", __func__, len, sas.sas_assoc_id);

	if (copy_to_user(optval, &sas, len))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt_recvrcvinfo(struct sock *sk,	int len,
				       char __user *optval,
				       int __user *optlen)
{
	int val = 0;

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);
	if (sctp_sk(sk)->recvrcvinfo)
		val = 1;
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt_recvnxtinfo(struct sock *sk,	int len,
				       char __user *optval,
				       int __user *optlen)
{
	int val = 0;

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);
	if (sctp_sk(sk)->recvnxtinfo)
		val = 1;
	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt_pr_supported(struct sock *sk, int len,
					char __user *optval,
					int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->peer.prsctp_capable
				  : sctp_sk(sk)->ep->prsctp_enable;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_default_prinfo(struct sock *sk, int len,
					  char __user *optval,
					  int __user *optlen)
{
	struct sctp_default_prinfo info;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(info)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(info);
	if (copy_from_user(&info, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, info.pr_assoc_id);
	if (!asoc && info.pr_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	if (asoc) {
		info.pr_policy = SCTP_PR_POLICY(asoc->default_flags);
		info.pr_value = asoc->default_timetolive;
	} else {
		struct sctp_sock *sp = sctp_sk(sk);

		info.pr_policy = SCTP_PR_POLICY(sp->default_flags);
		info.pr_value = sp->default_timetolive;
	}

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &info, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_pr_assocstatus(struct sock *sk, int len,
					  char __user *optval,
					  int __user *optlen)
{
	struct sctp_prstatus params;
	struct sctp_association *asoc;
	int policy;
	int retval = -EINVAL;

	if (len < sizeof(params))
		goto out;

	len = sizeof(params);
	if (copy_from_user(&params, optval, len)) {
		retval = -EFAULT;
		goto out;
	}

	policy = params.sprstat_policy;
	if (!policy || (policy & ~(SCTP_PR_SCTP_MASK | SCTP_PR_SCTP_ALL)) ||
	    ((policy & SCTP_PR_SCTP_ALL) && (policy & SCTP_PR_SCTP_MASK)))
		goto out;

	asoc = sctp_id2assoc(sk, params.sprstat_assoc_id);
	if (!asoc)
		goto out;

	if (policy == SCTP_PR_SCTP_ALL) {
		params.sprstat_abandoned_unsent = 0;
		params.sprstat_abandoned_sent = 0;
		for (policy = 0; policy <= SCTP_PR_INDEX(MAX); policy++) {
			params.sprstat_abandoned_unsent +=
				asoc->abandoned_unsent[policy];
			params.sprstat_abandoned_sent +=
				asoc->abandoned_sent[policy];
		}
	} else {
		params.sprstat_abandoned_unsent =
			asoc->abandoned_unsent[__SCTP_PR_INDEX(policy)];
		params.sprstat_abandoned_sent =
			asoc->abandoned_sent[__SCTP_PR_INDEX(policy)];
	}

	if (put_user(len, optlen)) {
		retval = -EFAULT;
		goto out;
	}

	if (copy_to_user(optval, &params, len)) {
		retval = -EFAULT;
		goto out;
	}

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_pr_streamstatus(struct sock *sk, int len,
					   char __user *optval,
					   int __user *optlen)
{
	struct sctp_stream_out_ext *streamoute;
	struct sctp_association *asoc;
	struct sctp_prstatus params;
	int retval = -EINVAL;
	int policy;

	if (len < sizeof(params))
		goto out;

	len = sizeof(params);
	if (copy_from_user(&params, optval, len)) {
		retval = -EFAULT;
		goto out;
	}

	policy = params.sprstat_policy;
	if (!policy || (policy & ~(SCTP_PR_SCTP_MASK | SCTP_PR_SCTP_ALL)) ||
	    ((policy & SCTP_PR_SCTP_ALL) && (policy & SCTP_PR_SCTP_MASK)))
		goto out;

	asoc = sctp_id2assoc(sk, params.sprstat_assoc_id);
	if (!asoc || params.sprstat_sid >= asoc->stream.outcnt)
		goto out;

	streamoute = SCTP_SO(&asoc->stream, params.sprstat_sid)->ext;
	if (!streamoute) {
		 
		params.sprstat_abandoned_unsent = 0;
		params.sprstat_abandoned_sent = 0;
		retval = 0;
		goto out;
	}

	if (policy == SCTP_PR_SCTP_ALL) {
		params.sprstat_abandoned_unsent = 0;
		params.sprstat_abandoned_sent = 0;
		for (policy = 0; policy <= SCTP_PR_INDEX(MAX); policy++) {
			params.sprstat_abandoned_unsent +=
				streamoute->abandoned_unsent[policy];
			params.sprstat_abandoned_sent +=
				streamoute->abandoned_sent[policy];
		}
	} else {
		params.sprstat_abandoned_unsent =
			streamoute->abandoned_unsent[__SCTP_PR_INDEX(policy)];
		params.sprstat_abandoned_sent =
			streamoute->abandoned_sent[__SCTP_PR_INDEX(policy)];
	}

	if (put_user(len, optlen) || copy_to_user(optval, &params, len)) {
		retval = -EFAULT;
		goto out;
	}

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_reconfig_supported(struct sock *sk, int len,
					      char __user *optval,
					      int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->peer.reconf_capable
				  : sctp_sk(sk)->ep->reconf_enable;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_enable_strreset(struct sock *sk, int len,
					   char __user *optval,
					   int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->strreset_enable
				  : sctp_sk(sk)->ep->strreset_enable;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_scheduler(struct sock *sk, int len,
				     char __user *optval,
				     int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? sctp_sched_get_sched(asoc)
				  : sctp_sk(sk)->default_ss;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_scheduler_value(struct sock *sk, int len,
					   char __user *optval,
					   int __user *optlen)
{
	struct sctp_stream_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc) {
		retval = -EINVAL;
		goto out;
	}

	retval = sctp_sched_get_value(asoc, params.stream_id,
				      &params.stream_value);
	if (retval)
		goto out;

	if (put_user(len, optlen)) {
		retval = -EFAULT;
		goto out;
	}

	if (copy_to_user(optval, &params, len)) {
		retval = -EFAULT;
		goto out;
	}

out:
	return retval;
}

static int sctp_getsockopt_interleaving_supported(struct sock *sk, int len,
						  char __user *optval,
						  int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->peer.intl_capable
				  : sctp_sk(sk)->ep->intl_enable;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_reuse_port(struct sock *sk, int len,
				      char __user *optval,
				      int __user *optlen)
{
	int val;

	if (len < sizeof(int))
		return -EINVAL;

	len = sizeof(int);
	val = sctp_sk(sk)->reuse;
	if (put_user(len, optlen))
		return -EFAULT;

	if (copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt_event(struct sock *sk, int len, char __user *optval,
				 int __user *optlen)
{
	struct sctp_association *asoc;
	struct sctp_event param;
	__u16 subscribe;

	if (len < sizeof(param))
		return -EINVAL;

	len = sizeof(param);
	if (copy_from_user(&param, optval, len))
		return -EFAULT;

	if (param.se_type < SCTP_SN_TYPE_BASE ||
	    param.se_type > SCTP_SN_TYPE_MAX)
		return -EINVAL;

	asoc = sctp_id2assoc(sk, param.se_assoc_id);
	if (!asoc && param.se_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP))
		return -EINVAL;

	subscribe = asoc ? asoc->subscribe : sctp_sk(sk)->subscribe;
	param.se_on = sctp_ulpevent_type_enabled(subscribe, param.se_type);

	if (put_user(len, optlen))
		return -EFAULT;

	if (copy_to_user(optval, &param, len))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt_asconf_supported(struct sock *sk, int len,
					    char __user *optval,
					    int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->peer.asconf_capable
				  : sctp_sk(sk)->ep->asconf_enable;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_auth_supported(struct sock *sk, int len,
					  char __user *optval,
					  int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->peer.auth_capable
				  : sctp_sk(sk)->ep->auth_enable;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_ecn_supported(struct sock *sk, int len,
					 char __user *optval,
					 int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->peer.ecn_capable
				  : sctp_sk(sk)->ep->ecn_enable;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_pf_expose(struct sock *sk, int len,
				     char __user *optval,
				     int __user *optlen)
{
	struct sctp_assoc_value params;
	struct sctp_association *asoc;
	int retval = -EFAULT;

	if (len < sizeof(params)) {
		retval = -EINVAL;
		goto out;
	}

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		goto out;

	asoc = sctp_id2assoc(sk, params.assoc_id);
	if (!asoc && params.assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		retval = -EINVAL;
		goto out;
	}

	params.assoc_value = asoc ? asoc->pf_expose
				  : sctp_sk(sk)->pf_expose;

	if (put_user(len, optlen))
		goto out;

	if (copy_to_user(optval, &params, len))
		goto out;

	retval = 0;

out:
	return retval;
}

static int sctp_getsockopt_encap_port(struct sock *sk, int len,
				      char __user *optval, int __user *optlen)
{
	struct sctp_association *asoc;
	struct sctp_udpencaps encap;
	struct sctp_transport *t;
	__be16 encap_port;

	if (len < sizeof(encap))
		return -EINVAL;

	len = sizeof(encap);
	if (copy_from_user(&encap, optval, len))
		return -EFAULT;

	 
	if (!sctp_is_any(sk, (union sctp_addr *)&encap.sue_address)) {
		t = sctp_addr_id2transport(sk, &encap.sue_address,
					   encap.sue_assoc_id);
		if (!t) {
			pr_debug("%s: failed no transport\n", __func__);
			return -EINVAL;
		}

		encap_port = t->encap_port;
		goto out;
	}

	 
	asoc = sctp_id2assoc(sk, encap.sue_assoc_id);
	if (!asoc && encap.sue_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		pr_debug("%s: failed no association\n", __func__);
		return -EINVAL;
	}

	if (asoc) {
		encap_port = asoc->encap_port;
		goto out;
	}

	encap_port = sctp_sk(sk)->encap_port;

out:
	encap.sue_port = (__force uint16_t)encap_port;
	if (copy_to_user(optval, &encap, len))
		return -EFAULT;

	if (put_user(len, optlen))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt_probe_interval(struct sock *sk, int len,
					  char __user *optval,
					  int __user *optlen)
{
	struct sctp_probeinterval params;
	struct sctp_association *asoc;
	struct sctp_transport *t;
	__u32 probe_interval;

	if (len < sizeof(params))
		return -EINVAL;

	len = sizeof(params);
	if (copy_from_user(&params, optval, len))
		return -EFAULT;

	 
	if (!sctp_is_any(sk, (union sctp_addr *)&params.spi_address)) {
		t = sctp_addr_id2transport(sk, &params.spi_address,
					   params.spi_assoc_id);
		if (!t) {
			pr_debug("%s: failed no transport\n", __func__);
			return -EINVAL;
		}

		probe_interval = jiffies_to_msecs(t->probe_interval);
		goto out;
	}

	 
	asoc = sctp_id2assoc(sk, params.spi_assoc_id);
	if (!asoc && params.spi_assoc_id != SCTP_FUTURE_ASSOC &&
	    sctp_style(sk, UDP)) {
		pr_debug("%s: failed no association\n", __func__);
		return -EINVAL;
	}

	if (asoc) {
		probe_interval = jiffies_to_msecs(asoc->probe_interval);
		goto out;
	}

	probe_interval = sctp_sk(sk)->probe_interval;

out:
	params.spi_interval = probe_interval;
	if (copy_to_user(optval, &params, len))
		return -EFAULT;

	if (put_user(len, optlen))
		return -EFAULT;

	return 0;
}

static int sctp_getsockopt(struct sock *sk, int level, int optname,
			   char __user *optval, int __user *optlen)
{
	int retval = 0;
	int len;

	pr_debug("%s: sk:%p, optname:%d\n", __func__, sk, optname);

	 
	if (level != SOL_SCTP) {
		struct sctp_af *af = sctp_sk(sk)->pf->af;

		retval = af->getsockopt(sk, level, optname, optval, optlen);
		return retval;
	}

	if (get_user(len, optlen))
		return -EFAULT;

	if (len < 0)
		return -EINVAL;

	lock_sock(sk);

	switch (optname) {
	case SCTP_STATUS:
		retval = sctp_getsockopt_sctp_status(sk, len, optval, optlen);
		break;
	case SCTP_DISABLE_FRAGMENTS:
		retval = sctp_getsockopt_disable_fragments(sk, len, optval,
							   optlen);
		break;
	case SCTP_EVENTS:
		retval = sctp_getsockopt_events(sk, len, optval, optlen);
		break;
	case SCTP_AUTOCLOSE:
		retval = sctp_getsockopt_autoclose(sk, len, optval, optlen);
		break;
	case SCTP_SOCKOPT_PEELOFF:
		retval = sctp_getsockopt_peeloff(sk, len, optval, optlen);
		break;
	case SCTP_SOCKOPT_PEELOFF_FLAGS:
		retval = sctp_getsockopt_peeloff_flags(sk, len, optval, optlen);
		break;
	case SCTP_PEER_ADDR_PARAMS:
		retval = sctp_getsockopt_peer_addr_params(sk, len, optval,
							  optlen);
		break;
	case SCTP_DELAYED_SACK:
		retval = sctp_getsockopt_delayed_ack(sk, len, optval,
							  optlen);
		break;
	case SCTP_INITMSG:
		retval = sctp_getsockopt_initmsg(sk, len, optval, optlen);
		break;
	case SCTP_GET_PEER_ADDRS:
		retval = sctp_getsockopt_peer_addrs(sk, len, optval,
						    optlen);
		break;
	case SCTP_GET_LOCAL_ADDRS:
		retval = sctp_getsockopt_local_addrs(sk, len, optval,
						     optlen);
		break;
	case SCTP_SOCKOPT_CONNECTX3:
		retval = sctp_getsockopt_connectx3(sk, len, optval, optlen);
		break;
	case SCTP_DEFAULT_SEND_PARAM:
		retval = sctp_getsockopt_default_send_param(sk, len,
							    optval, optlen);
		break;
	case SCTP_DEFAULT_SNDINFO:
		retval = sctp_getsockopt_default_sndinfo(sk, len,
							 optval, optlen);
		break;
	case SCTP_PRIMARY_ADDR:
		retval = sctp_getsockopt_primary_addr(sk, len, optval, optlen);
		break;
	case SCTP_NODELAY:
		retval = sctp_getsockopt_nodelay(sk, len, optval, optlen);
		break;
	case SCTP_RTOINFO:
		retval = sctp_getsockopt_rtoinfo(sk, len, optval, optlen);
		break;
	case SCTP_ASSOCINFO:
		retval = sctp_getsockopt_associnfo(sk, len, optval, optlen);
		break;
	case SCTP_I_WANT_MAPPED_V4_ADDR:
		retval = sctp_getsockopt_mappedv4(sk, len, optval, optlen);
		break;
	case SCTP_MAXSEG:
		retval = sctp_getsockopt_maxseg(sk, len, optval, optlen);
		break;
	case SCTP_GET_PEER_ADDR_INFO:
		retval = sctp_getsockopt_peer_addr_info(sk, len, optval,
							optlen);
		break;
	case SCTP_ADAPTATION_LAYER:
		retval = sctp_getsockopt_adaptation_layer(sk, len, optval,
							optlen);
		break;
	case SCTP_CONTEXT:
		retval = sctp_getsockopt_context(sk, len, optval, optlen);
		break;
	case SCTP_FRAGMENT_INTERLEAVE:
		retval = sctp_getsockopt_fragment_interleave(sk, len, optval,
							     optlen);
		break;
	case SCTP_PARTIAL_DELIVERY_POINT:
		retval = sctp_getsockopt_partial_delivery_point(sk, len, optval,
								optlen);
		break;
	case SCTP_MAX_BURST:
		retval = sctp_getsockopt_maxburst(sk, len, optval, optlen);
		break;
	case SCTP_AUTH_KEY:
	case SCTP_AUTH_CHUNK:
	case SCTP_AUTH_DELETE_KEY:
	case SCTP_AUTH_DEACTIVATE_KEY:
		retval = -EOPNOTSUPP;
		break;
	case SCTP_HMAC_IDENT:
		retval = sctp_getsockopt_hmac_ident(sk, len, optval, optlen);
		break;
	case SCTP_AUTH_ACTIVE_KEY:
		retval = sctp_getsockopt_active_key(sk, len, optval, optlen);
		break;
	case SCTP_PEER_AUTH_CHUNKS:
		retval = sctp_getsockopt_peer_auth_chunks(sk, len, optval,
							optlen);
		break;
	case SCTP_LOCAL_AUTH_CHUNKS:
		retval = sctp_getsockopt_local_auth_chunks(sk, len, optval,
							optlen);
		break;
	case SCTP_GET_ASSOC_NUMBER:
		retval = sctp_getsockopt_assoc_number(sk, len, optval, optlen);
		break;
	case SCTP_GET_ASSOC_ID_LIST:
		retval = sctp_getsockopt_assoc_ids(sk, len, optval, optlen);
		break;
	case SCTP_AUTO_ASCONF:
		retval = sctp_getsockopt_auto_asconf(sk, len, optval, optlen);
		break;
	case SCTP_PEER_ADDR_THLDS:
		retval = sctp_getsockopt_paddr_thresholds(sk, optval, len,
							  optlen, false);
		break;
	case SCTP_PEER_ADDR_THLDS_V2:
		retval = sctp_getsockopt_paddr_thresholds(sk, optval, len,
							  optlen, true);
		break;
	case SCTP_GET_ASSOC_STATS:
		retval = sctp_getsockopt_assoc_stats(sk, len, optval, optlen);
		break;
	case SCTP_RECVRCVINFO:
		retval = sctp_getsockopt_recvrcvinfo(sk, len, optval, optlen);
		break;
	case SCTP_RECVNXTINFO:
		retval = sctp_getsockopt_recvnxtinfo(sk, len, optval, optlen);
		break;
	case SCTP_PR_SUPPORTED:
		retval = sctp_getsockopt_pr_supported(sk, len, optval, optlen);
		break;
	case SCTP_DEFAULT_PRINFO:
		retval = sctp_getsockopt_default_prinfo(sk, len, optval,
							optlen);
		break;
	case SCTP_PR_ASSOC_STATUS:
		retval = sctp_getsockopt_pr_assocstatus(sk, len, optval,
							optlen);
		break;
	case SCTP_PR_STREAM_STATUS:
		retval = sctp_getsockopt_pr_streamstatus(sk, len, optval,
							 optlen);
		break;
	case SCTP_RECONFIG_SUPPORTED:
		retval = sctp_getsockopt_reconfig_supported(sk, len, optval,
							    optlen);
		break;
	case SCTP_ENABLE_STREAM_RESET:
		retval = sctp_getsockopt_enable_strreset(sk, len, optval,
							 optlen);
		break;
	case SCTP_STREAM_SCHEDULER:
		retval = sctp_getsockopt_scheduler(sk, len, optval,
						   optlen);
		break;
	case SCTP_STREAM_SCHEDULER_VALUE:
		retval = sctp_getsockopt_scheduler_value(sk, len, optval,
							 optlen);
		break;
	case SCTP_INTERLEAVING_SUPPORTED:
		retval = sctp_getsockopt_interleaving_supported(sk, len, optval,
								optlen);
		break;
	case SCTP_REUSE_PORT:
		retval = sctp_getsockopt_reuse_port(sk, len, optval, optlen);
		break;
	case SCTP_EVENT:
		retval = sctp_getsockopt_event(sk, len, optval, optlen);
		break;
	case SCTP_ASCONF_SUPPORTED:
		retval = sctp_getsockopt_asconf_supported(sk, len, optval,
							  optlen);
		break;
	case SCTP_AUTH_SUPPORTED:
		retval = sctp_getsockopt_auth_supported(sk, len, optval,
							optlen);
		break;
	case SCTP_ECN_SUPPORTED:
		retval = sctp_getsockopt_ecn_supported(sk, len, optval, optlen);
		break;
	case SCTP_EXPOSE_POTENTIALLY_FAILED_STATE:
		retval = sctp_getsockopt_pf_expose(sk, len, optval, optlen);
		break;
	case SCTP_REMOTE_UDP_ENCAPS_PORT:
		retval = sctp_getsockopt_encap_port(sk, len, optval, optlen);
		break;
	case SCTP_PLPMTUD_PROBE_INTERVAL:
		retval = sctp_getsockopt_probe_interval(sk, len, optval, optlen);
		break;
	default:
		retval = -ENOPROTOOPT;
		break;
	}

	release_sock(sk);
	return retval;
}

static bool sctp_bpf_bypass_getsockopt(int level, int optname)
{
	if (level == SOL_SCTP) {
		switch (optname) {
		case SCTP_SOCKOPT_PEELOFF:
		case SCTP_SOCKOPT_PEELOFF_FLAGS:
		case SCTP_SOCKOPT_CONNECTX3:
			return true;
		default:
			return false;
		}
	}

	return false;
}

static int sctp_hash(struct sock *sk)
{
	 
	return 0;
}

static void sctp_unhash(struct sock *sk)
{
	 
}

 
static struct sctp_bind_bucket *sctp_bucket_create(
	struct sctp_bind_hashbucket *head, struct net *, unsigned short snum);

static int sctp_get_port_local(struct sock *sk, union sctp_addr *addr)
{
	struct sctp_sock *sp = sctp_sk(sk);
	bool reuse = (sk->sk_reuse || sp->reuse);
	struct sctp_bind_hashbucket *head;  
	struct net *net = sock_net(sk);
	kuid_t uid = sock_i_uid(sk);
	struct sctp_bind_bucket *pp;
	unsigned short snum;
	int ret;

	snum = ntohs(addr->v4.sin_port);

	pr_debug("%s: begins, snum:%d\n", __func__, snum);

	if (snum == 0) {
		 
		int low, high, remaining, index;
		unsigned int rover;

		inet_sk_get_local_port_range(sk, &low, &high);
		remaining = (high - low) + 1;
		rover = get_random_u32_below(remaining) + low;

		do {
			rover++;
			if ((rover < low) || (rover > high))
				rover = low;
			if (inet_is_local_reserved_port(net, rover))
				continue;
			index = sctp_phashfn(net, rover);
			head = &sctp_port_hashtable[index];
			spin_lock_bh(&head->lock);
			sctp_for_each_hentry(pp, &head->chain)
				if ((pp->port == rover) &&
				    net_eq(net, pp->net))
					goto next;
			break;
		next:
			spin_unlock_bh(&head->lock);
			cond_resched();
		} while (--remaining > 0);

		 
		ret = 1;
		if (remaining <= 0)
			return ret;

		 
		snum = rover;
	} else {
		 
		head = &sctp_port_hashtable[sctp_phashfn(net, snum)];
		spin_lock_bh(&head->lock);
		sctp_for_each_hentry(pp, &head->chain) {
			if ((pp->port == snum) && net_eq(pp->net, net))
				goto pp_found;
		}
	}
	pp = NULL;
	goto pp_not_found;
pp_found:
	if (!hlist_empty(&pp->owner)) {
		 
		struct sock *sk2;

		pr_debug("%s: found a possible match\n", __func__);

		if ((pp->fastreuse && reuse &&
		     sk->sk_state != SCTP_SS_LISTENING) ||
		    (pp->fastreuseport && sk->sk_reuseport &&
		     uid_eq(pp->fastuid, uid)))
			goto success;

		 
		sk_for_each_bound(sk2, &pp->owner) {
			int bound_dev_if2 = READ_ONCE(sk2->sk_bound_dev_if);
			struct sctp_sock *sp2 = sctp_sk(sk2);
			struct sctp_endpoint *ep2 = sp2->ep;

			if (sk == sk2 ||
			    (reuse && (sk2->sk_reuse || sp2->reuse) &&
			     sk2->sk_state != SCTP_SS_LISTENING) ||
			    (sk->sk_reuseport && sk2->sk_reuseport &&
			     uid_eq(uid, sock_i_uid(sk2))))
				continue;

			if ((!sk->sk_bound_dev_if || !bound_dev_if2 ||
			     sk->sk_bound_dev_if == bound_dev_if2) &&
			    sctp_bind_addr_conflict(&ep2->base.bind_addr,
						    addr, sp2, sp)) {
				ret = 1;
				goto fail_unlock;
			}
		}

		pr_debug("%s: found a match\n", __func__);
	}
pp_not_found:
	 
	ret = 1;
	if (!pp && !(pp = sctp_bucket_create(head, net, snum)))
		goto fail_unlock;

	 
	if (hlist_empty(&pp->owner)) {
		if (reuse && sk->sk_state != SCTP_SS_LISTENING)
			pp->fastreuse = 1;
		else
			pp->fastreuse = 0;

		if (sk->sk_reuseport) {
			pp->fastreuseport = 1;
			pp->fastuid = uid;
		} else {
			pp->fastreuseport = 0;
		}
	} else {
		if (pp->fastreuse &&
		    (!reuse || sk->sk_state == SCTP_SS_LISTENING))
			pp->fastreuse = 0;

		if (pp->fastreuseport &&
		    (!sk->sk_reuseport || !uid_eq(pp->fastuid, uid)))
			pp->fastreuseport = 0;
	}

	 
success:
	if (!sp->bind_hash) {
		inet_sk(sk)->inet_num = snum;
		sk_add_bind_node(sk, &pp->owner);
		sp->bind_hash = pp;
	}
	ret = 0;

fail_unlock:
	spin_unlock_bh(&head->lock);
	return ret;
}

 
static int sctp_get_port(struct sock *sk, unsigned short snum)
{
	union sctp_addr addr;
	struct sctp_af *af = sctp_sk(sk)->pf->af;

	 
	af->from_sk(&addr, sk);
	addr.v4.sin_port = htons(snum);

	 
	return sctp_get_port_local(sk, &addr);
}

 
static int sctp_listen_start(struct sock *sk, int backlog)
{
	struct sctp_sock *sp = sctp_sk(sk);
	struct sctp_endpoint *ep = sp->ep;
	struct crypto_shash *tfm = NULL;
	char alg[32];

	 
	if (!sp->hmac && sp->sctp_hmac_alg) {
		sprintf(alg, "hmac(%s)", sp->sctp_hmac_alg);
		tfm = crypto_alloc_shash(alg, 0, 0);
		if (IS_ERR(tfm)) {
			net_info_ratelimited("failed to load transform for %s: %ld\n",
					     sp->sctp_hmac_alg, PTR_ERR(tfm));
			return -ENOSYS;
		}
		sctp_sk(sk)->hmac = tfm;
	}

	 
	inet_sk_set_state(sk, SCTP_SS_LISTENING);
	if (!ep->base.bind_addr.port) {
		if (sctp_autobind(sk))
			return -EAGAIN;
	} else {
		if (sctp_get_port(sk, inet_sk(sk)->inet_num)) {
			inet_sk_set_state(sk, SCTP_SS_CLOSED);
			return -EADDRINUSE;
		}
	}

	WRITE_ONCE(sk->sk_max_ack_backlog, backlog);
	return sctp_hash_endpoint(ep);
}

 
int sctp_inet_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;
	struct sctp_endpoint *ep = sctp_sk(sk)->ep;
	int err = -EINVAL;

	if (unlikely(backlog < 0))
		return err;

	lock_sock(sk);

	 
	if (sctp_style(sk, UDP_HIGH_BANDWIDTH))
		goto out;

	if (sock->state != SS_UNCONNECTED)
		goto out;

	if (!sctp_sstate(sk, LISTENING) && !sctp_sstate(sk, CLOSED))
		goto out;

	 
	if (!backlog) {
		if (sctp_sstate(sk, CLOSED))
			goto out;

		err = 0;
		sctp_unhash_endpoint(ep);
		sk->sk_state = SCTP_SS_CLOSED;
		if (sk->sk_reuse || sctp_sk(sk)->reuse)
			sctp_sk(sk)->bind_hash->fastreuse = 1;
		goto out;
	}

	 
	if (sctp_sstate(sk, LISTENING))
		WRITE_ONCE(sk->sk_max_ack_backlog, backlog);
	else {
		err = sctp_listen_start(sk, backlog);
		if (err)
			goto out;
	}

	err = 0;
out:
	release_sock(sk);
	return err;
}

 
__poll_t sctp_poll(struct file *file, struct socket *sock, poll_table *wait)
{
	struct sock *sk = sock->sk;
	struct sctp_sock *sp = sctp_sk(sk);
	__poll_t mask;

	poll_wait(file, sk_sleep(sk), wait);

	sock_rps_record_flow(sk);

	 
	if (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING))
		return (!list_empty(&sp->ep->asocs)) ?
			(EPOLLIN | EPOLLRDNORM) : 0;

	mask = 0;

	 
	if (sk->sk_err || !skb_queue_empty_lockless(&sk->sk_error_queue))
		mask |= EPOLLERR |
			(sock_flag(sk, SOCK_SELECT_ERR_QUEUE) ? EPOLLPRI : 0);
	if (sk->sk_shutdown & RCV_SHUTDOWN)
		mask |= EPOLLRDHUP | EPOLLIN | EPOLLRDNORM;
	if (sk->sk_shutdown == SHUTDOWN_MASK)
		mask |= EPOLLHUP;

	 
	if (!skb_queue_empty_lockless(&sk->sk_receive_queue))
		mask |= EPOLLIN | EPOLLRDNORM;

	 
	if (!sctp_style(sk, UDP) && sctp_sstate(sk, CLOSED))
		return mask;

	 
	if (sctp_writeable(sk)) {
		mask |= EPOLLOUT | EPOLLWRNORM;
	} else {
		sk_set_bit(SOCKWQ_ASYNC_NOSPACE, sk);
		 
		if (sctp_writeable(sk))
			mask |= EPOLLOUT | EPOLLWRNORM;
	}
	return mask;
}

 

static struct sctp_bind_bucket *sctp_bucket_create(
	struct sctp_bind_hashbucket *head, struct net *net, unsigned short snum)
{
	struct sctp_bind_bucket *pp;

	pp = kmem_cache_alloc(sctp_bucket_cachep, GFP_ATOMIC);
	if (pp) {
		SCTP_DBG_OBJCNT_INC(bind_bucket);
		pp->port = snum;
		pp->fastreuse = 0;
		INIT_HLIST_HEAD(&pp->owner);
		pp->net = net;
		hlist_add_head(&pp->node, &head->chain);
	}
	return pp;
}

 
static void sctp_bucket_destroy(struct sctp_bind_bucket *pp)
{
	if (pp && hlist_empty(&pp->owner)) {
		__hlist_del(&pp->node);
		kmem_cache_free(sctp_bucket_cachep, pp);
		SCTP_DBG_OBJCNT_DEC(bind_bucket);
	}
}

 
static inline void __sctp_put_port(struct sock *sk)
{
	struct sctp_bind_hashbucket *head =
		&sctp_port_hashtable[sctp_phashfn(sock_net(sk),
						  inet_sk(sk)->inet_num)];
	struct sctp_bind_bucket *pp;

	spin_lock(&head->lock);
	pp = sctp_sk(sk)->bind_hash;
	__sk_del_bind_node(sk);
	sctp_sk(sk)->bind_hash = NULL;
	inet_sk(sk)->inet_num = 0;
	sctp_bucket_destroy(pp);
	spin_unlock(&head->lock);
}

void sctp_put_port(struct sock *sk)
{
	local_bh_disable();
	__sctp_put_port(sk);
	local_bh_enable();
}

 
static int sctp_autobind(struct sock *sk)
{
	union sctp_addr autoaddr;
	struct sctp_af *af;
	__be16 port;

	 
	af = sctp_sk(sk)->pf->af;

	port = htons(inet_sk(sk)->inet_num);
	af->inaddr_any(&autoaddr, port);

	return sctp_do_bind(sk, &autoaddr, af->sockaddr_len);
}

 
static int sctp_msghdr_parse(const struct msghdr *msg, struct sctp_cmsgs *cmsgs)
{
	struct msghdr *my_msg = (struct msghdr *)msg;
	struct cmsghdr *cmsg;

	for_each_cmsghdr(cmsg, my_msg) {
		if (!CMSG_OK(my_msg, cmsg))
			return -EINVAL;

		 
		if (cmsg->cmsg_level != IPPROTO_SCTP)
			continue;

		 
		switch (cmsg->cmsg_type) {
		case SCTP_INIT:
			 
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(struct sctp_initmsg)))
				return -EINVAL;

			cmsgs->init = CMSG_DATA(cmsg);
			break;

		case SCTP_SNDRCV:
			 
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(struct sctp_sndrcvinfo)))
				return -EINVAL;

			cmsgs->srinfo = CMSG_DATA(cmsg);

			if (cmsgs->srinfo->sinfo_flags &
			    ~(SCTP_UNORDERED | SCTP_ADDR_OVER |
			      SCTP_SACK_IMMEDIATELY | SCTP_SENDALL |
			      SCTP_PR_SCTP_MASK | SCTP_ABORT | SCTP_EOF))
				return -EINVAL;
			break;

		case SCTP_SNDINFO:
			 
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(struct sctp_sndinfo)))
				return -EINVAL;

			cmsgs->sinfo = CMSG_DATA(cmsg);

			if (cmsgs->sinfo->snd_flags &
			    ~(SCTP_UNORDERED | SCTP_ADDR_OVER |
			      SCTP_SACK_IMMEDIATELY | SCTP_SENDALL |
			      SCTP_PR_SCTP_MASK | SCTP_ABORT | SCTP_EOF))
				return -EINVAL;
			break;
		case SCTP_PRINFO:
			 
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(struct sctp_prinfo)))
				return -EINVAL;

			cmsgs->prinfo = CMSG_DATA(cmsg);
			if (cmsgs->prinfo->pr_policy & ~SCTP_PR_SCTP_MASK)
				return -EINVAL;

			if (cmsgs->prinfo->pr_policy == SCTP_PR_SCTP_NONE)
				cmsgs->prinfo->pr_value = 0;
			break;
		case SCTP_AUTHINFO:
			 
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(struct sctp_authinfo)))
				return -EINVAL;

			cmsgs->authinfo = CMSG_DATA(cmsg);
			break;
		case SCTP_DSTADDRV4:
		case SCTP_DSTADDRV6:
			 
			cmsgs->addrs_msg = my_msg;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

 
static int sctp_wait_for_packet(struct sock *sk, int *err, long *timeo_p)
{
	int error;
	DEFINE_WAIT(wait);

	prepare_to_wait_exclusive(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);

	 
	error = sock_error(sk);
	if (error)
		goto out;

	if (!skb_queue_empty(&sk->sk_receive_queue))
		goto ready;

	 
	if (sk->sk_shutdown & RCV_SHUTDOWN)
		goto out;

	 
	error = -ENOTCONN;

	 
	if (list_empty(&sctp_sk(sk)->ep->asocs) && !sctp_sstate(sk, LISTENING))
		goto out;

	 
	if (signal_pending(current))
		goto interrupted;

	 
	release_sock(sk);
	*timeo_p = schedule_timeout(*timeo_p);
	lock_sock(sk);

ready:
	finish_wait(sk_sleep(sk), &wait);
	return 0;

interrupted:
	error = sock_intr_errno(*timeo_p);

out:
	finish_wait(sk_sleep(sk), &wait);
	*err = error;
	return error;
}

 
struct sk_buff *sctp_skb_recv_datagram(struct sock *sk, int flags, int *err)
{
	int error;
	struct sk_buff *skb;
	long timeo;

	timeo = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);

	pr_debug("%s: timeo:%ld, max:%ld\n", __func__, timeo,
		 MAX_SCHEDULE_TIMEOUT);

	do {
		 
		if (flags & MSG_PEEK) {
			skb = skb_peek(&sk->sk_receive_queue);
			if (skb)
				refcount_inc(&skb->users);
		} else {
			skb = __skb_dequeue(&sk->sk_receive_queue);
		}

		if (skb)
			return skb;

		 
		error = sock_error(sk);
		if (error)
			goto no_packet;

		if (sk->sk_shutdown & RCV_SHUTDOWN)
			break;


		 
		error = -EAGAIN;
		if (!timeo)
			goto no_packet;
	} while (sctp_wait_for_packet(sk, err, &timeo) == 0);

	return NULL;

no_packet:
	*err = error;
	return NULL;
}

 
static void __sctp_write_space(struct sctp_association *asoc)
{
	struct sock *sk = asoc->base.sk;

	if (sctp_wspace(asoc) <= 0)
		return;

	if (waitqueue_active(&asoc->wait))
		wake_up_interruptible(&asoc->wait);

	if (sctp_writeable(sk)) {
		struct socket_wq *wq;

		rcu_read_lock();
		wq = rcu_dereference(sk->sk_wq);
		if (wq) {
			if (waitqueue_active(&wq->wait))
				wake_up_interruptible(&wq->wait);

			 
			if (!(sk->sk_shutdown & SEND_SHUTDOWN))
				sock_wake_async(wq, SOCK_WAKE_SPACE, POLL_OUT);
		}
		rcu_read_unlock();
	}
}

static void sctp_wake_up_waiters(struct sock *sk,
				 struct sctp_association *asoc)
{
	struct sctp_association *tmp = asoc;

	 
	if (asoc->ep->sndbuf_policy)
		return __sctp_write_space(asoc);

	 
	if (asoc->base.dead)
		return sctp_write_space(sk);

	 
	for (tmp = list_next_entry(tmp, asocs); 1;
	     tmp = list_next_entry(tmp, asocs)) {
		 
		if (&tmp->asocs == &((sctp_sk(sk))->ep->asocs))
			continue;
		 
		__sctp_write_space(tmp);
		 
		if (tmp == asoc)
			break;
	}
}

 
static void sctp_wfree(struct sk_buff *skb)
{
	struct sctp_chunk *chunk = skb_shinfo(skb)->destructor_arg;
	struct sctp_association *asoc = chunk->asoc;
	struct sock *sk = asoc->base.sk;

	sk_mem_uncharge(sk, skb->truesize);
	sk_wmem_queued_add(sk, -(skb->truesize + sizeof(struct sctp_chunk)));
	asoc->sndbuf_used -= skb->truesize + sizeof(struct sctp_chunk);
	WARN_ON(refcount_sub_and_test(sizeof(struct sctp_chunk),
				      &sk->sk_wmem_alloc));

	if (chunk->shkey) {
		struct sctp_shared_key *shkey = chunk->shkey;

		 
		if (shkey->deactivated && !list_empty(&shkey->key_list) &&
		    refcount_read(&shkey->refcnt) == 2) {
			struct sctp_ulpevent *ev;

			ev = sctp_ulpevent_make_authkey(asoc, shkey->key_id,
							SCTP_AUTH_FREE_KEY,
							GFP_KERNEL);
			if (ev)
				asoc->stream.si->enqueue_event(&asoc->ulpq, ev);
		}
		sctp_auth_shkey_release(chunk->shkey);
	}

	sock_wfree(skb);
	sctp_wake_up_waiters(sk, asoc);

	sctp_association_put(asoc);
}

 
void sctp_sock_rfree(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct sctp_ulpevent *event = sctp_skb2event(skb);

	atomic_sub(event->rmem_len, &sk->sk_rmem_alloc);

	 
	sk_mem_uncharge(sk, event->rmem_len);
}


 
static int sctp_wait_for_sndbuf(struct sctp_association *asoc, long *timeo_p,
				size_t msg_len)
{
	struct sock *sk = asoc->base.sk;
	long current_timeo = *timeo_p;
	DEFINE_WAIT(wait);
	int err = 0;

	pr_debug("%s: asoc:%p, timeo:%ld, msg_len:%zu\n", __func__, asoc,
		 *timeo_p, msg_len);

	 
	sctp_association_hold(asoc);

	 
	for (;;) {
		prepare_to_wait_exclusive(&asoc->wait, &wait,
					  TASK_INTERRUPTIBLE);
		if (asoc->base.dead)
			goto do_dead;
		if (!*timeo_p)
			goto do_nonblock;
		if (sk->sk_err || asoc->state >= SCTP_STATE_SHUTDOWN_PENDING)
			goto do_error;
		if (signal_pending(current))
			goto do_interrupted;
		if ((int)msg_len <= sctp_wspace(asoc) &&
		    sk_wmem_schedule(sk, msg_len))
			break;

		 
		release_sock(sk);
		current_timeo = schedule_timeout(current_timeo);
		lock_sock(sk);
		if (sk != asoc->base.sk)
			goto do_error;

		*timeo_p = current_timeo;
	}

out:
	finish_wait(&asoc->wait, &wait);

	 
	sctp_association_put(asoc);

	return err;

do_dead:
	err = -ESRCH;
	goto out;

do_error:
	err = -EPIPE;
	goto out;

do_interrupted:
	err = sock_intr_errno(*timeo_p);
	goto out;

do_nonblock:
	err = -EAGAIN;
	goto out;
}

void sctp_data_ready(struct sock *sk)
{
	struct socket_wq *wq;

	trace_sk_data_ready(sk);

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (skwq_has_sleeper(wq))
		wake_up_interruptible_sync_poll(&wq->wait, EPOLLIN |
						EPOLLRDNORM | EPOLLRDBAND);
	sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_IN);
	rcu_read_unlock();
}

 
void sctp_write_space(struct sock *sk)
{
	struct sctp_association *asoc;

	 
	list_for_each_entry(asoc, &((sctp_sk(sk))->ep->asocs), asocs) {
		__sctp_write_space(asoc);
	}
}

 
static bool sctp_writeable(const struct sock *sk)
{
	return READ_ONCE(sk->sk_sndbuf) > READ_ONCE(sk->sk_wmem_queued);
}

 
static int sctp_wait_for_connect(struct sctp_association *asoc, long *timeo_p)
{
	struct sock *sk = asoc->base.sk;
	int err = 0;
	long current_timeo = *timeo_p;
	DEFINE_WAIT(wait);

	pr_debug("%s: asoc:%p, timeo:%ld\n", __func__, asoc, *timeo_p);

	 
	sctp_association_hold(asoc);

	for (;;) {
		prepare_to_wait_exclusive(&asoc->wait, &wait,
					  TASK_INTERRUPTIBLE);
		if (!*timeo_p)
			goto do_nonblock;
		if (sk->sk_shutdown & RCV_SHUTDOWN)
			break;
		if (sk->sk_err || asoc->state >= SCTP_STATE_SHUTDOWN_PENDING ||
		    asoc->base.dead)
			goto do_error;
		if (signal_pending(current))
			goto do_interrupted;

		if (sctp_state(asoc, ESTABLISHED))
			break;

		 
		release_sock(sk);
		current_timeo = schedule_timeout(current_timeo);
		lock_sock(sk);

		*timeo_p = current_timeo;
	}

out:
	finish_wait(&asoc->wait, &wait);

	 
	sctp_association_put(asoc);

	return err;

do_error:
	if (asoc->init_err_counter + 1 > asoc->max_init_attempts)
		err = -ETIMEDOUT;
	else
		err = -ECONNREFUSED;
	goto out;

do_interrupted:
	err = sock_intr_errno(*timeo_p);
	goto out;

do_nonblock:
	err = -EINPROGRESS;
	goto out;
}

static int sctp_wait_for_accept(struct sock *sk, long timeo)
{
	struct sctp_endpoint *ep;
	int err = 0;
	DEFINE_WAIT(wait);

	ep = sctp_sk(sk)->ep;


	for (;;) {
		prepare_to_wait_exclusive(sk_sleep(sk), &wait,
					  TASK_INTERRUPTIBLE);

		if (list_empty(&ep->asocs)) {
			release_sock(sk);
			timeo = schedule_timeout(timeo);
			lock_sock(sk);
		}

		err = -EINVAL;
		if (!sctp_sstate(sk, LISTENING))
			break;

		err = 0;
		if (!list_empty(&ep->asocs))
			break;

		err = sock_intr_errno(timeo);
		if (signal_pending(current))
			break;

		err = -EAGAIN;
		if (!timeo)
			break;
	}

	finish_wait(sk_sleep(sk), &wait);

	return err;
}

static void sctp_wait_for_close(struct sock *sk, long timeout)
{
	DEFINE_WAIT(wait);

	do {
		prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
		if (list_empty(&sctp_sk(sk)->ep->asocs))
			break;
		release_sock(sk);
		timeout = schedule_timeout(timeout);
		lock_sock(sk);
	} while (!signal_pending(current) && timeout);

	finish_wait(sk_sleep(sk), &wait);
}

static void sctp_skb_set_owner_r_frag(struct sk_buff *skb, struct sock *sk)
{
	struct sk_buff *frag;

	if (!skb->data_len)
		goto done;

	 
	skb_walk_frags(skb, frag)
		sctp_skb_set_owner_r_frag(frag, sk);

done:
	sctp_skb_set_owner_r(skb, sk);
}

void sctp_copy_sock(struct sock *newsk, struct sock *sk,
		    struct sctp_association *asoc)
{
	struct inet_sock *inet = inet_sk(sk);
	struct inet_sock *newinet;
	struct sctp_sock *sp = sctp_sk(sk);

	newsk->sk_type = sk->sk_type;
	newsk->sk_bound_dev_if = sk->sk_bound_dev_if;
	newsk->sk_flags = sk->sk_flags;
	newsk->sk_tsflags = sk->sk_tsflags;
	newsk->sk_no_check_tx = sk->sk_no_check_tx;
	newsk->sk_no_check_rx = sk->sk_no_check_rx;
	newsk->sk_reuse = sk->sk_reuse;
	sctp_sk(newsk)->reuse = sp->reuse;

	newsk->sk_shutdown = sk->sk_shutdown;
	newsk->sk_destruct = sk->sk_destruct;
	newsk->sk_family = sk->sk_family;
	newsk->sk_protocol = IPPROTO_SCTP;
	newsk->sk_backlog_rcv = sk->sk_prot->backlog_rcv;
	newsk->sk_sndbuf = sk->sk_sndbuf;
	newsk->sk_rcvbuf = sk->sk_rcvbuf;
	newsk->sk_lingertime = sk->sk_lingertime;
	newsk->sk_rcvtimeo = sk->sk_rcvtimeo;
	newsk->sk_sndtimeo = sk->sk_sndtimeo;
	newsk->sk_rxhash = sk->sk_rxhash;

	newinet = inet_sk(newsk);

	 
	newinet->inet_sport = inet->inet_sport;
	newinet->inet_saddr = inet->inet_saddr;
	newinet->inet_rcv_saddr = inet->inet_rcv_saddr;
	newinet->inet_dport = htons(asoc->peer.port);
	newinet->pmtudisc = inet->pmtudisc;
	atomic_set(&newinet->inet_id, get_random_u16());

	newinet->uc_ttl = inet->uc_ttl;
	inet_set_bit(MC_LOOP, newsk);
	newinet->mc_ttl = 1;
	newinet->mc_index = 0;
	newinet->mc_list = NULL;

	if (newsk->sk_flags & SK_FLAGS_TIMESTAMP)
		net_enable_timestamp();

	 
	security_sctp_sk_clone(asoc, sk, newsk);
}

static inline void sctp_copy_descendant(struct sock *sk_to,
					const struct sock *sk_from)
{
	size_t ancestor_size = sizeof(struct inet_sock);

	ancestor_size += sk_from->sk_prot->obj_size;
	ancestor_size -= offsetof(struct sctp_sock, pd_lobby);
	__inet_sk_copy_descendant(sk_to, sk_from, ancestor_size);
}

 
static int sctp_sock_migrate(struct sock *oldsk, struct sock *newsk,
			     struct sctp_association *assoc,
			     enum sctp_socket_type type)
{
	struct sctp_sock *oldsp = sctp_sk(oldsk);
	struct sctp_sock *newsp = sctp_sk(newsk);
	struct sctp_bind_bucket *pp;  
	struct sctp_endpoint *newep = newsp->ep;
	struct sk_buff *skb, *tmp;
	struct sctp_ulpevent *event;
	struct sctp_bind_hashbucket *head;
	int err;

	 
	newsk->sk_sndbuf = oldsk->sk_sndbuf;
	newsk->sk_rcvbuf = oldsk->sk_rcvbuf;
	 
	sctp_copy_descendant(newsk, oldsk);

	 
	newsp->ep = newep;
	newsp->hmac = NULL;

	 
	head = &sctp_port_hashtable[sctp_phashfn(sock_net(oldsk),
						 inet_sk(oldsk)->inet_num)];
	spin_lock_bh(&head->lock);
	pp = sctp_sk(oldsk)->bind_hash;
	sk_add_bind_node(newsk, &pp->owner);
	sctp_sk(newsk)->bind_hash = pp;
	inet_sk(newsk)->inet_num = inet_sk(oldsk)->inet_num;
	spin_unlock_bh(&head->lock);

	 
	err = sctp_bind_addr_dup(&newsp->ep->base.bind_addr,
				 &oldsp->ep->base.bind_addr, GFP_KERNEL);
	if (err)
		return err;

	 
	if (oldsp->ep->auth_hmacs) {
		err = sctp_auth_init_hmacs(newsp->ep, GFP_KERNEL);
		if (err)
			return err;
	}

	sctp_auto_asconf_init(newsp);

	 
	sctp_skb_for_each(skb, &oldsk->sk_receive_queue, tmp) {
		event = sctp_skb2event(skb);
		if (event->asoc == assoc) {
			__skb_unlink(skb, &oldsk->sk_receive_queue);
			__skb_queue_tail(&newsk->sk_receive_queue, skb);
			sctp_skb_set_owner_r_frag(skb, newsk);
		}
	}

	 
	atomic_set(&sctp_sk(newsk)->pd_mode, assoc->ulpq.pd_mode);

	if (atomic_read(&sctp_sk(oldsk)->pd_mode)) {
		struct sk_buff_head *queue;

		 
		if (assoc->ulpq.pd_mode) {
			queue = &newsp->pd_lobby;
		} else
			queue = &newsk->sk_receive_queue;

		 
		sctp_skb_for_each(skb, &oldsp->pd_lobby, tmp) {
			event = sctp_skb2event(skb);
			if (event->asoc == assoc) {
				__skb_unlink(skb, &oldsp->pd_lobby);
				__skb_queue_tail(queue, skb);
				sctp_skb_set_owner_r_frag(skb, newsk);
			}
		}

		 
		if (assoc->ulpq.pd_mode)
			sctp_clear_pd(oldsk, NULL);

	}

	sctp_for_each_rx_skb(assoc, newsk, sctp_skb_set_owner_r_frag);

	 
	newsp->type = type;

	 
	lock_sock_nested(newsk, SINGLE_DEPTH_NESTING);
	sctp_for_each_tx_datachunk(assoc, true, sctp_clear_owner_w);
	sctp_assoc_migrate(assoc, newsk);
	sctp_for_each_tx_datachunk(assoc, false, sctp_set_owner_w);

	 
	if (sctp_state(assoc, CLOSED) && sctp_style(newsk, TCP)) {
		inet_sk_set_state(newsk, SCTP_SS_CLOSED);
		newsk->sk_shutdown |= RCV_SHUTDOWN;
	} else {
		inet_sk_set_state(newsk, SCTP_SS_ESTABLISHED);
	}

	release_sock(newsk);

	return 0;
}


 
struct proto sctp_prot = {
	.name        =	"SCTP",
	.owner       =	THIS_MODULE,
	.close       =	sctp_close,
	.disconnect  =	sctp_disconnect,
	.accept      =	sctp_accept,
	.ioctl       =	sctp_ioctl,
	.init        =	sctp_init_sock,
	.destroy     =	sctp_destroy_sock,
	.shutdown    =	sctp_shutdown,
	.setsockopt  =	sctp_setsockopt,
	.getsockopt  =	sctp_getsockopt,
	.bpf_bypass_getsockopt	= sctp_bpf_bypass_getsockopt,
	.sendmsg     =	sctp_sendmsg,
	.recvmsg     =	sctp_recvmsg,
	.bind        =	sctp_bind,
	.bind_add    =  sctp_bind_add,
	.backlog_rcv =	sctp_backlog_rcv,
	.hash        =	sctp_hash,
	.unhash      =	sctp_unhash,
	.no_autobind =	true,
	.obj_size    =  sizeof(struct sctp_sock),
	.useroffset  =  offsetof(struct sctp_sock, subscribe),
	.usersize    =  offsetof(struct sctp_sock, initmsg) -
				offsetof(struct sctp_sock, subscribe) +
				sizeof_field(struct sctp_sock, initmsg),
	.sysctl_mem  =  sysctl_sctp_mem,
	.sysctl_rmem =  sysctl_sctp_rmem,
	.sysctl_wmem =  sysctl_sctp_wmem,
	.memory_pressure = &sctp_memory_pressure,
	.enter_memory_pressure = sctp_enter_memory_pressure,

	.memory_allocated = &sctp_memory_allocated,
	.per_cpu_fw_alloc = &sctp_memory_per_cpu_fw_alloc,

	.sockets_allocated = &sctp_sockets_allocated,
};

#if IS_ENABLED(CONFIG_IPV6)

static void sctp_v6_destruct_sock(struct sock *sk)
{
	sctp_destruct_common(sk);
	inet6_sock_destruct(sk);
}

static int sctp_v6_init_sock(struct sock *sk)
{
	int ret = sctp_init_sock(sk);

	if (!ret)
		sk->sk_destruct = sctp_v6_destruct_sock;

	return ret;
}

struct proto sctpv6_prot = {
	.name		= "SCTPv6",
	.owner		= THIS_MODULE,
	.close		= sctp_close,
	.disconnect	= sctp_disconnect,
	.accept		= sctp_accept,
	.ioctl		= sctp_ioctl,
	.init		= sctp_v6_init_sock,
	.destroy	= sctp_destroy_sock,
	.shutdown	= sctp_shutdown,
	.setsockopt	= sctp_setsockopt,
	.getsockopt	= sctp_getsockopt,
	.bpf_bypass_getsockopt	= sctp_bpf_bypass_getsockopt,
	.sendmsg	= sctp_sendmsg,
	.recvmsg	= sctp_recvmsg,
	.bind		= sctp_bind,
	.bind_add	= sctp_bind_add,
	.backlog_rcv	= sctp_backlog_rcv,
	.hash		= sctp_hash,
	.unhash		= sctp_unhash,
	.no_autobind	= true,
	.obj_size	= sizeof(struct sctp6_sock),
	.ipv6_pinfo_offset = offsetof(struct sctp6_sock, inet6),
	.useroffset	= offsetof(struct sctp6_sock, sctp.subscribe),
	.usersize	= offsetof(struct sctp6_sock, sctp.initmsg) -
				offsetof(struct sctp6_sock, sctp.subscribe) +
				sizeof_field(struct sctp6_sock, sctp.initmsg),
	.sysctl_mem	= sysctl_sctp_mem,
	.sysctl_rmem	= sysctl_sctp_rmem,
	.sysctl_wmem	= sysctl_sctp_wmem,
	.memory_pressure = &sctp_memory_pressure,
	.enter_memory_pressure = sctp_enter_memory_pressure,

	.memory_allocated = &sctp_memory_allocated,
	.per_cpu_fw_alloc = &sctp_memory_per_cpu_fw_alloc,

	.sockets_allocated = &sctp_sockets_allocated,
};
#endif  
