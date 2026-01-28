#ifndef _DRBD_REQ_H
#define _DRBD_REQ_H
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/drbd.h>
#include "drbd_int.h"
enum drbd_req_event {
	CREATED,
	TO_BE_SENT,
	TO_BE_SUBMITTED,
	QUEUE_FOR_NET_WRITE,
	QUEUE_FOR_NET_READ,
	QUEUE_FOR_SEND_OOS,
	QUEUE_AS_DRBD_BARRIER,
	SEND_CANCELED,
	SEND_FAILED,
	HANDED_OVER_TO_NETWORK,
	OOS_HANDED_TO_NETWORK,
	CONNECTION_LOST_WHILE_PENDING,
	READ_RETRY_REMOTE_CANCELED,
	RECV_ACKED_BY_PEER,
	WRITE_ACKED_BY_PEER,
	WRITE_ACKED_BY_PEER_AND_SIS,  
	CONFLICT_RESOLVED,
	POSTPONE_WRITE,
	NEG_ACKED,
	BARRIER_ACKED,  
	DATA_RECEIVED,  
	COMPLETED_OK,
	READ_COMPLETED_WITH_ERROR,
	READ_AHEAD_COMPLETED_WITH_ERROR,
	WRITE_COMPLETED_WITH_ERROR,
	DISCARD_COMPLETED_NOTSUPP,
	DISCARD_COMPLETED_WITH_ERROR,
	ABORT_DISK_IO,
	RESEND,
	FAIL_FROZEN_DISK_IO,
	RESTART_FROZEN_DISK_IO,
	NOTHING,
};
enum drbd_req_state_bits {
	__RQ_LOCAL_PENDING,
	__RQ_LOCAL_COMPLETED,
	__RQ_LOCAL_OK,
	__RQ_LOCAL_ABORTED,
	__RQ_NET_PENDING,
	__RQ_NET_QUEUED,
	__RQ_NET_SENT,
	__RQ_NET_DONE,
	__RQ_NET_OK,
	__RQ_NET_SIS,
	__RQ_NET_MAX,
	__RQ_WRITE,
	__RQ_WSAME,
	__RQ_UNMAP,
	__RQ_ZEROES,
	__RQ_IN_ACT_LOG,
	__RQ_UNPLUG,
	__RQ_POSTPONED,
	__RQ_COMPLETION_SUSP,
	__RQ_EXP_RECEIVE_ACK,
	__RQ_EXP_WRITE_ACK,
	__RQ_EXP_BARR_ACK,
};
#define RQ_LOCAL_PENDING   (1UL << __RQ_LOCAL_PENDING)
#define RQ_LOCAL_COMPLETED (1UL << __RQ_LOCAL_COMPLETED)
#define RQ_LOCAL_OK        (1UL << __RQ_LOCAL_OK)
#define RQ_LOCAL_ABORTED   (1UL << __RQ_LOCAL_ABORTED)
#define RQ_LOCAL_MASK      ((RQ_LOCAL_ABORTED << 1)-1)
#define RQ_NET_PENDING     (1UL << __RQ_NET_PENDING)
#define RQ_NET_QUEUED      (1UL << __RQ_NET_QUEUED)
#define RQ_NET_SENT        (1UL << __RQ_NET_SENT)
#define RQ_NET_DONE        (1UL << __RQ_NET_DONE)
#define RQ_NET_OK          (1UL << __RQ_NET_OK)
#define RQ_NET_SIS         (1UL << __RQ_NET_SIS)
#define RQ_NET_MASK        (((1UL << __RQ_NET_MAX)-1) & ~RQ_LOCAL_MASK)
#define RQ_WRITE           (1UL << __RQ_WRITE)
#define RQ_WSAME           (1UL << __RQ_WSAME)
#define RQ_UNMAP           (1UL << __RQ_UNMAP)
#define RQ_ZEROES          (1UL << __RQ_ZEROES)
#define RQ_IN_ACT_LOG      (1UL << __RQ_IN_ACT_LOG)
#define RQ_UNPLUG          (1UL << __RQ_UNPLUG)
#define RQ_POSTPONED	   (1UL << __RQ_POSTPONED)
#define RQ_COMPLETION_SUSP (1UL << __RQ_COMPLETION_SUSP)
#define RQ_EXP_RECEIVE_ACK (1UL << __RQ_EXP_RECEIVE_ACK)
#define RQ_EXP_WRITE_ACK   (1UL << __RQ_EXP_WRITE_ACK)
#define RQ_EXP_BARR_ACK    (1UL << __RQ_EXP_BARR_ACK)
#define MR_WRITE       1
#define MR_READ        2
struct bio_and_error {
	struct bio *bio;
	int error;
};
extern void start_new_tl_epoch(struct drbd_connection *connection);
extern void drbd_req_destroy(struct kref *kref);
extern int __req_mod(struct drbd_request *req, enum drbd_req_event what,
		struct drbd_peer_device *peer_device,
		struct bio_and_error *m);
extern void complete_master_bio(struct drbd_device *device,
		struct bio_and_error *m);
extern void request_timer_fn(struct timer_list *t);
extern void tl_restart(struct drbd_connection *connection, enum drbd_req_event what);
extern void _tl_restart(struct drbd_connection *connection, enum drbd_req_event what);
extern void tl_abort_disk_io(struct drbd_device *device);
extern void drbd_restart_request(struct drbd_request *req);
static inline int _req_mod(struct drbd_request *req, enum drbd_req_event what,
		struct drbd_peer_device *peer_device)
{
	struct drbd_device *device = req->device;
	struct bio_and_error m;
	int rv;
	rv = __req_mod(req, what, peer_device, &m);
	if (m.bio)
		complete_master_bio(device, &m);
	return rv;
}
static inline int req_mod(struct drbd_request *req,
		enum drbd_req_event what,
		struct drbd_peer_device *peer_device)
{
	unsigned long flags;
	struct drbd_device *device = req->device;
	struct bio_and_error m;
	int rv;
	spin_lock_irqsave(&device->resource->req_lock, flags);
	rv = __req_mod(req, what, peer_device, &m);
	spin_unlock_irqrestore(&device->resource->req_lock, flags);
	if (m.bio)
		complete_master_bio(device, &m);
	return rv;
}
extern bool drbd_should_do_remote(union drbd_dev_state);
#endif
