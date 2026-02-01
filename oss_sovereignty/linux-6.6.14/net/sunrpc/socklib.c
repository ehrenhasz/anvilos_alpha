
 

#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/gfp.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/pagemap.h>
#include <linux/udp.h>
#include <linux/sunrpc/msg_prot.h>
#include <linux/sunrpc/sched.h>
#include <linux/sunrpc/xdr.h>
#include <linux/export.h>

#include "socklib.h"

 
struct xdr_skb_reader {
	struct sk_buff	*skb;
	unsigned int	offset;
	size_t		count;
	__wsum		csum;
};

typedef size_t (*xdr_skb_read_actor)(struct xdr_skb_reader *desc, void *to,
				     size_t len);

 
static size_t
xdr_skb_read_bits(struct xdr_skb_reader *desc, void *to, size_t len)
{
	if (len > desc->count)
		len = desc->count;
	if (unlikely(skb_copy_bits(desc->skb, desc->offset, to, len)))
		return 0;
	desc->count -= len;
	desc->offset += len;
	return len;
}

 
static size_t xdr_skb_read_and_csum_bits(struct xdr_skb_reader *desc, void *to, size_t len)
{
	unsigned int pos;
	__wsum csum2;

	if (len > desc->count)
		len = desc->count;
	pos = desc->offset;
	csum2 = skb_copy_and_csum_bits(desc->skb, pos, to, len);
	desc->csum = csum_block_add(desc->csum, csum2, pos);
	desc->count -= len;
	desc->offset += len;
	return len;
}

 
static ssize_t
xdr_partial_copy_from_skb(struct xdr_buf *xdr, unsigned int base, struct xdr_skb_reader *desc, xdr_skb_read_actor copy_actor)
{
	struct page	**ppage = xdr->pages;
	unsigned int	len, pglen = xdr->page_len;
	ssize_t		copied = 0;
	size_t		ret;

	len = xdr->head[0].iov_len;
	if (base < len) {
		len -= base;
		ret = copy_actor(desc, (char *)xdr->head[0].iov_base + base, len);
		copied += ret;
		if (ret != len || !desc->count)
			goto out;
		base = 0;
	} else
		base -= len;

	if (unlikely(pglen == 0))
		goto copy_tail;
	if (unlikely(base >= pglen)) {
		base -= pglen;
		goto copy_tail;
	}
	if (base || xdr->page_base) {
		pglen -= base;
		base += xdr->page_base;
		ppage += base >> PAGE_SHIFT;
		base &= ~PAGE_MASK;
	}
	do {
		char *kaddr;

		 
		if ((xdr->flags & XDRBUF_SPARSE_PAGES) && *ppage == NULL) {
			*ppage = alloc_page(GFP_NOWAIT | __GFP_NOWARN);
			if (unlikely(*ppage == NULL)) {
				if (copied == 0)
					copied = -ENOMEM;
				goto out;
			}
		}

		len = PAGE_SIZE;
		kaddr = kmap_atomic(*ppage);
		if (base) {
			len -= base;
			if (pglen < len)
				len = pglen;
			ret = copy_actor(desc, kaddr + base, len);
			base = 0;
		} else {
			if (pglen < len)
				len = pglen;
			ret = copy_actor(desc, kaddr, len);
		}
		flush_dcache_page(*ppage);
		kunmap_atomic(kaddr);
		copied += ret;
		if (ret != len || !desc->count)
			goto out;
		ppage++;
	} while ((pglen -= len) != 0);
copy_tail:
	len = xdr->tail[0].iov_len;
	if (base < len)
		copied += copy_actor(desc, (char *)xdr->tail[0].iov_base + base, len - base);
out:
	return copied;
}

 
int csum_partial_copy_to_xdr(struct xdr_buf *xdr, struct sk_buff *skb)
{
	struct xdr_skb_reader	desc;

	desc.skb = skb;
	desc.offset = 0;
	desc.count = skb->len - desc.offset;

	if (skb_csum_unnecessary(skb))
		goto no_checksum;

	desc.csum = csum_partial(skb->data, desc.offset, skb->csum);
	if (xdr_partial_copy_from_skb(xdr, 0, &desc, xdr_skb_read_and_csum_bits) < 0)
		return -1;
	if (desc.offset != skb->len) {
		__wsum csum2;
		csum2 = skb_checksum(skb, desc.offset, skb->len - desc.offset, 0);
		desc.csum = csum_block_add(desc.csum, csum2, desc.offset);
	}
	if (desc.count)
		return -1;
	if (csum_fold(desc.csum))
		return -1;
	if (unlikely(skb->ip_summed == CHECKSUM_COMPLETE) &&
	    !skb->csum_complete_sw)
		netdev_rx_csum_fault(skb->dev, skb);
	return 0;
no_checksum:
	if (xdr_partial_copy_from_skb(xdr, 0, &desc, xdr_skb_read_bits) < 0)
		return -1;
	if (desc.count)
		return -1;
	return 0;
}
EXPORT_SYMBOL_GPL(csum_partial_copy_to_xdr);

static inline int xprt_sendmsg(struct socket *sock, struct msghdr *msg,
			       size_t seek)
{
	if (seek)
		iov_iter_advance(&msg->msg_iter, seek);
	return sock_sendmsg(sock, msg);
}

static int xprt_send_kvec(struct socket *sock, struct msghdr *msg,
			  struct kvec *vec, size_t seek)
{
	iov_iter_kvec(&msg->msg_iter, ITER_SOURCE, vec, 1, vec->iov_len);
	return xprt_sendmsg(sock, msg, seek);
}

static int xprt_send_pagedata(struct socket *sock, struct msghdr *msg,
			      struct xdr_buf *xdr, size_t base)
{
	iov_iter_bvec(&msg->msg_iter, ITER_SOURCE, xdr->bvec, xdr_buf_pagecount(xdr),
		      xdr->page_len + xdr->page_base);
	return xprt_sendmsg(sock, msg, base + xdr->page_base);
}

 
static int xprt_send_rm_and_kvec(struct socket *sock, struct msghdr *msg,
				 rpc_fraghdr marker, struct kvec *vec,
				 size_t base)
{
	struct kvec iov[2] = {
		[0] = {
			.iov_base	= &marker,
			.iov_len	= sizeof(marker)
		},
		[1] = *vec,
	};
	size_t len = iov[0].iov_len + iov[1].iov_len;

	iov_iter_kvec(&msg->msg_iter, ITER_SOURCE, iov, 2, len);
	return xprt_sendmsg(sock, msg, base);
}

 
int xprt_sock_sendmsg(struct socket *sock, struct msghdr *msg,
		      struct xdr_buf *xdr, unsigned int base,
		      rpc_fraghdr marker, unsigned int *sent_p)
{
	unsigned int rmsize = marker ? sizeof(marker) : 0;
	unsigned int remainder = rmsize + xdr->len - base;
	unsigned int want;
	int err = 0;

	*sent_p = 0;

	if (unlikely(!sock))
		return -ENOTSOCK;

	msg->msg_flags |= MSG_MORE;
	want = xdr->head[0].iov_len + rmsize;
	if (base < want) {
		unsigned int len = want - base;

		remainder -= len;
		if (remainder == 0)
			msg->msg_flags &= ~MSG_MORE;
		if (rmsize)
			err = xprt_send_rm_and_kvec(sock, msg, marker,
						    &xdr->head[0], base);
		else
			err = xprt_send_kvec(sock, msg, &xdr->head[0], base);
		if (remainder == 0 || err != len)
			goto out;
		*sent_p += err;
		base = 0;
	} else {
		base -= want;
	}

	if (base < xdr->page_len) {
		unsigned int len = xdr->page_len - base;

		remainder -= len;
		if (remainder == 0)
			msg->msg_flags &= ~MSG_MORE;
		err = xprt_send_pagedata(sock, msg, xdr, base);
		if (remainder == 0 || err != len)
			goto out;
		*sent_p += err;
		base = 0;
	} else {
		base -= xdr->page_len;
	}

	if (base >= xdr->tail[0].iov_len)
		return 0;
	msg->msg_flags &= ~MSG_MORE;
	err = xprt_send_kvec(sock, msg, &xdr->tail[0], base);
out:
	if (err > 0) {
		*sent_p += err;
		err = 0;
	}
	return err;
}
