 

#include <net/sock.h>
#include "core.h"
#include "msg.h"
#include "addr.h"
#include "name_table.h"
#include "crypto.h"

#define BUF_ALIGN(x) ALIGN(x, 4)
#define MAX_FORWARD_SIZE 1024
#ifdef CONFIG_TIPC_CRYPTO
#define BUF_HEADROOM ALIGN(((LL_MAX_HEADER + 48) + EHDR_MAX_SIZE), 16)
#define BUF_OVERHEAD (BUF_HEADROOM + TIPC_AES_GCM_TAG_SIZE)
#else
#define BUF_HEADROOM (LL_MAX_HEADER + 48)
#define BUF_OVERHEAD BUF_HEADROOM
#endif

const int one_page_mtu = PAGE_SIZE - SKB_DATA_ALIGN(BUF_OVERHEAD) -
			 SKB_DATA_ALIGN(sizeof(struct skb_shared_info));

 
struct sk_buff *tipc_buf_acquire(u32 size, gfp_t gfp)
{
	struct sk_buff *skb;

	skb = alloc_skb_fclone(BUF_OVERHEAD + size, gfp);
	if (skb) {
		skb_reserve(skb, BUF_HEADROOM);
		skb_put(skb, size);
		skb->next = NULL;
	}
	return skb;
}

void tipc_msg_init(u32 own_node, struct tipc_msg *m, u32 user, u32 type,
		   u32 hsize, u32 dnode)
{
	memset(m, 0, hsize);
	msg_set_version(m);
	msg_set_user(m, user);
	msg_set_hdr_sz(m, hsize);
	msg_set_size(m, hsize);
	msg_set_prevnode(m, own_node);
	msg_set_type(m, type);
	if (hsize > SHORT_H_SIZE) {
		msg_set_orignode(m, own_node);
		msg_set_destnode(m, dnode);
	}
}

struct sk_buff *tipc_msg_create(uint user, uint type,
				uint hdr_sz, uint data_sz, u32 dnode,
				u32 onode, u32 dport, u32 oport, int errcode)
{
	struct tipc_msg *msg;
	struct sk_buff *buf;

	buf = tipc_buf_acquire(hdr_sz + data_sz, GFP_ATOMIC);
	if (unlikely(!buf))
		return NULL;

	msg = buf_msg(buf);
	tipc_msg_init(onode, msg, user, type, hdr_sz, dnode);
	msg_set_size(msg, hdr_sz + data_sz);
	msg_set_origport(msg, oport);
	msg_set_destport(msg, dport);
	msg_set_errcode(msg, errcode);
	return buf;
}

 
int tipc_buf_append(struct sk_buff **headbuf, struct sk_buff **buf)
{
	struct sk_buff *head = *headbuf;
	struct sk_buff *frag = *buf;
	struct sk_buff *tail = NULL;
	struct tipc_msg *msg;
	u32 fragid;
	int delta;
	bool headstolen;

	if (!frag)
		goto err;

	msg = buf_msg(frag);
	fragid = msg_type(msg);
	frag->next = NULL;
	skb_pull(frag, msg_hdr_sz(msg));

	if (fragid == FIRST_FRAGMENT) {
		if (unlikely(head))
			goto err;
		*buf = NULL;
		if (skb_has_frag_list(frag) && __skb_linearize(frag))
			goto err;
		frag = skb_unshare(frag, GFP_ATOMIC);
		if (unlikely(!frag))
			goto err;
		head = *headbuf = frag;
		TIPC_SKB_CB(head)->tail = NULL;
		return 0;
	}

	if (!head)
		goto err;

	if (skb_try_coalesce(head, frag, &headstolen, &delta)) {
		kfree_skb_partial(frag, headstolen);
	} else {
		tail = TIPC_SKB_CB(head)->tail;
		if (!skb_has_frag_list(head))
			skb_shinfo(head)->frag_list = frag;
		else
			tail->next = frag;
		head->truesize += frag->truesize;
		head->data_len += frag->len;
		head->len += frag->len;
		TIPC_SKB_CB(head)->tail = frag;
	}

	if (fragid == LAST_FRAGMENT) {
		TIPC_SKB_CB(head)->validated = 0;
		if (unlikely(!tipc_msg_validate(&head)))
			goto err;
		*buf = head;
		TIPC_SKB_CB(head)->tail = NULL;
		*headbuf = NULL;
		return 1;
	}
	*buf = NULL;
	return 0;
err:
	kfree_skb(*buf);
	kfree_skb(*headbuf);
	*buf = *headbuf = NULL;
	return 0;
}

 
int tipc_msg_append(struct tipc_msg *_hdr, struct msghdr *m, int dlen,
		    int mss, struct sk_buff_head *txq)
{
	struct sk_buff *skb;
	int accounted, total, curr;
	int mlen, cpy, rem = dlen;
	struct tipc_msg *hdr;

	skb = skb_peek_tail(txq);
	accounted = skb ? msg_blocks(buf_msg(skb)) : 0;
	total = accounted;

	do {
		if (!skb || skb->len >= mss) {
			skb = tipc_buf_acquire(mss, GFP_KERNEL);
			if (unlikely(!skb))
				return -ENOMEM;
			skb_orphan(skb);
			skb_trim(skb, MIN_H_SIZE);
			hdr = buf_msg(skb);
			skb_copy_to_linear_data(skb, _hdr, MIN_H_SIZE);
			msg_set_hdr_sz(hdr, MIN_H_SIZE);
			msg_set_size(hdr, MIN_H_SIZE);
			__skb_queue_tail(txq, skb);
			total += 1;
		}
		hdr = buf_msg(skb);
		curr = msg_blocks(hdr);
		mlen = msg_size(hdr);
		cpy = min_t(size_t, rem, mss - mlen);
		if (cpy != copy_from_iter(skb->data + mlen, cpy, &m->msg_iter))
			return -EFAULT;
		msg_set_size(hdr, mlen + cpy);
		skb_put(skb, cpy);
		rem -= cpy;
		total += msg_blocks(hdr) - curr;
	} while (rem > 0);
	return total - accounted;
}

 
bool tipc_msg_validate(struct sk_buff **_skb)
{
	struct sk_buff *skb = *_skb;
	struct tipc_msg *hdr;
	int msz, hsz;

	 
	if (unlikely(skb->truesize / buf_roundup_len(skb) >= 4)) {
		skb = skb_copy_expand(skb, BUF_HEADROOM, 0, GFP_ATOMIC);
		if (!skb)
			return false;
		kfree_skb(*_skb);
		*_skb = skb;
	}

	if (unlikely(TIPC_SKB_CB(skb)->validated))
		return true;

	if (unlikely(!pskb_may_pull(skb, MIN_H_SIZE)))
		return false;

	hsz = msg_hdr_sz(buf_msg(skb));
	if (unlikely(hsz < MIN_H_SIZE) || (hsz > MAX_H_SIZE))
		return false;
	if (unlikely(!pskb_may_pull(skb, hsz)))
		return false;

	hdr = buf_msg(skb);
	if (unlikely(msg_version(hdr) != TIPC_VERSION))
		return false;

	msz = msg_size(hdr);
	if (unlikely(msz < hsz))
		return false;
	if (unlikely((msz - hsz) > TIPC_MAX_USER_MSG_SIZE))
		return false;
	if (unlikely(skb->len < msz))
		return false;

	TIPC_SKB_CB(skb)->validated = 1;
	return true;
}

 
int tipc_msg_fragment(struct sk_buff *skb, const struct tipc_msg *hdr,
		      int pktmax, struct sk_buff_head *frags)
{
	int pktno, nof_fragms, dsz, dmax, eat;
	struct tipc_msg *_hdr;
	struct sk_buff *_skb;
	u8 *data;

	 
	if (skb_linearize(skb))
		return -ENOMEM;

	data = (u8 *)skb->data;
	dsz = msg_size(buf_msg(skb));
	dmax = pktmax - INT_H_SIZE;
	if (dsz <= dmax || !dmax)
		return -EINVAL;

	nof_fragms = dsz / dmax + 1;
	for (pktno = 1; pktno <= nof_fragms; pktno++) {
		if (pktno < nof_fragms)
			eat = dmax;
		else
			eat = dsz % dmax;
		 
		_skb = tipc_buf_acquire(INT_H_SIZE + eat, GFP_ATOMIC);
		if (!_skb)
			goto error;
		skb_orphan(_skb);
		__skb_queue_tail(frags, _skb);
		 
		skb_copy_to_linear_data(_skb, hdr, INT_H_SIZE);
		skb_copy_to_linear_data_offset(_skb, INT_H_SIZE, data, eat);
		data += eat;
		 
		_hdr = buf_msg(_skb);
		msg_set_fragm_no(_hdr, pktno);
		msg_set_nof_fragms(_hdr, nof_fragms);
		msg_set_size(_hdr, INT_H_SIZE + eat);
	}
	return 0;

error:
	__skb_queue_purge(frags);
	__skb_queue_head_init(frags);
	return -ENOMEM;
}

 
int tipc_msg_build(struct tipc_msg *mhdr, struct msghdr *m, int offset,
		   int dsz, int pktmax, struct sk_buff_head *list)
{
	int mhsz = msg_hdr_sz(mhdr);
	struct tipc_msg pkthdr;
	int msz = mhsz + dsz;
	int pktrem = pktmax;
	struct sk_buff *skb;
	int drem = dsz;
	int pktno = 1;
	char *pktpos;
	int pktsz;
	int rc;

	msg_set_size(mhdr, msz);

	 
	if (likely(msz <= pktmax)) {
		skb = tipc_buf_acquire(msz, GFP_KERNEL);

		 
		if (unlikely(!skb)) {
			if (pktmax != MAX_MSG_SIZE)
				return -ENOMEM;
			rc = tipc_msg_build(mhdr, m, offset, dsz,
					    one_page_mtu, list);
			if (rc != dsz)
				return rc;
			if (tipc_msg_assemble(list))
				return dsz;
			return -ENOMEM;
		}
		skb_orphan(skb);
		__skb_queue_tail(list, skb);
		skb_copy_to_linear_data(skb, mhdr, mhsz);
		pktpos = skb->data + mhsz;
		if (copy_from_iter_full(pktpos, dsz, &m->msg_iter))
			return dsz;
		rc = -EFAULT;
		goto error;
	}

	 
	tipc_msg_init(msg_prevnode(mhdr), &pkthdr, MSG_FRAGMENTER,
		      FIRST_FRAGMENT, INT_H_SIZE, msg_destnode(mhdr));
	msg_set_size(&pkthdr, pktmax);
	msg_set_fragm_no(&pkthdr, pktno);
	msg_set_importance(&pkthdr, msg_importance(mhdr));

	 
	skb = tipc_buf_acquire(pktmax, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;
	skb_orphan(skb);
	__skb_queue_tail(list, skb);
	pktpos = skb->data;
	skb_copy_to_linear_data(skb, &pkthdr, INT_H_SIZE);
	pktpos += INT_H_SIZE;
	pktrem -= INT_H_SIZE;
	skb_copy_to_linear_data_offset(skb, INT_H_SIZE, mhdr, mhsz);
	pktpos += mhsz;
	pktrem -= mhsz;

	do {
		if (drem < pktrem)
			pktrem = drem;

		if (!copy_from_iter_full(pktpos, pktrem, &m->msg_iter)) {
			rc = -EFAULT;
			goto error;
		}
		drem -= pktrem;

		if (!drem)
			break;

		 
		if (drem < (pktmax - INT_H_SIZE))
			pktsz = drem + INT_H_SIZE;
		else
			pktsz = pktmax;
		skb = tipc_buf_acquire(pktsz, GFP_KERNEL);
		if (!skb) {
			rc = -ENOMEM;
			goto error;
		}
		skb_orphan(skb);
		__skb_queue_tail(list, skb);
		msg_set_type(&pkthdr, FRAGMENT);
		msg_set_size(&pkthdr, pktsz);
		msg_set_fragm_no(&pkthdr, ++pktno);
		skb_copy_to_linear_data(skb, &pkthdr, INT_H_SIZE);
		pktpos = skb->data + INT_H_SIZE;
		pktrem = pktsz - INT_H_SIZE;

	} while (1);
	msg_set_type(buf_msg(skb), LAST_FRAGMENT);
	return dsz;
error:
	__skb_queue_purge(list);
	__skb_queue_head_init(list);
	return rc;
}

 
static bool tipc_msg_bundle(struct sk_buff *bskb, struct tipc_msg *msg,
			    u32 max)
{
	struct tipc_msg *bmsg = buf_msg(bskb);
	u32 msz, bsz, offset, pad;

	msz = msg_size(msg);
	bsz = msg_size(bmsg);
	offset = BUF_ALIGN(bsz);
	pad = offset - bsz;

	if (unlikely(skb_tailroom(bskb) < (pad + msz)))
		return false;
	if (unlikely(max < (offset + msz)))
		return false;

	skb_put(bskb, pad + msz);
	skb_copy_to_linear_data_offset(bskb, offset, msg, msz);
	msg_set_size(bmsg, offset + msz);
	msg_set_msgcnt(bmsg, msg_msgcnt(bmsg) + 1);
	return true;
}

 
bool tipc_msg_try_bundle(struct sk_buff *tskb, struct sk_buff **skb, u32 mss,
			 u32 dnode, bool *new_bundle)
{
	struct tipc_msg *msg, *inner, *outer;
	u32 tsz;

	 
	msg = buf_msg(*skb);
	if (msg_user(msg) == MSG_FRAGMENTER)
		return false;
	if (msg_user(msg) == TUNNEL_PROTOCOL)
		return false;
	if (msg_user(msg) == BCAST_PROTOCOL)
		return false;
	if (mss <= INT_H_SIZE + msg_size(msg))
		return false;

	 
	if (unlikely(!tskb))
		return true;

	 
	if (msg_user(buf_msg(tskb)) == MSG_BUNDLER) {
		*new_bundle = false;
		goto bundle;
	}

	 
	tsz = msg_size(buf_msg(tskb));
	if (unlikely(mss < BUF_ALIGN(INT_H_SIZE + tsz) + msg_size(msg)))
		return true;
	if (unlikely(pskb_expand_head(tskb, INT_H_SIZE, mss - tsz - INT_H_SIZE,
				      GFP_ATOMIC)))
		return true;
	inner = buf_msg(tskb);
	skb_push(tskb, INT_H_SIZE);
	outer = buf_msg(tskb);
	tipc_msg_init(msg_prevnode(inner), outer, MSG_BUNDLER, 0, INT_H_SIZE,
		      dnode);
	msg_set_importance(outer, msg_importance(inner));
	msg_set_size(outer, INT_H_SIZE + tsz);
	msg_set_msgcnt(outer, 1);
	*new_bundle = true;

bundle:
	if (likely(tipc_msg_bundle(tskb, msg, mss))) {
		consume_skb(*skb);
		*skb = NULL;
	}
	return true;
}

 
bool tipc_msg_extract(struct sk_buff *skb, struct sk_buff **iskb, int *pos)
{
	struct tipc_msg *hdr, *ihdr;
	int imsz;

	*iskb = NULL;
	if (unlikely(skb_linearize(skb)))
		goto none;

	hdr = buf_msg(skb);
	if (unlikely(*pos > (msg_data_sz(hdr) - MIN_H_SIZE)))
		goto none;

	ihdr = (struct tipc_msg *)(msg_data(hdr) + *pos);
	imsz = msg_size(ihdr);

	if ((*pos + imsz) > msg_data_sz(hdr))
		goto none;

	*iskb = tipc_buf_acquire(imsz, GFP_ATOMIC);
	if (!*iskb)
		goto none;

	skb_copy_to_linear_data(*iskb, ihdr, imsz);
	if (unlikely(!tipc_msg_validate(iskb)))
		goto none;

	*pos += BUF_ALIGN(imsz);
	return true;
none:
	kfree_skb(skb);
	kfree_skb(*iskb);
	*iskb = NULL;
	return false;
}

 
bool tipc_msg_reverse(u32 own_node,  struct sk_buff **skb, int err)
{
	struct sk_buff *_skb = *skb;
	struct tipc_msg *_hdr, *hdr;
	int hlen, dlen;

	if (skb_linearize(_skb))
		goto exit;
	_hdr = buf_msg(_skb);
	dlen = min_t(uint, msg_data_sz(_hdr), MAX_FORWARD_SIZE);
	hlen = msg_hdr_sz(_hdr);

	if (msg_dest_droppable(_hdr))
		goto exit;
	if (msg_errcode(_hdr))
		goto exit;

	 
	if (hlen == SHORT_H_SIZE)
		hlen = BASIC_H_SIZE;

	 
	if (msg_is_syn(_hdr) && err == TIPC_ERR_OVERLOAD)
		dlen = 0;

	 
	*skb = tipc_buf_acquire(hlen + dlen, GFP_ATOMIC);
	if (!*skb)
		goto exit;
	memcpy((*skb)->data, _skb->data, msg_hdr_sz(_hdr));
	memcpy((*skb)->data + hlen, msg_data(_hdr), dlen);

	 
	hdr = buf_msg(*skb);
	msg_set_hdr_sz(hdr, hlen);
	msg_set_errcode(hdr, err);
	msg_set_non_seq(hdr, 0);
	msg_set_origport(hdr, msg_destport(_hdr));
	msg_set_destport(hdr, msg_origport(_hdr));
	msg_set_destnode(hdr, msg_prevnode(_hdr));
	msg_set_prevnode(hdr, own_node);
	msg_set_orignode(hdr, own_node);
	msg_set_size(hdr, hlen + dlen);
	skb_orphan(_skb);
	kfree_skb(_skb);
	return true;
exit:
	kfree_skb(_skb);
	*skb = NULL;
	return false;
}

bool tipc_msg_skb_clone(struct sk_buff_head *msg, struct sk_buff_head *cpy)
{
	struct sk_buff *skb, *_skb;

	skb_queue_walk(msg, skb) {
		_skb = skb_clone(skb, GFP_ATOMIC);
		if (!_skb) {
			__skb_queue_purge(cpy);
			pr_err_ratelimited("Failed to clone buffer chain\n");
			return false;
		}
		__skb_queue_tail(cpy, _skb);
	}
	return true;
}

 
bool tipc_msg_lookup_dest(struct net *net, struct sk_buff *skb, int *err)
{
	struct tipc_msg *msg = buf_msg(skb);
	u32 scope = msg_lookup_scope(msg);
	u32 self = tipc_own_addr(net);
	u32 inst = msg_nameinst(msg);
	struct tipc_socket_addr sk;
	struct tipc_uaddr ua;

	if (!msg_isdata(msg))
		return false;
	if (!msg_named(msg))
		return false;
	if (msg_errcode(msg))
		return false;
	*err = TIPC_ERR_NO_NAME;
	if (skb_linearize(skb))
		return false;
	msg = buf_msg(skb);
	if (msg_reroute_cnt(msg))
		return false;
	tipc_uaddr(&ua, TIPC_SERVICE_RANGE, scope,
		   msg_nametype(msg), inst, inst);
	sk.node = tipc_scope2node(net, scope);
	if (!tipc_nametbl_lookup_anycast(net, &ua, &sk))
		return false;
	msg_incr_reroute_cnt(msg);
	if (sk.node != self)
		msg_set_prevnode(msg, self);
	msg_set_destnode(msg, sk.node);
	msg_set_destport(msg, sk.ref);
	*err = TIPC_OK;

	return true;
}

 
bool tipc_msg_assemble(struct sk_buff_head *list)
{
	struct sk_buff *skb, *tmp = NULL;

	if (skb_queue_len(list) == 1)
		return true;

	while ((skb = __skb_dequeue(list))) {
		skb->next = NULL;
		if (tipc_buf_append(&tmp, &skb)) {
			__skb_queue_tail(list, skb);
			return true;
		}
		if (!tmp)
			break;
	}
	__skb_queue_purge(list);
	__skb_queue_head_init(list);
	pr_warn("Failed do assemble buffer\n");
	return false;
}

 
bool tipc_msg_reassemble(struct sk_buff_head *list, struct sk_buff_head *rcvq)
{
	struct sk_buff *skb, *_skb;
	struct sk_buff *frag = NULL;
	struct sk_buff *head = NULL;
	int hdr_len;

	 
	if (skb_queue_len(list) == 1) {
		skb = skb_peek(list);
		hdr_len = skb_headroom(skb) + msg_hdr_sz(buf_msg(skb));
		_skb = __pskb_copy(skb, hdr_len, GFP_ATOMIC);
		if (!_skb)
			return false;
		__skb_queue_tail(rcvq, _skb);
		return true;
	}

	 
	skb_queue_walk(list, skb) {
		frag = skb_clone(skb, GFP_ATOMIC);
		if (!frag)
			goto error;
		frag->next = NULL;
		if (tipc_buf_append(&head, &frag))
			break;
		if (!head)
			goto error;
	}
	__skb_queue_tail(rcvq, frag);
	return true;
error:
	pr_warn("Failed do clone local mcast rcv buffer\n");
	kfree_skb(head);
	return false;
}

bool tipc_msg_pskb_copy(u32 dst, struct sk_buff_head *msg,
			struct sk_buff_head *cpy)
{
	struct sk_buff *skb, *_skb;

	skb_queue_walk(msg, skb) {
		_skb = pskb_copy(skb, GFP_ATOMIC);
		if (!_skb) {
			__skb_queue_purge(cpy);
			return false;
		}
		msg_set_destnode(buf_msg(_skb), dst);
		__skb_queue_tail(cpy, _skb);
	}
	return true;
}

 
bool __tipc_skb_queue_sorted(struct sk_buff_head *list, u16 seqno,
			     struct sk_buff *skb)
{
	struct sk_buff *_skb, *tmp;

	if (skb_queue_empty(list) || less(seqno, buf_seqno(skb_peek(list)))) {
		__skb_queue_head(list, skb);
		return true;
	}

	if (more(seqno, buf_seqno(skb_peek_tail(list)))) {
		__skb_queue_tail(list, skb);
		return true;
	}

	skb_queue_walk_safe(list, _skb, tmp) {
		if (more(seqno, buf_seqno(_skb)))
			continue;
		if (seqno == buf_seqno(_skb))
			break;
		__skb_queue_before(list, _skb, skb);
		return true;
	}
	kfree_skb(skb);
	return false;
}

void tipc_skb_reject(struct net *net, int err, struct sk_buff *skb,
		     struct sk_buff_head *xmitq)
{
	if (tipc_msg_reverse(tipc_own_addr(net), &skb, err))
		__skb_queue_tail(xmitq, skb);
}
