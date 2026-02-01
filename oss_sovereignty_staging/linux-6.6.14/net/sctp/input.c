
 

#include <linux/types.h>
#include <linux/list.h>  
#include <linux/socket.h>
#include <linux/ip.h>
#include <linux/time.h>  
#include <linux/slab.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/snmp.h>
#include <net/sock.h>
#include <net/xfrm.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>
#include <net/sctp/checksum.h>
#include <net/net_namespace.h>
#include <linux/rhashtable.h>
#include <net/sock_reuseport.h>

 
static int sctp_rcv_ootb(struct sk_buff *);
static struct sctp_association *__sctp_rcv_lookup(struct net *net,
				      struct sk_buff *skb,
				      const union sctp_addr *paddr,
				      const union sctp_addr *laddr,
				      struct sctp_transport **transportp,
				      int dif, int sdif);
static struct sctp_endpoint *__sctp_rcv_lookup_endpoint(
					struct net *net, struct sk_buff *skb,
					const union sctp_addr *laddr,
					const union sctp_addr *daddr,
					int dif, int sdif);
static struct sctp_association *__sctp_lookup_association(
					struct net *net,
					const union sctp_addr *local,
					const union sctp_addr *peer,
					struct sctp_transport **pt,
					int dif, int sdif);

static int sctp_add_backlog(struct sock *sk, struct sk_buff *skb);


 
static inline int sctp_rcv_checksum(struct net *net, struct sk_buff *skb)
{
	struct sctphdr *sh = sctp_hdr(skb);
	__le32 cmp = sh->checksum;
	__le32 val = sctp_compute_cksum(skb, 0);

	if (val != cmp) {
		 
		__SCTP_INC_STATS(net, SCTP_MIB_CHECKSUMERRORS);
		return -1;
	}
	return 0;
}

 
int sctp_rcv(struct sk_buff *skb)
{
	struct sock *sk;
	struct sctp_association *asoc;
	struct sctp_endpoint *ep = NULL;
	struct sctp_ep_common *rcvr;
	struct sctp_transport *transport = NULL;
	struct sctp_chunk *chunk;
	union sctp_addr src;
	union sctp_addr dest;
	int family;
	struct sctp_af *af;
	struct net *net = dev_net(skb->dev);
	bool is_gso = skb_is_gso(skb) && skb_is_gso_sctp(skb);
	int dif, sdif;

	if (skb->pkt_type != PACKET_HOST)
		goto discard_it;

	__SCTP_INC_STATS(net, SCTP_MIB_INSCTPPACKS);

	 
	if (skb->len < sizeof(struct sctphdr) + sizeof(struct sctp_chunkhdr) +
		       skb_transport_offset(skb))
		goto discard_it;

	 
	if ((!is_gso && skb_linearize(skb)) ||
	    !pskb_may_pull(skb, sizeof(struct sctphdr)))
		goto discard_it;

	 
	__skb_pull(skb, skb_transport_offset(skb));

	skb->csum_valid = 0;  
	if (skb_csum_unnecessary(skb))
		__skb_decr_checksum_unnecessary(skb);
	else if (!sctp_checksum_disable &&
		 !is_gso &&
		 sctp_rcv_checksum(net, skb) < 0)
		goto discard_it;
	skb->csum_valid = 1;

	__skb_pull(skb, sizeof(struct sctphdr));

	family = ipver2af(ip_hdr(skb)->version);
	af = sctp_get_af_specific(family);
	if (unlikely(!af))
		goto discard_it;
	SCTP_INPUT_CB(skb)->af = af;

	 
	af->from_skb(&src, skb, 1);
	af->from_skb(&dest, skb, 0);
	dif = af->skb_iif(skb);
	sdif = af->skb_sdif(skb);

	 
	if (!af->addr_valid(&src, NULL, skb) ||
	    !af->addr_valid(&dest, NULL, skb))
		goto discard_it;

	asoc = __sctp_rcv_lookup(net, skb, &src, &dest, &transport, dif, sdif);

	if (!asoc)
		ep = __sctp_rcv_lookup_endpoint(net, skb, &dest, &src, dif, sdif);

	 
	rcvr = asoc ? &asoc->base : &ep->base;
	sk = rcvr->sk;

	 
	if (!asoc) {
		if (sctp_rcv_ootb(skb)) {
			__SCTP_INC_STATS(net, SCTP_MIB_OUTOFBLUES);
			goto discard_release;
		}
	}

	if (!xfrm_policy_check(sk, XFRM_POLICY_IN, skb, family))
		goto discard_release;
	nf_reset_ct(skb);

	if (sk_filter(sk, skb))
		goto discard_release;

	 
	chunk = sctp_chunkify(skb, asoc, sk, GFP_ATOMIC);
	if (!chunk)
		goto discard_release;
	SCTP_INPUT_CB(skb)->chunk = chunk;

	 
	chunk->rcvr = rcvr;

	 
	chunk->sctp_hdr = sctp_hdr(skb);

	 
	sctp_init_addrs(chunk, &src, &dest);

	 
	chunk->transport = transport;

	 
	bh_lock_sock(sk);

	if (sk != rcvr->sk) {
		 
		bh_unlock_sock(sk);
		sk = rcvr->sk;
		bh_lock_sock(sk);
	}

	if (sock_owned_by_user(sk) || !sctp_newsk_ready(sk)) {
		if (sctp_add_backlog(sk, skb)) {
			bh_unlock_sock(sk);
			sctp_chunk_free(chunk);
			skb = NULL;  
			goto discard_release;
		}
		__SCTP_INC_STATS(net, SCTP_MIB_IN_PKT_BACKLOG);
	} else {
		__SCTP_INC_STATS(net, SCTP_MIB_IN_PKT_SOFTIRQ);
		sctp_inq_push(&chunk->rcvr->inqueue, chunk);
	}

	bh_unlock_sock(sk);

	 
	if (transport)
		sctp_transport_put(transport);
	else
		sctp_endpoint_put(ep);

	return 0;

discard_it:
	__SCTP_INC_STATS(net, SCTP_MIB_IN_PKT_DISCARDS);
	kfree_skb(skb);
	return 0;

discard_release:
	 
	if (transport)
		sctp_transport_put(transport);
	else
		sctp_endpoint_put(ep);

	goto discard_it;
}

 
int sctp_backlog_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct sctp_chunk *chunk = SCTP_INPUT_CB(skb)->chunk;
	struct sctp_inq *inqueue = &chunk->rcvr->inqueue;
	struct sctp_transport *t = chunk->transport;
	struct sctp_ep_common *rcvr = NULL;
	int backloged = 0;

	rcvr = chunk->rcvr;

	 
	if (rcvr->dead) {
		sctp_chunk_free(chunk);
		goto done;
	}

	if (unlikely(rcvr->sk != sk)) {
		 

		sk = rcvr->sk;
		local_bh_disable();
		bh_lock_sock(sk);

		if (sock_owned_by_user(sk) || !sctp_newsk_ready(sk)) {
			if (sk_add_backlog(sk, skb, READ_ONCE(sk->sk_rcvbuf)))
				sctp_chunk_free(chunk);
			else
				backloged = 1;
		} else
			sctp_inq_push(inqueue, chunk);

		bh_unlock_sock(sk);
		local_bh_enable();

		 
		if (backloged)
			return 0;
	} else {
		if (!sctp_newsk_ready(sk)) {
			if (!sk_add_backlog(sk, skb, READ_ONCE(sk->sk_rcvbuf)))
				return 0;
			sctp_chunk_free(chunk);
		} else {
			sctp_inq_push(inqueue, chunk);
		}
	}

done:
	 
	if (SCTP_EP_TYPE_ASSOCIATION == rcvr->type)
		sctp_transport_put(t);
	else if (SCTP_EP_TYPE_SOCKET == rcvr->type)
		sctp_endpoint_put(sctp_ep(rcvr));
	else
		BUG();

	return 0;
}

static int sctp_add_backlog(struct sock *sk, struct sk_buff *skb)
{
	struct sctp_chunk *chunk = SCTP_INPUT_CB(skb)->chunk;
	struct sctp_transport *t = chunk->transport;
	struct sctp_ep_common *rcvr = chunk->rcvr;
	int ret;

	ret = sk_add_backlog(sk, skb, READ_ONCE(sk->sk_rcvbuf));
	if (!ret) {
		 
		if (SCTP_EP_TYPE_ASSOCIATION == rcvr->type)
			sctp_transport_hold(t);
		else if (SCTP_EP_TYPE_SOCKET == rcvr->type)
			sctp_endpoint_hold(sctp_ep(rcvr));
		else
			BUG();
	}
	return ret;

}

 
void sctp_icmp_frag_needed(struct sock *sk, struct sctp_association *asoc,
			   struct sctp_transport *t, __u32 pmtu)
{
	if (!t ||
	    (t->pathmtu <= pmtu &&
	     t->pl.probe_size + sctp_transport_pl_hlen(t) <= pmtu))
		return;

	if (sock_owned_by_user(sk)) {
		atomic_set(&t->mtu_info, pmtu);
		asoc->pmtu_pending = 1;
		t->pmtu_pending = 1;
		return;
	}

	if (!(t->param_flags & SPP_PMTUD_ENABLE))
		 
		return;

	 
	if (!sctp_transport_update_pmtu(t, pmtu))
		return;

	 
	sctp_assoc_sync_pmtu(asoc);

	 
	sctp_retransmit(&asoc->outqueue, t, SCTP_RTXR_PMTUD);
}

void sctp_icmp_redirect(struct sock *sk, struct sctp_transport *t,
			struct sk_buff *skb)
{
	struct dst_entry *dst;

	if (sock_owned_by_user(sk) || !t)
		return;
	dst = sctp_transport_dst_check(t);
	if (dst)
		dst->ops->redirect(dst, sk, skb);
}

 
void sctp_icmp_proto_unreachable(struct sock *sk,
			   struct sctp_association *asoc,
			   struct sctp_transport *t)
{
	if (sock_owned_by_user(sk)) {
		if (timer_pending(&t->proto_unreach_timer))
			return;
		else {
			if (!mod_timer(&t->proto_unreach_timer,
						jiffies + (HZ/20)))
				sctp_transport_hold(t);
		}
	} else {
		struct net *net = sock_net(sk);

		pr_debug("%s: unrecognized next header type "
			 "encountered!\n", __func__);

		if (del_timer(&t->proto_unreach_timer))
			sctp_transport_put(t);

		sctp_do_sm(net, SCTP_EVENT_T_OTHER,
			   SCTP_ST_OTHER(SCTP_EVENT_ICMP_PROTO_UNREACH),
			   asoc->state, asoc->ep, asoc, t,
			   GFP_ATOMIC);
	}
}

 
struct sock *sctp_err_lookup(struct net *net, int family, struct sk_buff *skb,
			     struct sctphdr *sctphdr,
			     struct sctp_association **app,
			     struct sctp_transport **tpp)
{
	struct sctp_init_chunk *chunkhdr, _chunkhdr;
	union sctp_addr saddr;
	union sctp_addr daddr;
	struct sctp_af *af;
	struct sock *sk = NULL;
	struct sctp_association *asoc;
	struct sctp_transport *transport = NULL;
	__u32 vtag = ntohl(sctphdr->vtag);
	int sdif = inet_sdif(skb);
	int dif = inet_iif(skb);

	*app = NULL; *tpp = NULL;

	af = sctp_get_af_specific(family);
	if (unlikely(!af)) {
		return NULL;
	}

	 
	af->from_skb(&saddr, skb, 1);
	af->from_skb(&daddr, skb, 0);

	 
	asoc = __sctp_lookup_association(net, &saddr, &daddr, &transport, dif, sdif);
	if (!asoc)
		return NULL;

	sk = asoc->base.sk;

	 
	if (vtag == 0) {
		 
		chunkhdr = skb_header_pointer(skb, skb_transport_offset(skb) +
					      sizeof(struct sctphdr),
					      sizeof(struct sctp_chunkhdr) +
					      sizeof(__be32), &_chunkhdr);
		if (!chunkhdr ||
		    chunkhdr->chunk_hdr.type != SCTP_CID_INIT ||
		    ntohl(chunkhdr->init_hdr.init_tag) != asoc->c.my_vtag)
			goto out;

	} else if (vtag != asoc->c.peer_vtag) {
		goto out;
	}

	bh_lock_sock(sk);

	 
	if (sock_owned_by_user(sk))
		__NET_INC_STATS(net, LINUX_MIB_LOCKDROPPEDICMPS);

	*app = asoc;
	*tpp = transport;
	return sk;

out:
	sctp_transport_put(transport);
	return NULL;
}

 
void sctp_err_finish(struct sock *sk, struct sctp_transport *t)
	__releases(&((__sk)->sk_lock.slock))
{
	bh_unlock_sock(sk);
	sctp_transport_put(t);
}

static void sctp_v4_err_handle(struct sctp_transport *t, struct sk_buff *skb,
			       __u8 type, __u8 code, __u32 info)
{
	struct sctp_association *asoc = t->asoc;
	struct sock *sk = asoc->base.sk;
	int err = 0;

	switch (type) {
	case ICMP_PARAMETERPROB:
		err = EPROTO;
		break;
	case ICMP_DEST_UNREACH:
		if (code > NR_ICMP_UNREACH)
			return;
		if (code == ICMP_FRAG_NEEDED) {
			sctp_icmp_frag_needed(sk, asoc, t, SCTP_TRUNC4(info));
			return;
		}
		if (code == ICMP_PROT_UNREACH) {
			sctp_icmp_proto_unreachable(sk, asoc, t);
			return;
		}
		err = icmp_err_convert[code].errno;
		break;
	case ICMP_TIME_EXCEEDED:
		if (code == ICMP_EXC_FRAGTIME)
			return;

		err = EHOSTUNREACH;
		break;
	case ICMP_REDIRECT:
		sctp_icmp_redirect(sk, t, skb);
		return;
	default:
		return;
	}
	if (!sock_owned_by_user(sk) && inet_test_bit(RECVERR, sk)) {
		sk->sk_err = err;
		sk_error_report(sk);
	} else {   
		WRITE_ONCE(sk->sk_err_soft, err);
	}
}

 
int sctp_v4_err(struct sk_buff *skb, __u32 info)
{
	const struct iphdr *iph = (const struct iphdr *)skb->data;
	const int type = icmp_hdr(skb)->type;
	const int code = icmp_hdr(skb)->code;
	struct net *net = dev_net(skb->dev);
	struct sctp_transport *transport;
	struct sctp_association *asoc;
	__u16 saveip, savesctp;
	struct sock *sk;

	 
	saveip = skb->network_header;
	savesctp = skb->transport_header;
	skb_reset_network_header(skb);
	skb_set_transport_header(skb, iph->ihl * 4);
	sk = sctp_err_lookup(net, AF_INET, skb, sctp_hdr(skb), &asoc, &transport);
	 
	skb->network_header = saveip;
	skb->transport_header = savesctp;
	if (!sk) {
		__ICMP_INC_STATS(net, ICMP_MIB_INERRORS);
		return -ENOENT;
	}

	sctp_v4_err_handle(transport, skb, type, code, info);
	sctp_err_finish(sk, transport);

	return 0;
}

int sctp_udp_v4_err(struct sock *sk, struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);
	struct sctp_association *asoc;
	struct sctp_transport *t;
	struct icmphdr *hdr;
	__u32 info = 0;

	skb->transport_header += sizeof(struct udphdr);
	sk = sctp_err_lookup(net, AF_INET, skb, sctp_hdr(skb), &asoc, &t);
	if (!sk) {
		__ICMP_INC_STATS(net, ICMP_MIB_INERRORS);
		return -ENOENT;
	}

	skb->transport_header -= sizeof(struct udphdr);
	hdr = (struct icmphdr *)(skb_network_header(skb) - sizeof(struct icmphdr));
	if (hdr->type == ICMP_REDIRECT) {
		 
		sctp_err_finish(sk, t);
		return 0;
	}
	if (hdr->type == ICMP_DEST_UNREACH && hdr->code == ICMP_FRAG_NEEDED)
		info = ntohs(hdr->un.frag.mtu);
	sctp_v4_err_handle(t, skb, hdr->type, hdr->code, info);

	sctp_err_finish(sk, t);
	return 1;
}

 
static int sctp_rcv_ootb(struct sk_buff *skb)
{
	struct sctp_chunkhdr *ch, _ch;
	int ch_end, offset = 0;

	 
	do {
		 
		if (offset + sizeof(_ch) > skb->len)
			break;

		ch = skb_header_pointer(skb, offset, sizeof(*ch), &_ch);

		 
		if (!ch || ntohs(ch->length) < sizeof(_ch))
			break;

		ch_end = offset + SCTP_PAD4(ntohs(ch->length));
		if (ch_end > skb->len)
			break;

		 
		if (SCTP_CID_ABORT == ch->type)
			goto discard;

		 
		if (SCTP_CID_SHUTDOWN_COMPLETE == ch->type)
			goto discard;

		 
		if (SCTP_CID_INIT == ch->type && (void *)ch != skb->data)
			goto discard;

		offset = ch_end;
	} while (ch_end < skb->len);

	return 0;

discard:
	return 1;
}

 
static int __sctp_hash_endpoint(struct sctp_endpoint *ep)
{
	struct sock *sk = ep->base.sk;
	struct net *net = sock_net(sk);
	struct sctp_hashbucket *head;

	ep->hashent = sctp_ep_hashfn(net, ep->base.bind_addr.port);
	head = &sctp_ep_hashtable[ep->hashent];

	if (sk->sk_reuseport) {
		bool any = sctp_is_ep_boundall(sk);
		struct sctp_endpoint *ep2;
		struct list_head *list;
		int cnt = 0, err = 1;

		list_for_each(list, &ep->base.bind_addr.address_list)
			cnt++;

		sctp_for_each_hentry(ep2, &head->chain) {
			struct sock *sk2 = ep2->base.sk;

			if (!net_eq(sock_net(sk2), net) || sk2 == sk ||
			    !uid_eq(sock_i_uid(sk2), sock_i_uid(sk)) ||
			    !sk2->sk_reuseport)
				continue;

			err = sctp_bind_addrs_check(sctp_sk(sk2),
						    sctp_sk(sk), cnt);
			if (!err) {
				err = reuseport_add_sock(sk, sk2, any);
				if (err)
					return err;
				break;
			} else if (err < 0) {
				return err;
			}
		}

		if (err) {
			err = reuseport_alloc(sk, any);
			if (err)
				return err;
		}
	}

	write_lock(&head->lock);
	hlist_add_head(&ep->node, &head->chain);
	write_unlock(&head->lock);
	return 0;
}

 
int sctp_hash_endpoint(struct sctp_endpoint *ep)
{
	int err;

	local_bh_disable();
	err = __sctp_hash_endpoint(ep);
	local_bh_enable();

	return err;
}

 
static void __sctp_unhash_endpoint(struct sctp_endpoint *ep)
{
	struct sock *sk = ep->base.sk;
	struct sctp_hashbucket *head;

	ep->hashent = sctp_ep_hashfn(sock_net(sk), ep->base.bind_addr.port);

	head = &sctp_ep_hashtable[ep->hashent];

	if (rcu_access_pointer(sk->sk_reuseport_cb))
		reuseport_detach_sock(sk);

	write_lock(&head->lock);
	hlist_del_init(&ep->node);
	write_unlock(&head->lock);
}

 
void sctp_unhash_endpoint(struct sctp_endpoint *ep)
{
	local_bh_disable();
	__sctp_unhash_endpoint(ep);
	local_bh_enable();
}

static inline __u32 sctp_hashfn(const struct net *net, __be16 lport,
				const union sctp_addr *paddr, __u32 seed)
{
	__u32 addr;

	if (paddr->sa.sa_family == AF_INET6)
		addr = jhash(&paddr->v6.sin6_addr, 16, seed);
	else
		addr = (__force __u32)paddr->v4.sin_addr.s_addr;

	return  jhash_3words(addr, ((__force __u32)paddr->v4.sin_port) << 16 |
			     (__force __u32)lport, net_hash_mix(net), seed);
}

 
static struct sctp_endpoint *__sctp_rcv_lookup_endpoint(
					struct net *net, struct sk_buff *skb,
					const union sctp_addr *laddr,
					const union sctp_addr *paddr,
					int dif, int sdif)
{
	struct sctp_hashbucket *head;
	struct sctp_endpoint *ep;
	struct sock *sk;
	__be16 lport;
	int hash;

	lport = laddr->v4.sin_port;
	hash = sctp_ep_hashfn(net, ntohs(lport));
	head = &sctp_ep_hashtable[hash];
	read_lock(&head->lock);
	sctp_for_each_hentry(ep, &head->chain) {
		if (sctp_endpoint_is_match(ep, net, laddr, dif, sdif))
			goto hit;
	}

	ep = sctp_sk(net->sctp.ctl_sock)->ep;

hit:
	sk = ep->base.sk;
	if (sk->sk_reuseport) {
		__u32 phash = sctp_hashfn(net, lport, paddr, 0);

		sk = reuseport_select_sock(sk, phash, skb,
					   sizeof(struct sctphdr));
		if (sk)
			ep = sctp_sk(sk)->ep;
	}
	sctp_endpoint_hold(ep);
	read_unlock(&head->lock);
	return ep;
}

 
struct sctp_hash_cmp_arg {
	const union sctp_addr	*paddr;
	const struct net	*net;
	__be16			lport;
};

static inline int sctp_hash_cmp(struct rhashtable_compare_arg *arg,
				const void *ptr)
{
	struct sctp_transport *t = (struct sctp_transport *)ptr;
	const struct sctp_hash_cmp_arg *x = arg->key;
	int err = 1;

	if (!sctp_cmp_addr_exact(&t->ipaddr, x->paddr))
		return err;
	if (!sctp_transport_hold(t))
		return err;

	if (!net_eq(t->asoc->base.net, x->net))
		goto out;
	if (x->lport != htons(t->asoc->base.bind_addr.port))
		goto out;

	err = 0;
out:
	sctp_transport_put(t);
	return err;
}

static inline __u32 sctp_hash_obj(const void *data, u32 len, u32 seed)
{
	const struct sctp_transport *t = data;

	return sctp_hashfn(t->asoc->base.net,
			   htons(t->asoc->base.bind_addr.port),
			   &t->ipaddr, seed);
}

static inline __u32 sctp_hash_key(const void *data, u32 len, u32 seed)
{
	const struct sctp_hash_cmp_arg *x = data;

	return sctp_hashfn(x->net, x->lport, x->paddr, seed);
}

static const struct rhashtable_params sctp_hash_params = {
	.head_offset		= offsetof(struct sctp_transport, node),
	.hashfn			= sctp_hash_key,
	.obj_hashfn		= sctp_hash_obj,
	.obj_cmpfn		= sctp_hash_cmp,
	.automatic_shrinking	= true,
};

int sctp_transport_hashtable_init(void)
{
	return rhltable_init(&sctp_transport_hashtable, &sctp_hash_params);
}

void sctp_transport_hashtable_destroy(void)
{
	rhltable_destroy(&sctp_transport_hashtable);
}

int sctp_hash_transport(struct sctp_transport *t)
{
	struct sctp_transport *transport;
	struct rhlist_head *tmp, *list;
	struct sctp_hash_cmp_arg arg;
	int err;

	if (t->asoc->temp)
		return 0;

	arg.net   = t->asoc->base.net;
	arg.paddr = &t->ipaddr;
	arg.lport = htons(t->asoc->base.bind_addr.port);

	rcu_read_lock();
	list = rhltable_lookup(&sctp_transport_hashtable, &arg,
			       sctp_hash_params);

	rhl_for_each_entry_rcu(transport, tmp, list, node)
		if (transport->asoc->ep == t->asoc->ep) {
			rcu_read_unlock();
			return -EEXIST;
		}
	rcu_read_unlock();

	err = rhltable_insert_key(&sctp_transport_hashtable, &arg,
				  &t->node, sctp_hash_params);
	if (err)
		pr_err_once("insert transport fail, errno %d\n", err);

	return err;
}

void sctp_unhash_transport(struct sctp_transport *t)
{
	if (t->asoc->temp)
		return;

	rhltable_remove(&sctp_transport_hashtable, &t->node,
			sctp_hash_params);
}

bool sctp_sk_bound_dev_eq(struct net *net, int bound_dev_if, int dif, int sdif)
{
	bool l3mdev_accept = true;

#if IS_ENABLED(CONFIG_NET_L3_MASTER_DEV)
	l3mdev_accept = !!READ_ONCE(net->sctp.l3mdev_accept);
#endif
	return inet_bound_dev_eq(l3mdev_accept, bound_dev_if, dif, sdif);
}

 
struct sctp_transport *sctp_addrs_lookup_transport(
				struct net *net,
				const union sctp_addr *laddr,
				const union sctp_addr *paddr,
				int dif, int sdif)
{
	struct rhlist_head *tmp, *list;
	struct sctp_transport *t;
	int bound_dev_if;
	struct sctp_hash_cmp_arg arg = {
		.paddr = paddr,
		.net   = net,
		.lport = laddr->v4.sin_port,
	};

	list = rhltable_lookup(&sctp_transport_hashtable, &arg,
			       sctp_hash_params);

	rhl_for_each_entry_rcu(t, tmp, list, node) {
		if (!sctp_transport_hold(t))
			continue;

		bound_dev_if = READ_ONCE(t->asoc->base.sk->sk_bound_dev_if);
		if (sctp_sk_bound_dev_eq(net, bound_dev_if, dif, sdif) &&
		    sctp_bind_addr_match(&t->asoc->base.bind_addr,
					 laddr, sctp_sk(t->asoc->base.sk)))
			return t;
		sctp_transport_put(t);
	}

	return NULL;
}

 
struct sctp_transport *sctp_epaddr_lookup_transport(
				const struct sctp_endpoint *ep,
				const union sctp_addr *paddr)
{
	struct rhlist_head *tmp, *list;
	struct sctp_transport *t;
	struct sctp_hash_cmp_arg arg = {
		.paddr = paddr,
		.net   = ep->base.net,
		.lport = htons(ep->base.bind_addr.port),
	};

	list = rhltable_lookup(&sctp_transport_hashtable, &arg,
			       sctp_hash_params);

	rhl_for_each_entry_rcu(t, tmp, list, node)
		if (ep == t->asoc->ep)
			return t;

	return NULL;
}

 
static struct sctp_association *__sctp_lookup_association(
					struct net *net,
					const union sctp_addr *local,
					const union sctp_addr *peer,
					struct sctp_transport **pt,
					int dif, int sdif)
{
	struct sctp_transport *t;
	struct sctp_association *asoc = NULL;

	t = sctp_addrs_lookup_transport(net, local, peer, dif, sdif);
	if (!t)
		goto out;

	asoc = t->asoc;
	*pt = t;

out:
	return asoc;
}

 
static
struct sctp_association *sctp_lookup_association(struct net *net,
						 const union sctp_addr *laddr,
						 const union sctp_addr *paddr,
						 struct sctp_transport **transportp,
						 int dif, int sdif)
{
	struct sctp_association *asoc;

	rcu_read_lock();
	asoc = __sctp_lookup_association(net, laddr, paddr, transportp, dif, sdif);
	rcu_read_unlock();

	return asoc;
}

 
bool sctp_has_association(struct net *net,
			  const union sctp_addr *laddr,
			  const union sctp_addr *paddr,
			  int dif, int sdif)
{
	struct sctp_transport *transport;

	if (sctp_lookup_association(net, laddr, paddr, &transport, dif, sdif)) {
		sctp_transport_put(transport);
		return true;
	}

	return false;
}

 
static struct sctp_association *__sctp_rcv_init_lookup(struct net *net,
	struct sk_buff *skb,
	const union sctp_addr *laddr, struct sctp_transport **transportp,
	int dif, int sdif)
{
	struct sctp_association *asoc;
	union sctp_addr addr;
	union sctp_addr *paddr = &addr;
	struct sctphdr *sh = sctp_hdr(skb);
	union sctp_params params;
	struct sctp_init_chunk *init;
	struct sctp_af *af;

	 

	 
	init = (struct sctp_init_chunk *)skb->data;

	 
	sctp_walk_params(params, init) {

		 
		af = sctp_get_af_specific(param_type2af(params.p->type));
		if (!af)
			continue;

		if (!af->from_addr_param(paddr, params.addr, sh->source, 0))
			continue;

		asoc = __sctp_lookup_association(net, laddr, paddr, transportp, dif, sdif);
		if (asoc)
			return asoc;
	}

	return NULL;
}

 
static struct sctp_association *__sctp_rcv_asconf_lookup(
					struct net *net,
					struct sctp_chunkhdr *ch,
					const union sctp_addr *laddr,
					__be16 peer_port,
					struct sctp_transport **transportp,
					int dif, int sdif)
{
	struct sctp_addip_chunk *asconf = (struct sctp_addip_chunk *)ch;
	struct sctp_af *af;
	union sctp_addr_param *param;
	union sctp_addr paddr;

	if (ntohs(ch->length) < sizeof(*asconf) + sizeof(struct sctp_paramhdr))
		return NULL;

	 
	param = (union sctp_addr_param *)(asconf + 1);

	af = sctp_get_af_specific(param_type2af(param->p.type));
	if (unlikely(!af))
		return NULL;

	if (!af->from_addr_param(&paddr, param, peer_port, 0))
		return NULL;

	return __sctp_lookup_association(net, laddr, &paddr, transportp, dif, sdif);
}


 
static struct sctp_association *__sctp_rcv_walk_lookup(struct net *net,
				      struct sk_buff *skb,
				      const union sctp_addr *laddr,
				      struct sctp_transport **transportp,
				      int dif, int sdif)
{
	struct sctp_association *asoc = NULL;
	struct sctp_chunkhdr *ch;
	int have_auth = 0;
	unsigned int chunk_num = 1;
	__u8 *ch_end;

	 
	ch = (struct sctp_chunkhdr *)skb->data;
	do {
		 
		if (ntohs(ch->length) < sizeof(*ch))
			break;

		ch_end = ((__u8 *)ch) + SCTP_PAD4(ntohs(ch->length));
		if (ch_end > skb_tail_pointer(skb))
			break;

		switch (ch->type) {
		case SCTP_CID_AUTH:
			have_auth = chunk_num;
			break;

		case SCTP_CID_COOKIE_ECHO:
			 
			if (have_auth == 1 && chunk_num == 2)
				return NULL;
			break;

		case SCTP_CID_ASCONF:
			if (have_auth || net->sctp.addip_noauth)
				asoc = __sctp_rcv_asconf_lookup(
						net, ch, laddr,
						sctp_hdr(skb)->source,
						transportp, dif, sdif);
			break;
		default:
			break;
		}

		if (asoc)
			break;

		ch = (struct sctp_chunkhdr *)ch_end;
		chunk_num++;
	} while (ch_end + sizeof(*ch) < skb_tail_pointer(skb));

	return asoc;
}

 
static struct sctp_association *__sctp_rcv_lookup_harder(struct net *net,
				      struct sk_buff *skb,
				      const union sctp_addr *laddr,
				      struct sctp_transport **transportp,
				      int dif, int sdif)
{
	struct sctp_chunkhdr *ch;

	 
	if (skb_is_gso(skb) && skb_is_gso_sctp(skb))
		return NULL;

	ch = (struct sctp_chunkhdr *)skb->data;

	 
	if (SCTP_PAD4(ntohs(ch->length)) > skb->len)
		return NULL;

	 
	if (ch->type == SCTP_CID_INIT || ch->type == SCTP_CID_INIT_ACK)
		return __sctp_rcv_init_lookup(net, skb, laddr, transportp, dif, sdif);

	return __sctp_rcv_walk_lookup(net, skb, laddr, transportp, dif, sdif);
}

 
static struct sctp_association *__sctp_rcv_lookup(struct net *net,
				      struct sk_buff *skb,
				      const union sctp_addr *paddr,
				      const union sctp_addr *laddr,
				      struct sctp_transport **transportp,
				      int dif, int sdif)
{
	struct sctp_association *asoc;

	asoc = __sctp_lookup_association(net, laddr, paddr, transportp, dif, sdif);
	if (asoc)
		goto out;

	 
	asoc = __sctp_rcv_lookup_harder(net, skb, laddr, transportp, dif, sdif);
	if (asoc)
		goto out;

	if (paddr->sa.sa_family == AF_INET)
		pr_debug("sctp: asoc not found for src:%pI4:%d dst:%pI4:%d\n",
			 &laddr->v4.sin_addr, ntohs(laddr->v4.sin_port),
			 &paddr->v4.sin_addr, ntohs(paddr->v4.sin_port));
	else
		pr_debug("sctp: asoc not found for src:%pI6:%d dst:%pI6:%d\n",
			 &laddr->v6.sin6_addr, ntohs(laddr->v6.sin6_port),
			 &paddr->v6.sin6_addr, ntohs(paddr->v6.sin6_port));

out:
	return asoc;
}
