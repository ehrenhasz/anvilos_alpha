
 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/poll.h>
#include <linux/highmem.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/uio.h>
#include <linux/indirect_call_wrapper.h>

#include <net/protocol.h>
#include <linux/skbuff.h>

#include <net/checksum.h>
#include <net/sock.h>
#include <net/tcp_states.h>
#include <trace/events/skb.h>
#include <net/busy_poll.h>

 
static inline int connection_based(struct sock *sk)
{
	return sk->sk_type == SOCK_SEQPACKET || sk->sk_type == SOCK_STREAM;
}

static int receiver_wake_function(wait_queue_entry_t *wait, unsigned int mode, int sync,
				  void *key)
{
	 
	if (key && !(key_to_poll(key) & (EPOLLIN | EPOLLERR)))
		return 0;
	return autoremove_wake_function(wait, mode, sync, key);
}
 
int __skb_wait_for_more_packets(struct sock *sk, struct sk_buff_head *queue,
				int *err, long *timeo_p,
				const struct sk_buff *skb)
{
	int error;
	DEFINE_WAIT_FUNC(wait, receiver_wake_function);

	prepare_to_wait_exclusive(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);

	 
	error = sock_error(sk);
	if (error)
		goto out_err;

	if (READ_ONCE(queue->prev) != skb)
		goto out;

	 
	if (sk->sk_shutdown & RCV_SHUTDOWN)
		goto out_noerr;

	 
	error = -ENOTCONN;
	if (connection_based(sk) &&
	    !(sk->sk_state == TCP_ESTABLISHED || sk->sk_state == TCP_LISTEN))
		goto out_err;

	 
	if (signal_pending(current))
		goto interrupted;

	error = 0;
	*timeo_p = schedule_timeout(*timeo_p);
out:
	finish_wait(sk_sleep(sk), &wait);
	return error;
interrupted:
	error = sock_intr_errno(*timeo_p);
out_err:
	*err = error;
	goto out;
out_noerr:
	*err = 0;
	error = 1;
	goto out;
}
EXPORT_SYMBOL(__skb_wait_for_more_packets);

static struct sk_buff *skb_set_peeked(struct sk_buff *skb)
{
	struct sk_buff *nskb;

	if (skb->peeked)
		return skb;

	 
	if (!skb_shared(skb))
		goto done;

	nskb = skb_clone(skb, GFP_ATOMIC);
	if (!nskb)
		return ERR_PTR(-ENOMEM);

	skb->prev->next = nskb;
	skb->next->prev = nskb;
	nskb->prev = skb->prev;
	nskb->next = skb->next;

	consume_skb(skb);
	skb = nskb;

done:
	skb->peeked = 1;

	return skb;
}

struct sk_buff *__skb_try_recv_from_queue(struct sock *sk,
					  struct sk_buff_head *queue,
					  unsigned int flags,
					  int *off, int *err,
					  struct sk_buff **last)
{
	bool peek_at_off = false;
	struct sk_buff *skb;
	int _off = 0;

	if (unlikely(flags & MSG_PEEK && *off >= 0)) {
		peek_at_off = true;
		_off = *off;
	}

	*last = queue->prev;
	skb_queue_walk(queue, skb) {
		if (flags & MSG_PEEK) {
			if (peek_at_off && _off >= skb->len &&
			    (_off || skb->peeked)) {
				_off -= skb->len;
				continue;
			}
			if (!skb->len) {
				skb = skb_set_peeked(skb);
				if (IS_ERR(skb)) {
					*err = PTR_ERR(skb);
					return NULL;
				}
			}
			refcount_inc(&skb->users);
		} else {
			__skb_unlink(skb, queue);
		}
		*off = _off;
		return skb;
	}
	return NULL;
}

 
struct sk_buff *__skb_try_recv_datagram(struct sock *sk,
					struct sk_buff_head *queue,
					unsigned int flags, int *off, int *err,
					struct sk_buff **last)
{
	struct sk_buff *skb;
	unsigned long cpu_flags;
	 
	int error = sock_error(sk);

	if (error)
		goto no_packet;

	do {
		 
		spin_lock_irqsave(&queue->lock, cpu_flags);
		skb = __skb_try_recv_from_queue(sk, queue, flags, off, &error,
						last);
		spin_unlock_irqrestore(&queue->lock, cpu_flags);
		if (error)
			goto no_packet;
		if (skb)
			return skb;

		if (!sk_can_busy_loop(sk))
			break;

		sk_busy_loop(sk, flags & MSG_DONTWAIT);
	} while (READ_ONCE(queue->prev) != *last);

	error = -EAGAIN;

no_packet:
	*err = error;
	return NULL;
}
EXPORT_SYMBOL(__skb_try_recv_datagram);

struct sk_buff *__skb_recv_datagram(struct sock *sk,
				    struct sk_buff_head *sk_queue,
				    unsigned int flags, int *off, int *err)
{
	struct sk_buff *skb, *last;
	long timeo;

	timeo = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);

	do {
		skb = __skb_try_recv_datagram(sk, sk_queue, flags, off, err,
					      &last);
		if (skb)
			return skb;

		if (*err != -EAGAIN)
			break;
	} while (timeo &&
		 !__skb_wait_for_more_packets(sk, sk_queue, err,
					      &timeo, last));

	return NULL;
}
EXPORT_SYMBOL(__skb_recv_datagram);

struct sk_buff *skb_recv_datagram(struct sock *sk, unsigned int flags,
				  int *err)
{
	int off = 0;

	return __skb_recv_datagram(sk, &sk->sk_receive_queue, flags,
				   &off, err);
}
EXPORT_SYMBOL(skb_recv_datagram);

void skb_free_datagram(struct sock *sk, struct sk_buff *skb)
{
	consume_skb(skb);
}
EXPORT_SYMBOL(skb_free_datagram);

void __skb_free_datagram_locked(struct sock *sk, struct sk_buff *skb, int len)
{
	bool slow;

	if (!skb_unref(skb)) {
		sk_peek_offset_bwd(sk, len);
		return;
	}

	slow = lock_sock_fast(sk);
	sk_peek_offset_bwd(sk, len);
	skb_orphan(skb);
	unlock_sock_fast(sk, slow);

	 
	__kfree_skb(skb);
}
EXPORT_SYMBOL(__skb_free_datagram_locked);

int __sk_queue_drop_skb(struct sock *sk, struct sk_buff_head *sk_queue,
			struct sk_buff *skb, unsigned int flags,
			void (*destructor)(struct sock *sk,
					   struct sk_buff *skb))
{
	int err = 0;

	if (flags & MSG_PEEK) {
		err = -ENOENT;
		spin_lock_bh(&sk_queue->lock);
		if (skb->next) {
			__skb_unlink(skb, sk_queue);
			refcount_dec(&skb->users);
			if (destructor)
				destructor(sk, skb);
			err = 0;
		}
		spin_unlock_bh(&sk_queue->lock);
	}

	atomic_inc(&sk->sk_drops);
	return err;
}
EXPORT_SYMBOL(__sk_queue_drop_skb);

 

int skb_kill_datagram(struct sock *sk, struct sk_buff *skb, unsigned int flags)
{
	int err = __sk_queue_drop_skb(sk, &sk->sk_receive_queue, skb, flags,
				      NULL);

	kfree_skb(skb);
	return err;
}
EXPORT_SYMBOL(skb_kill_datagram);

INDIRECT_CALLABLE_DECLARE(static size_t simple_copy_to_iter(const void *addr,
						size_t bytes,
						void *data __always_unused,
						struct iov_iter *i));

static int __skb_datagram_iter(const struct sk_buff *skb, int offset,
			       struct iov_iter *to, int len, bool fault_short,
			       size_t (*cb)(const void *, size_t, void *,
					    struct iov_iter *), void *data)
{
	int start = skb_headlen(skb);
	int i, copy = start - offset, start_off = offset, n;
	struct sk_buff *frag_iter;

	 
	if (copy > 0) {
		if (copy > len)
			copy = len;
		n = INDIRECT_CALL_1(cb, simple_copy_to_iter,
				    skb->data + offset, copy, data, to);
		offset += n;
		if (n != copy)
			goto short_copy;
		if ((len -= copy) == 0)
			return 0;
	}

	 
	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		int end;
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		WARN_ON(start > offset + len);

		end = start + skb_frag_size(frag);
		if ((copy = end - offset) > 0) {
			struct page *page = skb_frag_page(frag);
			u8 *vaddr = kmap(page);

			if (copy > len)
				copy = len;
			n = INDIRECT_CALL_1(cb, simple_copy_to_iter,
					vaddr + skb_frag_off(frag) + offset - start,
					copy, data, to);
			kunmap(page);
			offset += n;
			if (n != copy)
				goto short_copy;
			if (!(len -= copy))
				return 0;
		}
		start = end;
	}

	skb_walk_frags(skb, frag_iter) {
		int end;

		WARN_ON(start > offset + len);

		end = start + frag_iter->len;
		if ((copy = end - offset) > 0) {
			if (copy > len)
				copy = len;
			if (__skb_datagram_iter(frag_iter, offset - start,
						to, copy, fault_short, cb, data))
				goto fault;
			if ((len -= copy) == 0)
				return 0;
			offset += copy;
		}
		start = end;
	}
	if (!len)
		return 0;

	 

fault:
	iov_iter_revert(to, offset - start_off);
	return -EFAULT;

short_copy:
	if (fault_short || iov_iter_count(to))
		goto fault;

	return 0;
}

 
int skb_copy_and_hash_datagram_iter(const struct sk_buff *skb, int offset,
			   struct iov_iter *to, int len,
			   struct ahash_request *hash)
{
	return __skb_datagram_iter(skb, offset, to, len, true,
			hash_and_copy_to_iter, hash);
}
EXPORT_SYMBOL(skb_copy_and_hash_datagram_iter);

static size_t simple_copy_to_iter(const void *addr, size_t bytes,
		void *data __always_unused, struct iov_iter *i)
{
	return copy_to_iter(addr, bytes, i);
}

 
int skb_copy_datagram_iter(const struct sk_buff *skb, int offset,
			   struct iov_iter *to, int len)
{
	trace_skb_copy_datagram_iovec(skb, len);
	return __skb_datagram_iter(skb, offset, to, len, false,
			simple_copy_to_iter, NULL);
}
EXPORT_SYMBOL(skb_copy_datagram_iter);

 
int skb_copy_datagram_from_iter(struct sk_buff *skb, int offset,
				 struct iov_iter *from,
				 int len)
{
	int start = skb_headlen(skb);
	int i, copy = start - offset;
	struct sk_buff *frag_iter;

	 
	if (copy > 0) {
		if (copy > len)
			copy = len;
		if (copy_from_iter(skb->data + offset, copy, from) != copy)
			goto fault;
		if ((len -= copy) == 0)
			return 0;
		offset += copy;
	}

	 
	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		int end;
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		WARN_ON(start > offset + len);

		end = start + skb_frag_size(frag);
		if ((copy = end - offset) > 0) {
			size_t copied;

			if (copy > len)
				copy = len;
			copied = copy_page_from_iter(skb_frag_page(frag),
					  skb_frag_off(frag) + offset - start,
					  copy, from);
			if (copied != copy)
				goto fault;

			if (!(len -= copy))
				return 0;
			offset += copy;
		}
		start = end;
	}

	skb_walk_frags(skb, frag_iter) {
		int end;

		WARN_ON(start > offset + len);

		end = start + frag_iter->len;
		if ((copy = end - offset) > 0) {
			if (copy > len)
				copy = len;
			if (skb_copy_datagram_from_iter(frag_iter,
							offset - start,
							from, copy))
				goto fault;
			if ((len -= copy) == 0)
				return 0;
			offset += copy;
		}
		start = end;
	}
	if (!len)
		return 0;

fault:
	return -EFAULT;
}
EXPORT_SYMBOL(skb_copy_datagram_from_iter);

int __zerocopy_sg_from_iter(struct msghdr *msg, struct sock *sk,
			    struct sk_buff *skb, struct iov_iter *from,
			    size_t length)
{
	int frag;

	if (msg && msg->msg_ubuf && msg->sg_from_iter)
		return msg->sg_from_iter(sk, skb, from, length);

	frag = skb_shinfo(skb)->nr_frags;

	while (length && iov_iter_count(from)) {
		struct page *head, *last_head = NULL;
		struct page *pages[MAX_SKB_FRAGS];
		int refs, order, n = 0;
		size_t start;
		ssize_t copied;
		unsigned long truesize;

		if (frag == MAX_SKB_FRAGS)
			return -EMSGSIZE;

		copied = iov_iter_get_pages2(from, pages, length,
					    MAX_SKB_FRAGS - frag, &start);
		if (copied < 0)
			return -EFAULT;

		length -= copied;

		truesize = PAGE_ALIGN(copied + start);
		skb->data_len += copied;
		skb->len += copied;
		skb->truesize += truesize;
		if (sk && sk->sk_type == SOCK_STREAM) {
			sk_wmem_queued_add(sk, truesize);
			if (!skb_zcopy_pure(skb))
				sk_mem_charge(sk, truesize);
		} else {
			refcount_add(truesize, &skb->sk->sk_wmem_alloc);
		}

		head = compound_head(pages[n]);
		order = compound_order(head);

		for (refs = 0; copied != 0; start = 0) {
			int size = min_t(int, copied, PAGE_SIZE - start);

			if (pages[n] - head > (1UL << order) - 1) {
				head = compound_head(pages[n]);
				order = compound_order(head);
			}

			start += (pages[n] - head) << PAGE_SHIFT;
			copied -= size;
			n++;
			if (frag) {
				skb_frag_t *last = &skb_shinfo(skb)->frags[frag - 1];

				if (head == skb_frag_page(last) &&
				    start == skb_frag_off(last) + skb_frag_size(last)) {
					skb_frag_size_add(last, size);
					 
					last_head = head;
					refs++;
					continue;
				}
			}
			if (refs) {
				page_ref_sub(last_head, refs);
				refs = 0;
			}
			skb_fill_page_desc_noacc(skb, frag++, head, start, size);
		}
		if (refs)
			page_ref_sub(last_head, refs);
	}
	return 0;
}
EXPORT_SYMBOL(__zerocopy_sg_from_iter);

 
int zerocopy_sg_from_iter(struct sk_buff *skb, struct iov_iter *from)
{
	int copy = min_t(int, skb_headlen(skb), iov_iter_count(from));

	 
	if (skb_copy_datagram_from_iter(skb, 0, from, copy))
		return -EFAULT;

	return __zerocopy_sg_from_iter(NULL, NULL, skb, from, ~0U);
}
EXPORT_SYMBOL(zerocopy_sg_from_iter);

 
static int skb_copy_and_csum_datagram(const struct sk_buff *skb, int offset,
				      struct iov_iter *to, int len,
				      __wsum *csump)
{
	struct csum_state csdata = { .csum = *csump };
	int ret;

	ret = __skb_datagram_iter(skb, offset, to, len, true,
				  csum_and_copy_to_iter, &csdata);
	if (ret)
		return ret;

	*csump = csdata.csum;
	return 0;
}

 
int skb_copy_and_csum_datagram_msg(struct sk_buff *skb,
				   int hlen, struct msghdr *msg)
{
	__wsum csum;
	int chunk = skb->len - hlen;

	if (!chunk)
		return 0;

	if (msg_data_left(msg) < chunk) {
		if (__skb_checksum_complete(skb))
			return -EINVAL;
		if (skb_copy_datagram_msg(skb, hlen, msg, chunk))
			goto fault;
	} else {
		csum = csum_partial(skb->data, hlen, skb->csum);
		if (skb_copy_and_csum_datagram(skb, hlen, &msg->msg_iter,
					       chunk, &csum))
			goto fault;

		if (csum_fold(csum)) {
			iov_iter_revert(&msg->msg_iter, chunk);
			return -EINVAL;
		}

		if (unlikely(skb->ip_summed == CHECKSUM_COMPLETE) &&
		    !skb->csum_complete_sw)
			netdev_rx_csum_fault(NULL, skb);
	}
	return 0;
fault:
	return -EFAULT;
}
EXPORT_SYMBOL(skb_copy_and_csum_datagram_msg);

 
__poll_t datagram_poll(struct file *file, struct socket *sock,
			   poll_table *wait)
{
	struct sock *sk = sock->sk;
	__poll_t mask;
	u8 shutdown;

	sock_poll_wait(file, sock, wait);
	mask = 0;

	 
	if (READ_ONCE(sk->sk_err) ||
	    !skb_queue_empty_lockless(&sk->sk_error_queue))
		mask |= EPOLLERR |
			(sock_flag(sk, SOCK_SELECT_ERR_QUEUE) ? EPOLLPRI : 0);

	shutdown = READ_ONCE(sk->sk_shutdown);
	if (shutdown & RCV_SHUTDOWN)
		mask |= EPOLLRDHUP | EPOLLIN | EPOLLRDNORM;
	if (shutdown == SHUTDOWN_MASK)
		mask |= EPOLLHUP;

	 
	if (!skb_queue_empty_lockless(&sk->sk_receive_queue))
		mask |= EPOLLIN | EPOLLRDNORM;

	 
	if (connection_based(sk)) {
		int state = READ_ONCE(sk->sk_state);

		if (state == TCP_CLOSE)
			mask |= EPOLLHUP;
		 
		if (state == TCP_SYN_SENT)
			return mask;
	}

	 
	if (sock_writeable(sk))
		mask |= EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND;
	else
		sk_set_bit(SOCKWQ_ASYNC_NOSPACE, sk);

	return mask;
}
EXPORT_SYMBOL(datagram_poll);
