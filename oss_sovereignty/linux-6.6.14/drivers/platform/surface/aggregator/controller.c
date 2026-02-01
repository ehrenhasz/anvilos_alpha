
 

#include <linux/acpi.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kref.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/mutex.h>
#include <linux/rculist.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/serdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/serial_hub.h>

#include "controller.h"
#include "ssh_msgb.h"
#include "ssh_request_layer.h"

#include "trace.h"


 

 
static void ssh_seq_reset(struct ssh_seq_counter *c)
{
	WRITE_ONCE(c->value, 0);
}

 
static u8 ssh_seq_next(struct ssh_seq_counter *c)
{
	u8 old = READ_ONCE(c->value);
	u8 new = old + 1;
	u8 ret;

	while (unlikely((ret = cmpxchg(&c->value, old, new)) != old)) {
		old = ret;
		new = old + 1;
	}

	return old;
}

 
static void ssh_rqid_reset(struct ssh_rqid_counter *c)
{
	WRITE_ONCE(c->value, 0);
}

 
static u16 ssh_rqid_next(struct ssh_rqid_counter *c)
{
	u16 old = READ_ONCE(c->value);
	u16 new = ssh_rqid_next_valid(old);
	u16 ret;

	while (unlikely((ret = cmpxchg(&c->value, old, new)) != old)) {
		old = ret;
		new = ssh_rqid_next_valid(old);
	}

	return old;
}


 
 

 
static bool ssam_event_matches_notifier(const struct ssam_event_notifier *n,
					const struct ssam_event *event)
{
	bool match = n->event.id.target_category == event->target_category;

	if (n->event.mask & SSAM_EVENT_MASK_TARGET)
		match &= n->event.reg.target_id == event->target_id;

	if (n->event.mask & SSAM_EVENT_MASK_INSTANCE)
		match &= n->event.id.instance == event->instance_id;

	return match;
}

 
static int ssam_nfblk_call_chain(struct ssam_nf_head *nh, struct ssam_event *event)
{
	struct ssam_event_notifier *nf;
	int ret = 0, idx;

	idx = srcu_read_lock(&nh->srcu);

	list_for_each_entry_rcu(nf, &nh->head, base.node,
				srcu_read_lock_held(&nh->srcu)) {
		if (ssam_event_matches_notifier(nf, event)) {
			ret = (ret & SSAM_NOTIF_STATE_MASK) | nf->base.fn(nf, event);
			if (ret & SSAM_NOTIF_STOP)
				break;
		}
	}

	srcu_read_unlock(&nh->srcu, idx);
	return ret;
}

 
static int ssam_nfblk_insert(struct ssam_nf_head *nh, struct ssam_notifier_block *nb)
{
	struct ssam_notifier_block *p;
	struct list_head *h;

	 
	list_for_each(h, &nh->head) {
		p = list_entry(h, struct ssam_notifier_block, node);

		if (unlikely(p == nb)) {
			WARN(1, "double register detected");
			return -EEXIST;
		}

		if (nb->priority > p->priority)
			break;
	}

	list_add_tail_rcu(&nb->node, h);
	return 0;
}

 
static bool ssam_nfblk_find(struct ssam_nf_head *nh, struct ssam_notifier_block *nb)
{
	struct ssam_notifier_block *p;

	 
	list_for_each_entry(p, &nh->head, node) {
		if (p == nb)
			return true;
	}

	return false;
}

 
static void ssam_nfblk_remove(struct ssam_notifier_block *nb)
{
	list_del_rcu(&nb->node);
}

 
static int ssam_nf_head_init(struct ssam_nf_head *nh)
{
	int status;

	status = init_srcu_struct(&nh->srcu);
	if (status)
		return status;

	INIT_LIST_HEAD(&nh->head);
	return 0;
}

 
static void ssam_nf_head_destroy(struct ssam_nf_head *nh)
{
	cleanup_srcu_struct(&nh->srcu);
}


 

 
struct ssam_nf_refcount_key {
	struct ssam_event_registry reg;
	struct ssam_event_id id;
};

 
struct ssam_nf_refcount_entry {
	struct rb_node node;
	struct ssam_nf_refcount_key key;
	int refcount;
	u8 flags;
};

 
static struct ssam_nf_refcount_entry *
ssam_nf_refcount_inc(struct ssam_nf *nf, struct ssam_event_registry reg,
		     struct ssam_event_id id)
{
	struct ssam_nf_refcount_entry *entry;
	struct ssam_nf_refcount_key key;
	struct rb_node **link = &nf->refcount.rb_node;
	struct rb_node *parent = NULL;
	int cmp;

	lockdep_assert_held(&nf->lock);

	key.reg = reg;
	key.id = id;

	while (*link) {
		entry = rb_entry(*link, struct ssam_nf_refcount_entry, node);
		parent = *link;

		cmp = memcmp(&key, &entry->key, sizeof(key));
		if (cmp < 0) {
			link = &(*link)->rb_left;
		} else if (cmp > 0) {
			link = &(*link)->rb_right;
		} else if (entry->refcount < INT_MAX) {
			entry->refcount++;
			return entry;
		} else {
			WARN_ON(1);
			return ERR_PTR(-ENOSPC);
		}
	}

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return ERR_PTR(-ENOMEM);

	entry->key = key;
	entry->refcount = 1;

	rb_link_node(&entry->node, parent, link);
	rb_insert_color(&entry->node, &nf->refcount);

	return entry;
}

 
static struct ssam_nf_refcount_entry *
ssam_nf_refcount_dec(struct ssam_nf *nf, struct ssam_event_registry reg,
		     struct ssam_event_id id)
{
	struct ssam_nf_refcount_entry *entry;
	struct ssam_nf_refcount_key key;
	struct rb_node *node = nf->refcount.rb_node;
	int cmp;

	lockdep_assert_held(&nf->lock);

	key.reg = reg;
	key.id = id;

	while (node) {
		entry = rb_entry(node, struct ssam_nf_refcount_entry, node);

		cmp = memcmp(&key, &entry->key, sizeof(key));
		if (cmp < 0) {
			node = node->rb_left;
		} else if (cmp > 0) {
			node = node->rb_right;
		} else {
			entry->refcount--;
			if (entry->refcount == 0)
				rb_erase(&entry->node, &nf->refcount);

			return entry;
		}
	}

	return NULL;
}

 
static void ssam_nf_refcount_dec_free(struct ssam_nf *nf,
				      struct ssam_event_registry reg,
				      struct ssam_event_id id)
{
	struct ssam_nf_refcount_entry *entry;

	lockdep_assert_held(&nf->lock);

	entry = ssam_nf_refcount_dec(nf, reg, id);
	if (entry && entry->refcount == 0)
		kfree(entry);
}

 
static bool ssam_nf_refcount_empty(struct ssam_nf *nf)
{
	return RB_EMPTY_ROOT(&nf->refcount);
}

 
static void ssam_nf_call(struct ssam_nf *nf, struct device *dev, u16 rqid,
			 struct ssam_event *event)
{
	struct ssam_nf_head *nf_head;
	int status, nf_ret;

	if (!ssh_rqid_is_event(rqid)) {
		dev_warn(dev, "event: unsupported rqid: %#06x\n", rqid);
		return;
	}

	nf_head = &nf->head[ssh_rqid_to_event(rqid)];
	nf_ret = ssam_nfblk_call_chain(nf_head, event);
	status = ssam_notifier_to_errno(nf_ret);

	if (status < 0) {
		dev_err(dev,
			"event: error handling event: %d (tc: %#04x, tid: %#04x, cid: %#04x, iid: %#04x)\n",
			status, event->target_category, event->target_id,
			event->command_id, event->instance_id);
	} else if (!(nf_ret & SSAM_NOTIF_HANDLED)) {
		dev_warn(dev,
			 "event: unhandled event (rqid: %#04x, tc: %#04x, tid: %#04x, cid: %#04x, iid: %#04x)\n",
			 rqid, event->target_category, event->target_id,
			 event->command_id, event->instance_id);
	}
}

 
static int ssam_nf_init(struct ssam_nf *nf)
{
	int i, status;

	for (i = 0; i < SSH_NUM_EVENTS; i++) {
		status = ssam_nf_head_init(&nf->head[i]);
		if (status)
			break;
	}

	if (status) {
		while (i--)
			ssam_nf_head_destroy(&nf->head[i]);

		return status;
	}

	mutex_init(&nf->lock);
	return 0;
}

 
static void ssam_nf_destroy(struct ssam_nf *nf)
{
	int i;

	for (i = 0; i < SSH_NUM_EVENTS; i++)
		ssam_nf_head_destroy(&nf->head[i]);

	mutex_destroy(&nf->lock);
}


 

#define SSAM_CPLT_WQ_NAME	"ssam_cpltq"

 
#define SSAM_CPLT_WQ_BATCH	10

 
#define SSAM_EVENT_ITEM_CACHE_PAYLOAD_LEN	32

static struct kmem_cache *ssam_event_item_cache;

 
int ssam_event_item_cache_init(void)
{
	const unsigned int size = sizeof(struct ssam_event_item)
				  + SSAM_EVENT_ITEM_CACHE_PAYLOAD_LEN;
	const unsigned int align = __alignof__(struct ssam_event_item);
	struct kmem_cache *cache;

	cache = kmem_cache_create("ssam_event_item", size, align, 0, NULL);
	if (!cache)
		return -ENOMEM;

	ssam_event_item_cache = cache;
	return 0;
}

 
void ssam_event_item_cache_destroy(void)
{
	kmem_cache_destroy(ssam_event_item_cache);
	ssam_event_item_cache = NULL;
}

static void __ssam_event_item_free_cached(struct ssam_event_item *item)
{
	kmem_cache_free(ssam_event_item_cache, item);
}

static void __ssam_event_item_free_generic(struct ssam_event_item *item)
{
	kfree(item);
}

 
static void ssam_event_item_free(struct ssam_event_item *item)
{
	trace_ssam_event_item_free(item);
	item->ops.free(item);
}

 
static struct ssam_event_item *ssam_event_item_alloc(size_t len, gfp_t flags)
{
	struct ssam_event_item *item;

	if (len <= SSAM_EVENT_ITEM_CACHE_PAYLOAD_LEN) {
		item = kmem_cache_alloc(ssam_event_item_cache, flags);
		if (!item)
			return NULL;

		item->ops.free = __ssam_event_item_free_cached;
	} else {
		item = kzalloc(struct_size(item, event.data, len), flags);
		if (!item)
			return NULL;

		item->ops.free = __ssam_event_item_free_generic;
	}

	item->event.length = len;

	trace_ssam_event_item_alloc(item, len);
	return item;
}

 
static void ssam_event_queue_push(struct ssam_event_queue *q,
				  struct ssam_event_item *item)
{
	spin_lock(&q->lock);
	list_add_tail(&item->node, &q->head);
	spin_unlock(&q->lock);
}

 
static struct ssam_event_item *ssam_event_queue_pop(struct ssam_event_queue *q)
{
	struct ssam_event_item *item;

	spin_lock(&q->lock);
	item = list_first_entry_or_null(&q->head, struct ssam_event_item, node);
	if (item)
		list_del(&item->node);
	spin_unlock(&q->lock);

	return item;
}

 
static bool ssam_event_queue_is_empty(struct ssam_event_queue *q)
{
	bool empty;

	spin_lock(&q->lock);
	empty = list_empty(&q->head);
	spin_unlock(&q->lock);

	return empty;
}

 
static
struct ssam_event_queue *ssam_cplt_get_event_queue(struct ssam_cplt *cplt,
						   u8 tid, u16 rqid)
{
	u16 event = ssh_rqid_to_event(rqid);
	u16 tidx = ssh_tid_to_index(tid);

	if (!ssh_rqid_is_event(rqid)) {
		dev_err(cplt->dev, "event: unsupported request ID: %#06x\n", rqid);
		return NULL;
	}

	if (!ssh_tid_is_valid(tid)) {
		dev_warn(cplt->dev, "event: unsupported target ID: %u\n", tid);
		tidx = 0;
	}

	return &cplt->event.target[tidx].queue[event];
}

 
static bool ssam_cplt_submit(struct ssam_cplt *cplt, struct work_struct *work)
{
	return queue_work(cplt->wq, work);
}

 
static int ssam_cplt_submit_event(struct ssam_cplt *cplt,
				  struct ssam_event_item *item)
{
	struct ssam_event_queue *evq;

	evq = ssam_cplt_get_event_queue(cplt, item->event.target_id, item->rqid);
	if (!evq)
		return -EINVAL;

	ssam_event_queue_push(evq, item);
	ssam_cplt_submit(cplt, &evq->work);
	return 0;
}

 
static void ssam_cplt_flush(struct ssam_cplt *cplt)
{
	flush_workqueue(cplt->wq);
}

static void ssam_event_queue_work_fn(struct work_struct *work)
{
	struct ssam_event_queue *queue;
	struct ssam_event_item *item;
	struct ssam_nf *nf;
	struct device *dev;
	unsigned int iterations = SSAM_CPLT_WQ_BATCH;

	queue = container_of(work, struct ssam_event_queue, work);
	nf = &queue->cplt->event.notif;
	dev = queue->cplt->dev;

	 
	do {
		item = ssam_event_queue_pop(queue);
		if (!item)
			return;

		ssam_nf_call(nf, dev, item->rqid, &item->event);
		ssam_event_item_free(item);
	} while (--iterations);

	if (!ssam_event_queue_is_empty(queue))
		ssam_cplt_submit(queue->cplt, &queue->work);
}

 
static void ssam_event_queue_init(struct ssam_cplt *cplt,
				  struct ssam_event_queue *evq)
{
	evq->cplt = cplt;
	spin_lock_init(&evq->lock);
	INIT_LIST_HEAD(&evq->head);
	INIT_WORK(&evq->work, ssam_event_queue_work_fn);
}

 
static int ssam_cplt_init(struct ssam_cplt *cplt, struct device *dev)
{
	struct ssam_event_target *target;
	int status, c, i;

	cplt->dev = dev;

	cplt->wq = alloc_workqueue(SSAM_CPLT_WQ_NAME, WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
	if (!cplt->wq)
		return -ENOMEM;

	for (c = 0; c < ARRAY_SIZE(cplt->event.target); c++) {
		target = &cplt->event.target[c];

		for (i = 0; i < ARRAY_SIZE(target->queue); i++)
			ssam_event_queue_init(cplt, &target->queue[i]);
	}

	status = ssam_nf_init(&cplt->event.notif);
	if (status)
		destroy_workqueue(cplt->wq);

	return status;
}

 
static void ssam_cplt_destroy(struct ssam_cplt *cplt)
{
	 
	destroy_workqueue(cplt->wq);
	ssam_nf_destroy(&cplt->event.notif);
}


 

 
struct device *ssam_controller_device(struct ssam_controller *c)
{
	return ssh_rtl_get_device(&c->rtl);
}
EXPORT_SYMBOL_GPL(ssam_controller_device);

static void __ssam_controller_release(struct kref *kref)
{
	struct ssam_controller *ctrl = to_ssam_controller(kref, kref);

	 
	ssam_controller_lock(ctrl);
	ssam_controller_destroy(ctrl);
	ssam_controller_unlock(ctrl);

	kfree(ctrl);
}

 
struct ssam_controller *ssam_controller_get(struct ssam_controller *c)
{
	if (c)
		kref_get(&c->kref);
	return c;
}
EXPORT_SYMBOL_GPL(ssam_controller_get);

 
void ssam_controller_put(struct ssam_controller *c)
{
	if (c)
		kref_put(&c->kref, __ssam_controller_release);
}
EXPORT_SYMBOL_GPL(ssam_controller_put);

 
void ssam_controller_statelock(struct ssam_controller *c)
{
	down_read(&c->lock);
}
EXPORT_SYMBOL_GPL(ssam_controller_statelock);

 
void ssam_controller_stateunlock(struct ssam_controller *c)
{
	up_read(&c->lock);
}
EXPORT_SYMBOL_GPL(ssam_controller_stateunlock);

 
void ssam_controller_lock(struct ssam_controller *c)
{
	down_write(&c->lock);
}

 
void ssam_controller_unlock(struct ssam_controller *c)
{
	up_write(&c->lock);
}

static void ssam_handle_event(struct ssh_rtl *rtl,
			      const struct ssh_command *cmd,
			      const struct ssam_span *data)
{
	struct ssam_controller *ctrl = to_ssam_controller(rtl, rtl);
	struct ssam_event_item *item;

	item = ssam_event_item_alloc(data->len, GFP_KERNEL);
	if (!item)
		return;

	item->rqid = get_unaligned_le16(&cmd->rqid);
	item->event.target_category = cmd->tc;
	item->event.target_id = cmd->sid;
	item->event.command_id = cmd->cid;
	item->event.instance_id = cmd->iid;
	memcpy(&item->event.data[0], data->ptr, data->len);

	if (WARN_ON(ssam_cplt_submit_event(&ctrl->cplt, item)))
		ssam_event_item_free(item);
}

static const struct ssh_rtl_ops ssam_rtl_ops = {
	.handle_event = ssam_handle_event,
};

static bool ssam_notifier_is_empty(struct ssam_controller *ctrl);
static void ssam_notifier_unregister_all(struct ssam_controller *ctrl);

#define SSAM_SSH_DSM_REVISION	0

 
static const guid_t SSAM_SSH_DSM_GUID =
	GUID_INIT(0xd5e383e1, 0xd892, 0x4a76,
		  0x89, 0xfc, 0xf6, 0xaa, 0xae, 0x7e, 0xd5, 0xb5);

enum ssh_dsm_fn {
	SSH_DSM_FN_SSH_POWER_PROFILE             = 0x05,
	SSH_DSM_FN_SCREEN_ON_SLEEP_IDLE_TIMEOUT  = 0x06,
	SSH_DSM_FN_SCREEN_OFF_SLEEP_IDLE_TIMEOUT = 0x07,
	SSH_DSM_FN_D3_CLOSES_HANDLE              = 0x08,
	SSH_DSM_FN_SSH_BUFFER_SIZE               = 0x09,
};

static int ssam_dsm_get_functions(acpi_handle handle, u64 *funcs)
{
	union acpi_object *obj;
	u64 mask = 0;
	int i;

	*funcs = 0;

	 
	if (!acpi_has_method(handle, "_DSM"))
		return 0;

	obj = acpi_evaluate_dsm_typed(handle, &SSAM_SSH_DSM_GUID,
				      SSAM_SSH_DSM_REVISION, 0, NULL,
				      ACPI_TYPE_BUFFER);
	if (!obj)
		return -EIO;

	for (i = 0; i < obj->buffer.length && i < 8; i++)
		mask |= (((u64)obj->buffer.pointer[i]) << (i * 8));

	if (mask & BIT(0))
		*funcs = mask;

	ACPI_FREE(obj);
	return 0;
}

static int ssam_dsm_load_u32(acpi_handle handle, u64 funcs, u64 func, u32 *ret)
{
	union acpi_object *obj;
	u64 val;

	if (!(funcs & BIT_ULL(func)))
		return 0;  

	obj = acpi_evaluate_dsm_typed(handle, &SSAM_SSH_DSM_GUID,
				      SSAM_SSH_DSM_REVISION, func, NULL,
				      ACPI_TYPE_INTEGER);
	if (!obj)
		return -EIO;

	val = obj->integer.value;
	ACPI_FREE(obj);

	if (val > U32_MAX)
		return -ERANGE;

	*ret = val;
	return 0;
}

 
static
int ssam_controller_caps_load_from_acpi(acpi_handle handle,
					struct ssam_controller_caps *caps)
{
	u32 d3_closes_handle = false;
	u64 funcs;
	int status;

	 
	caps->ssh_power_profile = U32_MAX;
	caps->screen_on_sleep_idle_timeout = U32_MAX;
	caps->screen_off_sleep_idle_timeout = U32_MAX;
	caps->d3_closes_handle = false;
	caps->ssh_buffer_size = U32_MAX;

	 
	status = ssam_dsm_get_functions(handle, &funcs);
	if (status)
		return status;

	 
	status = ssam_dsm_load_u32(handle, funcs, SSH_DSM_FN_SSH_POWER_PROFILE,
				   &caps->ssh_power_profile);
	if (status)
		return status;

	status = ssam_dsm_load_u32(handle, funcs,
				   SSH_DSM_FN_SCREEN_ON_SLEEP_IDLE_TIMEOUT,
				   &caps->screen_on_sleep_idle_timeout);
	if (status)
		return status;

	status = ssam_dsm_load_u32(handle, funcs,
				   SSH_DSM_FN_SCREEN_OFF_SLEEP_IDLE_TIMEOUT,
				   &caps->screen_off_sleep_idle_timeout);
	if (status)
		return status;

	status = ssam_dsm_load_u32(handle, funcs, SSH_DSM_FN_D3_CLOSES_HANDLE,
				   &d3_closes_handle);
	if (status)
		return status;

	caps->d3_closes_handle = !!d3_closes_handle;

	status = ssam_dsm_load_u32(handle, funcs, SSH_DSM_FN_SSH_BUFFER_SIZE,
				   &caps->ssh_buffer_size);
	if (status)
		return status;

	return 0;
}

 
int ssam_controller_init(struct ssam_controller *ctrl,
			 struct serdev_device *serdev)
{
	acpi_handle handle = ACPI_HANDLE(&serdev->dev);
	int status;

	init_rwsem(&ctrl->lock);
	kref_init(&ctrl->kref);

	status = ssam_controller_caps_load_from_acpi(handle, &ctrl->caps);
	if (status)
		return status;

	dev_dbg(&serdev->dev,
		"device capabilities:\n"
		"  ssh_power_profile:             %u\n"
		"  ssh_buffer_size:               %u\n"
		"  screen_on_sleep_idle_timeout:  %u\n"
		"  screen_off_sleep_idle_timeout: %u\n"
		"  d3_closes_handle:              %u\n",
		ctrl->caps.ssh_power_profile,
		ctrl->caps.ssh_buffer_size,
		ctrl->caps.screen_on_sleep_idle_timeout,
		ctrl->caps.screen_off_sleep_idle_timeout,
		ctrl->caps.d3_closes_handle);

	ssh_seq_reset(&ctrl->counter.seq);
	ssh_rqid_reset(&ctrl->counter.rqid);

	 
	status = ssam_cplt_init(&ctrl->cplt, &serdev->dev);
	if (status)
		return status;

	 
	status = ssh_rtl_init(&ctrl->rtl, serdev, &ssam_rtl_ops);
	if (status) {
		ssam_cplt_destroy(&ctrl->cplt);
		return status;
	}

	 
	WRITE_ONCE(ctrl->state, SSAM_CONTROLLER_INITIALIZED);
	return 0;
}

 
int ssam_controller_start(struct ssam_controller *ctrl)
{
	int status;

	lockdep_assert_held_write(&ctrl->lock);

	if (ctrl->state != SSAM_CONTROLLER_INITIALIZED)
		return -EINVAL;

	status = ssh_rtl_start(&ctrl->rtl);
	if (status)
		return status;

	 
	WRITE_ONCE(ctrl->state, SSAM_CONTROLLER_STARTED);
	return 0;
}

 
#define SSAM_CTRL_SHUTDOWN_FLUSH_TIMEOUT	msecs_to_jiffies(5000)

 
void ssam_controller_shutdown(struct ssam_controller *ctrl)
{
	enum ssam_controller_state s = ctrl->state;
	int status;

	lockdep_assert_held_write(&ctrl->lock);

	if (s == SSAM_CONTROLLER_UNINITIALIZED || s == SSAM_CONTROLLER_STOPPED)
		return;

	 
	status = ssh_rtl_flush(&ctrl->rtl, SSAM_CTRL_SHUTDOWN_FLUSH_TIMEOUT);
	if (status) {
		ssam_err(ctrl, "failed to flush request transport layer: %d\n",
			 status);
	}

	 
	ssam_cplt_flush(&ctrl->cplt);

	 
	WARN_ON(!ssam_notifier_is_empty(ctrl));

	 
	ssam_notifier_unregister_all(ctrl);

	 
	ssh_rtl_shutdown(&ctrl->rtl);

	 
	WRITE_ONCE(ctrl->state, SSAM_CONTROLLER_STOPPED);
	ctrl->rtl.ptl.serdev = NULL;
}

 
void ssam_controller_destroy(struct ssam_controller *ctrl)
{
	lockdep_assert_held_write(&ctrl->lock);

	if (ctrl->state == SSAM_CONTROLLER_UNINITIALIZED)
		return;

	WARN_ON(ctrl->state != SSAM_CONTROLLER_STOPPED);

	 

	 
	ssam_cplt_destroy(&ctrl->cplt);
	ssh_rtl_destroy(&ctrl->rtl);

	 
	WRITE_ONCE(ctrl->state, SSAM_CONTROLLER_UNINITIALIZED);
}

 
int ssam_controller_suspend(struct ssam_controller *ctrl)
{
	ssam_controller_lock(ctrl);

	if (ctrl->state != SSAM_CONTROLLER_STARTED) {
		ssam_controller_unlock(ctrl);
		return -EINVAL;
	}

	ssam_dbg(ctrl, "pm: suspending controller\n");

	 
	WRITE_ONCE(ctrl->state, SSAM_CONTROLLER_SUSPENDED);

	ssam_controller_unlock(ctrl);
	return 0;
}

 
int ssam_controller_resume(struct ssam_controller *ctrl)
{
	ssam_controller_lock(ctrl);

	if (ctrl->state != SSAM_CONTROLLER_SUSPENDED) {
		ssam_controller_unlock(ctrl);
		return -EINVAL;
	}

	ssam_dbg(ctrl, "pm: resuming controller\n");

	 
	WRITE_ONCE(ctrl->state, SSAM_CONTROLLER_STARTED);

	ssam_controller_unlock(ctrl);
	return 0;
}


 

 
ssize_t ssam_request_write_data(struct ssam_span *buf,
				struct ssam_controller *ctrl,
				const struct ssam_request *spec)
{
	struct msgbuf msgb;
	u16 rqid;
	u8 seq;

	if (spec->length > SSH_COMMAND_MAX_PAYLOAD_SIZE)
		return -EINVAL;

	if (SSH_COMMAND_MESSAGE_LENGTH(spec->length) > buf->len)
		return -EINVAL;

	msgb_init(&msgb, buf->ptr, buf->len);
	seq = ssh_seq_next(&ctrl->counter.seq);
	rqid = ssh_rqid_next(&ctrl->counter.rqid);
	msgb_push_cmd(&msgb, seq, rqid, spec);

	return msgb_bytes_used(&msgb);
}
EXPORT_SYMBOL_GPL(ssam_request_write_data);

static void ssam_request_sync_complete(struct ssh_request *rqst,
				       const struct ssh_command *cmd,
				       const struct ssam_span *data, int status)
{
	struct ssh_rtl *rtl = ssh_request_rtl(rqst);
	struct ssam_request_sync *r;

	r = container_of(rqst, struct ssam_request_sync, base);
	r->status = status;

	if (r->resp)
		r->resp->length = 0;

	if (status) {
		rtl_dbg_cond(rtl, "rsp: request failed: %d\n", status);
		return;
	}

	if (!data)	 
		return;

	if (!r->resp || !r->resp->pointer) {
		if (data->len)
			rtl_warn(rtl, "rsp: no response buffer provided, dropping data\n");
		return;
	}

	if (data->len > r->resp->capacity) {
		rtl_err(rtl,
			"rsp: response buffer too small, capacity: %zu bytes, got: %zu bytes\n",
			r->resp->capacity, data->len);
		r->status = -ENOSPC;
		return;
	}

	r->resp->length = data->len;
	memcpy(r->resp->pointer, data->ptr, data->len);
}

static void ssam_request_sync_release(struct ssh_request *rqst)
{
	complete_all(&container_of(rqst, struct ssam_request_sync, base)->comp);
}

static const struct ssh_request_ops ssam_request_sync_ops = {
	.release = ssam_request_sync_release,
	.complete = ssam_request_sync_complete,
};

 
int ssam_request_sync_alloc(size_t payload_len, gfp_t flags,
			    struct ssam_request_sync **rqst,
			    struct ssam_span *buffer)
{
	size_t msglen = SSH_COMMAND_MESSAGE_LENGTH(payload_len);

	*rqst = kzalloc(sizeof(**rqst) + msglen, flags);
	if (!*rqst)
		return -ENOMEM;

	buffer->ptr = (u8 *)(*rqst + 1);
	buffer->len = msglen;

	return 0;
}
EXPORT_SYMBOL_GPL(ssam_request_sync_alloc);

 
void ssam_request_sync_free(struct ssam_request_sync *rqst)
{
	kfree(rqst);
}
EXPORT_SYMBOL_GPL(ssam_request_sync_free);

 
int ssam_request_sync_init(struct ssam_request_sync *rqst,
			   enum ssam_request_flags flags)
{
	int status;

	status = ssh_request_init(&rqst->base, flags, &ssam_request_sync_ops);
	if (status)
		return status;

	init_completion(&rqst->comp);
	rqst->resp = NULL;
	rqst->status = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(ssam_request_sync_init);

 
int ssam_request_sync_submit(struct ssam_controller *ctrl,
			     struct ssam_request_sync *rqst)
{
	int status;

	 
	if (WARN_ON(READ_ONCE(ctrl->state) != SSAM_CONTROLLER_STARTED)) {
		ssh_request_put(&rqst->base);
		return -ENODEV;
	}

	status = ssh_rtl_submit(&ctrl->rtl, &rqst->base);
	ssh_request_put(&rqst->base);

	return status;
}
EXPORT_SYMBOL_GPL(ssam_request_sync_submit);

 
int ssam_request_do_sync(struct ssam_controller *ctrl,
			 const struct ssam_request *spec,
			 struct ssam_response *rsp)
{
	struct ssam_request_sync *rqst;
	struct ssam_span buf;
	ssize_t len;
	int status;

	status = ssam_request_sync_alloc(spec->length, GFP_KERNEL, &rqst, &buf);
	if (status)
		return status;

	status = ssam_request_sync_init(rqst, spec->flags);
	if (status) {
		ssam_request_sync_free(rqst);
		return status;
	}

	ssam_request_sync_set_resp(rqst, rsp);

	len = ssam_request_write_data(&buf, ctrl, spec);
	if (len < 0) {
		ssam_request_sync_free(rqst);
		return len;
	}

	ssam_request_sync_set_data(rqst, buf.ptr, len);

	status = ssam_request_sync_submit(ctrl, rqst);
	if (!status)
		status = ssam_request_sync_wait(rqst);

	ssam_request_sync_free(rqst);
	return status;
}
EXPORT_SYMBOL_GPL(ssam_request_do_sync);

 
int ssam_request_do_sync_with_buffer(struct ssam_controller *ctrl,
				     const struct ssam_request *spec,
				     struct ssam_response *rsp,
				     struct ssam_span *buf)
{
	struct ssam_request_sync rqst;
	ssize_t len;
	int status;

	status = ssam_request_sync_init(&rqst, spec->flags);
	if (status)
		return status;

	ssam_request_sync_set_resp(&rqst, rsp);

	len = ssam_request_write_data(buf, ctrl, spec);
	if (len < 0)
		return len;

	ssam_request_sync_set_data(&rqst, buf->ptr, len);

	status = ssam_request_sync_submit(ctrl, &rqst);
	if (!status)
		status = ssam_request_sync_wait(&rqst);

	return status;
}
EXPORT_SYMBOL_GPL(ssam_request_do_sync_with_buffer);


 

SSAM_DEFINE_SYNC_REQUEST_R(ssam_ssh_get_firmware_version, __le32, {
	.target_category = SSAM_SSH_TC_SAM,
	.target_id       = SSAM_SSH_TID_SAM,
	.command_id      = 0x13,
	.instance_id     = 0x00,
});

SSAM_DEFINE_SYNC_REQUEST_R(ssam_ssh_notif_display_off, u8, {
	.target_category = SSAM_SSH_TC_SAM,
	.target_id       = SSAM_SSH_TID_SAM,
	.command_id      = 0x15,
	.instance_id     = 0x00,
});

SSAM_DEFINE_SYNC_REQUEST_R(ssam_ssh_notif_display_on, u8, {
	.target_category = SSAM_SSH_TC_SAM,
	.target_id       = SSAM_SSH_TID_SAM,
	.command_id      = 0x16,
	.instance_id     = 0x00,
});

SSAM_DEFINE_SYNC_REQUEST_R(ssam_ssh_notif_d0_exit, u8, {
	.target_category = SSAM_SSH_TC_SAM,
	.target_id       = SSAM_SSH_TID_SAM,
	.command_id      = 0x33,
	.instance_id     = 0x00,
});

SSAM_DEFINE_SYNC_REQUEST_R(ssam_ssh_notif_d0_entry, u8, {
	.target_category = SSAM_SSH_TC_SAM,
	.target_id       = SSAM_SSH_TID_SAM,
	.command_id      = 0x34,
	.instance_id     = 0x00,
});

 
struct ssh_notification_params {
	u8 target_category;
	u8 flags;
	__le16 request_id;
	u8 instance_id;
} __packed;

static_assert(sizeof(struct ssh_notification_params) == 5);

static int __ssam_ssh_event_request(struct ssam_controller *ctrl,
				    struct ssam_event_registry reg, u8 cid,
				    struct ssam_event_id id, u8 flags)
{
	struct ssh_notification_params params;
	struct ssam_request rqst;
	struct ssam_response result;
	int status;

	u16 rqid = ssh_tc_to_rqid(id.target_category);
	u8 buf = 0;

	 
	if (!ssh_rqid_is_event(rqid))
		return -EINVAL;

	params.target_category = id.target_category;
	params.instance_id = id.instance;
	params.flags = flags;
	put_unaligned_le16(rqid, &params.request_id);

	rqst.target_category = reg.target_category;
	rqst.target_id = reg.target_id;
	rqst.command_id = cid;
	rqst.instance_id = 0x00;
	rqst.flags = SSAM_REQUEST_HAS_RESPONSE;
	rqst.length = sizeof(params);
	rqst.payload = (u8 *)&params;

	result.capacity = sizeof(buf);
	result.length = 0;
	result.pointer = &buf;

	status = ssam_retry(ssam_request_do_sync_onstack, ctrl, &rqst, &result,
			    sizeof(params));

	return status < 0 ? status : buf;
}

 
static int ssam_ssh_event_enable(struct ssam_controller *ctrl,
				 struct ssam_event_registry reg,
				 struct ssam_event_id id, u8 flags)
{
	int status;

	status = __ssam_ssh_event_request(ctrl, reg, reg.cid_enable, id, flags);

	if (status < 0 && status != -EINVAL) {
		ssam_err(ctrl,
			 "failed to enable event source (tc: %#04x, iid: %#04x, reg: %#04x)\n",
			 id.target_category, id.instance, reg.target_category);
	}

	if (status > 0) {
		ssam_err(ctrl,
			 "unexpected result while enabling event source: %#04x (tc: %#04x, iid: %#04x, reg: %#04x)\n",
			 status, id.target_category, id.instance, reg.target_category);
		return -EPROTO;
	}

	return status;
}

 
static int ssam_ssh_event_disable(struct ssam_controller *ctrl,
				  struct ssam_event_registry reg,
				  struct ssam_event_id id, u8 flags)
{
	int status;

	status = __ssam_ssh_event_request(ctrl, reg, reg.cid_disable, id, flags);

	if (status < 0 && status != -EINVAL) {
		ssam_err(ctrl,
			 "failed to disable event source (tc: %#04x, iid: %#04x, reg: %#04x)\n",
			 id.target_category, id.instance, reg.target_category);
	}

	if (status > 0) {
		ssam_err(ctrl,
			 "unexpected result while disabling event source: %#04x (tc: %#04x, iid: %#04x, reg: %#04x)\n",
			 status, id.target_category, id.instance, reg.target_category);
		return -EPROTO;
	}

	return status;
}


 

 
int ssam_get_firmware_version(struct ssam_controller *ctrl, u32 *version)
{
	__le32 __version;
	int status;

	status = ssam_retry(ssam_ssh_get_firmware_version, ctrl, &__version);
	if (status)
		return status;

	*version = le32_to_cpu(__version);
	return 0;
}

 
int ssam_ctrl_notif_display_off(struct ssam_controller *ctrl)
{
	int status;
	u8 response;

	ssam_dbg(ctrl, "pm: notifying display off\n");

	status = ssam_retry(ssam_ssh_notif_display_off, ctrl, &response);
	if (status)
		return status;

	if (response != 0) {
		ssam_err(ctrl, "unexpected response from display-off notification: %#04x\n",
			 response);
		return -EPROTO;
	}

	return 0;
}

 
int ssam_ctrl_notif_display_on(struct ssam_controller *ctrl)
{
	int status;
	u8 response;

	ssam_dbg(ctrl, "pm: notifying display on\n");

	status = ssam_retry(ssam_ssh_notif_display_on, ctrl, &response);
	if (status)
		return status;

	if (response != 0) {
		ssam_err(ctrl, "unexpected response from display-on notification: %#04x\n",
			 response);
		return -EPROTO;
	}

	return 0;
}

 
int ssam_ctrl_notif_d0_exit(struct ssam_controller *ctrl)
{
	int status;
	u8 response;

	if (!ctrl->caps.d3_closes_handle)
		return 0;

	ssam_dbg(ctrl, "pm: notifying D0 exit\n");

	status = ssam_retry(ssam_ssh_notif_d0_exit, ctrl, &response);
	if (status)
		return status;

	if (response != 0) {
		ssam_err(ctrl, "unexpected response from D0-exit notification: %#04x\n",
			 response);
		return -EPROTO;
	}

	return 0;
}

 
int ssam_ctrl_notif_d0_entry(struct ssam_controller *ctrl)
{
	int status;
	u8 response;

	if (!ctrl->caps.d3_closes_handle)
		return 0;

	ssam_dbg(ctrl, "pm: notifying D0 entry\n");

	status = ssam_retry(ssam_ssh_notif_d0_entry, ctrl, &response);
	if (status)
		return status;

	if (response != 0) {
		ssam_err(ctrl, "unexpected response from D0-entry notification: %#04x\n",
			 response);
		return -EPROTO;
	}

	return 0;
}


 

 
static int ssam_nf_refcount_enable(struct ssam_controller *ctrl,
				   struct ssam_nf_refcount_entry *entry, u8 flags)
{
	const struct ssam_event_registry reg = entry->key.reg;
	const struct ssam_event_id id = entry->key.id;
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	int status;

	lockdep_assert_held(&nf->lock);

	ssam_dbg(ctrl, "enabling event (reg: %#04x, tc: %#04x, iid: %#04x, rc: %d)\n",
		 reg.target_category, id.target_category, id.instance, entry->refcount);

	if (entry->refcount == 1) {
		status = ssam_ssh_event_enable(ctrl, reg, id, flags);
		if (status)
			return status;

		entry->flags = flags;

	} else if (entry->flags != flags) {
		ssam_warn(ctrl,
			  "inconsistent flags when enabling event: got %#04x, expected %#04x (reg: %#04x, tc: %#04x, iid: %#04x)\n",
			  flags, entry->flags, reg.target_category, id.target_category,
			  id.instance);
	}

	return 0;
}

 
static int ssam_nf_refcount_disable_free(struct ssam_controller *ctrl,
					 struct ssam_nf_refcount_entry *entry, u8 flags, bool ec)
{
	const struct ssam_event_registry reg = entry->key.reg;
	const struct ssam_event_id id = entry->key.id;
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	int status = 0;

	lockdep_assert_held(&nf->lock);

	ssam_dbg(ctrl, "%s event (reg: %#04x, tc: %#04x, iid: %#04x, rc: %d)\n",
		 ec ? "disabling" : "detaching", reg.target_category, id.target_category,
		 id.instance, entry->refcount);

	if (entry->flags != flags) {
		ssam_warn(ctrl,
			  "inconsistent flags when disabling event: got %#04x, expected %#04x (reg: %#04x, tc: %#04x, iid: %#04x)\n",
			  flags, entry->flags, reg.target_category, id.target_category,
			  id.instance);
	}

	if (ec && entry->refcount == 0) {
		status = ssam_ssh_event_disable(ctrl, reg, id, flags);
		kfree(entry);
	}

	return status;
}

 
int ssam_notifier_register(struct ssam_controller *ctrl, struct ssam_event_notifier *n)
{
	u16 rqid = ssh_tc_to_rqid(n->event.id.target_category);
	struct ssam_nf_refcount_entry *entry = NULL;
	struct ssam_nf_head *nf_head;
	struct ssam_nf *nf;
	int status;

	if (!ssh_rqid_is_event(rqid))
		return -EINVAL;

	nf = &ctrl->cplt.event.notif;
	nf_head = &nf->head[ssh_rqid_to_event(rqid)];

	mutex_lock(&nf->lock);

	if (!(n->flags & SSAM_EVENT_NOTIFIER_OBSERVER)) {
		entry = ssam_nf_refcount_inc(nf, n->event.reg, n->event.id);
		if (IS_ERR(entry)) {
			mutex_unlock(&nf->lock);
			return PTR_ERR(entry);
		}
	}

	status = ssam_nfblk_insert(nf_head, &n->base);
	if (status) {
		if (entry)
			ssam_nf_refcount_dec_free(nf, n->event.reg, n->event.id);

		mutex_unlock(&nf->lock);
		return status;
	}

	if (entry) {
		status = ssam_nf_refcount_enable(ctrl, entry, n->event.flags);
		if (status) {
			ssam_nfblk_remove(&n->base);
			ssam_nf_refcount_dec_free(nf, n->event.reg, n->event.id);
			mutex_unlock(&nf->lock);
			synchronize_srcu(&nf_head->srcu);
			return status;
		}
	}

	mutex_unlock(&nf->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(ssam_notifier_register);

 
int __ssam_notifier_unregister(struct ssam_controller *ctrl, struct ssam_event_notifier *n,
			       bool disable)
{
	u16 rqid = ssh_tc_to_rqid(n->event.id.target_category);
	struct ssam_nf_refcount_entry *entry;
	struct ssam_nf_head *nf_head;
	struct ssam_nf *nf;
	int status = 0;

	if (!ssh_rqid_is_event(rqid))
		return -EINVAL;

	nf = &ctrl->cplt.event.notif;
	nf_head = &nf->head[ssh_rqid_to_event(rqid)];

	mutex_lock(&nf->lock);

	if (!ssam_nfblk_find(nf_head, &n->base)) {
		mutex_unlock(&nf->lock);
		return -ENOENT;
	}

	 
	if (!(n->flags & SSAM_EVENT_NOTIFIER_OBSERVER)) {
		entry = ssam_nf_refcount_dec(nf, n->event.reg, n->event.id);
		if (WARN_ON(!entry)) {
			 
			status = -ENOENT;
			goto remove;
		}

		status = ssam_nf_refcount_disable_free(ctrl, entry, n->event.flags, disable);
	}

remove:
	ssam_nfblk_remove(&n->base);
	mutex_unlock(&nf->lock);
	synchronize_srcu(&nf_head->srcu);

	return status;
}
EXPORT_SYMBOL_GPL(__ssam_notifier_unregister);

 
int ssam_controller_event_enable(struct ssam_controller *ctrl,
				 struct ssam_event_registry reg,
				 struct ssam_event_id id, u8 flags)
{
	u16 rqid = ssh_tc_to_rqid(id.target_category);
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	struct ssam_nf_refcount_entry *entry;
	int status;

	if (!ssh_rqid_is_event(rqid))
		return -EINVAL;

	mutex_lock(&nf->lock);

	entry = ssam_nf_refcount_inc(nf, reg, id);
	if (IS_ERR(entry)) {
		mutex_unlock(&nf->lock);
		return PTR_ERR(entry);
	}

	status = ssam_nf_refcount_enable(ctrl, entry, flags);
	if (status) {
		ssam_nf_refcount_dec_free(nf, reg, id);
		mutex_unlock(&nf->lock);
		return status;
	}

	mutex_unlock(&nf->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(ssam_controller_event_enable);

 
int ssam_controller_event_disable(struct ssam_controller *ctrl,
				  struct ssam_event_registry reg,
				  struct ssam_event_id id, u8 flags)
{
	u16 rqid = ssh_tc_to_rqid(id.target_category);
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	struct ssam_nf_refcount_entry *entry;
	int status;

	if (!ssh_rqid_is_event(rqid))
		return -EINVAL;

	mutex_lock(&nf->lock);

	entry = ssam_nf_refcount_dec(nf, reg, id);
	if (!entry) {
		mutex_unlock(&nf->lock);
		return -ENOENT;
	}

	status = ssam_nf_refcount_disable_free(ctrl, entry, flags, true);

	mutex_unlock(&nf->lock);
	return status;
}
EXPORT_SYMBOL_GPL(ssam_controller_event_disable);

 
int ssam_notifier_disable_registered(struct ssam_controller *ctrl)
{
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	struct rb_node *n;
	int status;

	mutex_lock(&nf->lock);
	for (n = rb_first(&nf->refcount); n; n = rb_next(n)) {
		struct ssam_nf_refcount_entry *e;

		e = rb_entry(n, struct ssam_nf_refcount_entry, node);
		status = ssam_ssh_event_disable(ctrl, e->key.reg,
						e->key.id, e->flags);
		if (status)
			goto err;
	}
	mutex_unlock(&nf->lock);

	return 0;

err:
	for (n = rb_prev(n); n; n = rb_prev(n)) {
		struct ssam_nf_refcount_entry *e;

		e = rb_entry(n, struct ssam_nf_refcount_entry, node);
		ssam_ssh_event_enable(ctrl, e->key.reg, e->key.id, e->flags);
	}
	mutex_unlock(&nf->lock);

	return status;
}

 
void ssam_notifier_restore_registered(struct ssam_controller *ctrl)
{
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	struct rb_node *n;

	mutex_lock(&nf->lock);
	for (n = rb_first(&nf->refcount); n; n = rb_next(n)) {
		struct ssam_nf_refcount_entry *e;

		e = rb_entry(n, struct ssam_nf_refcount_entry, node);

		 
		ssam_ssh_event_enable(ctrl, e->key.reg, e->key.id, e->flags);
	}
	mutex_unlock(&nf->lock);
}

 
static bool ssam_notifier_is_empty(struct ssam_controller *ctrl)
{
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	bool result;

	mutex_lock(&nf->lock);
	result = ssam_nf_refcount_empty(nf);
	mutex_unlock(&nf->lock);

	return result;
}

 
static void ssam_notifier_unregister_all(struct ssam_controller *ctrl)
{
	struct ssam_nf *nf = &ctrl->cplt.event.notif;
	struct ssam_nf_refcount_entry *e, *n;

	mutex_lock(&nf->lock);
	rbtree_postorder_for_each_entry_safe(e, n, &nf->refcount, node) {
		 
		ssam_ssh_event_disable(ctrl, e->key.reg, e->key.id, e->flags);
		kfree(e);
	}
	nf->refcount = RB_ROOT;
	mutex_unlock(&nf->lock);
}


 

static irqreturn_t ssam_irq_handle(int irq, void *dev_id)
{
	struct ssam_controller *ctrl = dev_id;

	ssam_dbg(ctrl, "pm: wake irq triggered\n");

	 

	return IRQ_HANDLED;
}

 
int ssam_irq_setup(struct ssam_controller *ctrl)
{
	struct device *dev = ssam_controller_device(ctrl);
	struct gpio_desc *gpiod;
	int irq;
	int status;

	 
	const int irqf = IRQF_ONESHOT | IRQF_TRIGGER_RISING | IRQF_NO_AUTOEN;

	gpiod = gpiod_get(dev, "ssam_wakeup-int", GPIOD_ASIS);
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	irq = gpiod_to_irq(gpiod);
	gpiod_put(gpiod);

	if (irq < 0)
		return irq;

	status = request_threaded_irq(irq, NULL, ssam_irq_handle, irqf,
				      "ssam_wakeup", ctrl);
	if (status)
		return status;

	ctrl->irq.num = irq;
	return 0;
}

 
void ssam_irq_free(struct ssam_controller *ctrl)
{
	free_irq(ctrl->irq.num, ctrl);
	ctrl->irq.num = -1;
}

 
int ssam_irq_arm_for_wakeup(struct ssam_controller *ctrl)
{
	struct device *dev = ssam_controller_device(ctrl);
	int status;

	enable_irq(ctrl->irq.num);
	if (device_may_wakeup(dev)) {
		status = enable_irq_wake(ctrl->irq.num);
		if (status) {
			ssam_err(ctrl, "failed to enable wake IRQ: %d\n", status);
			disable_irq(ctrl->irq.num);
			return status;
		}

		ctrl->irq.wakeup_enabled = true;
	} else {
		ctrl->irq.wakeup_enabled = false;
	}

	return 0;
}

 
void ssam_irq_disarm_wakeup(struct ssam_controller *ctrl)
{
	int status;

	if (ctrl->irq.wakeup_enabled) {
		status = disable_irq_wake(ctrl->irq.num);
		if (status)
			ssam_err(ctrl, "failed to disable wake IRQ: %d\n", status);

		ctrl->irq.wakeup_enabled = false;
	}
	disable_irq(ctrl->irq.num);
}
