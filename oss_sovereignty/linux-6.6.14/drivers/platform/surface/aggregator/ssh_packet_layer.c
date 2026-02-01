
 

#include <asm/unaligned.h>
#include <linux/atomic.h>
#include <linux/error-injection.h>
#include <linux/jiffies.h>
#include <linux/kfifo.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/serdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include <linux/surface_aggregator/serial_hub.h>

#include "ssh_msgb.h"
#include "ssh_packet_layer.h"
#include "ssh_parser.h"

#include "trace.h"

 

 
#define SSH_PTL_MAX_PACKET_TRIES		3

 
#define SSH_PTL_TX_TIMEOUT			HZ

 
#define SSH_PTL_PACKET_TIMEOUT			ms_to_ktime(1000)

 
#define SSH_PTL_PACKET_TIMEOUT_RESOLUTION	ms_to_ktime(max(2000 / HZ, 50))

 
#define SSH_PTL_MAX_PENDING			1

 
#define SSH_PTL_RX_BUF_LEN			4096

 
#define SSH_PTL_RX_FIFO_LEN			4096

#ifdef CONFIG_SURFACE_AGGREGATOR_ERROR_INJECTION

 
static noinline bool ssh_ptl_should_drop_ack_packet(void)
{
	return false;
}
ALLOW_ERROR_INJECTION(ssh_ptl_should_drop_ack_packet, TRUE);

 
static noinline bool ssh_ptl_should_drop_nak_packet(void)
{
	return false;
}
ALLOW_ERROR_INJECTION(ssh_ptl_should_drop_nak_packet, TRUE);

 
static noinline bool ssh_ptl_should_drop_dsq_packet(void)
{
	return false;
}
ALLOW_ERROR_INJECTION(ssh_ptl_should_drop_dsq_packet, TRUE);

 
static noinline int ssh_ptl_should_fail_write(void)
{
	return 0;
}
ALLOW_ERROR_INJECTION(ssh_ptl_should_fail_write, ERRNO);

 
static noinline bool ssh_ptl_should_corrupt_tx_data(void)
{
	return false;
}
ALLOW_ERROR_INJECTION(ssh_ptl_should_corrupt_tx_data, TRUE);

 
static noinline bool ssh_ptl_should_corrupt_rx_syn(void)
{
	return false;
}
ALLOW_ERROR_INJECTION(ssh_ptl_should_corrupt_rx_syn, TRUE);

 
static noinline bool ssh_ptl_should_corrupt_rx_data(void)
{
	return false;
}
ALLOW_ERROR_INJECTION(ssh_ptl_should_corrupt_rx_data, TRUE);

static bool __ssh_ptl_should_drop_ack_packet(struct ssh_packet *packet)
{
	if (likely(!ssh_ptl_should_drop_ack_packet()))
		return false;

	trace_ssam_ei_tx_drop_ack_packet(packet);
	ptl_info(packet->ptl, "packet error injection: dropping ACK packet %p\n",
		 packet);

	return true;
}

static bool __ssh_ptl_should_drop_nak_packet(struct ssh_packet *packet)
{
	if (likely(!ssh_ptl_should_drop_nak_packet()))
		return false;

	trace_ssam_ei_tx_drop_nak_packet(packet);
	ptl_info(packet->ptl, "packet error injection: dropping NAK packet %p\n",
		 packet);

	return true;
}

static bool __ssh_ptl_should_drop_dsq_packet(struct ssh_packet *packet)
{
	if (likely(!ssh_ptl_should_drop_dsq_packet()))
		return false;

	trace_ssam_ei_tx_drop_dsq_packet(packet);
	ptl_info(packet->ptl,
		 "packet error injection: dropping sequenced data packet %p\n",
		 packet);

	return true;
}

static bool ssh_ptl_should_drop_packet(struct ssh_packet *packet)
{
	 
	if (!packet->data.ptr || !packet->data.len)
		return false;

	switch (packet->data.ptr[SSH_MSGOFFSET_FRAME(type)]) {
	case SSH_FRAME_TYPE_ACK:
		return __ssh_ptl_should_drop_ack_packet(packet);

	case SSH_FRAME_TYPE_NAK:
		return __ssh_ptl_should_drop_nak_packet(packet);

	case SSH_FRAME_TYPE_DATA_SEQ:
		return __ssh_ptl_should_drop_dsq_packet(packet);

	default:
		return false;
	}
}

static int ssh_ptl_write_buf(struct ssh_ptl *ptl, struct ssh_packet *packet,
			     const unsigned char *buf, size_t count)
{
	int status;

	status = ssh_ptl_should_fail_write();
	if (unlikely(status)) {
		trace_ssam_ei_tx_fail_write(packet, status);
		ptl_info(packet->ptl,
			 "packet error injection: simulating transmit error %d, packet %p\n",
			 status, packet);

		return status;
	}

	return serdev_device_write_buf(ptl->serdev, buf, count);
}

static void ssh_ptl_tx_inject_invalid_data(struct ssh_packet *packet)
{
	 
	if (!packet->data.ptr || !packet->data.len)
		return;

	 
	if (packet->data.ptr[SSH_MSGOFFSET_FRAME(type)] != SSH_FRAME_TYPE_DATA_SEQ)
		return;

	if (likely(!ssh_ptl_should_corrupt_tx_data()))
		return;

	trace_ssam_ei_tx_corrupt_data(packet);
	ptl_info(packet->ptl,
		 "packet error injection: simulating invalid transmit data on packet %p\n",
		 packet);

	 
	memset(packet->data.ptr, 0xb3, packet->data.len);
}

static void ssh_ptl_rx_inject_invalid_syn(struct ssh_ptl *ptl,
					  struct ssam_span *data)
{
	struct ssam_span frame;

	 
	if (!sshp_find_syn(data, &frame))
		return;

	if (likely(!ssh_ptl_should_corrupt_rx_syn()))
		return;

	trace_ssam_ei_rx_corrupt_syn(data->len);

	data->ptr[1] = 0xb3;	 
}

static void ssh_ptl_rx_inject_invalid_data(struct ssh_ptl *ptl,
					   struct ssam_span *frame)
{
	size_t payload_len, message_len;
	struct ssh_frame *sshf;

	 
	if (frame->len < SSH_MESSAGE_LENGTH(0))
		return;

	 
	payload_len = get_unaligned_le16(&frame->ptr[SSH_MSGOFFSET_FRAME(len)]);
	message_len = SSH_MESSAGE_LENGTH(payload_len);
	if (frame->len < message_len)
		return;

	if (likely(!ssh_ptl_should_corrupt_rx_data()))
		return;

	sshf = (struct ssh_frame *)&frame->ptr[SSH_MSGOFFSET_FRAME(type)];
	trace_ssam_ei_rx_corrupt_data(sshf);

	 
	frame->ptr[frame->len - 2] = ~frame->ptr[frame->len - 2];
}

#else  

static inline bool ssh_ptl_should_drop_packet(struct ssh_packet *packet)
{
	return false;
}

static inline int ssh_ptl_write_buf(struct ssh_ptl *ptl,
				    struct ssh_packet *packet,
				    const unsigned char *buf,
				    size_t count)
{
	return serdev_device_write_buf(ptl->serdev, buf, count);
}

static inline void ssh_ptl_tx_inject_invalid_data(struct ssh_packet *packet)
{
}

static inline void ssh_ptl_rx_inject_invalid_syn(struct ssh_ptl *ptl,
						 struct ssam_span *data)
{
}

static inline void ssh_ptl_rx_inject_invalid_data(struct ssh_ptl *ptl,
						  struct ssam_span *frame)
{
}

#endif  

static void __ssh_ptl_packet_release(struct kref *kref)
{
	struct ssh_packet *p = container_of(kref, struct ssh_packet, refcnt);

	trace_ssam_packet_release(p);

	ptl_dbg_cond(p->ptl, "ptl: releasing packet %p\n", p);
	p->ops->release(p);
}

 
struct ssh_packet *ssh_packet_get(struct ssh_packet *packet)
{
	if (packet)
		kref_get(&packet->refcnt);
	return packet;
}
EXPORT_SYMBOL_GPL(ssh_packet_get);

 
void ssh_packet_put(struct ssh_packet *packet)
{
	if (packet)
		kref_put(&packet->refcnt, __ssh_ptl_packet_release);
}
EXPORT_SYMBOL_GPL(ssh_packet_put);

static u8 ssh_packet_get_seq(struct ssh_packet *packet)
{
	return packet->data.ptr[SSH_MSGOFFSET_FRAME(seq)];
}

 
void ssh_packet_init(struct ssh_packet *packet, unsigned long type,
		     u8 priority, const struct ssh_packet_ops *ops)
{
	kref_init(&packet->refcnt);

	packet->ptl = NULL;
	INIT_LIST_HEAD(&packet->queue_node);
	INIT_LIST_HEAD(&packet->pending_node);

	packet->state = type & SSH_PACKET_FLAGS_TY_MASK;
	packet->priority = priority;
	packet->timestamp = KTIME_MAX;

	packet->data.ptr = NULL;
	packet->data.len = 0;

	packet->ops = ops;
}

static struct kmem_cache *ssh_ctrl_packet_cache;

 
int ssh_ctrl_packet_cache_init(void)
{
	const unsigned int size = sizeof(struct ssh_packet) + SSH_MSG_LEN_CTRL;
	const unsigned int align = __alignof__(struct ssh_packet);
	struct kmem_cache *cache;

	cache = kmem_cache_create("ssam_ctrl_packet", size, align, 0, NULL);
	if (!cache)
		return -ENOMEM;

	ssh_ctrl_packet_cache = cache;
	return 0;
}

 
void ssh_ctrl_packet_cache_destroy(void)
{
	kmem_cache_destroy(ssh_ctrl_packet_cache);
	ssh_ctrl_packet_cache = NULL;
}

 
static int ssh_ctrl_packet_alloc(struct ssh_packet **packet,
				 struct ssam_span *buffer, gfp_t flags)
{
	*packet = kmem_cache_alloc(ssh_ctrl_packet_cache, flags);
	if (!*packet)
		return -ENOMEM;

	buffer->ptr = (u8 *)(*packet + 1);
	buffer->len = SSH_MSG_LEN_CTRL;

	trace_ssam_ctrl_packet_alloc(*packet, buffer->len);
	return 0;
}

 
static void ssh_ctrl_packet_free(struct ssh_packet *p)
{
	trace_ssam_ctrl_packet_free(p);
	kmem_cache_free(ssh_ctrl_packet_cache, p);
}

static const struct ssh_packet_ops ssh_ptl_ctrl_packet_ops = {
	.complete = NULL,
	.release = ssh_ctrl_packet_free,
};

static void ssh_ptl_timeout_reaper_mod(struct ssh_ptl *ptl, ktime_t now,
				       ktime_t expires)
{
	unsigned long delta = msecs_to_jiffies(ktime_ms_delta(expires, now));
	ktime_t aexp = ktime_add(expires, SSH_PTL_PACKET_TIMEOUT_RESOLUTION);

	spin_lock(&ptl->rtx_timeout.lock);

	 
	if (ktime_before(aexp, ptl->rtx_timeout.expires)) {
		ptl->rtx_timeout.expires = expires;
		mod_delayed_work(system_wq, &ptl->rtx_timeout.reaper, delta);
	}

	spin_unlock(&ptl->rtx_timeout.lock);
}

 
static void ssh_packet_next_try(struct ssh_packet *p)
{
	u8 base = ssh_packet_priority_get_base(p->priority);
	u8 try = ssh_packet_priority_get_try(p->priority);

	lockdep_assert_held(&p->ptl->queue.lock);

	 
	WRITE_ONCE(p->priority, __SSH_PACKET_PRIORITY(base, try + 1));
}

 
static struct list_head *__ssh_ptl_queue_find_entrypoint(struct ssh_packet *p)
{
	struct list_head *head;
	struct ssh_packet *q;

	lockdep_assert_held(&p->ptl->queue.lock);

	 

	if (p->priority > SSH_PACKET_PRIORITY(DATA, 0)) {
		list_for_each(head, &p->ptl->queue.head) {
			q = list_entry(head, struct ssh_packet, queue_node);

			if (q->priority < p->priority)
				break;
		}
	} else {
		list_for_each_prev(head, &p->ptl->queue.head) {
			q = list_entry(head, struct ssh_packet, queue_node);

			if (q->priority >= p->priority) {
				head = head->next;
				break;
			}
		}
	}

	return head;
}

 
static int __ssh_ptl_queue_push(struct ssh_packet *packet)
{
	struct ssh_ptl *ptl = packet->ptl;
	struct list_head *head;

	lockdep_assert_held(&ptl->queue.lock);

	if (test_bit(SSH_PTL_SF_SHUTDOWN_BIT, &ptl->state))
		return -ESHUTDOWN;

	 
	if (test_bit(SSH_PACKET_SF_LOCKED_BIT, &packet->state))
		return -EINVAL;

	 
	if (test_and_set_bit(SSH_PACKET_SF_QUEUED_BIT, &packet->state))
		return -EALREADY;

	head = __ssh_ptl_queue_find_entrypoint(packet);

	list_add_tail(&ssh_packet_get(packet)->queue_node, head);
	return 0;
}

static int ssh_ptl_queue_push(struct ssh_packet *packet)
{
	int status;

	spin_lock(&packet->ptl->queue.lock);
	status = __ssh_ptl_queue_push(packet);
	spin_unlock(&packet->ptl->queue.lock);

	return status;
}

static void ssh_ptl_queue_remove(struct ssh_packet *packet)
{
	struct ssh_ptl *ptl = packet->ptl;

	spin_lock(&ptl->queue.lock);

	if (!test_and_clear_bit(SSH_PACKET_SF_QUEUED_BIT, &packet->state)) {
		spin_unlock(&ptl->queue.lock);
		return;
	}

	list_del(&packet->queue_node);

	spin_unlock(&ptl->queue.lock);
	ssh_packet_put(packet);
}

static void ssh_ptl_pending_push(struct ssh_packet *p)
{
	struct ssh_ptl *ptl = p->ptl;
	const ktime_t timestamp = ktime_get_coarse_boottime();
	const ktime_t timeout = ptl->rtx_timeout.timeout;

	 

	spin_lock(&ptl->pending.lock);

	 
	if (test_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state)) {
		spin_unlock(&ptl->pending.lock);
		return;
	}

	 
	p->timestamp = timestamp;

	 
	if (!test_and_set_bit(SSH_PACKET_SF_PENDING_BIT, &p->state)) {
		atomic_inc(&ptl->pending.count);
		list_add_tail(&ssh_packet_get(p)->pending_node, &ptl->pending.head);
	}

	spin_unlock(&ptl->pending.lock);

	 
	ssh_ptl_timeout_reaper_mod(ptl, timestamp, timestamp + timeout);
}

static void ssh_ptl_pending_remove(struct ssh_packet *packet)
{
	struct ssh_ptl *ptl = packet->ptl;

	spin_lock(&ptl->pending.lock);

	if (!test_and_clear_bit(SSH_PACKET_SF_PENDING_BIT, &packet->state)) {
		spin_unlock(&ptl->pending.lock);
		return;
	}

	list_del(&packet->pending_node);
	atomic_dec(&ptl->pending.count);

	spin_unlock(&ptl->pending.lock);

	ssh_packet_put(packet);
}

 
static void __ssh_ptl_complete(struct ssh_packet *p, int status)
{
	struct ssh_ptl *ptl = READ_ONCE(p->ptl);

	trace_ssam_packet_complete(p, status);
	ptl_dbg_cond(ptl, "ptl: completing packet %p (status: %d)\n", p, status);

	if (p->ops->complete)
		p->ops->complete(p, status);
}

static void ssh_ptl_remove_and_complete(struct ssh_packet *p, int status)
{
	 

	if (test_and_set_bit(SSH_PACKET_SF_COMPLETED_BIT, &p->state))
		return;

	ssh_ptl_queue_remove(p);
	ssh_ptl_pending_remove(p);

	__ssh_ptl_complete(p, status);
}

static bool ssh_ptl_tx_can_process(struct ssh_packet *packet)
{
	struct ssh_ptl *ptl = packet->ptl;

	if (test_bit(SSH_PACKET_TY_FLUSH_BIT, &packet->state))
		return !atomic_read(&ptl->pending.count);

	 
	if (!test_bit(SSH_PACKET_TY_BLOCKING_BIT, &packet->state))
		return true;

	 
	if (test_bit(SSH_PACKET_SF_PENDING_BIT, &packet->state))
		return true;

	 
	return atomic_read(&ptl->pending.count) < SSH_PTL_MAX_PENDING;
}

static struct ssh_packet *ssh_ptl_tx_pop(struct ssh_ptl *ptl)
{
	struct ssh_packet *packet = ERR_PTR(-ENOENT);
	struct ssh_packet *p, *n;

	spin_lock(&ptl->queue.lock);
	list_for_each_entry_safe(p, n, &ptl->queue.head, queue_node) {
		 
		if (test_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state))
			continue;

		 
		if (!ssh_ptl_tx_can_process(p)) {
			packet = ERR_PTR(-EBUSY);
			break;
		}

		 

		list_del(&p->queue_node);

		set_bit(SSH_PACKET_SF_TRANSMITTING_BIT, &p->state);
		 
		smp_mb__before_atomic();
		clear_bit(SSH_PACKET_SF_QUEUED_BIT, &p->state);

		 
		ssh_packet_next_try(p);

		packet = p;
		break;
	}
	spin_unlock(&ptl->queue.lock);

	return packet;
}

static struct ssh_packet *ssh_ptl_tx_next(struct ssh_ptl *ptl)
{
	struct ssh_packet *p;

	p = ssh_ptl_tx_pop(ptl);
	if (IS_ERR(p))
		return p;

	if (test_bit(SSH_PACKET_TY_SEQUENCED_BIT, &p->state)) {
		ptl_dbg(ptl, "ptl: transmitting sequenced packet %p\n", p);
		ssh_ptl_pending_push(p);
	} else {
		ptl_dbg(ptl, "ptl: transmitting non-sequenced packet %p\n", p);
	}

	return p;
}

static void ssh_ptl_tx_compl_success(struct ssh_packet *packet)
{
	struct ssh_ptl *ptl = packet->ptl;

	ptl_dbg(ptl, "ptl: successfully transmitted packet %p\n", packet);

	 
	set_bit(SSH_PACKET_SF_TRANSMITTED_BIT, &packet->state);
	 
	smp_mb__before_atomic();
	clear_bit(SSH_PACKET_SF_TRANSMITTING_BIT, &packet->state);

	 
	if (!test_bit(SSH_PACKET_TY_SEQUENCED_BIT, &packet->state)) {
		set_bit(SSH_PACKET_SF_LOCKED_BIT, &packet->state);
		ssh_ptl_remove_and_complete(packet, 0);
	}

	 
	wake_up_all(&ptl->tx.packet_wq);
}

static void ssh_ptl_tx_compl_error(struct ssh_packet *packet, int status)
{
	 
	set_bit(SSH_PACKET_SF_LOCKED_BIT, &packet->state);
	 
	smp_mb__before_atomic();
	clear_bit(SSH_PACKET_SF_TRANSMITTING_BIT, &packet->state);

	ptl_err(packet->ptl, "ptl: transmission error: %d\n", status);
	ptl_dbg(packet->ptl, "ptl: failed to transmit packet: %p\n", packet);

	ssh_ptl_remove_and_complete(packet, status);

	 
	wake_up_all(&packet->ptl->tx.packet_wq);
}

static long ssh_ptl_tx_wait_packet(struct ssh_ptl *ptl)
{
	int status;

	status = wait_for_completion_interruptible(&ptl->tx.thread_cplt_pkt);
	reinit_completion(&ptl->tx.thread_cplt_pkt);

	 
	smp_mb__after_atomic();

	return status;
}

static long ssh_ptl_tx_wait_transfer(struct ssh_ptl *ptl, long timeout)
{
	long status;

	status = wait_for_completion_interruptible_timeout(&ptl->tx.thread_cplt_tx,
							   timeout);
	reinit_completion(&ptl->tx.thread_cplt_tx);

	 
	smp_mb__after_atomic();

	return status;
}

static int ssh_ptl_tx_packet(struct ssh_ptl *ptl, struct ssh_packet *packet)
{
	long timeout = SSH_PTL_TX_TIMEOUT;
	size_t offset = 0;

	 
	if (unlikely(!packet->data.ptr))
		return 0;

	 
	if (ssh_ptl_should_drop_packet(packet))
		return 0;

	 
	ssh_ptl_tx_inject_invalid_data(packet);

	ptl_dbg(ptl, "tx: sending data (length: %zu)\n", packet->data.len);
	print_hex_dump_debug("tx: ", DUMP_PREFIX_OFFSET, 16, 1,
			     packet->data.ptr, packet->data.len, false);

	do {
		ssize_t status, len;
		u8 *buf;

		buf = packet->data.ptr + offset;
		len = packet->data.len - offset;

		status = ssh_ptl_write_buf(ptl, packet, buf, len);
		if (status < 0)
			return status;

		if (status == len)
			return 0;

		offset += status;

		timeout = ssh_ptl_tx_wait_transfer(ptl, timeout);
		if (kthread_should_stop() || !atomic_read(&ptl->tx.running))
			return -ESHUTDOWN;

		if (timeout < 0)
			return -EINTR;

		if (timeout == 0)
			return -ETIMEDOUT;
	} while (true);
}

static int ssh_ptl_tx_threadfn(void *data)
{
	struct ssh_ptl *ptl = data;

	while (!kthread_should_stop() && atomic_read(&ptl->tx.running)) {
		struct ssh_packet *packet;
		int status;

		 
		packet = ssh_ptl_tx_next(ptl);

		 
		if (IS_ERR(packet)) {
			ssh_ptl_tx_wait_packet(ptl);
			continue;
		}

		 
		status = ssh_ptl_tx_packet(ptl, packet);
		if (status)
			ssh_ptl_tx_compl_error(packet, status);
		else
			ssh_ptl_tx_compl_success(packet);

		ssh_packet_put(packet);
	}

	return 0;
}

 
static void ssh_ptl_tx_wakeup_packet(struct ssh_ptl *ptl)
{
	if (test_bit(SSH_PTL_SF_SHUTDOWN_BIT, &ptl->state))
		return;

	complete(&ptl->tx.thread_cplt_pkt);
}

 
int ssh_ptl_tx_start(struct ssh_ptl *ptl)
{
	atomic_set_release(&ptl->tx.running, 1);

	ptl->tx.thread = kthread_run(ssh_ptl_tx_threadfn, ptl, "ssam_serial_hub-tx");
	if (IS_ERR(ptl->tx.thread))
		return PTR_ERR(ptl->tx.thread);

	return 0;
}

 
int ssh_ptl_tx_stop(struct ssh_ptl *ptl)
{
	int status = 0;

	if (!IS_ERR_OR_NULL(ptl->tx.thread)) {
		 
		atomic_set_release(&ptl->tx.running, 0);

		 
		complete(&ptl->tx.thread_cplt_pkt);
		complete(&ptl->tx.thread_cplt_tx);

		 
		status = kthread_stop(ptl->tx.thread);
		ptl->tx.thread = NULL;
	}

	return status;
}

static struct ssh_packet *ssh_ptl_ack_pop(struct ssh_ptl *ptl, u8 seq_id)
{
	struct ssh_packet *packet = ERR_PTR(-ENOENT);
	struct ssh_packet *p, *n;

	spin_lock(&ptl->pending.lock);
	list_for_each_entry_safe(p, n, &ptl->pending.head, pending_node) {
		 
		if (unlikely(ssh_packet_get_seq(p) != seq_id))
			continue;

		 
		if (unlikely(test_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state))) {
			packet = ERR_PTR(-EPERM);
			break;
		}

		 
		set_bit(SSH_PACKET_SF_ACKED_BIT, &p->state);
		 
		smp_mb__before_atomic();
		clear_bit(SSH_PACKET_SF_PENDING_BIT, &p->state);

		atomic_dec(&ptl->pending.count);
		list_del(&p->pending_node);
		packet = p;

		break;
	}
	spin_unlock(&ptl->pending.lock);

	return packet;
}

static void ssh_ptl_wait_until_transmitted(struct ssh_packet *packet)
{
	wait_event(packet->ptl->tx.packet_wq,
		   test_bit(SSH_PACKET_SF_TRANSMITTED_BIT, &packet->state) ||
		   test_bit(SSH_PACKET_SF_LOCKED_BIT, &packet->state));
}

static void ssh_ptl_acknowledge(struct ssh_ptl *ptl, u8 seq)
{
	struct ssh_packet *p;

	p = ssh_ptl_ack_pop(ptl, seq);
	if (IS_ERR(p)) {
		if (PTR_ERR(p) == -ENOENT) {
			 
			ptl_warn(ptl, "ptl: received ACK for non-pending packet\n");
		} else {
			 
			WARN_ON(PTR_ERR(p) != -EPERM);
		}
		return;
	}

	ptl_dbg(ptl, "ptl: received ACK for packet %p\n", p);

	 
	ssh_ptl_wait_until_transmitted(p);

	 
	if (unlikely(test_and_set_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state))) {
		if (unlikely(!test_bit(SSH_PACKET_SF_TRANSMITTED_BIT, &p->state)))
			ptl_err(ptl, "ptl: received ACK before packet had been fully transmitted\n");

		ssh_packet_put(p);
		return;
	}

	ssh_ptl_remove_and_complete(p, 0);
	ssh_packet_put(p);

	if (atomic_read(&ptl->pending.count) < SSH_PTL_MAX_PENDING)
		ssh_ptl_tx_wakeup_packet(ptl);
}

 
int ssh_ptl_submit(struct ssh_ptl *ptl, struct ssh_packet *p)
{
	struct ssh_ptl *ptl_old;
	int status;

	trace_ssam_packet_submit(p);

	 
	if (test_bit(SSH_PACKET_TY_FLUSH_BIT, &p->state)) {
		if (p->data.ptr || test_bit(SSH_PACKET_TY_SEQUENCED_BIT, &p->state))
			return -EINVAL;
	} else if (!p->data.ptr) {
		return -EINVAL;
	}

	 
	ptl_old = READ_ONCE(p->ptl);
	if (!ptl_old)
		WRITE_ONCE(p->ptl, ptl);
	else if (WARN_ON(ptl_old != ptl))
		return -EALREADY;	 

	status = ssh_ptl_queue_push(p);
	if (status)
		return status;

	if (!test_bit(SSH_PACKET_TY_BLOCKING_BIT, &p->state) ||
	    (atomic_read(&ptl->pending.count) < SSH_PTL_MAX_PENDING))
		ssh_ptl_tx_wakeup_packet(ptl);

	return 0;
}

 
static int __ssh_ptl_resubmit(struct ssh_packet *packet)
{
	int status;
	u8 try;

	lockdep_assert_held(&packet->ptl->pending.lock);

	trace_ssam_packet_resubmit(packet);

	spin_lock(&packet->ptl->queue.lock);

	 
	try = ssh_packet_priority_get_try(packet->priority);
	if (try >= SSH_PTL_MAX_PACKET_TRIES) {
		spin_unlock(&packet->ptl->queue.lock);
		return -ECANCELED;
	}

	status = __ssh_ptl_queue_push(packet);
	if (status) {
		 
		spin_unlock(&packet->ptl->queue.lock);
		return status;
	}

	packet->timestamp = KTIME_MAX;

	spin_unlock(&packet->ptl->queue.lock);
	return 0;
}

static void ssh_ptl_resubmit_pending(struct ssh_ptl *ptl)
{
	struct ssh_packet *p;
	bool resub = false;

	 

	spin_lock(&ptl->pending.lock);

	 
	list_for_each_entry(p, &ptl->pending.head, pending_node) {
		 
		if (!__ssh_ptl_resubmit(p))
			resub = true;
	}

	spin_unlock(&ptl->pending.lock);

	if (resub)
		ssh_ptl_tx_wakeup_packet(ptl);
}

 
void ssh_ptl_cancel(struct ssh_packet *p)
{
	if (test_and_set_bit(SSH_PACKET_SF_CANCELED_BIT, &p->state))
		return;

	trace_ssam_packet_cancel(p);

	 
	if (test_and_set_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state))
		return;

	 

	if (READ_ONCE(p->ptl)) {
		ssh_ptl_remove_and_complete(p, -ECANCELED);

		if (atomic_read(&p->ptl->pending.count) < SSH_PTL_MAX_PENDING)
			ssh_ptl_tx_wakeup_packet(p->ptl);

	} else if (!test_and_set_bit(SSH_PACKET_SF_COMPLETED_BIT, &p->state)) {
		__ssh_ptl_complete(p, -ECANCELED);
	}
}

 
static ktime_t ssh_packet_get_expiration(struct ssh_packet *p, ktime_t timeout)
{
	lockdep_assert_held(&p->ptl->pending.lock);

	if (p->timestamp != KTIME_MAX)
		return ktime_add(p->timestamp, timeout);
	else
		return KTIME_MAX;
}

static void ssh_ptl_timeout_reap(struct work_struct *work)
{
	struct ssh_ptl *ptl = to_ssh_ptl(work, rtx_timeout.reaper.work);
	struct ssh_packet *p, *n;
	LIST_HEAD(claimed);
	ktime_t now = ktime_get_coarse_boottime();
	ktime_t timeout = ptl->rtx_timeout.timeout;
	ktime_t next = KTIME_MAX;
	bool resub = false;
	int status;

	trace_ssam_ptl_timeout_reap(atomic_read(&ptl->pending.count));

	 
	spin_lock(&ptl->rtx_timeout.lock);
	ptl->rtx_timeout.expires = KTIME_MAX;
	spin_unlock(&ptl->rtx_timeout.lock);

	spin_lock(&ptl->pending.lock);

	list_for_each_entry_safe(p, n, &ptl->pending.head, pending_node) {
		ktime_t expires = ssh_packet_get_expiration(p, timeout);

		 
		if (ktime_after(expires, now)) {
			next = ktime_before(expires, next) ? expires : next;
			continue;
		}

		trace_ssam_packet_timeout(p);

		status = __ssh_ptl_resubmit(p);

		 
		if (!status)
			resub = true;

		 
		if (status != -ECANCELED)
			continue;

		 

		 
		if (test_and_set_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state))
			continue;

		 

		clear_bit(SSH_PACKET_SF_PENDING_BIT, &p->state);

		atomic_dec(&ptl->pending.count);
		list_move_tail(&p->pending_node, &claimed);
	}

	spin_unlock(&ptl->pending.lock);

	 
	list_for_each_entry_safe(p, n, &claimed, pending_node) {
		if (!test_and_set_bit(SSH_PACKET_SF_COMPLETED_BIT, &p->state)) {
			ssh_ptl_queue_remove(p);
			__ssh_ptl_complete(p, -ETIMEDOUT);
		}

		 
		list_del(&p->pending_node);
		ssh_packet_put(p);
	}

	 
	next = max(next, ktime_add(now, SSH_PTL_PACKET_TIMEOUT_RESOLUTION));
	if (next != KTIME_MAX)
		ssh_ptl_timeout_reaper_mod(ptl, now, next);

	if (resub)
		ssh_ptl_tx_wakeup_packet(ptl);
}

static bool ssh_ptl_rx_retransmit_check(struct ssh_ptl *ptl, const struct ssh_frame *frame)
{
	int i;

	 
	if (frame->type == SSH_FRAME_TYPE_DATA_NSQ)
		return false;

	 
	for (i = 0; i < ARRAY_SIZE(ptl->rx.blocked.seqs); i++) {
		if (likely(ptl->rx.blocked.seqs[i] != frame->seq))
			continue;

		ptl_dbg(ptl, "ptl: ignoring repeated data packet\n");
		return true;
	}

	 
	ptl->rx.blocked.seqs[ptl->rx.blocked.offset] = frame->seq;
	ptl->rx.blocked.offset = (ptl->rx.blocked.offset + 1)
				  % ARRAY_SIZE(ptl->rx.blocked.seqs);

	return false;
}

static void ssh_ptl_rx_dataframe(struct ssh_ptl *ptl,
				 const struct ssh_frame *frame,
				 const struct ssam_span *payload)
{
	if (ssh_ptl_rx_retransmit_check(ptl, frame))
		return;

	ptl->ops.data_received(ptl, payload);
}

static void ssh_ptl_send_ack(struct ssh_ptl *ptl, u8 seq)
{
	struct ssh_packet *packet;
	struct ssam_span buf;
	struct msgbuf msgb;
	int status;

	status = ssh_ctrl_packet_alloc(&packet, &buf, GFP_KERNEL);
	if (status) {
		ptl_err(ptl, "ptl: failed to allocate ACK packet\n");
		return;
	}

	ssh_packet_init(packet, 0, SSH_PACKET_PRIORITY(ACK, 0),
			&ssh_ptl_ctrl_packet_ops);

	msgb_init(&msgb, buf.ptr, buf.len);
	msgb_push_ack(&msgb, seq);
	ssh_packet_set_data(packet, msgb.begin, msgb_bytes_used(&msgb));

	ssh_ptl_submit(ptl, packet);
	ssh_packet_put(packet);
}

static void ssh_ptl_send_nak(struct ssh_ptl *ptl)
{
	struct ssh_packet *packet;
	struct ssam_span buf;
	struct msgbuf msgb;
	int status;

	status = ssh_ctrl_packet_alloc(&packet, &buf, GFP_KERNEL);
	if (status) {
		ptl_err(ptl, "ptl: failed to allocate NAK packet\n");
		return;
	}

	ssh_packet_init(packet, 0, SSH_PACKET_PRIORITY(NAK, 0),
			&ssh_ptl_ctrl_packet_ops);

	msgb_init(&msgb, buf.ptr, buf.len);
	msgb_push_nak(&msgb);
	ssh_packet_set_data(packet, msgb.begin, msgb_bytes_used(&msgb));

	ssh_ptl_submit(ptl, packet);
	ssh_packet_put(packet);
}

static size_t ssh_ptl_rx_eval(struct ssh_ptl *ptl, struct ssam_span *source)
{
	struct ssh_frame *frame;
	struct ssam_span payload;
	struct ssam_span aligned;
	bool syn_found;
	int status;

	 
	ssh_ptl_rx_inject_invalid_syn(ptl, source);

	 
	syn_found = sshp_find_syn(source, &aligned);

	if (unlikely(aligned.ptr != source->ptr)) {
		 

		ptl_warn(ptl, "rx: parser: invalid start of frame, skipping\n");

		 
		ssh_ptl_send_nak(ptl);
	}

	if (unlikely(!syn_found))
		return aligned.ptr - source->ptr;

	 
	ssh_ptl_rx_inject_invalid_data(ptl, &aligned);

	 
	status = sshp_parse_frame(&ptl->serdev->dev, &aligned, &frame, &payload,
				  SSH_PTL_RX_BUF_LEN);
	if (status)	 
		return aligned.ptr - source->ptr + sizeof(u16);
	if (!frame)	 
		return aligned.ptr - source->ptr;

	trace_ssam_rx_frame_received(frame);

	switch (frame->type) {
	case SSH_FRAME_TYPE_ACK:
		ssh_ptl_acknowledge(ptl, frame->seq);
		break;

	case SSH_FRAME_TYPE_NAK:
		ssh_ptl_resubmit_pending(ptl);
		break;

	case SSH_FRAME_TYPE_DATA_SEQ:
		ssh_ptl_send_ack(ptl, frame->seq);
		fallthrough;

	case SSH_FRAME_TYPE_DATA_NSQ:
		ssh_ptl_rx_dataframe(ptl, frame, &payload);
		break;

	default:
		ptl_warn(ptl, "ptl: received frame with unknown type %#04x\n",
			 frame->type);
		break;
	}

	return aligned.ptr - source->ptr + SSH_MESSAGE_LENGTH(payload.len);
}

static int ssh_ptl_rx_threadfn(void *data)
{
	struct ssh_ptl *ptl = data;

	while (true) {
		struct ssam_span span;
		size_t offs = 0;
		size_t n;

		wait_event_interruptible(ptl->rx.wq,
					 !kfifo_is_empty(&ptl->rx.fifo) ||
					 kthread_should_stop());
		if (kthread_should_stop())
			break;

		 
		n = sshp_buf_read_from_fifo(&ptl->rx.buf, &ptl->rx.fifo);

		ptl_dbg(ptl, "rx: received data (size: %zu)\n", n);
		print_hex_dump_debug("rx: ", DUMP_PREFIX_OFFSET, 16, 1,
				     ptl->rx.buf.ptr + ptl->rx.buf.len - n,
				     n, false);

		 
		while (offs < ptl->rx.buf.len) {
			sshp_buf_span_from(&ptl->rx.buf, offs, &span);
			n = ssh_ptl_rx_eval(ptl, &span);
			if (n == 0)
				break;	 

			offs += n;
		}

		 
		sshp_buf_drop(&ptl->rx.buf, offs);
	}

	return 0;
}

static void ssh_ptl_rx_wakeup(struct ssh_ptl *ptl)
{
	wake_up(&ptl->rx.wq);
}

 
int ssh_ptl_rx_start(struct ssh_ptl *ptl)
{
	if (ptl->rx.thread)
		return 0;

	ptl->rx.thread = kthread_run(ssh_ptl_rx_threadfn, ptl,
				     "ssam_serial_hub-rx");
	if (IS_ERR(ptl->rx.thread))
		return PTR_ERR(ptl->rx.thread);

	return 0;
}

 
int ssh_ptl_rx_stop(struct ssh_ptl *ptl)
{
	int status = 0;

	if (ptl->rx.thread) {
		status = kthread_stop(ptl->rx.thread);
		ptl->rx.thread = NULL;
	}

	return status;
}

 
int ssh_ptl_rx_rcvbuf(struct ssh_ptl *ptl, const u8 *buf, size_t n)
{
	int used;

	if (test_bit(SSH_PTL_SF_SHUTDOWN_BIT, &ptl->state))
		return -ESHUTDOWN;

	used = kfifo_in(&ptl->rx.fifo, buf, n);
	if (used)
		ssh_ptl_rx_wakeup(ptl);

	return used;
}

 
void ssh_ptl_shutdown(struct ssh_ptl *ptl)
{
	LIST_HEAD(complete_q);
	LIST_HEAD(complete_p);
	struct ssh_packet *p, *n;
	int status;

	 
	set_bit(SSH_PTL_SF_SHUTDOWN_BIT, &ptl->state);
	 
	smp_mb__after_atomic();

	status = ssh_ptl_rx_stop(ptl);
	if (status)
		ptl_err(ptl, "ptl: failed to stop receiver thread\n");

	status = ssh_ptl_tx_stop(ptl);
	if (status)
		ptl_err(ptl, "ptl: failed to stop transmitter thread\n");

	cancel_delayed_work_sync(&ptl->rtx_timeout.reaper);

	 

	 
	spin_lock(&ptl->queue.lock);
	list_for_each_entry_safe(p, n, &ptl->queue.head, queue_node) {
		set_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state);
		 
		smp_mb__before_atomic();
		clear_bit(SSH_PACKET_SF_QUEUED_BIT, &p->state);

		list_move_tail(&p->queue_node, &complete_q);
	}
	spin_unlock(&ptl->queue.lock);

	 
	spin_lock(&ptl->pending.lock);
	list_for_each_entry_safe(p, n, &ptl->pending.head, pending_node) {
		set_bit(SSH_PACKET_SF_LOCKED_BIT, &p->state);
		 
		smp_mb__before_atomic();
		clear_bit(SSH_PACKET_SF_PENDING_BIT, &p->state);

		list_move_tail(&p->pending_node, &complete_q);
	}
	atomic_set(&ptl->pending.count, 0);
	spin_unlock(&ptl->pending.lock);

	 
	list_for_each_entry(p, &complete_q, queue_node) {
		if (!test_and_set_bit(SSH_PACKET_SF_COMPLETED_BIT, &p->state))
			__ssh_ptl_complete(p, -ESHUTDOWN);

		ssh_packet_put(p);
	}

	 
	list_for_each_entry(p, &complete_p, pending_node) {
		if (!test_and_set_bit(SSH_PACKET_SF_COMPLETED_BIT, &p->state))
			__ssh_ptl_complete(p, -ESHUTDOWN);

		ssh_packet_put(p);
	}

	 
}

 
int ssh_ptl_init(struct ssh_ptl *ptl, struct serdev_device *serdev,
		 struct ssh_ptl_ops *ops)
{
	int i, status;

	ptl->serdev = serdev;
	ptl->state = 0;

	spin_lock_init(&ptl->queue.lock);
	INIT_LIST_HEAD(&ptl->queue.head);

	spin_lock_init(&ptl->pending.lock);
	INIT_LIST_HEAD(&ptl->pending.head);
	atomic_set_release(&ptl->pending.count, 0);

	ptl->tx.thread = NULL;
	atomic_set(&ptl->tx.running, 0);
	init_completion(&ptl->tx.thread_cplt_pkt);
	init_completion(&ptl->tx.thread_cplt_tx);
	init_waitqueue_head(&ptl->tx.packet_wq);

	ptl->rx.thread = NULL;
	init_waitqueue_head(&ptl->rx.wq);

	spin_lock_init(&ptl->rtx_timeout.lock);
	ptl->rtx_timeout.timeout = SSH_PTL_PACKET_TIMEOUT;
	ptl->rtx_timeout.expires = KTIME_MAX;
	INIT_DELAYED_WORK(&ptl->rtx_timeout.reaper, ssh_ptl_timeout_reap);

	ptl->ops = *ops;

	 
	for (i = 0; i < ARRAY_SIZE(ptl->rx.blocked.seqs); i++)
		ptl->rx.blocked.seqs[i] = U16_MAX;
	ptl->rx.blocked.offset = 0;

	status = kfifo_alloc(&ptl->rx.fifo, SSH_PTL_RX_FIFO_LEN, GFP_KERNEL);
	if (status)
		return status;

	status = sshp_buf_alloc(&ptl->rx.buf, SSH_PTL_RX_BUF_LEN, GFP_KERNEL);
	if (status)
		kfifo_free(&ptl->rx.fifo);

	return status;
}

 
void ssh_ptl_destroy(struct ssh_ptl *ptl)
{
	kfifo_free(&ptl->rx.fifo);
	sshp_buf_free(&ptl->rx.buf);
}
