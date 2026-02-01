
 

 

#define pr_fmt(fmt) "ipmi_si: " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <linux/rcupdate.h>
#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>
#include "ipmi_si.h"
#include "ipmi_si_sm.h"
#include <linux/string.h>
#include <linux/ctype.h>

 
#undef DEBUG_TIMING

 
#define SI_TIMEOUT_TIME_USEC	10000
#define SI_USEC_PER_JIFFY	(1000000/HZ)
#define SI_TIMEOUT_JIFFIES	(SI_TIMEOUT_TIME_USEC/SI_USEC_PER_JIFFY)
#define SI_SHORT_TIMEOUT_USEC  250  

enum si_intf_state {
	SI_NORMAL,
	SI_GETTING_FLAGS,
	SI_GETTING_EVENTS,
	SI_CLEARING_FLAGS,
	SI_GETTING_MESSAGES,
	SI_CHECKING_ENABLES,
	SI_SETTING_ENABLES
	 
};

 
#define IPMI_BT_INTMASK_REG		2
#define IPMI_BT_INTMASK_CLEAR_IRQ_BIT	2
#define IPMI_BT_INTMASK_ENABLE_IRQ_BIT	1

 
const char *const si_to_str[] = { "invalid", "kcs", "smic", "bt", NULL };

static bool initialized;

 
enum si_stat_indexes {
	 
	SI_STAT_short_timeouts = 0,

	 
	SI_STAT_long_timeouts,

	 
	SI_STAT_idles,

	 
	SI_STAT_interrupts,

	 
	SI_STAT_attentions,

	 
	SI_STAT_flag_fetches,

	 
	SI_STAT_hosed_count,

	 
	SI_STAT_complete_transactions,

	 
	SI_STAT_events,

	 
	SI_STAT_watchdog_pretimeouts,

	 
	SI_STAT_incoming_messages,


	 
	SI_NUM_STATS
};

struct smi_info {
	int                    si_num;
	struct ipmi_smi        *intf;
	struct si_sm_data      *si_sm;
	const struct si_sm_handlers *handlers;
	spinlock_t             si_lock;
	struct ipmi_smi_msg    *waiting_msg;
	struct ipmi_smi_msg    *curr_msg;
	enum si_intf_state     si_state;

	 
	struct si_sm_io io;

	 
	int (*oem_data_avail_handler)(struct smi_info *smi_info);

	 
#define RECEIVE_MSG_AVAIL	0x01
#define EVENT_MSG_BUFFER_FULL	0x02
#define WDT_PRE_TIMEOUT_INT	0x08
#define OEM0_DATA_AVAIL     0x20
#define OEM1_DATA_AVAIL     0x40
#define OEM2_DATA_AVAIL     0x80
#define OEM_DATA_AVAIL      (OEM0_DATA_AVAIL | \
			     OEM1_DATA_AVAIL | \
			     OEM2_DATA_AVAIL)
	unsigned char       msg_flags;

	 
	bool		    has_event_buffer;

	 
	atomic_t            req_events;

	 
	bool                run_to_completion;

	 
	struct timer_list   si_timer;

	 
	bool		    timer_can_start;

	 
	bool		    timer_running;

	 
	unsigned long       last_timeout_jiffies;

	 
	atomic_t            need_watch;

	 
	bool interrupt_disabled;

	 
	bool supports_event_msg_buff;

	 
	bool cannot_disable_irq;

	 
	bool irq_enable_broken;

	 
	bool in_maintenance_mode;

	 
	bool got_attn;

	 
	struct ipmi_device_id device_id;

	 
	bool dev_group_added;

	 
	atomic_t stats[SI_NUM_STATS];

	struct task_struct *thread;

	struct list_head link;
};

#define smi_inc_stat(smi, stat) \
	atomic_inc(&(smi)->stats[SI_STAT_ ## stat])
#define smi_get_stat(smi, stat) \
	((unsigned int) atomic_read(&(smi)->stats[SI_STAT_ ## stat]))

#define IPMI_MAX_INTFS 4
static int force_kipmid[IPMI_MAX_INTFS];
static int num_force_kipmid;

static unsigned int kipmid_max_busy_us[IPMI_MAX_INTFS];
static int num_max_busy_us;

static bool unload_when_empty = true;

static int try_smi_init(struct smi_info *smi);
static void cleanup_one_si(struct smi_info *smi_info);
static void cleanup_ipmi_si(void);

#ifdef DEBUG_TIMING
void debug_timestamp(struct smi_info *smi_info, char *msg)
{
	struct timespec64 t;

	ktime_get_ts64(&t);
	dev_dbg(smi_info->io.dev, "**%s: %lld.%9.9ld\n",
		msg, t.tv_sec, t.tv_nsec);
}
#else
#define debug_timestamp(smi_info, x)
#endif

static ATOMIC_NOTIFIER_HEAD(xaction_notifier_list);
static int register_xaction_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&xaction_notifier_list, nb);
}

static void deliver_recv_msg(struct smi_info *smi_info,
			     struct ipmi_smi_msg *msg)
{
	 
	ipmi_smi_msg_received(smi_info->intf, msg);
}

static void return_hosed_msg(struct smi_info *smi_info, int cCode)
{
	struct ipmi_smi_msg *msg = smi_info->curr_msg;

	if (cCode < 0 || cCode > IPMI_ERR_UNSPECIFIED)
		cCode = IPMI_ERR_UNSPECIFIED;
	 

	 
	msg->rsp[0] = msg->data[0] | 4;
	msg->rsp[1] = msg->data[1];
	msg->rsp[2] = cCode;
	msg->rsp_size = 3;

	smi_info->curr_msg = NULL;
	deliver_recv_msg(smi_info, msg);
}

static enum si_sm_result start_next_msg(struct smi_info *smi_info)
{
	int              rv;

	if (!smi_info->waiting_msg) {
		smi_info->curr_msg = NULL;
		rv = SI_SM_IDLE;
	} else {
		int err;

		smi_info->curr_msg = smi_info->waiting_msg;
		smi_info->waiting_msg = NULL;
		debug_timestamp(smi_info, "Start2");
		err = atomic_notifier_call_chain(&xaction_notifier_list,
				0, smi_info);
		if (err & NOTIFY_STOP_MASK) {
			rv = SI_SM_CALL_WITHOUT_DELAY;
			goto out;
		}
		err = smi_info->handlers->start_transaction(
			smi_info->si_sm,
			smi_info->curr_msg->data,
			smi_info->curr_msg->data_size);
		if (err)
			return_hosed_msg(smi_info, err);

		rv = SI_SM_CALL_WITHOUT_DELAY;
	}
out:
	return rv;
}

static void smi_mod_timer(struct smi_info *smi_info, unsigned long new_val)
{
	if (!smi_info->timer_can_start)
		return;
	smi_info->last_timeout_jiffies = jiffies;
	mod_timer(&smi_info->si_timer, new_val);
	smi_info->timer_running = true;
}

 
static void start_new_msg(struct smi_info *smi_info, unsigned char *msg,
			  unsigned int size)
{
	smi_mod_timer(smi_info, jiffies + SI_TIMEOUT_JIFFIES);

	if (smi_info->thread)
		wake_up_process(smi_info->thread);

	smi_info->handlers->start_transaction(smi_info->si_sm, msg, size);
}

static void start_check_enables(struct smi_info *smi_info)
{
	unsigned char msg[2];

	msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
	msg[1] = IPMI_GET_BMC_GLOBAL_ENABLES_CMD;

	start_new_msg(smi_info, msg, 2);
	smi_info->si_state = SI_CHECKING_ENABLES;
}

static void start_clear_flags(struct smi_info *smi_info)
{
	unsigned char msg[3];

	 
	msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
	msg[1] = IPMI_CLEAR_MSG_FLAGS_CMD;
	msg[2] = WDT_PRE_TIMEOUT_INT;

	start_new_msg(smi_info, msg, 3);
	smi_info->si_state = SI_CLEARING_FLAGS;
}

static void start_getting_msg_queue(struct smi_info *smi_info)
{
	smi_info->curr_msg->data[0] = (IPMI_NETFN_APP_REQUEST << 2);
	smi_info->curr_msg->data[1] = IPMI_GET_MSG_CMD;
	smi_info->curr_msg->data_size = 2;

	start_new_msg(smi_info, smi_info->curr_msg->data,
		      smi_info->curr_msg->data_size);
	smi_info->si_state = SI_GETTING_MESSAGES;
}

static void start_getting_events(struct smi_info *smi_info)
{
	smi_info->curr_msg->data[0] = (IPMI_NETFN_APP_REQUEST << 2);
	smi_info->curr_msg->data[1] = IPMI_READ_EVENT_MSG_BUFFER_CMD;
	smi_info->curr_msg->data_size = 2;

	start_new_msg(smi_info, smi_info->curr_msg->data,
		      smi_info->curr_msg->data_size);
	smi_info->si_state = SI_GETTING_EVENTS;
}

 
static inline bool disable_si_irq(struct smi_info *smi_info)
{
	if ((smi_info->io.irq) && (!smi_info->interrupt_disabled)) {
		smi_info->interrupt_disabled = true;
		start_check_enables(smi_info);
		return true;
	}
	return false;
}

static inline bool enable_si_irq(struct smi_info *smi_info)
{
	if ((smi_info->io.irq) && (smi_info->interrupt_disabled)) {
		smi_info->interrupt_disabled = false;
		start_check_enables(smi_info);
		return true;
	}
	return false;
}

 
static struct ipmi_smi_msg *alloc_msg_handle_irq(struct smi_info *smi_info)
{
	struct ipmi_smi_msg *msg;

	msg = ipmi_alloc_smi_msg();
	if (!msg) {
		if (!disable_si_irq(smi_info))
			smi_info->si_state = SI_NORMAL;
	} else if (enable_si_irq(smi_info)) {
		ipmi_free_smi_msg(msg);
		msg = NULL;
	}
	return msg;
}

static void handle_flags(struct smi_info *smi_info)
{
retry:
	if (smi_info->msg_flags & WDT_PRE_TIMEOUT_INT) {
		 
		smi_inc_stat(smi_info, watchdog_pretimeouts);

		start_clear_flags(smi_info);
		smi_info->msg_flags &= ~WDT_PRE_TIMEOUT_INT;
		ipmi_smi_watchdog_pretimeout(smi_info->intf);
	} else if (smi_info->msg_flags & RECEIVE_MSG_AVAIL) {
		 
		smi_info->curr_msg = alloc_msg_handle_irq(smi_info);
		if (!smi_info->curr_msg)
			return;

		start_getting_msg_queue(smi_info);
	} else if (smi_info->msg_flags & EVENT_MSG_BUFFER_FULL) {
		 
		smi_info->curr_msg = alloc_msg_handle_irq(smi_info);
		if (!smi_info->curr_msg)
			return;

		start_getting_events(smi_info);
	} else if (smi_info->msg_flags & OEM_DATA_AVAIL &&
		   smi_info->oem_data_avail_handler) {
		if (smi_info->oem_data_avail_handler(smi_info))
			goto retry;
	} else
		smi_info->si_state = SI_NORMAL;
}

 
#define GLOBAL_ENABLES_MASK (IPMI_BMC_EVT_MSG_BUFF | IPMI_BMC_RCV_MSG_INTR | \
			     IPMI_BMC_EVT_MSG_INTR)

static u8 current_global_enables(struct smi_info *smi_info, u8 base,
				 bool *irq_on)
{
	u8 enables = 0;

	if (smi_info->supports_event_msg_buff)
		enables |= IPMI_BMC_EVT_MSG_BUFF;

	if (((smi_info->io.irq && !smi_info->interrupt_disabled) ||
	     smi_info->cannot_disable_irq) &&
	    !smi_info->irq_enable_broken)
		enables |= IPMI_BMC_RCV_MSG_INTR;

	if (smi_info->supports_event_msg_buff &&
	    smi_info->io.irq && !smi_info->interrupt_disabled &&
	    !smi_info->irq_enable_broken)
		enables |= IPMI_BMC_EVT_MSG_INTR;

	*irq_on = enables & (IPMI_BMC_EVT_MSG_INTR | IPMI_BMC_RCV_MSG_INTR);

	return enables;
}

static void check_bt_irq(struct smi_info *smi_info, bool irq_on)
{
	u8 irqstate = smi_info->io.inputb(&smi_info->io, IPMI_BT_INTMASK_REG);

	irqstate &= IPMI_BT_INTMASK_ENABLE_IRQ_BIT;

	if ((bool)irqstate == irq_on)
		return;

	if (irq_on)
		smi_info->io.outputb(&smi_info->io, IPMI_BT_INTMASK_REG,
				     IPMI_BT_INTMASK_ENABLE_IRQ_BIT);
	else
		smi_info->io.outputb(&smi_info->io, IPMI_BT_INTMASK_REG, 0);
}

static void handle_transaction_done(struct smi_info *smi_info)
{
	struct ipmi_smi_msg *msg;

	debug_timestamp(smi_info, "Done");
	switch (smi_info->si_state) {
	case SI_NORMAL:
		if (!smi_info->curr_msg)
			break;

		smi_info->curr_msg->rsp_size
			= smi_info->handlers->get_result(
				smi_info->si_sm,
				smi_info->curr_msg->rsp,
				IPMI_MAX_MSG_LENGTH);

		 
		msg = smi_info->curr_msg;
		smi_info->curr_msg = NULL;
		deliver_recv_msg(smi_info, msg);
		break;

	case SI_GETTING_FLAGS:
	{
		unsigned char msg[4];
		unsigned int  len;

		 
		len = smi_info->handlers->get_result(smi_info->si_sm, msg, 4);
		if (msg[2] != 0) {
			 
			smi_info->si_state = SI_NORMAL;
		} else if (len < 4) {
			 
			smi_info->si_state = SI_NORMAL;
		} else {
			smi_info->msg_flags = msg[3];
			handle_flags(smi_info);
		}
		break;
	}

	case SI_CLEARING_FLAGS:
	{
		unsigned char msg[3];

		 
		smi_info->handlers->get_result(smi_info->si_sm, msg, 3);
		if (msg[2] != 0) {
			 
			dev_warn_ratelimited(smi_info->io.dev,
				 "Error clearing flags: %2.2x\n", msg[2]);
		}
		smi_info->si_state = SI_NORMAL;
		break;
	}

	case SI_GETTING_EVENTS:
	{
		smi_info->curr_msg->rsp_size
			= smi_info->handlers->get_result(
				smi_info->si_sm,
				smi_info->curr_msg->rsp,
				IPMI_MAX_MSG_LENGTH);

		 
		msg = smi_info->curr_msg;
		smi_info->curr_msg = NULL;
		if (msg->rsp[2] != 0) {
			 
			msg->done(msg);

			 
			smi_info->msg_flags &= ~EVENT_MSG_BUFFER_FULL;
			handle_flags(smi_info);
		} else {
			smi_inc_stat(smi_info, events);

			 
			handle_flags(smi_info);

			deliver_recv_msg(smi_info, msg);
		}
		break;
	}

	case SI_GETTING_MESSAGES:
	{
		smi_info->curr_msg->rsp_size
			= smi_info->handlers->get_result(
				smi_info->si_sm,
				smi_info->curr_msg->rsp,
				IPMI_MAX_MSG_LENGTH);

		 
		msg = smi_info->curr_msg;
		smi_info->curr_msg = NULL;
		if (msg->rsp[2] != 0) {
			 
			msg->done(msg);

			 
			smi_info->msg_flags &= ~RECEIVE_MSG_AVAIL;
			handle_flags(smi_info);
		} else {
			smi_inc_stat(smi_info, incoming_messages);

			 
			handle_flags(smi_info);

			deliver_recv_msg(smi_info, msg);
		}
		break;
	}

	case SI_CHECKING_ENABLES:
	{
		unsigned char msg[4];
		u8 enables;
		bool irq_on;

		 
		smi_info->handlers->get_result(smi_info->si_sm, msg, 4);
		if (msg[2] != 0) {
			dev_warn_ratelimited(smi_info->io.dev,
				"Couldn't get irq info: %x,\n"
				"Maybe ok, but ipmi might run very slowly.\n",
				msg[2]);
			smi_info->si_state = SI_NORMAL;
			break;
		}
		enables = current_global_enables(smi_info, 0, &irq_on);
		if (smi_info->io.si_type == SI_BT)
			 
			check_bt_irq(smi_info, irq_on);
		if (enables != (msg[3] & GLOBAL_ENABLES_MASK)) {
			 
			msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
			msg[1] = IPMI_SET_BMC_GLOBAL_ENABLES_CMD;
			msg[2] = enables | (msg[3] & ~GLOBAL_ENABLES_MASK);
			smi_info->handlers->start_transaction(
				smi_info->si_sm, msg, 3);
			smi_info->si_state = SI_SETTING_ENABLES;
		} else if (smi_info->supports_event_msg_buff) {
			smi_info->curr_msg = ipmi_alloc_smi_msg();
			if (!smi_info->curr_msg) {
				smi_info->si_state = SI_NORMAL;
				break;
			}
			start_getting_events(smi_info);
		} else {
			smi_info->si_state = SI_NORMAL;
		}
		break;
	}

	case SI_SETTING_ENABLES:
	{
		unsigned char msg[4];

		smi_info->handlers->get_result(smi_info->si_sm, msg, 4);
		if (msg[2] != 0)
			dev_warn_ratelimited(smi_info->io.dev,
				 "Could not set the global enables: 0x%x.\n",
				 msg[2]);

		if (smi_info->supports_event_msg_buff) {
			smi_info->curr_msg = ipmi_alloc_smi_msg();
			if (!smi_info->curr_msg) {
				smi_info->si_state = SI_NORMAL;
				break;
			}
			start_getting_events(smi_info);
		} else {
			smi_info->si_state = SI_NORMAL;
		}
		break;
	}
	}
}

 
static enum si_sm_result smi_event_handler(struct smi_info *smi_info,
					   int time)
{
	enum si_sm_result si_sm_result;

restart:
	 
	si_sm_result = smi_info->handlers->event(smi_info->si_sm, time);
	time = 0;
	while (si_sm_result == SI_SM_CALL_WITHOUT_DELAY)
		si_sm_result = smi_info->handlers->event(smi_info->si_sm, 0);

	if (si_sm_result == SI_SM_TRANSACTION_COMPLETE) {
		smi_inc_stat(smi_info, complete_transactions);

		handle_transaction_done(smi_info);
		goto restart;
	} else if (si_sm_result == SI_SM_HOSED) {
		smi_inc_stat(smi_info, hosed_count);

		 
		smi_info->si_state = SI_NORMAL;
		if (smi_info->curr_msg != NULL) {
			 
			return_hosed_msg(smi_info, IPMI_ERR_UNSPECIFIED);
		}
		goto restart;
	}

	 
	if (si_sm_result == SI_SM_ATTN || smi_info->got_attn) {
		unsigned char msg[2];

		if (smi_info->si_state != SI_NORMAL) {
			 
			smi_info->got_attn = true;
		} else {
			smi_info->got_attn = false;
			smi_inc_stat(smi_info, attentions);

			 
			msg[0] = (IPMI_NETFN_APP_REQUEST << 2);
			msg[1] = IPMI_GET_MSG_FLAGS_CMD;

			start_new_msg(smi_info, msg, 2);
			smi_info->si_state = SI_GETTING_FLAGS;
			goto restart;
		}
	}

	 
	if (si_sm_result == SI_SM_IDLE) {
		smi_inc_stat(smi_info, idles);

		si_sm_result = start_next_msg(smi_info);
		if (si_sm_result != SI_SM_IDLE)
			goto restart;
	}

	if ((si_sm_result == SI_SM_IDLE)
	    && (atomic_read(&smi_info->req_events))) {
		 
		atomic_set(&smi_info->req_events, 0);

		 
		if (smi_info->supports_event_msg_buff || smi_info->io.irq) {
			start_check_enables(smi_info);
		} else {
			smi_info->curr_msg = alloc_msg_handle_irq(smi_info);
			if (!smi_info->curr_msg)
				goto out;

			start_getting_events(smi_info);
		}
		goto restart;
	}

	if (si_sm_result == SI_SM_IDLE && smi_info->timer_running) {
		 
		if (del_timer(&smi_info->si_timer))
			smi_info->timer_running = false;
	}

out:
	return si_sm_result;
}

static void check_start_timer_thread(struct smi_info *smi_info)
{
	if (smi_info->si_state == SI_NORMAL && smi_info->curr_msg == NULL) {
		smi_mod_timer(smi_info, jiffies + SI_TIMEOUT_JIFFIES);

		if (smi_info->thread)
			wake_up_process(smi_info->thread);

		start_next_msg(smi_info);
		smi_event_handler(smi_info, 0);
	}
}

static void flush_messages(void *send_info)
{
	struct smi_info *smi_info = send_info;
	enum si_sm_result result;

	 
	result = smi_event_handler(smi_info, 0);
	while (result != SI_SM_IDLE) {
		udelay(SI_SHORT_TIMEOUT_USEC);
		result = smi_event_handler(smi_info, SI_SHORT_TIMEOUT_USEC);
	}
}

static void sender(void                *send_info,
		   struct ipmi_smi_msg *msg)
{
	struct smi_info   *smi_info = send_info;
	unsigned long     flags;

	debug_timestamp(smi_info, "Enqueue");

	if (smi_info->run_to_completion) {
		 
		smi_info->waiting_msg = msg;
		return;
	}

	spin_lock_irqsave(&smi_info->si_lock, flags);
	 
	BUG_ON(smi_info->waiting_msg);
	smi_info->waiting_msg = msg;
	check_start_timer_thread(smi_info);
	spin_unlock_irqrestore(&smi_info->si_lock, flags);
}

static void set_run_to_completion(void *send_info, bool i_run_to_completion)
{
	struct smi_info   *smi_info = send_info;

	smi_info->run_to_completion = i_run_to_completion;
	if (i_run_to_completion)
		flush_messages(smi_info);
}

 
#define IPMI_TIME_NOT_BUSY ns_to_ktime(-1ull)
static inline bool ipmi_thread_busy_wait(enum si_sm_result smi_result,
					 const struct smi_info *smi_info,
					 ktime_t *busy_until)
{
	unsigned int max_busy_us = 0;

	if (smi_info->si_num < num_max_busy_us)
		max_busy_us = kipmid_max_busy_us[smi_info->si_num];
	if (max_busy_us == 0 || smi_result != SI_SM_CALL_WITH_DELAY)
		*busy_until = IPMI_TIME_NOT_BUSY;
	else if (*busy_until == IPMI_TIME_NOT_BUSY) {
		*busy_until = ktime_get() + max_busy_us * NSEC_PER_USEC;
	} else {
		if (unlikely(ktime_get() > *busy_until)) {
			*busy_until = IPMI_TIME_NOT_BUSY;
			return false;
		}
	}
	return true;
}


 
static int ipmi_thread(void *data)
{
	struct smi_info *smi_info = data;
	unsigned long flags;
	enum si_sm_result smi_result;
	ktime_t busy_until = IPMI_TIME_NOT_BUSY;

	set_user_nice(current, MAX_NICE);
	while (!kthread_should_stop()) {
		int busy_wait;

		spin_lock_irqsave(&(smi_info->si_lock), flags);
		smi_result = smi_event_handler(smi_info, 0);

		 
		if (smi_result != SI_SM_IDLE && !smi_info->timer_running)
			smi_mod_timer(smi_info, jiffies + SI_TIMEOUT_JIFFIES);

		spin_unlock_irqrestore(&(smi_info->si_lock), flags);
		busy_wait = ipmi_thread_busy_wait(smi_result, smi_info,
						  &busy_until);
		if (smi_result == SI_SM_CALL_WITHOUT_DELAY) {
			;  
		} else if (smi_result == SI_SM_CALL_WITH_DELAY && busy_wait) {
			 
			if (smi_info->in_maintenance_mode)
				schedule();
			else
				usleep_range(100, 200);
		} else if (smi_result == SI_SM_IDLE) {
			if (atomic_read(&smi_info->need_watch)) {
				schedule_timeout_interruptible(100);
			} else {
				 
				__set_current_state(TASK_INTERRUPTIBLE);
				schedule();
			}
		} else {
			schedule_timeout_interruptible(1);
		}
	}
	return 0;
}


static void poll(void *send_info)
{
	struct smi_info *smi_info = send_info;
	unsigned long flags = 0;
	bool run_to_completion = smi_info->run_to_completion;

	 
	udelay(10);
	if (!run_to_completion)
		spin_lock_irqsave(&smi_info->si_lock, flags);
	smi_event_handler(smi_info, 10);
	if (!run_to_completion)
		spin_unlock_irqrestore(&smi_info->si_lock, flags);
}

static void request_events(void *send_info)
{
	struct smi_info *smi_info = send_info;

	if (!smi_info->has_event_buffer)
		return;

	atomic_set(&smi_info->req_events, 1);
}

static void set_need_watch(void *send_info, unsigned int watch_mask)
{
	struct smi_info *smi_info = send_info;
	unsigned long flags;
	int enable;

	enable = !!watch_mask;

	atomic_set(&smi_info->need_watch, enable);
	spin_lock_irqsave(&smi_info->si_lock, flags);
	check_start_timer_thread(smi_info);
	spin_unlock_irqrestore(&smi_info->si_lock, flags);
}

static void smi_timeout(struct timer_list *t)
{
	struct smi_info   *smi_info = from_timer(smi_info, t, si_timer);
	enum si_sm_result smi_result;
	unsigned long     flags;
	unsigned long     jiffies_now;
	long              time_diff;
	long		  timeout;

	spin_lock_irqsave(&(smi_info->si_lock), flags);
	debug_timestamp(smi_info, "Timer");

	jiffies_now = jiffies;
	time_diff = (((long)jiffies_now - (long)smi_info->last_timeout_jiffies)
		     * SI_USEC_PER_JIFFY);
	smi_result = smi_event_handler(smi_info, time_diff);

	if ((smi_info->io.irq) && (!smi_info->interrupt_disabled)) {
		 
		timeout = jiffies + SI_TIMEOUT_JIFFIES;
		smi_inc_stat(smi_info, long_timeouts);
		goto do_mod_timer;
	}

	 
	if (smi_result == SI_SM_CALL_WITH_DELAY) {
		smi_inc_stat(smi_info, short_timeouts);
		timeout = jiffies + 1;
	} else {
		smi_inc_stat(smi_info, long_timeouts);
		timeout = jiffies + SI_TIMEOUT_JIFFIES;
	}

do_mod_timer:
	if (smi_result != SI_SM_IDLE)
		smi_mod_timer(smi_info, timeout);
	else
		smi_info->timer_running = false;
	spin_unlock_irqrestore(&(smi_info->si_lock), flags);
}

irqreturn_t ipmi_si_irq_handler(int irq, void *data)
{
	struct smi_info *smi_info = data;
	unsigned long   flags;

	if (smi_info->io.si_type == SI_BT)
		 
		smi_info->io.outputb(&smi_info->io, IPMI_BT_INTMASK_REG,
				     IPMI_BT_INTMASK_CLEAR_IRQ_BIT
				     | IPMI_BT_INTMASK_ENABLE_IRQ_BIT);

	spin_lock_irqsave(&(smi_info->si_lock), flags);

	smi_inc_stat(smi_info, interrupts);

	debug_timestamp(smi_info, "Interrupt");

	smi_event_handler(smi_info, 0);
	spin_unlock_irqrestore(&(smi_info->si_lock), flags);
	return IRQ_HANDLED;
}

static int smi_start_processing(void            *send_info,
				struct ipmi_smi *intf)
{
	struct smi_info *new_smi = send_info;
	int             enable = 0;

	new_smi->intf = intf;

	 
	timer_setup(&new_smi->si_timer, smi_timeout, 0);
	new_smi->timer_can_start = true;
	smi_mod_timer(new_smi, jiffies + SI_TIMEOUT_JIFFIES);

	 
	if (new_smi->io.irq_setup) {
		new_smi->io.irq_handler_data = new_smi;
		new_smi->io.irq_setup(&new_smi->io);
	}

	 
	if (new_smi->si_num < num_force_kipmid)
		enable = force_kipmid[new_smi->si_num];
	 
	else if ((new_smi->io.si_type != SI_BT) && (!new_smi->io.irq))
		enable = 1;

	if (enable) {
		new_smi->thread = kthread_run(ipmi_thread, new_smi,
					      "kipmi%d", new_smi->si_num);
		if (IS_ERR(new_smi->thread)) {
			dev_notice(new_smi->io.dev,
				   "Could not start kernel thread due to error %ld, only using timers to drive the interface\n",
				   PTR_ERR(new_smi->thread));
			new_smi->thread = NULL;
		}
	}

	return 0;
}

static int get_smi_info(void *send_info, struct ipmi_smi_info *data)
{
	struct smi_info *smi = send_info;

	data->addr_src = smi->io.addr_source;
	data->dev = smi->io.dev;
	data->addr_info = smi->io.addr_info;
	get_device(smi->io.dev);

	return 0;
}

static void set_maintenance_mode(void *send_info, bool enable)
{
	struct smi_info   *smi_info = send_info;

	if (!enable)
		atomic_set(&smi_info->req_events, 0);
	smi_info->in_maintenance_mode = enable;
}

static void shutdown_smi(void *send_info);
static const struct ipmi_smi_handlers handlers = {
	.owner                  = THIS_MODULE,
	.start_processing       = smi_start_processing,
	.shutdown               = shutdown_smi,
	.get_smi_info		= get_smi_info,
	.sender			= sender,
	.request_events		= request_events,
	.set_need_watch		= set_need_watch,
	.set_maintenance_mode   = set_maintenance_mode,
	.set_run_to_completion  = set_run_to_completion,
	.flush_messages		= flush_messages,
	.poll			= poll,
};

static LIST_HEAD(smi_infos);
static DEFINE_MUTEX(smi_infos_lock);
static int smi_num;  

static const char * const addr_space_to_str[] = { "i/o", "mem" };

module_param_array(force_kipmid, int, &num_force_kipmid, 0);
MODULE_PARM_DESC(force_kipmid,
		 "Force the kipmi daemon to be enabled (1) or disabled(0).  Normally the IPMI driver auto-detects this, but the value may be overridden by this parm.");
module_param(unload_when_empty, bool, 0);
MODULE_PARM_DESC(unload_when_empty,
		 "Unload the module if no interfaces are specified or found, default is 1.  Setting to 0 is useful for hot add of devices using hotmod.");
module_param_array(kipmid_max_busy_us, uint, &num_max_busy_us, 0644);
MODULE_PARM_DESC(kipmid_max_busy_us,
		 "Max time (in microseconds) to busy-wait for IPMI data before sleeping. 0 (default) means to wait forever. Set to 100-500 if kipmid is using up a lot of CPU time.");

void ipmi_irq_finish_setup(struct si_sm_io *io)
{
	if (io->si_type == SI_BT)
		 
		io->outputb(io, IPMI_BT_INTMASK_REG,
			    IPMI_BT_INTMASK_ENABLE_IRQ_BIT);
}

void ipmi_irq_start_cleanup(struct si_sm_io *io)
{
	if (io->si_type == SI_BT)
		 
		io->outputb(io, IPMI_BT_INTMASK_REG, 0);
}

static void std_irq_cleanup(struct si_sm_io *io)
{
	ipmi_irq_start_cleanup(io);
	free_irq(io->irq, io->irq_handler_data);
}

int ipmi_std_irq_setup(struct si_sm_io *io)
{
	int rv;

	if (!io->irq)
		return 0;

	rv = request_irq(io->irq,
			 ipmi_si_irq_handler,
			 IRQF_SHARED,
			 SI_DEVICE_NAME,
			 io->irq_handler_data);
	if (rv) {
		dev_warn(io->dev, "%s unable to claim interrupt %d, running polled\n",
			 SI_DEVICE_NAME, io->irq);
		io->irq = 0;
	} else {
		io->irq_cleanup = std_irq_cleanup;
		ipmi_irq_finish_setup(io);
		dev_info(io->dev, "Using irq %d\n", io->irq);
	}

	return rv;
}

static int wait_for_msg_done(struct smi_info *smi_info)
{
	enum si_sm_result     smi_result;

	smi_result = smi_info->handlers->event(smi_info->si_sm, 0);
	for (;;) {
		if (smi_result == SI_SM_CALL_WITH_DELAY ||
		    smi_result == SI_SM_CALL_WITH_TICK_DELAY) {
			schedule_timeout_uninterruptible(1);
			smi_result = smi_info->handlers->event(
				smi_info->si_sm, jiffies_to_usecs(1));
		} else if (smi_result == SI_SM_CALL_WITHOUT_DELAY) {
			smi_result = smi_info->handlers->event(
				smi_info->si_sm, 0);
		} else
			break;
	}
	if (smi_result == SI_SM_HOSED)
		 
		return -ENODEV;

	return 0;
}

static int try_get_dev_id(struct smi_info *smi_info)
{
	unsigned char         msg[2];
	unsigned char         *resp;
	unsigned long         resp_len;
	int                   rv = 0;
	unsigned int          retry_count = 0;

	resp = kmalloc(IPMI_MAX_MSG_LENGTH, GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	 
	msg[0] = IPMI_NETFN_APP_REQUEST << 2;
	msg[1] = IPMI_GET_DEVICE_ID_CMD;

retry:
	smi_info->handlers->start_transaction(smi_info->si_sm, msg, 2);

	rv = wait_for_msg_done(smi_info);
	if (rv)
		goto out;

	resp_len = smi_info->handlers->get_result(smi_info->si_sm,
						  resp, IPMI_MAX_MSG_LENGTH);

	 
	rv = ipmi_demangle_device_id(resp[0] >> 2, resp[1],
			resp + 2, resp_len - 2, &smi_info->device_id);
	if (rv) {
		 
		unsigned char cc = *(resp + 2);

		if (cc != IPMI_CC_NO_ERROR &&
		    ++retry_count <= GET_DEVICE_ID_MAX_RETRY) {
			dev_warn_ratelimited(smi_info->io.dev,
			    "BMC returned 0x%2.2x, retry get bmc device id\n",
			    cc);
			goto retry;
		}
	}

out:
	kfree(resp);
	return rv;
}

static int get_global_enables(struct smi_info *smi_info, u8 *enables)
{
	unsigned char         msg[3];
	unsigned char         *resp;
	unsigned long         resp_len;
	int                   rv;

	resp = kmalloc(IPMI_MAX_MSG_LENGTH, GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	msg[0] = IPMI_NETFN_APP_REQUEST << 2;
	msg[1] = IPMI_GET_BMC_GLOBAL_ENABLES_CMD;
	smi_info->handlers->start_transaction(smi_info->si_sm, msg, 2);

	rv = wait_for_msg_done(smi_info);
	if (rv) {
		dev_warn(smi_info->io.dev,
			 "Error getting response from get global enables command: %d\n",
			 rv);
		goto out;
	}

	resp_len = smi_info->handlers->get_result(smi_info->si_sm,
						  resp, IPMI_MAX_MSG_LENGTH);

	if (resp_len < 4 ||
			resp[0] != (IPMI_NETFN_APP_REQUEST | 1) << 2 ||
			resp[1] != IPMI_GET_BMC_GLOBAL_ENABLES_CMD   ||
			resp[2] != 0) {
		dev_warn(smi_info->io.dev,
			 "Invalid return from get global enables command: %ld %x %x %x\n",
			 resp_len, resp[0], resp[1], resp[2]);
		rv = -EINVAL;
		goto out;
	} else {
		*enables = resp[3];
	}

out:
	kfree(resp);
	return rv;
}

 
static int set_global_enables(struct smi_info *smi_info, u8 enables)
{
	unsigned char         msg[3];
	unsigned char         *resp;
	unsigned long         resp_len;
	int                   rv;

	resp = kmalloc(IPMI_MAX_MSG_LENGTH, GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	msg[0] = IPMI_NETFN_APP_REQUEST << 2;
	msg[1] = IPMI_SET_BMC_GLOBAL_ENABLES_CMD;
	msg[2] = enables;
	smi_info->handlers->start_transaction(smi_info->si_sm, msg, 3);

	rv = wait_for_msg_done(smi_info);
	if (rv) {
		dev_warn(smi_info->io.dev,
			 "Error getting response from set global enables command: %d\n",
			 rv);
		goto out;
	}

	resp_len = smi_info->handlers->get_result(smi_info->si_sm,
						  resp, IPMI_MAX_MSG_LENGTH);

	if (resp_len < 3 ||
			resp[0] != (IPMI_NETFN_APP_REQUEST | 1) << 2 ||
			resp[1] != IPMI_SET_BMC_GLOBAL_ENABLES_CMD) {
		dev_warn(smi_info->io.dev,
			 "Invalid return from set global enables command: %ld %x %x\n",
			 resp_len, resp[0], resp[1]);
		rv = -EINVAL;
		goto out;
	}

	if (resp[2] != 0)
		rv = 1;

out:
	kfree(resp);
	return rv;
}

 
static void check_clr_rcv_irq(struct smi_info *smi_info)
{
	u8 enables = 0;
	int rv;

	rv = get_global_enables(smi_info, &enables);
	if (!rv) {
		if ((enables & IPMI_BMC_RCV_MSG_INTR) == 0)
			 
			return;

		enables &= ~IPMI_BMC_RCV_MSG_INTR;
		rv = set_global_enables(smi_info, enables);
	}

	if (rv < 0) {
		dev_err(smi_info->io.dev,
			"Cannot check clearing the rcv irq: %d\n", rv);
		return;
	}

	if (rv) {
		 
		dev_warn(smi_info->io.dev,
			 "The BMC does not support clearing the recv irq bit, compensating, but the BMC needs to be fixed.\n");
		smi_info->cannot_disable_irq = true;
	}
}

 
static void check_set_rcv_irq(struct smi_info *smi_info)
{
	u8 enables = 0;
	int rv;

	if (!smi_info->io.irq)
		return;

	rv = get_global_enables(smi_info, &enables);
	if (!rv) {
		enables |= IPMI_BMC_RCV_MSG_INTR;
		rv = set_global_enables(smi_info, enables);
	}

	if (rv < 0) {
		dev_err(smi_info->io.dev,
			"Cannot check setting the rcv irq: %d\n", rv);
		return;
	}

	if (rv) {
		 
		dev_warn(smi_info->io.dev,
			 "The BMC does not support setting the recv irq bit, compensating, but the BMC needs to be fixed.\n");
		smi_info->cannot_disable_irq = true;
		smi_info->irq_enable_broken = true;
	}
}

static int try_enable_event_buffer(struct smi_info *smi_info)
{
	unsigned char         msg[3];
	unsigned char         *resp;
	unsigned long         resp_len;
	int                   rv = 0;

	resp = kmalloc(IPMI_MAX_MSG_LENGTH, GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	msg[0] = IPMI_NETFN_APP_REQUEST << 2;
	msg[1] = IPMI_GET_BMC_GLOBAL_ENABLES_CMD;
	smi_info->handlers->start_transaction(smi_info->si_sm, msg, 2);

	rv = wait_for_msg_done(smi_info);
	if (rv) {
		pr_warn("Error getting response from get global enables command, the event buffer is not enabled\n");
		goto out;
	}

	resp_len = smi_info->handlers->get_result(smi_info->si_sm,
						  resp, IPMI_MAX_MSG_LENGTH);

	if (resp_len < 4 ||
			resp[0] != (IPMI_NETFN_APP_REQUEST | 1) << 2 ||
			resp[1] != IPMI_GET_BMC_GLOBAL_ENABLES_CMD   ||
			resp[2] != 0) {
		pr_warn("Invalid return from get global enables command, cannot enable the event buffer\n");
		rv = -EINVAL;
		goto out;
	}

	if (resp[3] & IPMI_BMC_EVT_MSG_BUFF) {
		 
		smi_info->supports_event_msg_buff = true;
		goto out;
	}

	msg[0] = IPMI_NETFN_APP_REQUEST << 2;
	msg[1] = IPMI_SET_BMC_GLOBAL_ENABLES_CMD;
	msg[2] = resp[3] | IPMI_BMC_EVT_MSG_BUFF;
	smi_info->handlers->start_transaction(smi_info->si_sm, msg, 3);

	rv = wait_for_msg_done(smi_info);
	if (rv) {
		pr_warn("Error getting response from set global, enables command, the event buffer is not enabled\n");
		goto out;
	}

	resp_len = smi_info->handlers->get_result(smi_info->si_sm,
						  resp, IPMI_MAX_MSG_LENGTH);

	if (resp_len < 3 ||
			resp[0] != (IPMI_NETFN_APP_REQUEST | 1) << 2 ||
			resp[1] != IPMI_SET_BMC_GLOBAL_ENABLES_CMD) {
		pr_warn("Invalid return from get global, enables command, not enable the event buffer\n");
		rv = -EINVAL;
		goto out;
	}

	if (resp[2] != 0)
		 
		rv = -ENOENT;
	else
		smi_info->supports_event_msg_buff = true;

out:
	kfree(resp);
	return rv;
}

#define IPMI_SI_ATTR(name) \
static ssize_t name##_show(struct device *dev,			\
			   struct device_attribute *attr,		\
			   char *buf)					\
{									\
	struct smi_info *smi_info = dev_get_drvdata(dev);		\
									\
	return sysfs_emit(buf, "%u\n", smi_get_stat(smi_info, name));	\
}									\
static DEVICE_ATTR_RO(name)

static ssize_t type_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct smi_info *smi_info = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", si_to_str[smi_info->io.si_type]);
}
static DEVICE_ATTR_RO(type);

static ssize_t interrupts_enabled_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct smi_info *smi_info = dev_get_drvdata(dev);
	int enabled = smi_info->io.irq && !smi_info->interrupt_disabled;

	return sysfs_emit(buf, "%d\n", enabled);
}
static DEVICE_ATTR_RO(interrupts_enabled);

IPMI_SI_ATTR(short_timeouts);
IPMI_SI_ATTR(long_timeouts);
IPMI_SI_ATTR(idles);
IPMI_SI_ATTR(interrupts);
IPMI_SI_ATTR(attentions);
IPMI_SI_ATTR(flag_fetches);
IPMI_SI_ATTR(hosed_count);
IPMI_SI_ATTR(complete_transactions);
IPMI_SI_ATTR(events);
IPMI_SI_ATTR(watchdog_pretimeouts);
IPMI_SI_ATTR(incoming_messages);

static ssize_t params_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct smi_info *smi_info = dev_get_drvdata(dev);

	return sysfs_emit(buf,
			"%s,%s,0x%lx,rsp=%d,rsi=%d,rsh=%d,irq=%d,ipmb=%d\n",
			si_to_str[smi_info->io.si_type],
			addr_space_to_str[smi_info->io.addr_space],
			smi_info->io.addr_data,
			smi_info->io.regspacing,
			smi_info->io.regsize,
			smi_info->io.regshift,
			smi_info->io.irq,
			smi_info->io.slave_addr);
}
static DEVICE_ATTR_RO(params);

static struct attribute *ipmi_si_dev_attrs[] = {
	&dev_attr_type.attr,
	&dev_attr_interrupts_enabled.attr,
	&dev_attr_short_timeouts.attr,
	&dev_attr_long_timeouts.attr,
	&dev_attr_idles.attr,
	&dev_attr_interrupts.attr,
	&dev_attr_attentions.attr,
	&dev_attr_flag_fetches.attr,
	&dev_attr_hosed_count.attr,
	&dev_attr_complete_transactions.attr,
	&dev_attr_events.attr,
	&dev_attr_watchdog_pretimeouts.attr,
	&dev_attr_incoming_messages.attr,
	&dev_attr_params.attr,
	NULL
};

static const struct attribute_group ipmi_si_dev_attr_group = {
	.attrs		= ipmi_si_dev_attrs,
};

 
static int oem_data_avail_to_receive_msg_avail(struct smi_info *smi_info)
{
	smi_info->msg_flags = ((smi_info->msg_flags & ~OEM_DATA_AVAIL) |
			       RECEIVE_MSG_AVAIL);
	return 1;
}

 
#define DELL_POWEREDGE_8G_BMC_DEVICE_ID  0x20
#define DELL_POWEREDGE_8G_BMC_DEVICE_REV 0x80
#define DELL_POWEREDGE_8G_BMC_IPMI_VERSION 0x51
#define DELL_IANA_MFR_ID 0x0002a2
static void setup_dell_poweredge_oem_data_handler(struct smi_info *smi_info)
{
	struct ipmi_device_id *id = &smi_info->device_id;
	if (id->manufacturer_id == DELL_IANA_MFR_ID) {
		if (id->device_id       == DELL_POWEREDGE_8G_BMC_DEVICE_ID  &&
		    id->device_revision == DELL_POWEREDGE_8G_BMC_DEVICE_REV &&
		    id->ipmi_version   == DELL_POWEREDGE_8G_BMC_IPMI_VERSION) {
			smi_info->oem_data_avail_handler =
				oem_data_avail_to_receive_msg_avail;
		} else if (ipmi_version_major(id) < 1 ||
			   (ipmi_version_major(id) == 1 &&
			    ipmi_version_minor(id) < 5)) {
			smi_info->oem_data_avail_handler =
				oem_data_avail_to_receive_msg_avail;
		}
	}
}

#define CANNOT_RETURN_REQUESTED_LENGTH 0xCA
static void return_hosed_msg_badsize(struct smi_info *smi_info)
{
	struct ipmi_smi_msg *msg = smi_info->curr_msg;

	 
	msg->rsp[0] = msg->data[0] | 4;
	msg->rsp[1] = msg->data[1];
	msg->rsp[2] = CANNOT_RETURN_REQUESTED_LENGTH;
	msg->rsp_size = 3;
	smi_info->curr_msg = NULL;
	deliver_recv_msg(smi_info, msg);
}

 

#define STORAGE_NETFN 0x0A
#define STORAGE_CMD_GET_SDR 0x23
static int dell_poweredge_bt_xaction_handler(struct notifier_block *self,
					     unsigned long unused,
					     void *in)
{
	struct smi_info *smi_info = in;
	unsigned char *data = smi_info->curr_msg->data;
	unsigned int size   = smi_info->curr_msg->data_size;
	if (size >= 8 &&
	    (data[0]>>2) == STORAGE_NETFN &&
	    data[1] == STORAGE_CMD_GET_SDR &&
	    data[7] == 0x3A) {
		return_hosed_msg_badsize(smi_info);
		return NOTIFY_STOP;
	}
	return NOTIFY_DONE;
}

static struct notifier_block dell_poweredge_bt_xaction_notifier = {
	.notifier_call	= dell_poweredge_bt_xaction_handler,
};

 
static void
setup_dell_poweredge_bt_xaction_handler(struct smi_info *smi_info)
{
	struct ipmi_device_id *id = &smi_info->device_id;
	if (id->manufacturer_id == DELL_IANA_MFR_ID &&
	    smi_info->io.si_type == SI_BT)
		register_xaction_notifier(&dell_poweredge_bt_xaction_notifier);
}

 

static void setup_oem_data_handler(struct smi_info *smi_info)
{
	setup_dell_poweredge_oem_data_handler(smi_info);
}

static void setup_xaction_handlers(struct smi_info *smi_info)
{
	setup_dell_poweredge_bt_xaction_handler(smi_info);
}

static void check_for_broken_irqs(struct smi_info *smi_info)
{
	check_clr_rcv_irq(smi_info);
	check_set_rcv_irq(smi_info);
}

static inline void stop_timer_and_thread(struct smi_info *smi_info)
{
	if (smi_info->thread != NULL) {
		kthread_stop(smi_info->thread);
		smi_info->thread = NULL;
	}

	smi_info->timer_can_start = false;
	del_timer_sync(&smi_info->si_timer);
}

static struct smi_info *find_dup_si(struct smi_info *info)
{
	struct smi_info *e;

	list_for_each_entry(e, &smi_infos, link) {
		if (e->io.addr_space != info->io.addr_space)
			continue;
		if (e->io.addr_data == info->io.addr_data) {
			 
			if (info->io.slave_addr && !e->io.slave_addr)
				e->io.slave_addr = info->io.slave_addr;
			return e;
		}
	}

	return NULL;
}

int ipmi_si_add_smi(struct si_sm_io *io)
{
	int rv = 0;
	struct smi_info *new_smi, *dup;

	 
	if (io->addr_source != SI_HARDCODED && io->addr_source != SI_HOTMOD &&
	    ipmi_si_hardcode_match(io->addr_space, io->addr_data)) {
		dev_info(io->dev,
			 "Hard-coded device at this address already exists");
		return -ENODEV;
	}

	if (!io->io_setup) {
		if (io->addr_space == IPMI_IO_ADDR_SPACE) {
			io->io_setup = ipmi_si_port_setup;
		} else if (io->addr_space == IPMI_MEM_ADDR_SPACE) {
			io->io_setup = ipmi_si_mem_setup;
		} else {
			return -EINVAL;
		}
	}

	new_smi = kzalloc(sizeof(*new_smi), GFP_KERNEL);
	if (!new_smi)
		return -ENOMEM;
	spin_lock_init(&new_smi->si_lock);

	new_smi->io = *io;

	mutex_lock(&smi_infos_lock);
	dup = find_dup_si(new_smi);
	if (dup) {
		if (new_smi->io.addr_source == SI_ACPI &&
		    dup->io.addr_source == SI_SMBIOS) {
			 
			dev_info(dup->io.dev,
				 "Removing SMBIOS-specified %s state machine in favor of ACPI\n",
				 si_to_str[new_smi->io.si_type]);
			cleanup_one_si(dup);
		} else {
			dev_info(new_smi->io.dev,
				 "%s-specified %s state machine: duplicate\n",
				 ipmi_addr_src_to_str(new_smi->io.addr_source),
				 si_to_str[new_smi->io.si_type]);
			rv = -EBUSY;
			kfree(new_smi);
			goto out_err;
		}
	}

	pr_info("Adding %s-specified %s state machine\n",
		ipmi_addr_src_to_str(new_smi->io.addr_source),
		si_to_str[new_smi->io.si_type]);

	list_add_tail(&new_smi->link, &smi_infos);

	if (initialized)
		rv = try_smi_init(new_smi);
out_err:
	mutex_unlock(&smi_infos_lock);
	return rv;
}

 
static int try_smi_init(struct smi_info *new_smi)
{
	int rv = 0;
	int i;

	pr_info("Trying %s-specified %s state machine at %s address 0x%lx, slave address 0x%x, irq %d\n",
		ipmi_addr_src_to_str(new_smi->io.addr_source),
		si_to_str[new_smi->io.si_type],
		addr_space_to_str[new_smi->io.addr_space],
		new_smi->io.addr_data,
		new_smi->io.slave_addr, new_smi->io.irq);

	switch (new_smi->io.si_type) {
	case SI_KCS:
		new_smi->handlers = &kcs_smi_handlers;
		break;

	case SI_SMIC:
		new_smi->handlers = &smic_smi_handlers;
		break;

	case SI_BT:
		new_smi->handlers = &bt_smi_handlers;
		break;

	default:
		 
		rv = -EIO;
		goto out_err;
	}

	new_smi->si_num = smi_num;

	 
	if (!new_smi->io.dev) {
		pr_err("IPMI interface added with no device\n");
		rv = -EIO;
		goto out_err;
	}

	 
	new_smi->si_sm = kmalloc(new_smi->handlers->size(), GFP_KERNEL);
	if (!new_smi->si_sm) {
		rv = -ENOMEM;
		goto out_err;
	}
	new_smi->io.io_size = new_smi->handlers->init_data(new_smi->si_sm,
							   &new_smi->io);

	 
	rv = new_smi->io.io_setup(&new_smi->io);
	if (rv) {
		dev_err(new_smi->io.dev, "Could not set up I/O space\n");
		goto out_err;
	}

	 
	if (new_smi->handlers->detect(new_smi->si_sm)) {
		if (new_smi->io.addr_source)
			dev_err(new_smi->io.dev,
				"Interface detection failed\n");
		rv = -ENODEV;
		goto out_err;
	}

	 
	rv = try_get_dev_id(new_smi);
	if (rv) {
		if (new_smi->io.addr_source)
			dev_err(new_smi->io.dev,
			       "There appears to be no BMC at this location\n");
		goto out_err;
	}

	setup_oem_data_handler(new_smi);
	setup_xaction_handlers(new_smi);
	check_for_broken_irqs(new_smi);

	new_smi->waiting_msg = NULL;
	new_smi->curr_msg = NULL;
	atomic_set(&new_smi->req_events, 0);
	new_smi->run_to_completion = false;
	for (i = 0; i < SI_NUM_STATS; i++)
		atomic_set(&new_smi->stats[i], 0);

	new_smi->interrupt_disabled = true;
	atomic_set(&new_smi->need_watch, 0);

	rv = try_enable_event_buffer(new_smi);
	if (rv == 0)
		new_smi->has_event_buffer = true;

	 
	start_clear_flags(new_smi);

	 
	if (new_smi->io.irq) {
		new_smi->interrupt_disabled = false;
		atomic_set(&new_smi->req_events, 1);
	}

	dev_set_drvdata(new_smi->io.dev, new_smi);
	rv = device_add_group(new_smi->io.dev, &ipmi_si_dev_attr_group);
	if (rv) {
		dev_err(new_smi->io.dev,
			"Unable to add device attributes: error %d\n",
			rv);
		goto out_err;
	}
	new_smi->dev_group_added = true;

	rv = ipmi_register_smi(&handlers,
			       new_smi,
			       new_smi->io.dev,
			       new_smi->io.slave_addr);
	if (rv) {
		dev_err(new_smi->io.dev,
			"Unable to register device: error %d\n",
			rv);
		goto out_err;
	}

	 
	smi_num++;

	dev_info(new_smi->io.dev, "IPMI %s interface initialized\n",
		 si_to_str[new_smi->io.si_type]);

	WARN_ON(new_smi->io.dev->init_name != NULL);

 out_err:
	if (rv && new_smi->io.io_cleanup) {
		new_smi->io.io_cleanup(&new_smi->io);
		new_smi->io.io_cleanup = NULL;
	}

	if (rv && new_smi->si_sm) {
		kfree(new_smi->si_sm);
		new_smi->si_sm = NULL;
	}

	return rv;
}

static int __init init_ipmi_si(void)
{
	struct smi_info *e;
	enum ipmi_addr_src type = SI_INVALID;

	if (initialized)
		return 0;

	ipmi_hardcode_init();

	pr_info("IPMI System Interface driver\n");

	ipmi_si_platform_init();

	ipmi_si_pci_init();

	ipmi_si_parisc_init();

	 
	mutex_lock(&smi_infos_lock);
	list_for_each_entry(e, &smi_infos, link) {
		 
		if (e->io.irq && (!type || e->io.addr_source == type)) {
			if (!try_smi_init(e)) {
				type = e->io.addr_source;
			}
		}
	}

	 
	if (type)
		goto skip_fallback_noirq;

	 

	list_for_each_entry(e, &smi_infos, link) {
		if (!e->io.irq && (!type || e->io.addr_source == type)) {
			if (!try_smi_init(e)) {
				type = e->io.addr_source;
			}
		}
	}

skip_fallback_noirq:
	initialized = true;
	mutex_unlock(&smi_infos_lock);

	if (type)
		return 0;

	mutex_lock(&smi_infos_lock);
	if (unload_when_empty && list_empty(&smi_infos)) {
		mutex_unlock(&smi_infos_lock);
		cleanup_ipmi_si();
		pr_warn("Unable to find any System Interface(s)\n");
		return -ENODEV;
	} else {
		mutex_unlock(&smi_infos_lock);
		return 0;
	}
}
module_init(init_ipmi_si);

static void wait_msg_processed(struct smi_info *smi_info)
{
	unsigned long jiffies_now;
	long time_diff;

	while (smi_info->curr_msg || (smi_info->si_state != SI_NORMAL)) {
		jiffies_now = jiffies;
		time_diff = (((long)jiffies_now - (long)smi_info->last_timeout_jiffies)
		     * SI_USEC_PER_JIFFY);
		smi_event_handler(smi_info, time_diff);
		schedule_timeout_uninterruptible(1);
	}
}

static void shutdown_smi(void *send_info)
{
	struct smi_info *smi_info = send_info;

	if (smi_info->dev_group_added) {
		device_remove_group(smi_info->io.dev, &ipmi_si_dev_attr_group);
		smi_info->dev_group_added = false;
	}
	if (smi_info->io.dev)
		dev_set_drvdata(smi_info->io.dev, NULL);

	 
	smi_info->interrupt_disabled = true;
	if (smi_info->io.irq_cleanup) {
		smi_info->io.irq_cleanup(&smi_info->io);
		smi_info->io.irq_cleanup = NULL;
	}
	stop_timer_and_thread(smi_info);

	 
	synchronize_rcu();

	 
	wait_msg_processed(smi_info);

	if (smi_info->handlers)
		disable_si_irq(smi_info);

	wait_msg_processed(smi_info);

	if (smi_info->handlers)
		smi_info->handlers->cleanup(smi_info->si_sm);

	if (smi_info->io.io_cleanup) {
		smi_info->io.io_cleanup(&smi_info->io);
		smi_info->io.io_cleanup = NULL;
	}

	kfree(smi_info->si_sm);
	smi_info->si_sm = NULL;

	smi_info->intf = NULL;
}

 
static void cleanup_one_si(struct smi_info *smi_info)
{
	if (!smi_info)
		return;

	list_del(&smi_info->link);
	ipmi_unregister_smi(smi_info->intf);
	kfree(smi_info);
}

void ipmi_si_remove_by_dev(struct device *dev)
{
	struct smi_info *e;

	mutex_lock(&smi_infos_lock);
	list_for_each_entry(e, &smi_infos, link) {
		if (e->io.dev == dev) {
			cleanup_one_si(e);
			break;
		}
	}
	mutex_unlock(&smi_infos_lock);
}

struct device *ipmi_si_remove_by_data(int addr_space, enum si_type si_type,
				      unsigned long addr)
{
	 
	struct smi_info *e, *tmp_e;
	struct device *dev = NULL;

	mutex_lock(&smi_infos_lock);
	list_for_each_entry_safe(e, tmp_e, &smi_infos, link) {
		if (e->io.addr_space != addr_space)
			continue;
		if (e->io.si_type != si_type)
			continue;
		if (e->io.addr_data == addr) {
			dev = get_device(e->io.dev);
			cleanup_one_si(e);
		}
	}
	mutex_unlock(&smi_infos_lock);

	return dev;
}

static void cleanup_ipmi_si(void)
{
	struct smi_info *e, *tmp_e;

	if (!initialized)
		return;

	ipmi_si_pci_shutdown();

	ipmi_si_parisc_shutdown();

	ipmi_si_platform_shutdown();

	mutex_lock(&smi_infos_lock);
	list_for_each_entry_safe(e, tmp_e, &smi_infos, link)
		cleanup_one_si(e);
	mutex_unlock(&smi_infos_lock);

	ipmi_si_hardcode_exit();
	ipmi_si_hotmod_exit();
}
module_exit(cleanup_ipmi_si);

MODULE_ALIAS("platform:dmi-ipmi-si");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Corey Minyard <minyard@mvista.com>");
MODULE_DESCRIPTION("Interface to the IPMI driver for the KCS, SMIC, and BT system interfaces.");
