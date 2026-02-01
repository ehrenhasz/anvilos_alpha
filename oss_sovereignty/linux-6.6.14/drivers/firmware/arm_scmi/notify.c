
 
 

#define dev_fmt(fmt) "SCMI Notifications - " fmt
#define pr_fmt(fmt) "SCMI Notifications - " fmt

#include <linux/bitfield.h>
#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/refcount.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "common.h"
#include "notify.h"

#define SCMI_MAX_PROTO		256

#define PROTO_ID_MASK		GENMASK(31, 24)
#define EVT_ID_MASK		GENMASK(23, 16)
#define SRC_ID_MASK		GENMASK(15, 0)

 
#define MAKE_HASH_KEY(p, e, s)			\
	(FIELD_PREP(PROTO_ID_MASK, (p)) |	\
	   FIELD_PREP(EVT_ID_MASK, (e)) |	\
	   FIELD_PREP(SRC_ID_MASK, (s)))

#define MAKE_ALL_SRCS_KEY(p, e)		MAKE_HASH_KEY((p), (e), SRC_ID_MASK)

 
#define KEY_FIND(__ht, __obj, __k)				\
({								\
	typeof(__k) k_ = __k;					\
	typeof(__obj) obj_;					\
								\
	hash_for_each_possible((__ht), obj_, hash, k_)		\
		if (obj_->key == k_)				\
			break;					\
	__obj = obj_;						\
})

#define KEY_XTRACT_PROTO_ID(key)	FIELD_GET(PROTO_ID_MASK, (key))
#define KEY_XTRACT_EVT_ID(key)		FIELD_GET(EVT_ID_MASK, (key))
#define KEY_XTRACT_SRC_ID(key)		FIELD_GET(SRC_ID_MASK, (key))

 
#define SCMI_GET_PROTO(__ni, __pid)					\
({									\
	typeof(__ni) ni_ = __ni;					\
	struct scmi_registered_events_desc *__pd = NULL;		\
									\
	if (ni_)							\
		__pd = READ_ONCE(ni_->registered_protocols[(__pid)]);	\
	__pd;								\
})

#define SCMI_GET_REVT_FROM_PD(__pd, __eid)				\
({									\
	typeof(__pd) pd_ = __pd;					\
	typeof(__eid) eid_ = __eid;					\
	struct scmi_registered_event *__revt = NULL;			\
									\
	if (pd_ && eid_ < pd_->num_events)				\
		__revt = READ_ONCE(pd_->registered_events[eid_]);	\
	__revt;								\
})

#define SCMI_GET_REVT(__ni, __pid, __eid)				\
({									\
	struct scmi_registered_event *__revt;				\
	struct scmi_registered_events_desc *__pd;			\
									\
	__pd = SCMI_GET_PROTO((__ni), (__pid));				\
	__revt = SCMI_GET_REVT_FROM_PD(__pd, (__eid));			\
	__revt;								\
})

 
#define REVT_NOTIFY_SET_STATUS(revt, eid, sid, state)		\
({								\
	typeof(revt) r = revt;					\
	r->proto->ops->set_notify_enabled(r->proto->ph,		\
					(eid), (sid), (state));	\
})

#define REVT_NOTIFY_ENABLE(revt, eid, sid)			\
	REVT_NOTIFY_SET_STATUS((revt), (eid), (sid), true)

#define REVT_NOTIFY_DISABLE(revt, eid, sid)			\
	REVT_NOTIFY_SET_STATUS((revt), (eid), (sid), false)

#define REVT_FILL_REPORT(revt, ...)				\
({								\
	typeof(revt) r = revt;					\
	r->proto->ops->fill_custom_report(r->proto->ph,		\
					  __VA_ARGS__);		\
})

#define SCMI_PENDING_HASH_SZ		4
#define SCMI_REGISTERED_HASH_SZ		6

struct scmi_registered_events_desc;

 
struct scmi_notify_instance {
	void			*gid;
	struct scmi_handle	*handle;
	struct work_struct	init_work;
	struct workqueue_struct	*notify_wq;
	 
	struct mutex		pending_mtx;
	struct scmi_registered_events_desc	**registered_protocols;
	DECLARE_HASHTABLE(pending_events_handlers, SCMI_PENDING_HASH_SZ);
};

 
struct events_queue {
	size_t			sz;
	struct kfifo		kfifo;
	struct work_struct	notify_work;
	struct workqueue_struct	*wq;
};

 
struct scmi_event_header {
	ktime_t timestamp;
	size_t payld_sz;
	unsigned char evt_id;
	unsigned char payld[];
};

struct scmi_registered_event;

 
struct scmi_registered_events_desc {
	u8				id;
	const struct scmi_event_ops	*ops;
	struct events_queue		equeue;
	struct scmi_notify_instance	*ni;
	struct scmi_event_header	*eh;
	size_t				eh_sz;
	void				*in_flight;
	int				num_events;
	struct scmi_registered_event	**registered_events;
	 
	struct mutex			registered_mtx;
	const struct scmi_protocol_handle	*ph;
	DECLARE_HASHTABLE(registered_events_handlers, SCMI_REGISTERED_HASH_SZ);
};

 
struct scmi_registered_event {
	struct scmi_registered_events_desc *proto;
	const struct scmi_event	*evt;
	void		*report;
	u32		num_sources;
	refcount_t	*sources;
	 
	struct mutex	sources_mtx;
};

 
struct scmi_event_handler {
	u32				key;
	refcount_t			users;
	struct scmi_registered_event	*r_evt;
	struct blocking_notifier_head	chain;
	struct hlist_node		hash;
	bool				enabled;
};

#define IS_HNDL_PENDING(hndl)	(!(hndl)->r_evt)

static struct scmi_event_handler *
scmi_get_active_handler(struct scmi_notify_instance *ni, u32 evt_key);
static void scmi_put_active_handler(struct scmi_notify_instance *ni,
				    struct scmi_event_handler *hndl);
static bool scmi_put_handler_unlocked(struct scmi_notify_instance *ni,
				      struct scmi_event_handler *hndl);

 
static inline void
scmi_lookup_and_call_event_chain(struct scmi_notify_instance *ni,
				 u32 evt_key, void *report)
{
	int ret;
	struct scmi_event_handler *hndl;

	 
	hndl = scmi_get_active_handler(ni, evt_key);
	if (!hndl)
		return;

	ret = blocking_notifier_call_chain(&hndl->chain,
					   KEY_XTRACT_EVT_ID(evt_key),
					   report);
	 
	WARN_ON_ONCE(ret & NOTIFY_STOP_MASK);

	scmi_put_active_handler(ni, hndl);
}

 
static inline struct scmi_registered_event *
scmi_process_event_header(struct events_queue *eq,
			  struct scmi_registered_events_desc *pd)
{
	unsigned int outs;
	struct scmi_registered_event *r_evt;

	outs = kfifo_out(&eq->kfifo, pd->eh,
			 sizeof(struct scmi_event_header));
	if (!outs)
		return NULL;
	if (outs != sizeof(struct scmi_event_header)) {
		dev_err(pd->ni->handle->dev, "corrupted EVT header. Flush.\n");
		kfifo_reset_out(&eq->kfifo);
		return NULL;
	}

	r_evt = SCMI_GET_REVT_FROM_PD(pd, pd->eh->evt_id);
	if (!r_evt)
		r_evt = ERR_PTR(-EINVAL);

	return r_evt;
}

 
static inline bool
scmi_process_event_payload(struct events_queue *eq,
			   struct scmi_registered_events_desc *pd,
			   struct scmi_registered_event *r_evt)
{
	u32 src_id, key;
	unsigned int outs;
	void *report = NULL;

	outs = kfifo_out(&eq->kfifo, pd->eh->payld, pd->eh->payld_sz);
	if (!outs)
		return false;

	 
	pd->in_flight = NULL;

	if (outs != pd->eh->payld_sz) {
		dev_err(pd->ni->handle->dev, "corrupted EVT Payload. Flush.\n");
		kfifo_reset_out(&eq->kfifo);
		return false;
	}

	if (IS_ERR(r_evt)) {
		dev_warn(pd->ni->handle->dev,
			 "SKIP UNKNOWN EVT - proto:%X  evt:%d\n",
			 pd->id, pd->eh->evt_id);
		return true;
	}

	report = REVT_FILL_REPORT(r_evt, pd->eh->evt_id, pd->eh->timestamp,
				  pd->eh->payld, pd->eh->payld_sz,
				  r_evt->report, &src_id);
	if (!report) {
		dev_err(pd->ni->handle->dev,
			"report not available - proto:%X  evt:%d\n",
			pd->id, pd->eh->evt_id);
		return true;
	}

	 
	key = MAKE_ALL_SRCS_KEY(pd->id, pd->eh->evt_id);
	scmi_lookup_and_call_event_chain(pd->ni, key, report);

	 
	key = MAKE_HASH_KEY(pd->id, pd->eh->evt_id, src_id);
	scmi_lookup_and_call_event_chain(pd->ni, key, report);

	return true;
}

 
static void scmi_events_dispatcher(struct work_struct *work)
{
	struct events_queue *eq;
	struct scmi_registered_events_desc *pd;
	struct scmi_registered_event *r_evt;

	eq = container_of(work, struct events_queue, notify_work);
	pd = container_of(eq, struct scmi_registered_events_desc, equeue);
	 
	do {
		if (!pd->in_flight) {
			r_evt = scmi_process_event_header(eq, pd);
			if (!r_evt)
				break;
			pd->in_flight = r_evt;
		} else {
			r_evt = pd->in_flight;
		}
	} while (scmi_process_event_payload(eq, pd, r_evt));
}

 
int scmi_notify(const struct scmi_handle *handle, u8 proto_id, u8 evt_id,
		const void *buf, size_t len, ktime_t ts)
{
	struct scmi_registered_event *r_evt;
	struct scmi_event_header eh;
	struct scmi_notify_instance *ni;

	ni = scmi_notification_instance_data_get(handle);
	if (!ni)
		return 0;

	r_evt = SCMI_GET_REVT(ni, proto_id, evt_id);
	if (!r_evt)
		return -EINVAL;

	if (len > r_evt->evt->max_payld_sz) {
		dev_err(handle->dev, "discard badly sized message\n");
		return -EINVAL;
	}
	if (kfifo_avail(&r_evt->proto->equeue.kfifo) < sizeof(eh) + len) {
		dev_warn(handle->dev,
			 "queue full, dropping proto_id:%d  evt_id:%d  ts:%lld\n",
			 proto_id, evt_id, ktime_to_ns(ts));
		return -ENOMEM;
	}

	eh.timestamp = ts;
	eh.evt_id = evt_id;
	eh.payld_sz = len;
	 
	kfifo_in(&r_evt->proto->equeue.kfifo, &eh, sizeof(eh));
	kfifo_in(&r_evt->proto->equeue.kfifo, buf, len);
	 
	queue_work(r_evt->proto->equeue.wq,
		   &r_evt->proto->equeue.notify_work);

	return 0;
}

 
static void scmi_kfifo_free(void *kfifo)
{
	kfifo_free((struct kfifo *)kfifo);
}

 
static int scmi_initialize_events_queue(struct scmi_notify_instance *ni,
					struct events_queue *equeue, size_t sz)
{
	int ret;

	if (kfifo_alloc(&equeue->kfifo, sz, GFP_KERNEL))
		return -ENOMEM;
	 
	equeue->sz = kfifo_size(&equeue->kfifo);

	ret = devm_add_action_or_reset(ni->handle->dev, scmi_kfifo_free,
				       &equeue->kfifo);
	if (ret)
		return ret;

	INIT_WORK(&equeue->notify_work, scmi_events_dispatcher);
	equeue->wq = ni->notify_wq;

	return ret;
}

 
static struct scmi_registered_events_desc *
scmi_allocate_registered_events_desc(struct scmi_notify_instance *ni,
				     u8 proto_id, size_t queue_sz, size_t eh_sz,
				     int num_events,
				     const struct scmi_event_ops *ops)
{
	int ret;
	struct scmi_registered_events_desc *pd;

	 
	smp_rmb();
	if (WARN_ON(ni->registered_protocols[proto_id]))
		return ERR_PTR(-EINVAL);

	pd = devm_kzalloc(ni->handle->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);
	pd->id = proto_id;
	pd->ops = ops;
	pd->ni = ni;

	ret = scmi_initialize_events_queue(ni, &pd->equeue, queue_sz);
	if (ret)
		return ERR_PTR(ret);

	pd->eh = devm_kzalloc(ni->handle->dev, eh_sz, GFP_KERNEL);
	if (!pd->eh)
		return ERR_PTR(-ENOMEM);
	pd->eh_sz = eh_sz;

	pd->registered_events = devm_kcalloc(ni->handle->dev, num_events,
					     sizeof(char *), GFP_KERNEL);
	if (!pd->registered_events)
		return ERR_PTR(-ENOMEM);
	pd->num_events = num_events;

	 
	mutex_init(&pd->registered_mtx);
	hash_init(pd->registered_events_handlers);

	return pd;
}

 
int scmi_register_protocol_events(const struct scmi_handle *handle, u8 proto_id,
				  const struct scmi_protocol_handle *ph,
				  const struct scmi_protocol_events *ee)
{
	int i;
	unsigned int num_sources;
	size_t payld_sz = 0;
	struct scmi_registered_events_desc *pd;
	struct scmi_notify_instance *ni;
	const struct scmi_event *evt;

	if (!ee || !ee->ops || !ee->evts || !ph ||
	    (!ee->num_sources && !ee->ops->get_num_sources))
		return -EINVAL;

	ni = scmi_notification_instance_data_get(handle);
	if (!ni)
		return -ENOMEM;

	 
	if (ee->num_sources) {
		num_sources = ee->num_sources;
	} else {
		int nsrc = ee->ops->get_num_sources(ph);

		if (nsrc <= 0)
			return -EINVAL;
		num_sources = nsrc;
	}

	evt = ee->evts;
	for (i = 0; i < ee->num_events; i++)
		payld_sz = max_t(size_t, payld_sz, evt[i].max_payld_sz);
	payld_sz += sizeof(struct scmi_event_header);

	pd = scmi_allocate_registered_events_desc(ni, proto_id, ee->queue_sz,
						  payld_sz, ee->num_events,
						  ee->ops);
	if (IS_ERR(pd))
		return PTR_ERR(pd);

	pd->ph = ph;
	for (i = 0; i < ee->num_events; i++, evt++) {
		struct scmi_registered_event *r_evt;

		r_evt = devm_kzalloc(ni->handle->dev, sizeof(*r_evt),
				     GFP_KERNEL);
		if (!r_evt)
			return -ENOMEM;
		r_evt->proto = pd;
		r_evt->evt = evt;

		r_evt->sources = devm_kcalloc(ni->handle->dev, num_sources,
					      sizeof(refcount_t), GFP_KERNEL);
		if (!r_evt->sources)
			return -ENOMEM;
		r_evt->num_sources = num_sources;
		mutex_init(&r_evt->sources_mtx);

		r_evt->report = devm_kzalloc(ni->handle->dev,
					     evt->max_report_sz, GFP_KERNEL);
		if (!r_evt->report)
			return -ENOMEM;

		pd->registered_events[i] = r_evt;
		 
		smp_wmb();
		dev_dbg(handle->dev, "registered event - %lX\n",
			MAKE_ALL_SRCS_KEY(r_evt->proto->id, r_evt->evt->id));
	}

	 
	ni->registered_protocols[proto_id] = pd;
	 
	smp_wmb();

	 
	schedule_work(&ni->init_work);

	return 0;
}

 
void scmi_deregister_protocol_events(const struct scmi_handle *handle,
				     u8 proto_id)
{
	struct scmi_notify_instance *ni;
	struct scmi_registered_events_desc *pd;

	ni = scmi_notification_instance_data_get(handle);
	if (!ni)
		return;

	pd = ni->registered_protocols[proto_id];
	if (!pd)
		return;

	ni->registered_protocols[proto_id] = NULL;
	 
	smp_wmb();

	cancel_work_sync(&pd->equeue.notify_work);
}

 
static struct scmi_event_handler *
scmi_allocate_event_handler(struct scmi_notify_instance *ni, u32 evt_key)
{
	struct scmi_event_handler *hndl;

	hndl = kzalloc(sizeof(*hndl), GFP_KERNEL);
	if (!hndl)
		return NULL;
	hndl->key = evt_key;
	BLOCKING_INIT_NOTIFIER_HEAD(&hndl->chain);
	refcount_set(&hndl->users, 1);
	 
	hash_add(ni->pending_events_handlers, &hndl->hash, hndl->key);

	return hndl;
}

 
static void scmi_free_event_handler(struct scmi_event_handler *hndl)
{
	hash_del(&hndl->hash);
	kfree(hndl);
}

 
static inline int scmi_bind_event_handler(struct scmi_notify_instance *ni,
					  struct scmi_event_handler *hndl)
{
	struct scmi_registered_event *r_evt;

	r_evt = SCMI_GET_REVT(ni, KEY_XTRACT_PROTO_ID(hndl->key),
			      KEY_XTRACT_EVT_ID(hndl->key));
	if (!r_evt)
		return -EINVAL;

	 
	hash_del(&hndl->hash);
	 
	scmi_protocol_acquire(ni->handle, KEY_XTRACT_PROTO_ID(hndl->key));
	hndl->r_evt = r_evt;

	mutex_lock(&r_evt->proto->registered_mtx);
	hash_add(r_evt->proto->registered_events_handlers,
		 &hndl->hash, hndl->key);
	mutex_unlock(&r_evt->proto->registered_mtx);

	return 0;
}

 
static inline int scmi_valid_pending_handler(struct scmi_notify_instance *ni,
					     struct scmi_event_handler *hndl)
{
	struct scmi_registered_events_desc *pd;

	if (!IS_HNDL_PENDING(hndl))
		return -EINVAL;

	pd = SCMI_GET_PROTO(ni, KEY_XTRACT_PROTO_ID(hndl->key));
	if (pd)
		return -EINVAL;

	return 0;
}

 
static int scmi_register_event_handler(struct scmi_notify_instance *ni,
				       struct scmi_event_handler *hndl)
{
	int ret;

	ret = scmi_bind_event_handler(ni, hndl);
	if (!ret) {
		dev_dbg(ni->handle->dev, "registered NEW handler - key:%X\n",
			hndl->key);
	} else {
		ret = scmi_valid_pending_handler(ni, hndl);
		if (!ret)
			dev_dbg(ni->handle->dev,
				"registered PENDING handler - key:%X\n",
				hndl->key);
	}

	return ret;
}

 
static inline struct scmi_event_handler *
__scmi_event_handler_get_ops(struct scmi_notify_instance *ni,
			     u32 evt_key, bool create)
{
	struct scmi_registered_event *r_evt;
	struct scmi_event_handler *hndl = NULL;

	r_evt = SCMI_GET_REVT(ni, KEY_XTRACT_PROTO_ID(evt_key),
			      KEY_XTRACT_EVT_ID(evt_key));

	mutex_lock(&ni->pending_mtx);
	 
	if (r_evt) {
		mutex_lock(&r_evt->proto->registered_mtx);
		hndl = KEY_FIND(r_evt->proto->registered_events_handlers,
				hndl, evt_key);
		if (hndl)
			refcount_inc(&hndl->users);
		mutex_unlock(&r_evt->proto->registered_mtx);
	}

	 
	if (!hndl) {
		hndl = KEY_FIND(ni->pending_events_handlers, hndl, evt_key);
		if (hndl)
			refcount_inc(&hndl->users);
	}

	 
	if (!hndl && create) {
		hndl = scmi_allocate_event_handler(ni, evt_key);
		if (hndl && scmi_register_event_handler(ni, hndl)) {
			dev_dbg(ni->handle->dev,
				"purging UNKNOWN handler - key:%X\n",
				hndl->key);
			 
			scmi_put_handler_unlocked(ni, hndl);
			hndl = NULL;
		}
	}
	mutex_unlock(&ni->pending_mtx);

	return hndl;
}

static struct scmi_event_handler *
scmi_get_handler(struct scmi_notify_instance *ni, u32 evt_key)
{
	return __scmi_event_handler_get_ops(ni, evt_key, false);
}

static struct scmi_event_handler *
scmi_get_or_create_handler(struct scmi_notify_instance *ni, u32 evt_key)
{
	return __scmi_event_handler_get_ops(ni, evt_key, true);
}

 
static struct scmi_event_handler *
scmi_get_active_handler(struct scmi_notify_instance *ni, u32 evt_key)
{
	struct scmi_registered_event *r_evt;
	struct scmi_event_handler *hndl = NULL;

	r_evt = SCMI_GET_REVT(ni, KEY_XTRACT_PROTO_ID(evt_key),
			      KEY_XTRACT_EVT_ID(evt_key));
	if (r_evt) {
		mutex_lock(&r_evt->proto->registered_mtx);
		hndl = KEY_FIND(r_evt->proto->registered_events_handlers,
				hndl, evt_key);
		if (hndl)
			refcount_inc(&hndl->users);
		mutex_unlock(&r_evt->proto->registered_mtx);
	}

	return hndl;
}

 
static inline int __scmi_enable_evt(struct scmi_registered_event *r_evt,
				    u32 src_id, bool enable)
{
	int retvals = 0;
	u32 num_sources;
	refcount_t *sid;

	if (src_id == SRC_ID_MASK) {
		src_id = 0;
		num_sources = r_evt->num_sources;
	} else if (src_id < r_evt->num_sources) {
		num_sources = 1;
	} else {
		return -EINVAL;
	}

	mutex_lock(&r_evt->sources_mtx);
	if (enable) {
		for (; num_sources; src_id++, num_sources--) {
			int ret = 0;

			sid = &r_evt->sources[src_id];
			if (refcount_read(sid) == 0) {
				ret = REVT_NOTIFY_ENABLE(r_evt, r_evt->evt->id,
							 src_id);
				if (!ret)
					refcount_set(sid, 1);
			} else {
				refcount_inc(sid);
			}
			retvals += !ret;
		}
	} else {
		for (; num_sources; src_id++, num_sources--) {
			sid = &r_evt->sources[src_id];
			if (refcount_dec_and_test(sid))
				REVT_NOTIFY_DISABLE(r_evt,
						    r_evt->evt->id, src_id);
		}
		retvals = 1;
	}
	mutex_unlock(&r_evt->sources_mtx);

	return retvals ? 0 : -EINVAL;
}

static int scmi_enable_events(struct scmi_event_handler *hndl)
{
	int ret = 0;

	if (!hndl->enabled) {
		ret = __scmi_enable_evt(hndl->r_evt,
					KEY_XTRACT_SRC_ID(hndl->key), true);
		if (!ret)
			hndl->enabled = true;
	}

	return ret;
}

static int scmi_disable_events(struct scmi_event_handler *hndl)
{
	int ret = 0;

	if (hndl->enabled) {
		ret = __scmi_enable_evt(hndl->r_evt,
					KEY_XTRACT_SRC_ID(hndl->key), false);
		if (!ret)
			hndl->enabled = false;
	}

	return ret;
}

 
static bool scmi_put_handler_unlocked(struct scmi_notify_instance *ni,
				      struct scmi_event_handler *hndl)
{
	bool freed = false;

	if (refcount_dec_and_test(&hndl->users)) {
		if (!IS_HNDL_PENDING(hndl))
			scmi_disable_events(hndl);
		scmi_free_event_handler(hndl);
		freed = true;
	}

	return freed;
}

static void scmi_put_handler(struct scmi_notify_instance *ni,
			     struct scmi_event_handler *hndl)
{
	bool freed;
	u8 protocol_id;
	struct scmi_registered_event *r_evt = hndl->r_evt;

	mutex_lock(&ni->pending_mtx);
	if (r_evt) {
		protocol_id = r_evt->proto->id;
		mutex_lock(&r_evt->proto->registered_mtx);
	}

	freed = scmi_put_handler_unlocked(ni, hndl);

	if (r_evt) {
		mutex_unlock(&r_evt->proto->registered_mtx);
		 
		if (freed)
			scmi_protocol_release(ni->handle, protocol_id);
	}
	mutex_unlock(&ni->pending_mtx);
}

static void scmi_put_active_handler(struct scmi_notify_instance *ni,
				    struct scmi_event_handler *hndl)
{
	bool freed;
	struct scmi_registered_event *r_evt = hndl->r_evt;
	u8 protocol_id = r_evt->proto->id;

	mutex_lock(&r_evt->proto->registered_mtx);
	freed = scmi_put_handler_unlocked(ni, hndl);
	mutex_unlock(&r_evt->proto->registered_mtx);
	if (freed)
		scmi_protocol_release(ni->handle, protocol_id);
}

 
static int scmi_event_handler_enable_events(struct scmi_event_handler *hndl)
{
	if (scmi_enable_events(hndl)) {
		pr_err("Failed to ENABLE events for key:%X !\n", hndl->key);
		return -EINVAL;
	}

	return 0;
}

 
static int scmi_notifier_register(const struct scmi_handle *handle,
				  u8 proto_id, u8 evt_id, const u32 *src_id,
				  struct notifier_block *nb)
{
	int ret = 0;
	u32 evt_key;
	struct scmi_event_handler *hndl;
	struct scmi_notify_instance *ni;

	ni = scmi_notification_instance_data_get(handle);
	if (!ni)
		return -ENODEV;

	evt_key = MAKE_HASH_KEY(proto_id, evt_id,
				src_id ? *src_id : SRC_ID_MASK);
	hndl = scmi_get_or_create_handler(ni, evt_key);
	if (!hndl)
		return -EINVAL;

	blocking_notifier_chain_register(&hndl->chain, nb);

	 
	if (!IS_HNDL_PENDING(hndl)) {
		ret = scmi_event_handler_enable_events(hndl);
		if (ret)
			scmi_put_handler(ni, hndl);
	}

	return ret;
}

 
static int scmi_notifier_unregister(const struct scmi_handle *handle,
				    u8 proto_id, u8 evt_id, const u32 *src_id,
				    struct notifier_block *nb)
{
	u32 evt_key;
	struct scmi_event_handler *hndl;
	struct scmi_notify_instance *ni;

	ni = scmi_notification_instance_data_get(handle);
	if (!ni)
		return -ENODEV;

	evt_key = MAKE_HASH_KEY(proto_id, evt_id,
				src_id ? *src_id : SRC_ID_MASK);
	hndl = scmi_get_handler(ni, evt_key);
	if (!hndl)
		return -EINVAL;

	 
	blocking_notifier_chain_unregister(&hndl->chain, nb);
	scmi_put_handler(ni, hndl);

	 
	scmi_put_handler(ni, hndl);

	return 0;
}

struct scmi_notifier_devres {
	const struct scmi_handle *handle;
	u8 proto_id;
	u8 evt_id;
	u32 __src_id;
	u32 *src_id;
	struct notifier_block *nb;
};

static void scmi_devm_release_notifier(struct device *dev, void *res)
{
	struct scmi_notifier_devres *dres = res;

	scmi_notifier_unregister(dres->handle, dres->proto_id, dres->evt_id,
				 dres->src_id, dres->nb);
}

 
static int scmi_devm_notifier_register(struct scmi_device *sdev,
				       u8 proto_id, u8 evt_id,
				       const u32 *src_id,
				       struct notifier_block *nb)
{
	int ret;
	struct scmi_notifier_devres *dres;

	dres = devres_alloc(scmi_devm_release_notifier,
			    sizeof(*dres), GFP_KERNEL);
	if (!dres)
		return -ENOMEM;

	ret = scmi_notifier_register(sdev->handle, proto_id,
				     evt_id, src_id, nb);
	if (ret) {
		devres_free(dres);
		return ret;
	}

	dres->handle = sdev->handle;
	dres->proto_id = proto_id;
	dres->evt_id = evt_id;
	dres->nb = nb;
	if (src_id) {
		dres->__src_id = *src_id;
		dres->src_id = &dres->__src_id;
	} else {
		dres->src_id = NULL;
	}
	devres_add(&sdev->dev, dres);

	return ret;
}

static int scmi_devm_notifier_match(struct device *dev, void *res, void *data)
{
	struct scmi_notifier_devres *dres = res;
	struct scmi_notifier_devres *xres = data;

	if (WARN_ON(!dres || !xres))
		return 0;

	return dres->proto_id == xres->proto_id &&
		dres->evt_id == xres->evt_id &&
		dres->nb == xres->nb &&
		((!dres->src_id && !xres->src_id) ||
		  (dres->src_id && xres->src_id &&
		   dres->__src_id == xres->__src_id));
}

 
static int scmi_devm_notifier_unregister(struct scmi_device *sdev,
					 u8 proto_id, u8 evt_id,
					 const u32 *src_id,
					 struct notifier_block *nb)
{
	int ret;
	struct scmi_notifier_devres dres;

	dres.handle = sdev->handle;
	dres.proto_id = proto_id;
	dres.evt_id = evt_id;
	if (src_id) {
		dres.__src_id = *src_id;
		dres.src_id = &dres.__src_id;
	} else {
		dres.src_id = NULL;
	}

	ret = devres_release(&sdev->dev, scmi_devm_release_notifier,
			     scmi_devm_notifier_match, &dres);

	WARN_ON(ret);

	return ret;
}

 
static void scmi_protocols_late_init(struct work_struct *work)
{
	int bkt;
	struct scmi_event_handler *hndl;
	struct scmi_notify_instance *ni;
	struct hlist_node *tmp;

	ni = container_of(work, struct scmi_notify_instance, init_work);

	 
	smp_rmb();

	mutex_lock(&ni->pending_mtx);
	hash_for_each_safe(ni->pending_events_handlers, bkt, tmp, hndl, hash) {
		int ret;

		ret = scmi_bind_event_handler(ni, hndl);
		if (!ret) {
			dev_dbg(ni->handle->dev,
				"finalized PENDING handler - key:%X\n",
				hndl->key);
			ret = scmi_event_handler_enable_events(hndl);
			if (ret) {
				dev_dbg(ni->handle->dev,
					"purging INVALID handler - key:%X\n",
					hndl->key);
				scmi_put_active_handler(ni, hndl);
			}
		} else {
			ret = scmi_valid_pending_handler(ni, hndl);
			if (ret) {
				dev_dbg(ni->handle->dev,
					"purging PENDING handler - key:%X\n",
					hndl->key);
				 
				scmi_put_handler_unlocked(ni, hndl);
			}
		}
	}
	mutex_unlock(&ni->pending_mtx);
}

 
static const struct scmi_notify_ops notify_ops = {
	.devm_event_notifier_register = scmi_devm_notifier_register,
	.devm_event_notifier_unregister = scmi_devm_notifier_unregister,
	.event_notifier_register = scmi_notifier_register,
	.event_notifier_unregister = scmi_notifier_unregister,
};

 
int scmi_notification_init(struct scmi_handle *handle)
{
	void *gid;
	struct scmi_notify_instance *ni;

	gid = devres_open_group(handle->dev, NULL, GFP_KERNEL);
	if (!gid)
		return -ENOMEM;

	ni = devm_kzalloc(handle->dev, sizeof(*ni), GFP_KERNEL);
	if (!ni)
		goto err;

	ni->gid = gid;
	ni->handle = handle;

	ni->registered_protocols = devm_kcalloc(handle->dev, SCMI_MAX_PROTO,
						sizeof(char *), GFP_KERNEL);
	if (!ni->registered_protocols)
		goto err;

	ni->notify_wq = alloc_workqueue(dev_name(handle->dev),
					WQ_UNBOUND | WQ_FREEZABLE | WQ_SYSFS,
					0);
	if (!ni->notify_wq)
		goto err;

	mutex_init(&ni->pending_mtx);
	hash_init(ni->pending_events_handlers);

	INIT_WORK(&ni->init_work, scmi_protocols_late_init);

	scmi_notification_instance_data_set(handle, ni);
	handle->notify_ops = &notify_ops;
	 
	smp_wmb();

	dev_info(handle->dev, "Core Enabled.\n");

	devres_close_group(handle->dev, ni->gid);

	return 0;

err:
	dev_warn(handle->dev, "Initialization Failed.\n");
	devres_release_group(handle->dev, gid);
	return -ENOMEM;
}

 
void scmi_notification_exit(struct scmi_handle *handle)
{
	struct scmi_notify_instance *ni;

	ni = scmi_notification_instance_data_get(handle);
	if (!ni)
		return;
	scmi_notification_instance_data_set(handle, NULL);

	 
	destroy_workqueue(ni->notify_wq);

	devres_release_group(ni->handle->dev, ni->gid);
}
