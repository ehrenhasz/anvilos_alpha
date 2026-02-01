
 

#include <asm/unaligned.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/error-injection.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include <linux/surface_aggregator/serial_hub.h>
#include <linux/surface_aggregator/controller.h>

#include "ssh_packet_layer.h"
#include "ssh_request_layer.h"

#include "trace.h"

 
#define SSH_RTL_REQUEST_TIMEOUT			ms_to_ktime(3000)

 
#define SSH_RTL_REQUEST_TIMEOUT_RESOLUTION	ms_to_ktime(max(2000 / HZ, 50))

 
#define SSH_RTL_MAX_PENDING		3

 
#define SSH_RTL_TX_BATCH		10

#ifdef CONFIG_SURFACE_AGGREGATOR_ERROR_INJECTION

 
static noinline bool ssh_rtl_should_drop_response(void)
{
	return false;
}
ALLOW_ERROR_INJECTION(ssh_rtl_should_drop_response, TRUE);

#else

static inline bool ssh_rtl_should_drop_response(void)
{
	return false;
}

#endif

static u16 ssh_request_get_rqid(struct ssh_request *rqst)
{
	return get_unaligned_le16(rqst->packet.data.ptr
				  + SSH_MSGOFFSET_COMMAND(rqid));
}

static u32 ssh_request_get_rqid_safe(struct ssh_request *rqst)
{
	if (!rqst->packet.data.ptr)
		return U32_MAX;

	return ssh_request_get_rqid(rqst);
}

static void ssh_rtl_queue_remove(struct ssh_request *rqst)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);

	spin_lock(&rtl->queue.lock);

	if (!test_and_clear_bit(SSH_REQUEST_SF_QUEUED_BIT, &rqst->state)) {
		spin_unlock(&rtl->queue.lock);
		return;
	}

	list_del(&rqst->node);

	spin_unlock(&rtl->queue.lock);
	ssh_request_put(rqst);
}

static bool ssh_rtl_queue_empty(struct ssh_rtl *rtl)
{
	bool empty;

	spin_lock(&rtl->queue.lock);
	empty = list_empty(&rtl->queue.head);
	spin_unlock(&rtl->queue.lock);

	return empty;
}

static void ssh_rtl_pending_remove(struct ssh_request *rqst)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);

	spin_lock(&rtl->pending.lock);

	if (!test_and_clear_bit(SSH_REQUEST_SF_PENDING_BIT, &rqst->state)) {
		spin_unlock(&rtl->pending.lock);
		return;
	}

	atomic_dec(&rtl->pending.count);
	list_del(&rqst->node);

	spin_unlock(&rtl->pending.lock);

	ssh_request_put(rqst);
}

static int ssh_rtl_tx_pending_push(struct ssh_request *rqst)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);

	spin_lock(&rtl->pending.lock);

	if (test_bit(SSH_REQUEST_SF_LOCKED_BIT, &rqst->state)) {
		spin_unlock(&rtl->pending.lock);
		return -EINVAL;
	}

	if (test_and_set_bit(SSH_REQUEST_SF_PENDING_BIT, &rqst->state)) {
		spin_unlock(&rtl->pending.lock);
		return -EALREADY;
	}

	atomic_inc(&rtl->pending.count);
	list_add_tail(&ssh_request_get(rqst)->node, &rtl->pending.head);

	spin_unlock(&rtl->pending.lock);
	return 0;
}

static void ssh_rtl_complete_with_status(struct ssh_request *rqst, int status)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);

	trace_ssam_request_complete(rqst, status);

	 
	rtl_dbg_cond(rtl, "rtl: completing request (rqid: %#06x, status: %d)\n",
		     ssh_request_get_rqid_safe(rqst), status);

	rqst->ops->complete(rqst, NULL, NULL, status);
}

static void ssh_rtl_complete_with_rsp(struct ssh_request *rqst,
				      const struct ssh_command *cmd,
				      const struct ssam_span *data)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);

	trace_ssam_request_complete(rqst, 0);

	rtl_dbg(rtl, "rtl: completing request with response (rqid: %#06x)\n",
		ssh_request_get_rqid(rqst));

	rqst->ops->complete(rqst, cmd, data, 0);
}

static bool ssh_rtl_tx_can_process(struct ssh_request *rqst)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);

	if (test_bit(SSH_REQUEST_TY_FLUSH_BIT, &rqst->state))
		return !atomic_read(&rtl->pending.count);

	return atomic_read(&rtl->pending.count) < SSH_RTL_MAX_PENDING;
}

static struct ssh_request *ssh_rtl_tx_next(struct ssh_rtl *rtl)
{
	struct ssh_request *rqst = ERR_PTR(-ENOENT);
	struct ssh_request *p, *n;

	spin_lock(&rtl->queue.lock);

	 
	list_for_each_entry_safe(p, n, &rtl->queue.head, node) {
		if (unlikely(test_bit(SSH_REQUEST_SF_LOCKED_BIT, &p->state)))
			continue;

		if (!ssh_rtl_tx_can_process(p)) {
			rqst = ERR_PTR(-EBUSY);
			break;
		}

		 
		set_bit(SSH_REQUEST_SF_TRANSMITTING_BIT, &p->state);
		 
		smp_mb__before_atomic();
		clear_bit(SSH_REQUEST_SF_QUEUED_BIT, &p->state);

		list_del(&p->node);

		rqst = p;
		break;
	}

	spin_unlock(&rtl->queue.lock);
	return rqst;
}

static int ssh_rtl_tx_try_process_one(struct ssh_rtl *rtl)
{
	struct ssh_request *rqst;
	int status;

	 
	rqst = ssh_rtl_tx_next(rtl);
	if (IS_ERR(rqst))
		return PTR_ERR(rqst);

	 
	status = ssh_rtl_tx_pending_push(rqst);
	if (status) {
		ssh_request_put(rqst);
		return -EAGAIN;
	}

	 
	status = ssh_ptl_submit(&rtl->ptl, &rqst->packet);
	if (status == -ESHUTDOWN) {
		 
		set_bit(SSH_REQUEST_SF_LOCKED_BIT, &rqst->state);
		 

		ssh_rtl_pending_remove(rqst);
		ssh_rtl_complete_with_status(rqst, -ESHUTDOWN);

		ssh_request_put(rqst);
		return -ESHUTDOWN;

	} else if (status) {
		 

		WARN_ON(status != -EINVAL);

		ssh_request_put(rqst);
		return -EAGAIN;
	}

	ssh_request_put(rqst);
	return 0;
}

static bool ssh_rtl_tx_schedule(struct ssh_rtl *rtl)
{
	if (atomic_read(&rtl->pending.count) >= SSH_RTL_MAX_PENDING)
		return false;

	if (ssh_rtl_queue_empty(rtl))
		return false;

	return schedule_work(&rtl->tx.work);
}

static void ssh_rtl_tx_work_fn(struct work_struct *work)
{
	struct ssh_rtl *rtl = to_ssh_rtl(work, tx.work);
	unsigned int iterations = SSH_RTL_TX_BATCH;
	int status;

	 
	do {
		status = ssh_rtl_tx_try_process_one(rtl);
		if (status == -ENOENT || status == -EBUSY)
			return;		 

		if (status == -ESHUTDOWN) {
			 
			return;
		}

		WARN_ON(status != 0 && status != -EAGAIN);
	} while (--iterations);

	 
	ssh_rtl_tx_schedule(rtl);
}

 
int ssh_rtl_submit(struct ssh_rtl *rtl, struct ssh_request *rqst)
{
	trace_ssam_request_submit(rqst);

	 
	if (test_bit(SSH_REQUEST_TY_HAS_RESPONSE_BIT, &rqst->state))
		if (!test_bit(SSH_PACKET_TY_SEQUENCED_BIT, &rqst->packet.state))
			return -EINVAL;

	spin_lock(&rtl->queue.lock);

	 
	if (cmpxchg(&rqst->packet.ptl, NULL, &rtl->ptl)) {
		spin_unlock(&rtl->queue.lock);
		return -EALREADY;
	}

	 
	smp_mb__after_atomic();

	if (test_bit(SSH_RTL_SF_SHUTDOWN_BIT, &rtl->state)) {
		spin_unlock(&rtl->queue.lock);
		return -ESHUTDOWN;
	}

	if (test_bit(SSH_REQUEST_SF_LOCKED_BIT, &rqst->state)) {
		spin_unlock(&rtl->queue.lock);
		return -EINVAL;
	}

	set_bit(SSH_REQUEST_SF_QUEUED_BIT, &rqst->state);
	list_add_tail(&ssh_request_get(rqst)->node, &rtl->queue.head);

	spin_unlock(&rtl->queue.lock);

	ssh_rtl_tx_schedule(rtl);
	return 0;
}

static void ssh_rtl_timeout_reaper_mod(struct ssh_rtl *rtl, ktime_t now,
				       ktime_t expires)
{
	unsigned long delta = msecs_to_jiffies(ktime_ms_delta(expires, now));
	ktime_t aexp = ktime_add(expires, SSH_RTL_REQUEST_TIMEOUT_RESOLUTION);

	spin_lock(&rtl->rtx_timeout.lock);

	 
	if (ktime_before(aexp, rtl->rtx_timeout.expires)) {
		rtl->rtx_timeout.expires = expires;
		mod_delayed_work(system_wq, &rtl->rtx_timeout.reaper, delta);
	}

	spin_unlock(&rtl->rtx_timeout.lock);
}

static void ssh_rtl_timeout_start(struct ssh_request *rqst)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);
	ktime_t timestamp = ktime_get_coarse_boottime();
	ktime_t timeout = rtl->rtx_timeout.timeout;

	if (test_bit(SSH_REQUEST_SF_LOCKED_BIT, &rqst->state))
		return;

	 
	WRITE_ONCE(rqst->timestamp, timestamp);
	 
	smp_mb__after_atomic();

	ssh_rtl_timeout_reaper_mod(rtl, timestamp, timestamp + timeout);
}

static void ssh_rtl_complete(struct ssh_rtl *rtl,
			     const struct ssh_command *command,
			     const struct ssam_span *command_data)
{
	struct ssh_request *r = NULL;
	struct ssh_request *p, *n;
	u16 rqid = get_unaligned_le16(&command->rqid);

	trace_ssam_rx_response_received(command, command_data->len);

	 
	spin_lock(&rtl->pending.lock);
	list_for_each_entry_safe(p, n, &rtl->pending.head, node) {
		 
		if (unlikely(ssh_request_get_rqid(p) != rqid))
			continue;

		 
		if (ssh_rtl_should_drop_response()) {
			spin_unlock(&rtl->pending.lock);

			trace_ssam_ei_rx_drop_response(p);
			rtl_info(rtl, "request error injection: dropping response for request %p\n",
				 &p->packet);
			return;
		}

		 
		set_bit(SSH_REQUEST_SF_LOCKED_BIT, &p->state);
		set_bit(SSH_REQUEST_SF_RSPRCVD_BIT, &p->state);
		 
		smp_mb__before_atomic();
		clear_bit(SSH_REQUEST_SF_PENDING_BIT, &p->state);

		atomic_dec(&rtl->pending.count);
		list_del(&p->node);

		r = p;
		break;
	}
	spin_unlock(&rtl->pending.lock);

	if (!r) {
		rtl_warn(rtl, "rtl: dropping unexpected command message (rqid = %#06x)\n",
			 rqid);
		return;
	}

	 
	if (test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state)) {
		ssh_request_put(r);
		ssh_rtl_tx_schedule(rtl);
		return;
	}

	 
	if (!test_bit(SSH_REQUEST_SF_TRANSMITTED_BIT, &r->state)) {
		rtl_err(rtl, "rtl: received response before ACK for request (rqid = %#06x)\n",
			rqid);

		 
		ssh_rtl_queue_remove(r);

		ssh_rtl_complete_with_status(r, -EREMOTEIO);
		ssh_request_put(r);

		ssh_rtl_tx_schedule(rtl);
		return;
	}

	 

	ssh_rtl_complete_with_rsp(r, command, command_data);
	ssh_request_put(r);

	ssh_rtl_tx_schedule(rtl);
}

static bool ssh_rtl_cancel_nonpending(struct ssh_request *r)
{
	struct ssh_rtl *rtl;
	unsigned long flags, fixed;
	bool remove;

	 
	fixed = READ_ONCE(r->state) & SSH_REQUEST_FLAGS_TY_MASK;
	flags = cmpxchg(&r->state, fixed, SSH_REQUEST_SF_LOCKED_BIT);

	 
	smp_mb__after_atomic();

	if (flags == fixed && !READ_ONCE(r->packet.ptl)) {
		if (test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
			return true;

		ssh_rtl_complete_with_status(r, -ECANCELED);
		return true;
	}

	rtl = ssh_request_rtl(r);
	spin_lock(&rtl->queue.lock);

	 

	remove = test_and_clear_bit(SSH_REQUEST_SF_QUEUED_BIT, &r->state);
	if (!remove) {
		spin_unlock(&rtl->queue.lock);
		return false;
	}

	set_bit(SSH_REQUEST_SF_LOCKED_BIT, &r->state);
	list_del(&r->node);

	spin_unlock(&rtl->queue.lock);

	ssh_request_put(r);	 

	if (test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
		return true;

	ssh_rtl_complete_with_status(r, -ECANCELED);
	return true;
}

static bool ssh_rtl_cancel_pending(struct ssh_request *r)
{
	 
	if (test_and_set_bit(SSH_REQUEST_SF_LOCKED_BIT, &r->state))
		return true;

	 
	if (!READ_ONCE(r->packet.ptl)) {
		if (test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
			return true;

		ssh_rtl_complete_with_status(r, -ECANCELED);
		return true;
	}

	 
	ssh_ptl_cancel(&r->packet);

	 
	if (test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
		return true;

	ssh_rtl_queue_remove(r);
	ssh_rtl_pending_remove(r);
	ssh_rtl_complete_with_status(r, -ECANCELED);

	return true;
}

 
bool ssh_rtl_cancel(struct ssh_request *rqst, bool pending)
{
	struct ssh_rtl *rtl;
	bool canceled;

	if (test_and_set_bit(SSH_REQUEST_SF_CANCELED_BIT, &rqst->state))
		return true;

	trace_ssam_request_cancel(rqst);

	if (pending)
		canceled = ssh_rtl_cancel_pending(rqst);
	else
		canceled = ssh_rtl_cancel_nonpending(rqst);

	 
	rtl = ssh_request_rtl(rqst);
	if (canceled && rtl)
		ssh_rtl_tx_schedule(rtl);

	return canceled;
}

static void ssh_rtl_packet_callback(struct ssh_packet *p, int status)
{
	struct ssh_request *r = to_ssh_request(p);

	if (unlikely(status)) {
		set_bit(SSH_REQUEST_SF_LOCKED_BIT, &r->state);

		if (test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
			return;

		 
		ssh_rtl_queue_remove(r);
		ssh_rtl_pending_remove(r);
		ssh_rtl_complete_with_status(r, status);

		ssh_rtl_tx_schedule(ssh_request_rtl(r));
		return;
	}

	 
	set_bit(SSH_REQUEST_SF_TRANSMITTED_BIT, &r->state);
	 
	smp_mb__before_atomic();
	clear_bit(SSH_REQUEST_SF_TRANSMITTING_BIT, &r->state);

	 
	if (test_bit(SSH_REQUEST_TY_HAS_RESPONSE_BIT, &r->state)) {
		 
		ssh_rtl_timeout_start(r);
		return;
	}

	 

	set_bit(SSH_REQUEST_SF_LOCKED_BIT, &r->state);
	if (test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
		return;

	ssh_rtl_pending_remove(r);
	ssh_rtl_complete_with_status(r, 0);

	ssh_rtl_tx_schedule(ssh_request_rtl(r));
}

static ktime_t ssh_request_get_expiration(struct ssh_request *r, ktime_t timeout)
{
	ktime_t timestamp = READ_ONCE(r->timestamp);

	if (timestamp != KTIME_MAX)
		return ktime_add(timestamp, timeout);
	else
		return KTIME_MAX;
}

static void ssh_rtl_timeout_reap(struct work_struct *work)
{
	struct ssh_rtl *rtl = to_ssh_rtl(work, rtx_timeout.reaper.work);
	struct ssh_request *r, *n;
	LIST_HEAD(claimed);
	ktime_t now = ktime_get_coarse_boottime();
	ktime_t timeout = rtl->rtx_timeout.timeout;
	ktime_t next = KTIME_MAX;

	trace_ssam_rtl_timeout_reap(atomic_read(&rtl->pending.count));

	 
	spin_lock(&rtl->rtx_timeout.lock);
	rtl->rtx_timeout.expires = KTIME_MAX;
	spin_unlock(&rtl->rtx_timeout.lock);

	spin_lock(&rtl->pending.lock);
	list_for_each_entry_safe(r, n, &rtl->pending.head, node) {
		ktime_t expires = ssh_request_get_expiration(r, timeout);

		 
		if (ktime_after(expires, now)) {
			next = ktime_before(expires, next) ? expires : next;
			continue;
		}

		 
		if (test_and_set_bit(SSH_REQUEST_SF_LOCKED_BIT, &r->state))
			continue;

		 

		clear_bit(SSH_REQUEST_SF_PENDING_BIT, &r->state);

		atomic_dec(&rtl->pending.count);
		list_move_tail(&r->node, &claimed);
	}
	spin_unlock(&rtl->pending.lock);

	 
	list_for_each_entry_safe(r, n, &claimed, node) {
		trace_ssam_request_timeout(r);

		 
		if (!test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
			ssh_rtl_complete_with_status(r, -ETIMEDOUT);

		 
		list_del(&r->node);
		ssh_request_put(r);
	}

	 
	next = max(next, ktime_add(now, SSH_RTL_REQUEST_TIMEOUT_RESOLUTION));
	if (next != KTIME_MAX)
		ssh_rtl_timeout_reaper_mod(rtl, now, next);

	ssh_rtl_tx_schedule(rtl);
}

static void ssh_rtl_rx_event(struct ssh_rtl *rtl, const struct ssh_command *cmd,
			     const struct ssam_span *data)
{
	trace_ssam_rx_event_received(cmd, data->len);

	rtl_dbg(rtl, "rtl: handling event (rqid: %#06x)\n",
		get_unaligned_le16(&cmd->rqid));

	rtl->ops.handle_event(rtl, cmd, data);
}

static void ssh_rtl_rx_command(struct ssh_ptl *p, const struct ssam_span *data)
{
	struct ssh_rtl *rtl = to_ssh_rtl(p, ptl);
	struct device *dev = &p->serdev->dev;
	struct ssh_command *command;
	struct ssam_span command_data;

	if (sshp_parse_command(dev, data, &command, &command_data))
		return;

	 
	if (command->tid != SSAM_SSH_TID_HOST) {
		rtl_warn(rtl, "rtl: dropping message not intended for us (tid = %#04x)\n",
			 command->tid);
		return;
	}

	if (ssh_rqid_is_event(get_unaligned_le16(&command->rqid)))
		ssh_rtl_rx_event(rtl, command, &command_data);
	else
		ssh_rtl_complete(rtl, command, &command_data);
}

static void ssh_rtl_rx_data(struct ssh_ptl *p, const struct ssam_span *data)
{
	if (!data->len) {
		ptl_err(p, "rtl: rx: no data frame payload\n");
		return;
	}

	switch (data->ptr[0]) {
	case SSH_PLD_TYPE_CMD:
		ssh_rtl_rx_command(p, data);
		break;

	default:
		ptl_err(p, "rtl: rx: unknown frame payload type (type: %#04x)\n",
			data->ptr[0]);
		break;
	}
}

static void ssh_rtl_packet_release(struct ssh_packet *p)
{
	struct ssh_request *rqst;

	rqst = to_ssh_request(p);
	rqst->ops->release(rqst);
}

static const struct ssh_packet_ops ssh_rtl_packet_ops = {
	.complete = ssh_rtl_packet_callback,
	.release = ssh_rtl_packet_release,
};

 
int ssh_request_init(struct ssh_request *rqst, enum ssam_request_flags flags,
		     const struct ssh_request_ops *ops)
{
	unsigned long type = BIT(SSH_PACKET_TY_BLOCKING_BIT);

	 
	if (flags & SSAM_REQUEST_UNSEQUENCED && flags & SSAM_REQUEST_HAS_RESPONSE)
		return -EINVAL;

	if (!(flags & SSAM_REQUEST_UNSEQUENCED))
		type |= BIT(SSH_PACKET_TY_SEQUENCED_BIT);

	ssh_packet_init(&rqst->packet, type, SSH_PACKET_PRIORITY(DATA, 0),
			&ssh_rtl_packet_ops);

	INIT_LIST_HEAD(&rqst->node);

	rqst->state = 0;
	if (flags & SSAM_REQUEST_HAS_RESPONSE)
		rqst->state |= BIT(SSH_REQUEST_TY_HAS_RESPONSE_BIT);

	rqst->timestamp = KTIME_MAX;
	rqst->ops = ops;

	return 0;
}

 
int ssh_rtl_init(struct ssh_rtl *rtl, struct serdev_device *serdev,
		 const struct ssh_rtl_ops *ops)
{
	struct ssh_ptl_ops ptl_ops;
	int status;

	ptl_ops.data_received = ssh_rtl_rx_data;

	status = ssh_ptl_init(&rtl->ptl, serdev, &ptl_ops);
	if (status)
		return status;

	spin_lock_init(&rtl->queue.lock);
	INIT_LIST_HEAD(&rtl->queue.head);

	spin_lock_init(&rtl->pending.lock);
	INIT_LIST_HEAD(&rtl->pending.head);
	atomic_set_release(&rtl->pending.count, 0);

	INIT_WORK(&rtl->tx.work, ssh_rtl_tx_work_fn);

	spin_lock_init(&rtl->rtx_timeout.lock);
	rtl->rtx_timeout.timeout = SSH_RTL_REQUEST_TIMEOUT;
	rtl->rtx_timeout.expires = KTIME_MAX;
	INIT_DELAYED_WORK(&rtl->rtx_timeout.reaper, ssh_rtl_timeout_reap);

	rtl->ops = *ops;

	return 0;
}

 
void ssh_rtl_destroy(struct ssh_rtl *rtl)
{
	ssh_ptl_destroy(&rtl->ptl);
}

 
int ssh_rtl_start(struct ssh_rtl *rtl)
{
	int status;

	status = ssh_ptl_tx_start(&rtl->ptl);
	if (status)
		return status;

	ssh_rtl_tx_schedule(rtl);

	status = ssh_ptl_rx_start(&rtl->ptl);
	if (status) {
		ssh_rtl_flush(rtl, msecs_to_jiffies(5000));
		ssh_ptl_tx_stop(&rtl->ptl);
		return status;
	}

	return 0;
}

struct ssh_flush_request {
	struct ssh_request base;
	struct completion completion;
	int status;
};

static void ssh_rtl_flush_request_complete(struct ssh_request *r,
					   const struct ssh_command *cmd,
					   const struct ssam_span *data,
					   int status)
{
	struct ssh_flush_request *rqst;

	rqst = container_of(r, struct ssh_flush_request, base);
	rqst->status = status;
}

static void ssh_rtl_flush_request_release(struct ssh_request *r)
{
	struct ssh_flush_request *rqst;

	rqst = container_of(r, struct ssh_flush_request, base);
	complete_all(&rqst->completion);
}

static const struct ssh_request_ops ssh_rtl_flush_request_ops = {
	.complete = ssh_rtl_flush_request_complete,
	.release = ssh_rtl_flush_request_release,
};

 
int ssh_rtl_flush(struct ssh_rtl *rtl, unsigned long timeout)
{
	const unsigned int init_flags = SSAM_REQUEST_UNSEQUENCED;
	struct ssh_flush_request rqst;
	int status;

	ssh_request_init(&rqst.base, init_flags, &ssh_rtl_flush_request_ops);
	rqst.base.packet.state |= BIT(SSH_PACKET_TY_FLUSH_BIT);
	rqst.base.packet.priority = SSH_PACKET_PRIORITY(FLUSH, 0);
	rqst.base.state |= BIT(SSH_REQUEST_TY_FLUSH_BIT);

	init_completion(&rqst.completion);

	status = ssh_rtl_submit(rtl, &rqst.base);
	if (status)
		return status;

	ssh_request_put(&rqst.base);

	if (!wait_for_completion_timeout(&rqst.completion, timeout)) {
		ssh_rtl_cancel(&rqst.base, true);
		wait_for_completion(&rqst.completion);
	}

	WARN_ON(rqst.status != 0 && rqst.status != -ECANCELED &&
		rqst.status != -ESHUTDOWN && rqst.status != -EINTR);

	return rqst.status == -ECANCELED ? -ETIMEDOUT : rqst.status;
}

 
void ssh_rtl_shutdown(struct ssh_rtl *rtl)
{
	struct ssh_request *r, *n;
	LIST_HEAD(claimed);
	int pending;

	set_bit(SSH_RTL_SF_SHUTDOWN_BIT, &rtl->state);
	 
	smp_mb__after_atomic();

	 
	spin_lock(&rtl->queue.lock);
	list_for_each_entry_safe(r, n, &rtl->queue.head, node) {
		set_bit(SSH_REQUEST_SF_LOCKED_BIT, &r->state);
		 
		smp_mb__before_atomic();
		clear_bit(SSH_REQUEST_SF_QUEUED_BIT, &r->state);

		list_move_tail(&r->node, &claimed);
	}
	spin_unlock(&rtl->queue.lock);

	 

	cancel_work_sync(&rtl->tx.work);
	ssh_ptl_shutdown(&rtl->ptl);
	cancel_delayed_work_sync(&rtl->rtx_timeout.reaper);

	 

	pending = atomic_read(&rtl->pending.count);
	if (WARN_ON(pending)) {
		spin_lock(&rtl->pending.lock);
		list_for_each_entry_safe(r, n, &rtl->pending.head, node) {
			set_bit(SSH_REQUEST_SF_LOCKED_BIT, &r->state);
			 
			smp_mb__before_atomic();
			clear_bit(SSH_REQUEST_SF_PENDING_BIT, &r->state);

			list_move_tail(&r->node, &claimed);
		}
		spin_unlock(&rtl->pending.lock);
	}

	 
	list_for_each_entry_safe(r, n, &claimed, node) {
		 
		if (!test_and_set_bit(SSH_REQUEST_SF_COMPLETED_BIT, &r->state))
			ssh_rtl_complete_with_status(r, -ESHUTDOWN);

		 
		list_del(&r->node);
		ssh_request_put(r);
	}
}
