
 

#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/panic_notifier.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/reboot.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/types.h>
#include <asm/irq.h>
#include <asm/debug.h>

#include "sclp.h"

#define SCLP_HEADER		"sclp: "

struct sclp_trace_entry {
	char id[4] __nonstring;
	u32 a;
	u64 b;
};

#define SCLP_TRACE_ENTRY_SIZE		sizeof(struct sclp_trace_entry)
#define SCLP_TRACE_MAX_SIZE		128
#define SCLP_TRACE_EVENT_MAX_SIZE	64

 
DEFINE_STATIC_DEBUG_INFO(sclp_debug, "sclp", 8, 1, SCLP_TRACE_ENTRY_SIZE,
			 &debug_hex_ascii_view);

 
DEFINE_STATIC_DEBUG_INFO(sclp_debug_err, "sclp_err", 4, 1,
			 SCLP_TRACE_ENTRY_SIZE, &debug_hex_ascii_view);

 
static DEFINE_SPINLOCK(sclp_lock);

 
static sccb_mask_t sclp_receive_mask;

 
static sccb_mask_t sclp_send_mask;

 
static LIST_HEAD(sclp_reg_list);

 
static LIST_HEAD(sclp_req_queue);

 
static struct sclp_req sclp_read_req;
static struct sclp_req sclp_init_req;
static void *sclp_read_sccb;
static struct init_sccb *sclp_init_sccb;

 
int sclp_console_pages = SCLP_CONSOLE_PAGES;
 
bool sclp_console_drop = true;
 
unsigned long sclp_console_full;

 
static sclp_cmdw_t active_cmd;

static inline void sclp_trace(int prio, char *id, u32 a, u64 b, bool err)
{
	struct sclp_trace_entry e;

	memset(&e, 0, sizeof(e));
	strncpy(e.id, id, sizeof(e.id));
	e.a = a;
	e.b = b;
	debug_event(&sclp_debug, prio, &e, sizeof(e));
	if (err)
		debug_event(&sclp_debug_err, 0, &e, sizeof(e));
}

static inline int no_zeroes_len(void *data, int len)
{
	char *d = data;

	 
	while (len > SCLP_TRACE_ENTRY_SIZE && d[len - 1] == 0)
		len--;

	return len;
}

static inline void sclp_trace_bin(int prio, void *d, int len, int errlen)
{
	debug_event(&sclp_debug, prio, d, no_zeroes_len(d, len));
	if (errlen)
		debug_event(&sclp_debug_err, 0, d, no_zeroes_len(d, errlen));
}

static inline int abbrev_len(sclp_cmdw_t cmd, struct sccb_header *sccb)
{
	struct evbuf_header *evbuf = (struct evbuf_header *)(sccb + 1);
	int len = sccb->length, limit = SCLP_TRACE_MAX_SIZE;

	 
	if (sclp_debug.level == DEBUG_MAX_LEVEL)
		return len;

	 
	if (cmd == SCLP_CMDW_WRITE_EVENT_DATA &&
	    (evbuf->type == EVTYP_MSG  || evbuf->type == EVTYP_VT220MSG))
		limit = SCLP_TRACE_ENTRY_SIZE;

	return min(len, limit);
}

static inline void sclp_trace_sccb(int prio, char *id, u32 a, u64 b,
				   sclp_cmdw_t cmd, struct sccb_header *sccb,
				   bool err)
{
	sclp_trace(prio, id, a, b, err);
	if (sccb) {
		sclp_trace_bin(prio + 1, sccb, abbrev_len(cmd, sccb),
			       err ? sccb->length : 0);
	}
}

static inline void sclp_trace_evbuf(int prio, char *id, u32 a, u64 b,
				    struct evbuf_header *evbuf, bool err)
{
	sclp_trace(prio, id, a, b, err);
	sclp_trace_bin(prio + 1, evbuf,
		       min((int)evbuf->length, (int)SCLP_TRACE_EVENT_MAX_SIZE),
		       err ? evbuf->length : 0);
}

static inline void sclp_trace_req(int prio, char *id, struct sclp_req *req,
				  bool err)
{
	struct sccb_header *sccb = req->sccb;
	union {
		struct {
			u16 status;
			u16 response;
			u16 timeout;
			u16 start_count;
		};
		u64 b;
	} summary;

	summary.status = req->status;
	summary.response = sccb ? sccb->response_code : 0;
	summary.timeout = (u16)req->queue_timeout;
	summary.start_count = (u16)req->start_count;

	sclp_trace(prio, id, __pa(sccb), summary.b, err);
}

static inline void sclp_trace_register(int prio, char *id, u32 a, u64 b,
				       struct sclp_register *reg)
{
	struct {
		u64 receive;
		u64 send;
	} d;

	d.receive = reg->receive_mask;
	d.send = reg->send_mask;

	sclp_trace(prio, id, a, b, false);
	sclp_trace_bin(prio, &d, sizeof(d), 0);
}

static int __init sclp_setup_console_pages(char *str)
{
	int pages, rc;

	rc = kstrtoint(str, 0, &pages);
	if (!rc && pages >= SCLP_CONSOLE_PAGES)
		sclp_console_pages = pages;
	return 1;
}

__setup("sclp_con_pages=", sclp_setup_console_pages);

static int __init sclp_setup_console_drop(char *str)
{
	return kstrtobool(str, &sclp_console_drop) == 0;
}

__setup("sclp_con_drop=", sclp_setup_console_drop);

 
static struct timer_list sclp_request_timer;

 
static struct timer_list sclp_queue_timer;

 
static volatile enum sclp_running_state_t {
	sclp_running_state_idle,
	sclp_running_state_running,
	sclp_running_state_reset_pending
} sclp_running_state = sclp_running_state_idle;

 
static volatile enum sclp_reading_state_t {
	sclp_reading_state_idle,
	sclp_reading_state_reading
} sclp_reading_state = sclp_reading_state_idle;

 
static volatile enum sclp_activation_state_t {
	sclp_activation_state_active,
	sclp_activation_state_deactivating,
	sclp_activation_state_inactive,
	sclp_activation_state_activating
} sclp_activation_state = sclp_activation_state_active;

 
static volatile enum sclp_mask_state_t {
	sclp_mask_state_idle,
	sclp_mask_state_initializing
} sclp_mask_state = sclp_mask_state_idle;

 
#define SCLP_INIT_RETRY		3
#define SCLP_MASK_RETRY		3

 
#define SCLP_BUSY_INTERVAL	10
#define SCLP_RETRY_INTERVAL	30

static void sclp_request_timeout(bool force_restart);
static void sclp_process_queue(void);
static void __sclp_make_read_req(void);
static int sclp_init_mask(int calculate);
static int sclp_init(void);

static void
__sclp_queue_read_req(void)
{
	if (sclp_reading_state == sclp_reading_state_idle) {
		sclp_reading_state = sclp_reading_state_reading;
		__sclp_make_read_req();
		 
		list_add(&sclp_read_req.list, &sclp_req_queue);
	}
}

 
static inline void
__sclp_set_request_timer(unsigned long time, void (*cb)(struct timer_list *))
{
	del_timer(&sclp_request_timer);
	sclp_request_timer.function = cb;
	sclp_request_timer.expires = jiffies + time;
	add_timer(&sclp_request_timer);
}

static void sclp_request_timeout_restart(struct timer_list *unused)
{
	sclp_request_timeout(true);
}

static void sclp_request_timeout_normal(struct timer_list *unused)
{
	sclp_request_timeout(false);
}

 
static void sclp_request_timeout(bool force_restart)
{
	unsigned long flags;

	 
	sclp_trace(2, "TMO", force_restart, 0, true);

	spin_lock_irqsave(&sclp_lock, flags);
	if (force_restart) {
		if (sclp_running_state == sclp_running_state_running) {
			 
			__sclp_queue_read_req();
			sclp_running_state = sclp_running_state_idle;
		}
	} else {
		__sclp_set_request_timer(SCLP_BUSY_INTERVAL * HZ,
					 sclp_request_timeout_normal);
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
	sclp_process_queue();
}

 
static unsigned long __sclp_req_queue_find_next_timeout(void)
{
	unsigned long expires_next = 0;
	struct sclp_req *req;

	list_for_each_entry(req, &sclp_req_queue, list) {
		if (!req->queue_expires)
			continue;
		if (!expires_next ||
		   (time_before(req->queue_expires, expires_next)))
				expires_next = req->queue_expires;
	}
	return expires_next;
}

 
static struct sclp_req *__sclp_req_queue_remove_expired_req(void)
{
	unsigned long flags, now;
	struct sclp_req *req;

	spin_lock_irqsave(&sclp_lock, flags);
	now = jiffies;
	 
	list_for_each_entry(req, &sclp_req_queue, list) {
		if (!req->queue_expires)
			continue;
		if (time_before_eq(req->queue_expires, now)) {
			if (req->status == SCLP_REQ_QUEUED) {
				req->status = SCLP_REQ_QUEUED_TIMEOUT;
				list_del(&req->list);
				goto out;
			}
		}
	}
	req = NULL;
out:
	spin_unlock_irqrestore(&sclp_lock, flags);
	return req;
}

 
static void sclp_req_queue_timeout(struct timer_list *unused)
{
	unsigned long flags, expires_next;
	struct sclp_req *req;

	do {
		req = __sclp_req_queue_remove_expired_req();

		if (req) {
			 
			sclp_trace_req(2, "RQTM", req, true);
		}

		if (req && req->callback)
			req->callback(req, req->callback_data);
	} while (req);

	spin_lock_irqsave(&sclp_lock, flags);
	expires_next = __sclp_req_queue_find_next_timeout();
	if (expires_next)
		mod_timer(&sclp_queue_timer, expires_next);
	spin_unlock_irqrestore(&sclp_lock, flags);
}

static int sclp_service_call_trace(sclp_cmdw_t command, void *sccb)
{
	static u64 srvc_count;
	int rc;

	 
	sclp_trace_sccb(0, "SRV1", command, (u64)sccb, command, sccb, false);

	rc = sclp_service_call(command, sccb);

	 
	sclp_trace(0, "SRV2", -rc, ++srvc_count, rc != 0);

	if (rc == 0)
		active_cmd = command;

	return rc;
}

 
static int
__sclp_start_request(struct sclp_req *req)
{
	int rc;

	if (sclp_running_state != sclp_running_state_idle)
		return 0;
	del_timer(&sclp_request_timer);
	rc = sclp_service_call_trace(req->command, req->sccb);
	req->start_count++;

	if (rc == 0) {
		 
		req->status = SCLP_REQ_RUNNING;
		sclp_running_state = sclp_running_state_running;
		__sclp_set_request_timer(SCLP_RETRY_INTERVAL * HZ,
					 sclp_request_timeout_restart);
		return 0;
	} else if (rc == -EBUSY) {
		 
		__sclp_set_request_timer(SCLP_BUSY_INTERVAL * HZ,
					 sclp_request_timeout_normal);
		return 0;
	}
	 
	req->status = SCLP_REQ_FAILED;
	return rc;
}

 
static void
sclp_process_queue(void)
{
	struct sclp_req *req;
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&sclp_lock, flags);
	if (sclp_running_state != sclp_running_state_idle) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return;
	}
	del_timer(&sclp_request_timer);
	while (!list_empty(&sclp_req_queue)) {
		req = list_entry(sclp_req_queue.next, struct sclp_req, list);
		rc = __sclp_start_request(req);
		if (rc == 0)
			break;
		 
		if (req->start_count > 1) {
			 
			__sclp_set_request_timer(SCLP_BUSY_INTERVAL * HZ,
						 sclp_request_timeout_normal);
			break;
		}
		 
		list_del(&req->list);

		 
		sclp_trace_req(2, "RQAB", req, true);

		if (req->callback) {
			spin_unlock_irqrestore(&sclp_lock, flags);
			req->callback(req, req->callback_data);
			spin_lock_irqsave(&sclp_lock, flags);
		}
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
}

static int __sclp_can_add_request(struct sclp_req *req)
{
	if (req == &sclp_init_req)
		return 1;
	if (sclp_init_state != sclp_init_state_initialized)
		return 0;
	if (sclp_activation_state != sclp_activation_state_active)
		return 0;
	return 1;
}

 
int
sclp_add_request(struct sclp_req *req)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	if (!__sclp_can_add_request(req)) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EIO;
	}

	 
	sclp_trace(2, "RQAD", __pa(req->sccb), _RET_IP_, false);

	req->status = SCLP_REQ_QUEUED;
	req->start_count = 0;
	list_add_tail(&req->list, &sclp_req_queue);
	rc = 0;
	if (req->queue_timeout) {
		req->queue_expires = jiffies + req->queue_timeout * HZ;
		if (!timer_pending(&sclp_queue_timer) ||
		    time_after(sclp_queue_timer.expires, req->queue_expires))
			mod_timer(&sclp_queue_timer, req->queue_expires);
	} else
		req->queue_expires = 0;
	 
	if (sclp_running_state == sclp_running_state_idle &&
	    req->list.prev == &sclp_req_queue) {
		rc = __sclp_start_request(req);
		if (rc)
			list_del(&req->list);
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

EXPORT_SYMBOL(sclp_add_request);

 
static int
sclp_dispatch_evbufs(struct sccb_header *sccb)
{
	unsigned long flags;
	struct evbuf_header *evbuf;
	struct list_head *l;
	struct sclp_register *reg;
	int offset;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	rc = 0;
	for (offset = sizeof(struct sccb_header); offset < sccb->length;
	     offset += evbuf->length) {
		evbuf = (struct evbuf_header *) ((addr_t) sccb + offset);
		 
		if (evbuf->length == 0)
			break;
		 
		reg = NULL;
		list_for_each(l, &sclp_reg_list) {
			reg = list_entry(l, struct sclp_register, list);
			if (reg->receive_mask & SCLP_EVTYP_MASK(evbuf->type))
				break;
			else
				reg = NULL;
		}

		 
		sclp_trace_evbuf(2, "EVNT", 0, reg ? (u64)reg->receiver_fn : 0,
				 evbuf, !reg);

		if (reg && reg->receiver_fn) {
			spin_unlock_irqrestore(&sclp_lock, flags);
			reg->receiver_fn(evbuf);
			spin_lock_irqsave(&sclp_lock, flags);
		} else if (reg == NULL)
			rc = -EOPNOTSUPP;
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

 
static void
sclp_read_cb(struct sclp_req *req, void *data)
{
	unsigned long flags;
	struct sccb_header *sccb;

	sccb = (struct sccb_header *) req->sccb;
	if (req->status == SCLP_REQ_DONE && (sccb->response_code == 0x20 ||
	    sccb->response_code == 0x220))
		sclp_dispatch_evbufs(sccb);
	spin_lock_irqsave(&sclp_lock, flags);
	sclp_reading_state = sclp_reading_state_idle;
	spin_unlock_irqrestore(&sclp_lock, flags);
}

 
static void __sclp_make_read_req(void)
{
	struct sccb_header *sccb;

	sccb = (struct sccb_header *) sclp_read_sccb;
	clear_page(sccb);
	memset(&sclp_read_req, 0, sizeof(struct sclp_req));
	sclp_read_req.command = SCLP_CMDW_READ_EVENT_DATA;
	sclp_read_req.status = SCLP_REQ_QUEUED;
	sclp_read_req.start_count = 0;
	sclp_read_req.callback = sclp_read_cb;
	sclp_read_req.sccb = sccb;
	sccb->length = PAGE_SIZE;
	sccb->function_code = 0;
	sccb->control_mask[2] = 0x80;
}

 
static inline struct sclp_req *
__sclp_find_req(u32 sccb)
{
	struct list_head *l;
	struct sclp_req *req;

	list_for_each(l, &sclp_req_queue) {
		req = list_entry(l, struct sclp_req, list);
		if (sccb == __pa(req->sccb))
			return req;
	}
	return NULL;
}

static bool ok_response(u32 sccb_int, sclp_cmdw_t cmd)
{
	struct sccb_header *sccb = (struct sccb_header *)__va(sccb_int);
	struct evbuf_header *evbuf;
	u16 response;

	if (!sccb)
		return true;

	 
	response = sccb->response_code & 0xff;
	if (response != 0x10 && response != 0x20)
		return false;

	 
	if (cmd == SCLP_CMDW_WRITE_EVENT_DATA) {
		evbuf = (struct evbuf_header *)(sccb + 1);
		if (!(evbuf->flags & 0x80))
			return false;
	}

	return true;
}

 
static void sclp_interrupt_handler(struct ext_code ext_code,
				   unsigned int param32, unsigned long param64)
{
	struct sclp_req *req;
	u32 finished_sccb;
	u32 evbuf_pending;

	inc_irq_stat(IRQEXT_SCP);
	spin_lock(&sclp_lock);
	finished_sccb = param32 & 0xfffffff8;
	evbuf_pending = param32 & 0x3;

	 
	sclp_trace_sccb(0, "INT", param32, active_cmd, active_cmd,
			(struct sccb_header *)__va(finished_sccb),
			!ok_response(finished_sccb, active_cmd));

	if (finished_sccb) {
		del_timer(&sclp_request_timer);
		sclp_running_state = sclp_running_state_reset_pending;
		req = __sclp_find_req(finished_sccb);
		if (req) {
			 
			list_del(&req->list);
			req->status = SCLP_REQ_DONE;

			 
			sclp_trace_req(2, "RQOK", req, false);

			if (req->callback) {
				spin_unlock(&sclp_lock);
				req->callback(req, req->callback_data);
				spin_lock(&sclp_lock);
			}
		} else {
			 
			sclp_trace(0, "UNEX", finished_sccb, 0, true);
		}
		sclp_running_state = sclp_running_state_idle;
		active_cmd = 0;
	}
	if (evbuf_pending &&
	    sclp_activation_state == sclp_activation_state_active)
		__sclp_queue_read_req();
	spin_unlock(&sclp_lock);
	sclp_process_queue();
}

 
static inline u64
sclp_tod_from_jiffies(unsigned long jiffies)
{
	return (u64) (jiffies / HZ) << 32;
}

 
void
sclp_sync_wait(void)
{
	unsigned long long old_tick;
	unsigned long flags;
	unsigned long cr0, cr0_sync;
	static u64 sync_count;
	u64 timeout;
	int irq_context;

	 
	sclp_trace(4, "SYN1", sclp_running_state, ++sync_count, false);

	 
	timeout = 0;
	if (timer_pending(&sclp_request_timer)) {
		 
		timeout = get_tod_clock_fast() +
			  sclp_tod_from_jiffies(sclp_request_timer.expires -
						jiffies);
	}
	local_irq_save(flags);
	 
	irq_context = in_interrupt();
	if (!irq_context)
		local_bh_disable();
	 
	old_tick = local_tick_disable();
	trace_hardirqs_on();
	__ctl_store(cr0, 0, 0);
	cr0_sync = cr0 & ~CR0_IRQ_SUBCLASS_MASK;
	cr0_sync |= 1UL << (63 - 54);
	__ctl_load(cr0_sync, 0, 0);
	__arch_local_irq_stosm(0x01);
	 
	while (sclp_running_state != sclp_running_state_idle) {
		 
		if (get_tod_clock_fast() > timeout && del_timer(&sclp_request_timer))
			sclp_request_timer.function(&sclp_request_timer);
		cpu_relax();
	}
	local_irq_disable();
	__ctl_load(cr0, 0, 0);
	if (!irq_context)
		_local_bh_enable();
	local_tick_enable(old_tick);
	local_irq_restore(flags);

	 
	sclp_trace(4, "SYN2", sclp_running_state, sync_count, false);
}
EXPORT_SYMBOL(sclp_sync_wait);

 
static void
sclp_dispatch_state_change(void)
{
	struct list_head *l;
	struct sclp_register *reg;
	unsigned long flags;
	sccb_mask_t receive_mask;
	sccb_mask_t send_mask;

	do {
		spin_lock_irqsave(&sclp_lock, flags);
		reg = NULL;
		list_for_each(l, &sclp_reg_list) {
			reg = list_entry(l, struct sclp_register, list);
			receive_mask = reg->send_mask & sclp_receive_mask;
			send_mask = reg->receive_mask & sclp_send_mask;
			if (reg->sclp_receive_mask != receive_mask ||
			    reg->sclp_send_mask != send_mask) {
				reg->sclp_receive_mask = receive_mask;
				reg->sclp_send_mask = send_mask;
				break;
			} else
				reg = NULL;
		}
		spin_unlock_irqrestore(&sclp_lock, flags);
		if (reg && reg->state_change_fn) {
			 
			sclp_trace(2, "STCG", 0, (u64)reg->state_change_fn,
				   false);

			reg->state_change_fn(reg);
		}
	} while (reg);
}

struct sclp_statechangebuf {
	struct evbuf_header	header;
	u8		validity_sclp_active_facility_mask : 1;
	u8		validity_sclp_receive_mask : 1;
	u8		validity_sclp_send_mask : 1;
	u8		validity_read_data_function_mask : 1;
	u16		_zeros : 12;
	u16		mask_length;
	u64		sclp_active_facility_mask;
	u8		masks[2 * 1021 + 4];	 
	 
} __attribute__((packed));


 
static void
sclp_state_change_cb(struct evbuf_header *evbuf)
{
	unsigned long flags;
	struct sclp_statechangebuf *scbuf;

	BUILD_BUG_ON(sizeof(struct sclp_statechangebuf) > PAGE_SIZE);

	scbuf = (struct sclp_statechangebuf *) evbuf;
	spin_lock_irqsave(&sclp_lock, flags);
	if (scbuf->validity_sclp_receive_mask)
		sclp_receive_mask = sccb_get_recv_mask(scbuf);
	if (scbuf->validity_sclp_send_mask)
		sclp_send_mask = sccb_get_send_mask(scbuf);
	spin_unlock_irqrestore(&sclp_lock, flags);
	if (scbuf->validity_sclp_active_facility_mask)
		sclp.facilities = scbuf->sclp_active_facility_mask;
	sclp_dispatch_state_change();
}

static struct sclp_register sclp_state_change_event = {
	.receive_mask = EVTYP_STATECHANGE_MASK,
	.receiver_fn = sclp_state_change_cb
};

 
static inline void
__sclp_get_mask(sccb_mask_t *receive_mask, sccb_mask_t *send_mask)
{
	struct list_head *l;
	struct sclp_register *t;

	*receive_mask = 0;
	*send_mask = 0;
	list_for_each(l, &sclp_reg_list) {
		t = list_entry(l, struct sclp_register, list);
		*receive_mask |= t->receive_mask;
		*send_mask |= t->send_mask;
	}
}

 
int
sclp_register(struct sclp_register *reg)
{
	unsigned long flags;
	sccb_mask_t receive_mask;
	sccb_mask_t send_mask;
	int rc;

	 
	sclp_trace_register(2, "REG", 0, _RET_IP_, reg);

	rc = sclp_init();
	if (rc)
		return rc;
	spin_lock_irqsave(&sclp_lock, flags);
	 
	__sclp_get_mask(&receive_mask, &send_mask);
	if (reg->receive_mask & receive_mask || reg->send_mask & send_mask) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EBUSY;
	}
	 
	reg->sclp_receive_mask = 0;
	reg->sclp_send_mask = 0;
	list_add(&reg->list, &sclp_reg_list);
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_init_mask(1);
	if (rc) {
		spin_lock_irqsave(&sclp_lock, flags);
		list_del(&reg->list);
		spin_unlock_irqrestore(&sclp_lock, flags);
	}
	return rc;
}

EXPORT_SYMBOL(sclp_register);

 
void
sclp_unregister(struct sclp_register *reg)
{
	unsigned long flags;

	 
	sclp_trace_register(2, "UREG", 0, _RET_IP_, reg);

	spin_lock_irqsave(&sclp_lock, flags);
	list_del(&reg->list);
	spin_unlock_irqrestore(&sclp_lock, flags);
	sclp_init_mask(1);
}

EXPORT_SYMBOL(sclp_unregister);

 
int
sclp_remove_processed(struct sccb_header *sccb)
{
	struct evbuf_header *evbuf;
	int unprocessed;
	u16 remaining;

	evbuf = (struct evbuf_header *) (sccb + 1);
	unprocessed = 0;
	remaining = sccb->length - sizeof(struct sccb_header);
	while (remaining > 0) {
		remaining -= evbuf->length;
		if (evbuf->flags & 0x80) {
			sccb->length -= evbuf->length;
			memcpy(evbuf, (void *) ((addr_t) evbuf + evbuf->length),
			       remaining);
		} else {
			unprocessed++;
			evbuf = (struct evbuf_header *)
					((addr_t) evbuf + evbuf->length);
		}
	}
	return unprocessed;
}

EXPORT_SYMBOL(sclp_remove_processed);

 
static inline void
__sclp_make_init_req(sccb_mask_t receive_mask, sccb_mask_t send_mask)
{
	struct init_sccb *sccb = sclp_init_sccb;

	clear_page(sccb);
	memset(&sclp_init_req, 0, sizeof(struct sclp_req));
	sclp_init_req.command = SCLP_CMDW_WRITE_EVENT_MASK;
	sclp_init_req.status = SCLP_REQ_FILLED;
	sclp_init_req.start_count = 0;
	sclp_init_req.callback = NULL;
	sclp_init_req.callback_data = NULL;
	sclp_init_req.sccb = sccb;
	sccb->header.length = sizeof(*sccb);
	if (sclp_mask_compat_mode)
		sccb->mask_length = SCLP_MASK_SIZE_COMPAT;
	else
		sccb->mask_length = sizeof(sccb_mask_t);
	sccb_set_recv_mask(sccb, receive_mask);
	sccb_set_send_mask(sccb, send_mask);
	sccb_set_sclp_recv_mask(sccb, 0);
	sccb_set_sclp_send_mask(sccb, 0);
}

 
static int
sclp_init_mask(int calculate)
{
	unsigned long flags;
	struct init_sccb *sccb = sclp_init_sccb;
	sccb_mask_t receive_mask;
	sccb_mask_t send_mask;
	int retry;
	int rc;
	unsigned long wait;

	spin_lock_irqsave(&sclp_lock, flags);
	 
	if (sclp_mask_state != sclp_mask_state_idle) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EBUSY;
	}
	if (sclp_activation_state == sclp_activation_state_inactive) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EINVAL;
	}
	sclp_mask_state = sclp_mask_state_initializing;
	 
	if (calculate)
		__sclp_get_mask(&receive_mask, &send_mask);
	else {
		receive_mask = 0;
		send_mask = 0;
	}
	rc = -EIO;
	for (retry = 0; retry <= SCLP_MASK_RETRY; retry++) {
		 
		__sclp_make_init_req(receive_mask, send_mask);
		spin_unlock_irqrestore(&sclp_lock, flags);
		if (sclp_add_request(&sclp_init_req)) {
			 
			wait = jiffies + SCLP_BUSY_INTERVAL * HZ;
			while (time_before(jiffies, wait))
				sclp_sync_wait();
			spin_lock_irqsave(&sclp_lock, flags);
			continue;
		}
		while (sclp_init_req.status != SCLP_REQ_DONE &&
		       sclp_init_req.status != SCLP_REQ_FAILED)
			sclp_sync_wait();
		spin_lock_irqsave(&sclp_lock, flags);
		if (sclp_init_req.status == SCLP_REQ_DONE &&
		    sccb->header.response_code == 0x20) {
			 
			if (calculate) {
				sclp_receive_mask = sccb_get_sclp_recv_mask(sccb);
				sclp_send_mask = sccb_get_sclp_send_mask(sccb);
			} else {
				sclp_receive_mask = 0;
				sclp_send_mask = 0;
			}
			spin_unlock_irqrestore(&sclp_lock, flags);
			sclp_dispatch_state_change();
			spin_lock_irqsave(&sclp_lock, flags);
			rc = 0;
			break;
		}
	}
	sclp_mask_state = sclp_mask_state_idle;
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

 
int
sclp_deactivate(void)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	 
	if (sclp_activation_state != sclp_activation_state_active) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EINVAL;
	}
	sclp_activation_state = sclp_activation_state_deactivating;
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_init_mask(0);
	spin_lock_irqsave(&sclp_lock, flags);
	if (rc == 0)
		sclp_activation_state = sclp_activation_state_inactive;
	else
		sclp_activation_state = sclp_activation_state_active;
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

EXPORT_SYMBOL(sclp_deactivate);

 
int
sclp_reactivate(void)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	 
	if (sclp_activation_state != sclp_activation_state_inactive) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return -EINVAL;
	}
	sclp_activation_state = sclp_activation_state_activating;
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_init_mask(1);
	spin_lock_irqsave(&sclp_lock, flags);
	if (rc == 0)
		sclp_activation_state = sclp_activation_state_active;
	else
		sclp_activation_state = sclp_activation_state_inactive;
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

EXPORT_SYMBOL(sclp_reactivate);

 
static void sclp_check_handler(struct ext_code ext_code,
			       unsigned int param32, unsigned long param64)
{
	u32 finished_sccb;

	inc_irq_stat(IRQEXT_SCP);
	finished_sccb = param32 & 0xfffffff8;
	 
	if (finished_sccb == 0)
		return;
	if (finished_sccb != __pa(sclp_init_sccb))
		panic("sclp: unsolicited interrupt for buffer at 0x%x\n",
		      finished_sccb);
	spin_lock(&sclp_lock);
	if (sclp_running_state == sclp_running_state_running) {
		sclp_init_req.status = SCLP_REQ_DONE;
		sclp_running_state = sclp_running_state_idle;
	}
	spin_unlock(&sclp_lock);
}

 
static void
sclp_check_timeout(struct timer_list *unused)
{
	unsigned long flags;

	spin_lock_irqsave(&sclp_lock, flags);
	if (sclp_running_state == sclp_running_state_running) {
		sclp_init_req.status = SCLP_REQ_FAILED;
		sclp_running_state = sclp_running_state_idle;
	}
	spin_unlock_irqrestore(&sclp_lock, flags);
}

 
static int
sclp_check_interface(void)
{
	struct init_sccb *sccb;
	unsigned long flags;
	int retry;
	int rc;

	spin_lock_irqsave(&sclp_lock, flags);
	 
	rc = register_external_irq(EXT_IRQ_SERVICE_SIG, sclp_check_handler);
	if (rc) {
		spin_unlock_irqrestore(&sclp_lock, flags);
		return rc;
	}
	for (retry = 0; retry <= SCLP_INIT_RETRY; retry++) {
		__sclp_make_init_req(0, 0);
		sccb = (struct init_sccb *) sclp_init_req.sccb;
		rc = sclp_service_call_trace(sclp_init_req.command, sccb);
		if (rc == -EIO)
			break;
		sclp_init_req.status = SCLP_REQ_RUNNING;
		sclp_running_state = sclp_running_state_running;
		__sclp_set_request_timer(SCLP_RETRY_INTERVAL * HZ,
					 sclp_check_timeout);
		spin_unlock_irqrestore(&sclp_lock, flags);
		 
		irq_subclass_register(IRQ_SUBCLASS_SERVICE_SIGNAL);
		 
		sclp_sync_wait();
		 
		irq_subclass_unregister(IRQ_SUBCLASS_SERVICE_SIGNAL);
		spin_lock_irqsave(&sclp_lock, flags);
		del_timer(&sclp_request_timer);
		rc = -EBUSY;
		if (sclp_init_req.status == SCLP_REQ_DONE) {
			if (sccb->header.response_code == 0x20) {
				rc = 0;
				break;
			} else if (sccb->header.response_code == 0x74f0) {
				if (!sclp_mask_compat_mode) {
					sclp_mask_compat_mode = true;
					retry = 0;
				}
			}
		}
	}
	unregister_external_irq(EXT_IRQ_SERVICE_SIG, sclp_check_handler);
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

 
static int
sclp_reboot_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	sclp_deactivate();
	return NOTIFY_DONE;
}

static struct notifier_block sclp_reboot_notifier = {
	.notifier_call = sclp_reboot_event
};

static ssize_t con_pages_show(struct device_driver *dev, char *buf)
{
	return sysfs_emit(buf, "%i\n", sclp_console_pages);
}

static DRIVER_ATTR_RO(con_pages);

static ssize_t con_drop_store(struct device_driver *dev, const char *buf, size_t count)
{
	int rc;

	rc = kstrtobool(buf, &sclp_console_drop);
	return rc ?: count;
}

static ssize_t con_drop_show(struct device_driver *dev, char *buf)
{
	return sysfs_emit(buf, "%i\n", sclp_console_drop);
}

static DRIVER_ATTR_RW(con_drop);

static ssize_t con_full_show(struct device_driver *dev, char *buf)
{
	return sysfs_emit(buf, "%lu\n", sclp_console_full);
}

static DRIVER_ATTR_RO(con_full);

static struct attribute *sclp_drv_attrs[] = {
	&driver_attr_con_pages.attr,
	&driver_attr_con_drop.attr,
	&driver_attr_con_full.attr,
	NULL,
};
static struct attribute_group sclp_drv_attr_group = {
	.attrs = sclp_drv_attrs,
};
static const struct attribute_group *sclp_drv_attr_groups[] = {
	&sclp_drv_attr_group,
	NULL,
};

static struct platform_driver sclp_pdrv = {
	.driver = {
		.name	= "sclp",
		.groups = sclp_drv_attr_groups,
	},
};

 
static int
sclp_init(void)
{
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&sclp_lock, flags);
	 
	if (sclp_init_state != sclp_init_state_uninitialized)
		goto fail_unlock;
	sclp_init_state = sclp_init_state_initializing;
	sclp_read_sccb = (void *) __get_free_page(GFP_ATOMIC | GFP_DMA);
	sclp_init_sccb = (void *) __get_free_page(GFP_ATOMIC | GFP_DMA);
	BUG_ON(!sclp_read_sccb || !sclp_init_sccb);
	 
	list_add(&sclp_state_change_event.list, &sclp_reg_list);
	timer_setup(&sclp_request_timer, NULL, 0);
	timer_setup(&sclp_queue_timer, sclp_req_queue_timeout, 0);
	 
	spin_unlock_irqrestore(&sclp_lock, flags);
	rc = sclp_check_interface();
	spin_lock_irqsave(&sclp_lock, flags);
	if (rc)
		goto fail_init_state_uninitialized;
	 
	rc = register_reboot_notifier(&sclp_reboot_notifier);
	if (rc)
		goto fail_init_state_uninitialized;
	 
	rc = register_external_irq(EXT_IRQ_SERVICE_SIG, sclp_interrupt_handler);
	if (rc)
		goto fail_unregister_reboot_notifier;
	sclp_init_state = sclp_init_state_initialized;
	spin_unlock_irqrestore(&sclp_lock, flags);
	 
	irq_subclass_register(IRQ_SUBCLASS_SERVICE_SIGNAL);
	sclp_init_mask(1);
	return 0;

fail_unregister_reboot_notifier:
	unregister_reboot_notifier(&sclp_reboot_notifier);
fail_init_state_uninitialized:
	sclp_init_state = sclp_init_state_uninitialized;
	free_page((unsigned long) sclp_read_sccb);
	free_page((unsigned long) sclp_init_sccb);
fail_unlock:
	spin_unlock_irqrestore(&sclp_lock, flags);
	return rc;
}

static __init int sclp_initcall(void)
{
	int rc;

	rc = platform_driver_register(&sclp_pdrv);
	if (rc)
		return rc;

	return sclp_init();
}

arch_initcall(sclp_initcall);
