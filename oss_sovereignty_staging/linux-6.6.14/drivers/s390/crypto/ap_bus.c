
 

#define KMSG_COMPONENT "ap"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel_stat.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <asm/airq.h>
#include <asm/tpi.h>
#include <linux/atomic.h>
#include <asm/isc.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <asm/facility.h>
#include <linux/crypto.h>
#include <linux/mod_devicetable.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/module.h>

#include "ap_bus.h"
#include "ap_debug.h"

 
int ap_domain_index = -1;	 
static DEFINE_SPINLOCK(ap_domain_lock);
module_param_named(domain, ap_domain_index, int, 0440);
MODULE_PARM_DESC(domain, "domain index for ap devices");
EXPORT_SYMBOL(ap_domain_index);

static int ap_thread_flag;
module_param_named(poll_thread, ap_thread_flag, int, 0440);
MODULE_PARM_DESC(poll_thread, "Turn on/off poll thread, default is 0 (off).");

static char *apm_str;
module_param_named(apmask, apm_str, charp, 0440);
MODULE_PARM_DESC(apmask, "AP bus adapter mask.");

static char *aqm_str;
module_param_named(aqmask, aqm_str, charp, 0440);
MODULE_PARM_DESC(aqmask, "AP bus domain mask.");

static int ap_useirq = 1;
module_param_named(useirq, ap_useirq, int, 0440);
MODULE_PARM_DESC(useirq, "Use interrupt if available, default is 1 (on).");

atomic_t ap_max_msg_size = ATOMIC_INIT(AP_DEFAULT_MAX_MSG_SIZE);
EXPORT_SYMBOL(ap_max_msg_size);

static struct device *ap_root_device;

 
DEFINE_HASHTABLE(ap_queues, 8);
 
DEFINE_SPINLOCK(ap_queues_lock);

 
struct ap_perms ap_perms;
EXPORT_SYMBOL(ap_perms);
DEFINE_MUTEX(ap_perms_mutex);
EXPORT_SYMBOL(ap_perms_mutex);

 
static atomic64_t ap_scan_bus_count;

 
static atomic64_t ap_bindings_complete_count = ATOMIC64_INIT(0);

 
static DECLARE_COMPLETION(ap_init_apqn_bindings_complete);

static struct ap_config_info *ap_qci_info;
static struct ap_config_info *ap_qci_info_old;

 
debug_info_t *ap_dbf_info;

 
static struct timer_list ap_config_timer;
static int ap_config_time = AP_CONFIG_TIME;
static void ap_scan_bus(struct work_struct *);
static DECLARE_WORK(ap_scan_work, ap_scan_bus);

 
static void ap_tasklet_fn(unsigned long);
static DECLARE_TASKLET_OLD(ap_tasklet, ap_tasklet_fn);
static DECLARE_WAIT_QUEUE_HEAD(ap_poll_wait);
static struct task_struct *ap_poll_kthread;
static DEFINE_MUTEX(ap_poll_thread_mutex);
static DEFINE_SPINLOCK(ap_poll_timer_lock);
static struct hrtimer ap_poll_timer;
 
static unsigned long poll_high_timeout = 250000UL;

 
static unsigned long poll_low_timeout = 40000000UL;

 
static int ap_max_domain_id = 15;
 
static int ap_max_adapter_id = 63;

static struct bus_type ap_bus_type;

 
static void ap_interrupt_handler(struct airq_struct *airq,
				 struct tpi_info *tpi_info);

static bool ap_irq_flag;

static struct airq_struct ap_airq = {
	.handler = ap_interrupt_handler,
	.isc = AP_ISC,
};

 
void *ap_airq_ptr(void)
{
	if (ap_irq_flag)
		return ap_airq.lsi_ptr;
	return NULL;
}

 
static int ap_interrupts_available(void)
{
	return test_facility(65);
}

 
static int ap_qci_available(void)
{
	return test_facility(12);
}

 
static int ap_apft_available(void)
{
	return test_facility(15);
}

 
static inline int ap_qact_available(void)
{
	if (ap_qci_info)
		return ap_qci_info->qact;
	return 0;
}

 
int ap_sb_available(void)
{
	if (ap_qci_info)
		return ap_qci_info->apsb;
	return 0;
}

 
bool ap_is_se_guest(void)
{
	return is_prot_virt_guest() && ap_sb_available();
}
EXPORT_SYMBOL(ap_is_se_guest);

 
static inline int ap_fetch_qci_info(struct ap_config_info *info)
{
	if (!ap_qci_available())
		return -EOPNOTSUPP;
	if (!info)
		return -EINVAL;
	return ap_qci(info);
}

 
static void __init ap_init_qci_info(void)
{
	if (!ap_qci_available()) {
		AP_DBF_INFO("%s QCI not supported\n", __func__);
		return;
	}

	ap_qci_info = kzalloc(sizeof(*ap_qci_info), GFP_KERNEL);
	if (!ap_qci_info)
		return;
	ap_qci_info_old = kzalloc(sizeof(*ap_qci_info_old), GFP_KERNEL);
	if (!ap_qci_info_old) {
		kfree(ap_qci_info);
		ap_qci_info = NULL;
		return;
	}
	if (ap_fetch_qci_info(ap_qci_info) != 0) {
		kfree(ap_qci_info);
		kfree(ap_qci_info_old);
		ap_qci_info = NULL;
		ap_qci_info_old = NULL;
		return;
	}
	AP_DBF_INFO("%s successful fetched initial qci info\n", __func__);

	if (ap_qci_info->apxa) {
		if (ap_qci_info->na) {
			ap_max_adapter_id = ap_qci_info->na;
			AP_DBF_INFO("%s new ap_max_adapter_id is %d\n",
				    __func__, ap_max_adapter_id);
		}
		if (ap_qci_info->nd) {
			ap_max_domain_id = ap_qci_info->nd;
			AP_DBF_INFO("%s new ap_max_domain_id is %d\n",
				    __func__, ap_max_domain_id);
		}
	}

	memcpy(ap_qci_info_old, ap_qci_info, sizeof(*ap_qci_info));
}

 
static inline int ap_test_config(unsigned int *field, unsigned int nr)
{
	return ap_test_bit((field + (nr >> 5)), (nr & 0x1f));
}

 
static inline int ap_test_config_card_id(unsigned int id)
{
	if (id > ap_max_adapter_id)
		return 0;
	if (ap_qci_info)
		return ap_test_config(ap_qci_info->apm, id);
	return 1;
}

 
int ap_test_config_usage_domain(unsigned int domain)
{
	if (domain > ap_max_domain_id)
		return 0;
	if (ap_qci_info)
		return ap_test_config(ap_qci_info->aqm, domain);
	return 1;
}
EXPORT_SYMBOL(ap_test_config_usage_domain);

 
int ap_test_config_ctrl_domain(unsigned int domain)
{
	if (!ap_qci_info || domain > ap_max_domain_id)
		return 0;
	return ap_test_config(ap_qci_info->adm, domain);
}
EXPORT_SYMBOL(ap_test_config_ctrl_domain);

 
static int ap_queue_info(ap_qid_t qid, int *q_type, unsigned int *q_fac,
			 int *q_depth, int *q_ml, bool *q_decfg, bool *q_cstop)
{
	struct ap_queue_status status;
	struct ap_tapq_gr2 tapq_info;

	tapq_info.value = 0;

	 
	if (AP_QID_CARD(qid) > ap_max_adapter_id ||
	    AP_QID_QUEUE(qid) > ap_max_domain_id)
		return -1;

	 
	status = ap_test_queue(qid, ap_apft_available(), &tapq_info);

	 
	if (status.async)
		return 0;

	switch (status.response_code) {
	case AP_RESPONSE_NORMAL:
	case AP_RESPONSE_RESET_IN_PROGRESS:
	case AP_RESPONSE_DECONFIGURED:
	case AP_RESPONSE_CHECKSTOPPED:
	case AP_RESPONSE_BUSY:
		 
		if (WARN_ON_ONCE(!tapq_info.value))
			return 0;
		*q_type = tapq_info.at;
		*q_fac = tapq_info.fac;
		*q_depth = tapq_info.qd;
		*q_ml = tapq_info.ml;
		*q_decfg = status.response_code == AP_RESPONSE_DECONFIGURED;
		*q_cstop = status.response_code == AP_RESPONSE_CHECKSTOPPED;
		return 1;
	default:
		 
		return -1;
	}
}

void ap_wait(enum ap_sm_wait wait)
{
	ktime_t hr_time;

	switch (wait) {
	case AP_SM_WAIT_AGAIN:
	case AP_SM_WAIT_INTERRUPT:
		if (ap_irq_flag)
			break;
		if (ap_poll_kthread) {
			wake_up(&ap_poll_wait);
			break;
		}
		fallthrough;
	case AP_SM_WAIT_LOW_TIMEOUT:
	case AP_SM_WAIT_HIGH_TIMEOUT:
		spin_lock_bh(&ap_poll_timer_lock);
		if (!hrtimer_is_queued(&ap_poll_timer)) {
			hr_time =
				wait == AP_SM_WAIT_LOW_TIMEOUT ?
				poll_low_timeout : poll_high_timeout;
			hrtimer_forward_now(&ap_poll_timer, hr_time);
			hrtimer_restart(&ap_poll_timer);
		}
		spin_unlock_bh(&ap_poll_timer_lock);
		break;
	case AP_SM_WAIT_NONE:
	default:
		break;
	}
}

 
void ap_request_timeout(struct timer_list *t)
{
	struct ap_queue *aq = from_timer(aq, t, timeout);

	spin_lock_bh(&aq->lock);
	ap_wait(ap_sm_event(aq, AP_SM_EVENT_TIMEOUT));
	spin_unlock_bh(&aq->lock);
}

 
static enum hrtimer_restart ap_poll_timeout(struct hrtimer *unused)
{
	tasklet_schedule(&ap_tasklet);
	return HRTIMER_NORESTART;
}

 
static void ap_interrupt_handler(struct airq_struct *airq,
				 struct tpi_info *tpi_info)
{
	inc_irq_stat(IRQIO_APB);
	tasklet_schedule(&ap_tasklet);
}

 
static void ap_tasklet_fn(unsigned long dummy)
{
	int bkt;
	struct ap_queue *aq;
	enum ap_sm_wait wait = AP_SM_WAIT_NONE;

	 
	if (ap_irq_flag)
		xchg(ap_airq.lsi_ptr, 0);

	spin_lock_bh(&ap_queues_lock);
	hash_for_each(ap_queues, bkt, aq, hnode) {
		spin_lock_bh(&aq->lock);
		wait = min(wait, ap_sm_event_loop(aq, AP_SM_EVENT_POLL));
		spin_unlock_bh(&aq->lock);
	}
	spin_unlock_bh(&ap_queues_lock);

	ap_wait(wait);
}

static int ap_pending_requests(void)
{
	int bkt;
	struct ap_queue *aq;

	spin_lock_bh(&ap_queues_lock);
	hash_for_each(ap_queues, bkt, aq, hnode) {
		if (aq->queue_count == 0)
			continue;
		spin_unlock_bh(&ap_queues_lock);
		return 1;
	}
	spin_unlock_bh(&ap_queues_lock);
	return 0;
}

 
static int ap_poll_thread(void *data)
{
	DECLARE_WAITQUEUE(wait, current);

	set_user_nice(current, MAX_NICE);
	set_freezable();
	while (!kthread_should_stop()) {
		add_wait_queue(&ap_poll_wait, &wait);
		set_current_state(TASK_INTERRUPTIBLE);
		if (!ap_pending_requests()) {
			schedule();
			try_to_freeze();
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&ap_poll_wait, &wait);
		if (need_resched()) {
			schedule();
			try_to_freeze();
			continue;
		}
		ap_tasklet_fn(0);
	}

	return 0;
}

static int ap_poll_thread_start(void)
{
	int rc;

	if (ap_irq_flag || ap_poll_kthread)
		return 0;
	mutex_lock(&ap_poll_thread_mutex);
	ap_poll_kthread = kthread_run(ap_poll_thread, NULL, "appoll");
	rc = PTR_ERR_OR_ZERO(ap_poll_kthread);
	if (rc)
		ap_poll_kthread = NULL;
	mutex_unlock(&ap_poll_thread_mutex);
	return rc;
}

static void ap_poll_thread_stop(void)
{
	if (!ap_poll_kthread)
		return;
	mutex_lock(&ap_poll_thread_mutex);
	kthread_stop(ap_poll_kthread);
	ap_poll_kthread = NULL;
	mutex_unlock(&ap_poll_thread_mutex);
}

#define is_card_dev(x) ((x)->parent == ap_root_device)
#define is_queue_dev(x) ((x)->parent != ap_root_device)

 
static int ap_bus_match(struct device *dev, struct device_driver *drv)
{
	struct ap_driver *ap_drv = to_ap_drv(drv);
	struct ap_device_id *id;

	 
	for (id = ap_drv->ids; id->match_flags; id++) {
		if (is_card_dev(dev) &&
		    id->match_flags & AP_DEVICE_ID_MATCH_CARD_TYPE &&
		    id->dev_type == to_ap_dev(dev)->device_type)
			return 1;
		if (is_queue_dev(dev) &&
		    id->match_flags & AP_DEVICE_ID_MATCH_QUEUE_TYPE &&
		    id->dev_type == to_ap_dev(dev)->device_type)
			return 1;
	}
	return 0;
}

 
static int ap_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	int rc = 0;
	const struct ap_device *ap_dev = to_ap_dev(dev);

	 
	if (dev == ap_root_device)
		return 0;

	if (is_card_dev(dev)) {
		struct ap_card *ac = to_ap_card(&ap_dev->device);

		 
		rc = add_uevent_var(env, "DEV_TYPE=%04X", ap_dev->device_type);
		if (rc)
			return rc;
		 
		rc = add_uevent_var(env, "MODALIAS=ap:t%02X", ap_dev->device_type);
		if (rc)
			return rc;

		 
		if (ap_test_bit(&ac->functions, AP_FUNC_ACCEL))
			rc = add_uevent_var(env, "MODE=accel");
		else if (ap_test_bit(&ac->functions, AP_FUNC_COPRO))
			rc = add_uevent_var(env, "MODE=cca");
		else if (ap_test_bit(&ac->functions, AP_FUNC_EP11))
			rc = add_uevent_var(env, "MODE=ep11");
		if (rc)
			return rc;
	} else {
		struct ap_queue *aq = to_ap_queue(&ap_dev->device);

		 
		if (ap_test_bit(&aq->card->functions, AP_FUNC_ACCEL))
			rc = add_uevent_var(env, "MODE=accel");
		else if (ap_test_bit(&aq->card->functions, AP_FUNC_COPRO))
			rc = add_uevent_var(env, "MODE=cca");
		else if (ap_test_bit(&aq->card->functions, AP_FUNC_EP11))
			rc = add_uevent_var(env, "MODE=ep11");
		if (rc)
			return rc;
	}

	return 0;
}

static void ap_send_init_scan_done_uevent(void)
{
	char *envp[] = { "INITSCAN=done", NULL };

	kobject_uevent_env(&ap_root_device->kobj, KOBJ_CHANGE, envp);
}

static void ap_send_bindings_complete_uevent(void)
{
	char buf[32];
	char *envp[] = { "BINDINGS=complete", buf, NULL };

	snprintf(buf, sizeof(buf), "COMPLETECOUNT=%llu",
		 atomic64_inc_return(&ap_bindings_complete_count));
	kobject_uevent_env(&ap_root_device->kobj, KOBJ_CHANGE, envp);
}

void ap_send_config_uevent(struct ap_device *ap_dev, bool cfg)
{
	char buf[16];
	char *envp[] = { buf, NULL };

	snprintf(buf, sizeof(buf), "CONFIG=%d", cfg ? 1 : 0);

	kobject_uevent_env(&ap_dev->device.kobj, KOBJ_CHANGE, envp);
}
EXPORT_SYMBOL(ap_send_config_uevent);

void ap_send_online_uevent(struct ap_device *ap_dev, int online)
{
	char buf[16];
	char *envp[] = { buf, NULL };

	snprintf(buf, sizeof(buf), "ONLINE=%d", online ? 1 : 0);

	kobject_uevent_env(&ap_dev->device.kobj, KOBJ_CHANGE, envp);
}
EXPORT_SYMBOL(ap_send_online_uevent);

static void ap_send_mask_changed_uevent(unsigned long *newapm,
					unsigned long *newaqm)
{
	char buf[100];
	char *envp[] = { buf, NULL };

	if (newapm)
		snprintf(buf, sizeof(buf),
			 "APMASK=0x%016lx%016lx%016lx%016lx\n",
			 newapm[0], newapm[1], newapm[2], newapm[3]);
	else
		snprintf(buf, sizeof(buf),
			 "AQMASK=0x%016lx%016lx%016lx%016lx\n",
			 newaqm[0], newaqm[1], newaqm[2], newaqm[3]);

	kobject_uevent_env(&ap_root_device->kobj, KOBJ_CHANGE, envp);
}

 

struct __ap_calc_ctrs {
	unsigned int apqns;
	unsigned int bound;
};

static int __ap_calc_helper(struct device *dev, void *arg)
{
	struct __ap_calc_ctrs *pctrs = (struct __ap_calc_ctrs *)arg;

	if (is_queue_dev(dev)) {
		pctrs->apqns++;
		if (dev->driver)
			pctrs->bound++;
	}

	return 0;
}

static void ap_calc_bound_apqns(unsigned int *apqns, unsigned int *bound)
{
	struct __ap_calc_ctrs ctrs;

	memset(&ctrs, 0, sizeof(ctrs));
	bus_for_each_dev(&ap_bus_type, NULL, (void *)&ctrs, __ap_calc_helper);

	*apqns = ctrs.apqns;
	*bound = ctrs.bound;
}

 
static void ap_check_bindings_complete(void)
{
	unsigned int apqns, bound;

	if (atomic64_read(&ap_scan_bus_count) >= 1) {
		ap_calc_bound_apqns(&apqns, &bound);
		if (bound == apqns) {
			if (!completion_done(&ap_init_apqn_bindings_complete)) {
				complete_all(&ap_init_apqn_bindings_complete);
				AP_DBF_INFO("%s complete\n", __func__);
			}
			ap_send_bindings_complete_uevent();
		}
	}
}

 
int ap_wait_init_apqn_bindings_complete(unsigned long timeout)
{
	long l;

	if (completion_done(&ap_init_apqn_bindings_complete))
		return 0;

	if (timeout)
		l = wait_for_completion_interruptible_timeout(
			&ap_init_apqn_bindings_complete, timeout);
	else
		l = wait_for_completion_interruptible(
			&ap_init_apqn_bindings_complete);
	if (l < 0)
		return l == -ERESTARTSYS ? -EINTR : l;
	else if (l == 0 && timeout)
		return -ETIME;

	return 0;
}
EXPORT_SYMBOL(ap_wait_init_apqn_bindings_complete);

static int __ap_queue_devices_with_id_unregister(struct device *dev, void *data)
{
	if (is_queue_dev(dev) &&
	    AP_QID_CARD(to_ap_queue(dev)->qid) == (int)(long)data)
		device_unregister(dev);
	return 0;
}

static int __ap_revise_reserved(struct device *dev, void *dummy)
{
	int rc, card, queue, devres, drvres;

	if (is_queue_dev(dev)) {
		card = AP_QID_CARD(to_ap_queue(dev)->qid);
		queue = AP_QID_QUEUE(to_ap_queue(dev)->qid);
		mutex_lock(&ap_perms_mutex);
		devres = test_bit_inv(card, ap_perms.apm) &&
			test_bit_inv(queue, ap_perms.aqm);
		mutex_unlock(&ap_perms_mutex);
		drvres = to_ap_drv(dev->driver)->flags
			& AP_DRIVER_FLAG_DEFAULT;
		if (!!devres != !!drvres) {
			AP_DBF_DBG("%s reprobing queue=%02x.%04x\n",
				   __func__, card, queue);
			rc = device_reprobe(dev);
			if (rc)
				AP_DBF_WARN("%s reprobing queue=%02x.%04x failed\n",
					    __func__, card, queue);
		}
	}

	return 0;
}

static void ap_bus_revise_bindings(void)
{
	bus_for_each_dev(&ap_bus_type, NULL, NULL, __ap_revise_reserved);
}

 
int ap_owned_by_def_drv(int card, int queue)
{
	int rc = 0;

	if (card < 0 || card >= AP_DEVICES || queue < 0 || queue >= AP_DOMAINS)
		return -EINVAL;

	if (test_bit_inv(card, ap_perms.apm) &&
	    test_bit_inv(queue, ap_perms.aqm))
		rc = 1;

	return rc;
}
EXPORT_SYMBOL(ap_owned_by_def_drv);

 
int ap_apqn_in_matrix_owned_by_def_drv(unsigned long *apm,
				       unsigned long *aqm)
{
	int card, queue, rc = 0;

	for (card = 0; !rc && card < AP_DEVICES; card++)
		if (test_bit_inv(card, apm) &&
		    test_bit_inv(card, ap_perms.apm))
			for (queue = 0; !rc && queue < AP_DOMAINS; queue++)
				if (test_bit_inv(queue, aqm) &&
				    test_bit_inv(queue, ap_perms.aqm))
					rc = 1;

	return rc;
}
EXPORT_SYMBOL(ap_apqn_in_matrix_owned_by_def_drv);

static int ap_device_probe(struct device *dev)
{
	struct ap_device *ap_dev = to_ap_dev(dev);
	struct ap_driver *ap_drv = to_ap_drv(dev->driver);
	int card, queue, devres, drvres, rc = -ENODEV;

	if (!get_device(dev))
		return rc;

	if (is_queue_dev(dev)) {
		 
		card = AP_QID_CARD(to_ap_queue(dev)->qid);
		queue = AP_QID_QUEUE(to_ap_queue(dev)->qid);
		mutex_lock(&ap_perms_mutex);
		devres = test_bit_inv(card, ap_perms.apm) &&
			test_bit_inv(queue, ap_perms.aqm);
		mutex_unlock(&ap_perms_mutex);
		drvres = ap_drv->flags & AP_DRIVER_FLAG_DEFAULT;
		if (!!devres != !!drvres)
			goto out;
	}

	 
	spin_lock_bh(&ap_queues_lock);
	if (is_queue_dev(dev))
		hash_add(ap_queues, &to_ap_queue(dev)->hnode,
			 to_ap_queue(dev)->qid);
	spin_unlock_bh(&ap_queues_lock);

	rc = ap_drv->probe ? ap_drv->probe(ap_dev) : -ENODEV;

	if (rc) {
		spin_lock_bh(&ap_queues_lock);
		if (is_queue_dev(dev))
			hash_del(&to_ap_queue(dev)->hnode);
		spin_unlock_bh(&ap_queues_lock);
	} else {
		ap_check_bindings_complete();
	}

out:
	if (rc)
		put_device(dev);
	return rc;
}

static void ap_device_remove(struct device *dev)
{
	struct ap_device *ap_dev = to_ap_dev(dev);
	struct ap_driver *ap_drv = to_ap_drv(dev->driver);

	 
	if (is_queue_dev(dev))
		ap_queue_prepare_remove(to_ap_queue(dev));

	 
	if (ap_drv->remove)
		ap_drv->remove(ap_dev);

	 
	if (is_queue_dev(dev))
		ap_queue_remove(to_ap_queue(dev));

	 
	spin_lock_bh(&ap_queues_lock);
	if (is_queue_dev(dev))
		hash_del(&to_ap_queue(dev)->hnode);
	spin_unlock_bh(&ap_queues_lock);

	put_device(dev);
}

struct ap_queue *ap_get_qdev(ap_qid_t qid)
{
	int bkt;
	struct ap_queue *aq;

	spin_lock_bh(&ap_queues_lock);
	hash_for_each(ap_queues, bkt, aq, hnode) {
		if (aq->qid == qid) {
			get_device(&aq->ap_dev.device);
			spin_unlock_bh(&ap_queues_lock);
			return aq;
		}
	}
	spin_unlock_bh(&ap_queues_lock);

	return NULL;
}
EXPORT_SYMBOL(ap_get_qdev);

int ap_driver_register(struct ap_driver *ap_drv, struct module *owner,
		       char *name)
{
	struct device_driver *drv = &ap_drv->driver;

	drv->bus = &ap_bus_type;
	drv->owner = owner;
	drv->name = name;
	return driver_register(drv);
}
EXPORT_SYMBOL(ap_driver_register);

void ap_driver_unregister(struct ap_driver *ap_drv)
{
	driver_unregister(&ap_drv->driver);
}
EXPORT_SYMBOL(ap_driver_unregister);

void ap_bus_force_rescan(void)
{
	 
	if (atomic64_read(&ap_scan_bus_count) <= 0)
		return;

	 
	del_timer(&ap_config_timer);
	queue_work(system_long_wq, &ap_scan_work);
	flush_work(&ap_scan_work);
}
EXPORT_SYMBOL(ap_bus_force_rescan);

 
void ap_bus_cfg_chg(void)
{
	AP_DBF_DBG("%s config change, forcing bus rescan\n", __func__);

	ap_bus_force_rescan();
}

 
static int hex2bitmap(const char *str, unsigned long *bitmap, int bits)
{
	int i, n, b;

	 
	if (bits & 0x07)
		return -EINVAL;

	if (str[0] == '0' && str[1] == 'x')
		str++;
	if (*str == 'x')
		str++;

	for (i = 0; isxdigit(*str) && i < bits; str++) {
		b = hex_to_bin(*str);
		for (n = 0; n < 4; n++)
			if (b & (0x08 >> n))
				set_bit_inv(i + n, bitmap);
		i += 4;
	}

	if (*str == '\n')
		str++;
	if (*str)
		return -EINVAL;
	return 0;
}

 
static int modify_bitmap(const char *str, unsigned long *bitmap, int bits)
{
	int a, i, z;
	char *np, sign;

	 
	if (bits & 0x07)
		return -EINVAL;

	while (*str) {
		sign = *str++;
		if (sign != '+' && sign != '-')
			return -EINVAL;
		a = z = simple_strtoul(str, &np, 0);
		if (str == np || a >= bits)
			return -EINVAL;
		str = np;
		if (*str == '-') {
			z = simple_strtoul(++str, &np, 0);
			if (str == np || a > z || z >= bits)
				return -EINVAL;
			str = np;
		}
		for (i = a; i <= z; i++)
			if (sign == '+')
				set_bit_inv(i, bitmap);
			else
				clear_bit_inv(i, bitmap);
		while (*str == ',' || *str == '\n')
			str++;
	}

	return 0;
}

static int ap_parse_bitmap_str(const char *str, unsigned long *bitmap, int bits,
			       unsigned long *newmap)
{
	unsigned long size;
	int rc;

	size = BITS_TO_LONGS(bits) * sizeof(unsigned long);
	if (*str == '+' || *str == '-') {
		memcpy(newmap, bitmap, size);
		rc = modify_bitmap(str, newmap, bits);
	} else {
		memset(newmap, 0, size);
		rc = hex2bitmap(str, newmap, bits);
	}
	return rc;
}

int ap_parse_mask_str(const char *str,
		      unsigned long *bitmap, int bits,
		      struct mutex *lock)
{
	unsigned long *newmap, size;
	int rc;

	 
	if (bits & 0x07)
		return -EINVAL;

	size = BITS_TO_LONGS(bits) * sizeof(unsigned long);
	newmap = kmalloc(size, GFP_KERNEL);
	if (!newmap)
		return -ENOMEM;
	if (mutex_lock_interruptible(lock)) {
		kfree(newmap);
		return -ERESTARTSYS;
	}
	rc = ap_parse_bitmap_str(str, bitmap, bits, newmap);
	if (rc == 0)
		memcpy(bitmap, newmap, size);
	mutex_unlock(lock);
	kfree(newmap);
	return rc;
}
EXPORT_SYMBOL(ap_parse_mask_str);

 

static ssize_t ap_domain_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%d\n", ap_domain_index);
}

static ssize_t ap_domain_store(const struct bus_type *bus,
			       const char *buf, size_t count)
{
	int domain;

	if (sscanf(buf, "%i\n", &domain) != 1 ||
	    domain < 0 || domain > ap_max_domain_id ||
	    !test_bit_inv(domain, ap_perms.aqm))
		return -EINVAL;

	spin_lock_bh(&ap_domain_lock);
	ap_domain_index = domain;
	spin_unlock_bh(&ap_domain_lock);

	AP_DBF_INFO("%s stored new default domain=%d\n",
		    __func__, domain);

	return count;
}

static BUS_ATTR_RW(ap_domain);

static ssize_t ap_control_domain_mask_show(const struct bus_type *bus, char *buf)
{
	if (!ap_qci_info)	 
		return sysfs_emit(buf, "not supported\n");

	return sysfs_emit(buf, "0x%08x%08x%08x%08x%08x%08x%08x%08x\n",
			  ap_qci_info->adm[0], ap_qci_info->adm[1],
			  ap_qci_info->adm[2], ap_qci_info->adm[3],
			  ap_qci_info->adm[4], ap_qci_info->adm[5],
			  ap_qci_info->adm[6], ap_qci_info->adm[7]);
}

static BUS_ATTR_RO(ap_control_domain_mask);

static ssize_t ap_usage_domain_mask_show(const struct bus_type *bus, char *buf)
{
	if (!ap_qci_info)	 
		return sysfs_emit(buf, "not supported\n");

	return sysfs_emit(buf, "0x%08x%08x%08x%08x%08x%08x%08x%08x\n",
			  ap_qci_info->aqm[0], ap_qci_info->aqm[1],
			  ap_qci_info->aqm[2], ap_qci_info->aqm[3],
			  ap_qci_info->aqm[4], ap_qci_info->aqm[5],
			  ap_qci_info->aqm[6], ap_qci_info->aqm[7]);
}

static BUS_ATTR_RO(ap_usage_domain_mask);

static ssize_t ap_adapter_mask_show(const struct bus_type *bus, char *buf)
{
	if (!ap_qci_info)	 
		return sysfs_emit(buf, "not supported\n");

	return sysfs_emit(buf, "0x%08x%08x%08x%08x%08x%08x%08x%08x\n",
			  ap_qci_info->apm[0], ap_qci_info->apm[1],
			  ap_qci_info->apm[2], ap_qci_info->apm[3],
			  ap_qci_info->apm[4], ap_qci_info->apm[5],
			  ap_qci_info->apm[6], ap_qci_info->apm[7]);
}

static BUS_ATTR_RO(ap_adapter_mask);

static ssize_t ap_interrupts_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%d\n", ap_irq_flag ? 1 : 0);
}

static BUS_ATTR_RO(ap_interrupts);

static ssize_t config_time_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%d\n", ap_config_time);
}

static ssize_t config_time_store(const struct bus_type *bus,
				 const char *buf, size_t count)
{
	int time;

	if (sscanf(buf, "%d\n", &time) != 1 || time < 5 || time > 120)
		return -EINVAL;
	ap_config_time = time;
	mod_timer(&ap_config_timer, jiffies + ap_config_time * HZ);
	return count;
}

static BUS_ATTR_RW(config_time);

static ssize_t poll_thread_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%d\n", ap_poll_kthread ? 1 : 0);
}

static ssize_t poll_thread_store(const struct bus_type *bus,
				 const char *buf, size_t count)
{
	bool value;
	int rc;

	rc = kstrtobool(buf, &value);
	if (rc)
		return rc;

	if (value) {
		rc = ap_poll_thread_start();
		if (rc)
			count = rc;
	} else {
		ap_poll_thread_stop();
	}
	return count;
}

static BUS_ATTR_RW(poll_thread);

static ssize_t poll_timeout_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%lu\n", poll_high_timeout);
}

static ssize_t poll_timeout_store(const struct bus_type *bus, const char *buf,
				  size_t count)
{
	unsigned long value;
	ktime_t hr_time;
	int rc;

	rc = kstrtoul(buf, 0, &value);
	if (rc)
		return rc;

	 
	if (value > 120000000000UL)
		return -EINVAL;
	poll_high_timeout = value;
	hr_time = poll_high_timeout;

	spin_lock_bh(&ap_poll_timer_lock);
	hrtimer_cancel(&ap_poll_timer);
	hrtimer_set_expires(&ap_poll_timer, hr_time);
	hrtimer_start_expires(&ap_poll_timer, HRTIMER_MODE_ABS);
	spin_unlock_bh(&ap_poll_timer_lock);

	return count;
}

static BUS_ATTR_RW(poll_timeout);

static ssize_t ap_max_domain_id_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%d\n", ap_max_domain_id);
}

static BUS_ATTR_RO(ap_max_domain_id);

static ssize_t ap_max_adapter_id_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%d\n", ap_max_adapter_id);
}

static BUS_ATTR_RO(ap_max_adapter_id);

static ssize_t apmask_show(const struct bus_type *bus, char *buf)
{
	int rc;

	if (mutex_lock_interruptible(&ap_perms_mutex))
		return -ERESTARTSYS;
	rc = sysfs_emit(buf, "0x%016lx%016lx%016lx%016lx\n",
			ap_perms.apm[0], ap_perms.apm[1],
			ap_perms.apm[2], ap_perms.apm[3]);
	mutex_unlock(&ap_perms_mutex);

	return rc;
}

static int __verify_card_reservations(struct device_driver *drv, void *data)
{
	int rc = 0;
	struct ap_driver *ap_drv = to_ap_drv(drv);
	unsigned long *newapm = (unsigned long *)data;

	 
	if (!try_module_get(drv->owner))
		return 0;

	if (ap_drv->in_use) {
		rc = ap_drv->in_use(newapm, ap_perms.aqm);
		if (rc)
			rc = -EBUSY;
	}

	 
	module_put(drv->owner);

	return rc;
}

static int apmask_commit(unsigned long *newapm)
{
	int rc;
	unsigned long reserved[BITS_TO_LONGS(AP_DEVICES)];

	 
	if (bitmap_andnot(reserved, newapm, ap_perms.apm, AP_DEVICES)) {
		rc = bus_for_each_drv(&ap_bus_type, NULL, reserved,
				      __verify_card_reservations);
		if (rc)
			return rc;
	}

	memcpy(ap_perms.apm, newapm, APMASKSIZE);

	return 0;
}

static ssize_t apmask_store(const struct bus_type *bus, const char *buf,
			    size_t count)
{
	int rc, changes = 0;
	DECLARE_BITMAP(newapm, AP_DEVICES);

	if (mutex_lock_interruptible(&ap_perms_mutex))
		return -ERESTARTSYS;

	rc = ap_parse_bitmap_str(buf, ap_perms.apm, AP_DEVICES, newapm);
	if (rc)
		goto done;

	changes = memcmp(ap_perms.apm, newapm, APMASKSIZE);
	if (changes)
		rc = apmask_commit(newapm);

done:
	mutex_unlock(&ap_perms_mutex);
	if (rc)
		return rc;

	if (changes) {
		ap_bus_revise_bindings();
		ap_send_mask_changed_uevent(newapm, NULL);
	}

	return count;
}

static BUS_ATTR_RW(apmask);

static ssize_t aqmask_show(const struct bus_type *bus, char *buf)
{
	int rc;

	if (mutex_lock_interruptible(&ap_perms_mutex))
		return -ERESTARTSYS;
	rc = sysfs_emit(buf, "0x%016lx%016lx%016lx%016lx\n",
			ap_perms.aqm[0], ap_perms.aqm[1],
			ap_perms.aqm[2], ap_perms.aqm[3]);
	mutex_unlock(&ap_perms_mutex);

	return rc;
}

static int __verify_queue_reservations(struct device_driver *drv, void *data)
{
	int rc = 0;
	struct ap_driver *ap_drv = to_ap_drv(drv);
	unsigned long *newaqm = (unsigned long *)data;

	 
	if (!try_module_get(drv->owner))
		return 0;

	if (ap_drv->in_use) {
		rc = ap_drv->in_use(ap_perms.apm, newaqm);
		if (rc)
			rc = -EBUSY;
	}

	 
	module_put(drv->owner);

	return rc;
}

static int aqmask_commit(unsigned long *newaqm)
{
	int rc;
	unsigned long reserved[BITS_TO_LONGS(AP_DOMAINS)];

	 
	if (bitmap_andnot(reserved, newaqm, ap_perms.aqm, AP_DOMAINS)) {
		rc = bus_for_each_drv(&ap_bus_type, NULL, reserved,
				      __verify_queue_reservations);
		if (rc)
			return rc;
	}

	memcpy(ap_perms.aqm, newaqm, AQMASKSIZE);

	return 0;
}

static ssize_t aqmask_store(const struct bus_type *bus, const char *buf,
			    size_t count)
{
	int rc, changes = 0;
	DECLARE_BITMAP(newaqm, AP_DOMAINS);

	if (mutex_lock_interruptible(&ap_perms_mutex))
		return -ERESTARTSYS;

	rc = ap_parse_bitmap_str(buf, ap_perms.aqm, AP_DOMAINS, newaqm);
	if (rc)
		goto done;

	changes = memcmp(ap_perms.aqm, newaqm, APMASKSIZE);
	if (changes)
		rc = aqmask_commit(newaqm);

done:
	mutex_unlock(&ap_perms_mutex);
	if (rc)
		return rc;

	if (changes) {
		ap_bus_revise_bindings();
		ap_send_mask_changed_uevent(NULL, newaqm);
	}

	return count;
}

static BUS_ATTR_RW(aqmask);

static ssize_t scans_show(const struct bus_type *bus, char *buf)
{
	return sysfs_emit(buf, "%llu\n", atomic64_read(&ap_scan_bus_count));
}

static ssize_t scans_store(const struct bus_type *bus, const char *buf,
			   size_t count)
{
	AP_DBF_INFO("%s force AP bus rescan\n", __func__);

	ap_bus_force_rescan();

	return count;
}

static BUS_ATTR_RW(scans);

static ssize_t bindings_show(const struct bus_type *bus, char *buf)
{
	int rc;
	unsigned int apqns, n;

	ap_calc_bound_apqns(&apqns, &n);
	if (atomic64_read(&ap_scan_bus_count) >= 1 && n == apqns)
		rc = sysfs_emit(buf, "%u/%u (complete)\n", n, apqns);
	else
		rc = sysfs_emit(buf, "%u/%u\n", n, apqns);

	return rc;
}

static BUS_ATTR_RO(bindings);

static ssize_t features_show(const struct bus_type *bus, char *buf)
{
	int n = 0;

	if (!ap_qci_info)	 
		return sysfs_emit(buf, "-\n");

	if (ap_qci_info->apsc)
		n += sysfs_emit_at(buf, n, "APSC ");
	if (ap_qci_info->apxa)
		n += sysfs_emit_at(buf, n, "APXA ");
	if (ap_qci_info->qact)
		n += sysfs_emit_at(buf, n, "QACT ");
	if (ap_qci_info->rc8a)
		n += sysfs_emit_at(buf, n, "RC8A ");
	if (ap_qci_info->apsb)
		n += sysfs_emit_at(buf, n, "APSB ");

	sysfs_emit_at(buf, n == 0 ? 0 : n - 1, "\n");

	return n;
}

static BUS_ATTR_RO(features);

static struct attribute *ap_bus_attrs[] = {
	&bus_attr_ap_domain.attr,
	&bus_attr_ap_control_domain_mask.attr,
	&bus_attr_ap_usage_domain_mask.attr,
	&bus_attr_ap_adapter_mask.attr,
	&bus_attr_config_time.attr,
	&bus_attr_poll_thread.attr,
	&bus_attr_ap_interrupts.attr,
	&bus_attr_poll_timeout.attr,
	&bus_attr_ap_max_domain_id.attr,
	&bus_attr_ap_max_adapter_id.attr,
	&bus_attr_apmask.attr,
	&bus_attr_aqmask.attr,
	&bus_attr_scans.attr,
	&bus_attr_bindings.attr,
	&bus_attr_features.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ap_bus);

static struct bus_type ap_bus_type = {
	.name = "ap",
	.bus_groups = ap_bus_groups,
	.match = &ap_bus_match,
	.uevent = &ap_uevent,
	.probe = ap_device_probe,
	.remove = ap_device_remove,
};

 
static void ap_select_domain(void)
{
	struct ap_queue_status status;
	int card, dom;

	 
	spin_lock_bh(&ap_domain_lock);
	if (ap_domain_index >= 0) {
		 
		goto out;
	}
	for (dom = 0; dom <= ap_max_domain_id; dom++) {
		if (!ap_test_config_usage_domain(dom) ||
		    !test_bit_inv(dom, ap_perms.aqm))
			continue;
		for (card = 0; card <= ap_max_adapter_id; card++) {
			if (!ap_test_config_card_id(card) ||
			    !test_bit_inv(card, ap_perms.apm))
				continue;
			status = ap_test_queue(AP_MKQID(card, dom),
					       ap_apft_available(),
					       NULL);
			if (status.response_code == AP_RESPONSE_NORMAL)
				break;
		}
		if (card <= ap_max_adapter_id)
			break;
	}
	if (dom <= ap_max_domain_id) {
		ap_domain_index = dom;
		AP_DBF_INFO("%s new default domain is %d\n",
			    __func__, ap_domain_index);
	}
out:
	spin_unlock_bh(&ap_domain_lock);
}

 
static int ap_get_compatible_type(ap_qid_t qid, int rawtype, unsigned int func)
{
	int comp_type = 0;

	 
	if (rawtype < AP_DEVICE_TYPE_CEX4) {
		AP_DBF_WARN("%s queue=%02x.%04x unsupported type %d\n",
			    __func__, AP_QID_CARD(qid),
			    AP_QID_QUEUE(qid), rawtype);
		return 0;
	}
	 
	if (rawtype <= AP_DEVICE_TYPE_CEX8)
		return rawtype;
	 
	if (ap_qact_available()) {
		struct ap_queue_status status;
		union ap_qact_ap_info apinfo = {0};

		apinfo.mode = (func >> 26) & 0x07;
		apinfo.cat = AP_DEVICE_TYPE_CEX8;
		status = ap_qact(qid, 0, &apinfo);
		if (status.response_code == AP_RESPONSE_NORMAL &&
		    apinfo.cat >= AP_DEVICE_TYPE_CEX4 &&
		    apinfo.cat <= AP_DEVICE_TYPE_CEX8)
			comp_type = apinfo.cat;
	}
	if (!comp_type)
		AP_DBF_WARN("%s queue=%02x.%04x unable to map type %d\n",
			    __func__, AP_QID_CARD(qid),
			    AP_QID_QUEUE(qid), rawtype);
	else if (comp_type != rawtype)
		AP_DBF_INFO("%s queue=%02x.%04x map type %d to %d\n",
			    __func__, AP_QID_CARD(qid), AP_QID_QUEUE(qid),
			    rawtype, comp_type);
	return comp_type;
}

 
static int __match_card_device_with_id(struct device *dev, const void *data)
{
	return is_card_dev(dev) && to_ap_card(dev)->id == (int)(long)(void *)data;
}

 
static int __match_queue_device_with_qid(struct device *dev, const void *data)
{
	return is_queue_dev(dev) && to_ap_queue(dev)->qid == (int)(long)data;
}

 
static int __match_queue_device_with_queue_id(struct device *dev, const void *data)
{
	return is_queue_dev(dev) &&
		AP_QID_QUEUE(to_ap_queue(dev)->qid) == (int)(long)data;
}

 
static int __drv_notify_config_changed(struct device_driver *drv, void *data)
{
	struct ap_driver *ap_drv = to_ap_drv(drv);

	if (try_module_get(drv->owner)) {
		if (ap_drv->on_config_changed)
			ap_drv->on_config_changed(ap_qci_info, ap_qci_info_old);
		module_put(drv->owner);
	}

	return 0;
}

 
static inline void notify_config_changed(void)
{
	bus_for_each_drv(&ap_bus_type, NULL, NULL,
			 __drv_notify_config_changed);
}

 
static int __drv_notify_scan_complete(struct device_driver *drv, void *data)
{
	struct ap_driver *ap_drv = to_ap_drv(drv);

	if (try_module_get(drv->owner)) {
		if (ap_drv->on_scan_complete)
			ap_drv->on_scan_complete(ap_qci_info,
						 ap_qci_info_old);
		module_put(drv->owner);
	}

	return 0;
}

 
static inline void notify_scan_complete(void)
{
	bus_for_each_drv(&ap_bus_type, NULL, NULL,
			 __drv_notify_scan_complete);
}

 
static inline void ap_scan_rm_card_dev_and_queue_devs(struct ap_card *ac)
{
	bus_for_each_dev(&ap_bus_type, NULL,
			 (void *)(long)ac->id,
			 __ap_queue_devices_with_id_unregister);
	device_unregister(&ac->ap_dev.device);
}

 
static inline void ap_scan_domains(struct ap_card *ac)
{
	int rc, dom, depth, type, ml;
	bool decfg, chkstop;
	struct ap_queue *aq;
	struct device *dev;
	unsigned int func;
	ap_qid_t qid;

	 

	for (dom = 0; dom <= ap_max_domain_id; dom++) {
		qid = AP_MKQID(ac->id, dom);
		dev = bus_find_device(&ap_bus_type, NULL,
				      (void *)(long)qid,
				      __match_queue_device_with_qid);
		aq = dev ? to_ap_queue(dev) : NULL;
		if (!ap_test_config_usage_domain(dom)) {
			if (dev) {
				AP_DBF_INFO("%s(%d,%d) not in config anymore, rm queue dev\n",
					    __func__, ac->id, dom);
				device_unregister(dev);
			}
			goto put_dev_and_continue;
		}
		 
		rc = ap_queue_info(qid, &type, &func, &depth,
				   &ml, &decfg, &chkstop);
		switch (rc) {
		case -1:
			if (dev) {
				AP_DBF_INFO("%s(%d,%d) queue_info() failed, rm queue dev\n",
					    __func__, ac->id, dom);
				device_unregister(dev);
			}
			fallthrough;
		case 0:
			goto put_dev_and_continue;
		default:
			break;
		}
		 
		if (!aq) {
			aq = ap_queue_create(qid, ac->ap_dev.device_type);
			if (!aq) {
				AP_DBF_WARN("%s(%d,%d) ap_queue_create() failed\n",
					    __func__, ac->id, dom);
				continue;
			}
			aq->card = ac;
			aq->config = !decfg;
			aq->chkstop = chkstop;
			dev = &aq->ap_dev.device;
			dev->bus = &ap_bus_type;
			dev->parent = &ac->ap_dev.device;
			dev_set_name(dev, "%02x.%04x", ac->id, dom);
			 
			rc = device_register(dev);
			if (rc) {
				AP_DBF_WARN("%s(%d,%d) device_register() failed\n",
					    __func__, ac->id, dom);
				goto put_dev_and_continue;
			}
			 
			get_device(dev);
			if (decfg) {
				AP_DBF_INFO("%s(%d,%d) new (decfg) queue dev created\n",
					    __func__, ac->id, dom);
			} else if (chkstop) {
				AP_DBF_INFO("%s(%d,%d) new (chkstop) queue dev created\n",
					    __func__, ac->id, dom);
			} else {
				 
				ap_queue_init_state(aq);
				AP_DBF_INFO("%s(%d,%d) new queue dev created\n",
					    __func__, ac->id, dom);
			}
			goto put_dev_and_continue;
		}
		 
		spin_lock_bh(&aq->lock);
		 
		if (chkstop && !aq->chkstop) {
			 
			aq->chkstop = true;
			if (aq->dev_state > AP_DEV_STATE_UNINITIATED) {
				aq->dev_state = AP_DEV_STATE_ERROR;
				aq->last_err_rc = AP_RESPONSE_CHECKSTOPPED;
			}
			spin_unlock_bh(&aq->lock);
			AP_DBF_DBG("%s(%d,%d) queue dev checkstop on\n",
				   __func__, ac->id, dom);
			 
			ap_flush_queue(aq);
			goto put_dev_and_continue;
		} else if (!chkstop && aq->chkstop) {
			 
			aq->chkstop = false;
			if (aq->dev_state > AP_DEV_STATE_UNINITIATED)
				_ap_queue_init_state(aq);
			spin_unlock_bh(&aq->lock);
			AP_DBF_DBG("%s(%d,%d) queue dev checkstop off\n",
				   __func__, ac->id, dom);
			goto put_dev_and_continue;
		}
		 
		if (decfg && aq->config) {
			 
			aq->config = false;
			if (aq->dev_state > AP_DEV_STATE_UNINITIATED) {
				aq->dev_state = AP_DEV_STATE_ERROR;
				aq->last_err_rc = AP_RESPONSE_DECONFIGURED;
			}
			spin_unlock_bh(&aq->lock);
			AP_DBF_DBG("%s(%d,%d) queue dev config off\n",
				   __func__, ac->id, dom);
			ap_send_config_uevent(&aq->ap_dev, aq->config);
			 
			ap_flush_queue(aq);
			goto put_dev_and_continue;
		} else if (!decfg && !aq->config) {
			 
			aq->config = true;
			if (aq->dev_state > AP_DEV_STATE_UNINITIATED)
				_ap_queue_init_state(aq);
			spin_unlock_bh(&aq->lock);
			AP_DBF_DBG("%s(%d,%d) queue dev config on\n",
				   __func__, ac->id, dom);
			ap_send_config_uevent(&aq->ap_dev, aq->config);
			goto put_dev_and_continue;
		}
		 
		if (!decfg && aq->dev_state == AP_DEV_STATE_ERROR) {
			spin_unlock_bh(&aq->lock);
			 
			ap_flush_queue(aq);
			 
			ap_queue_init_state(aq);
			AP_DBF_INFO("%s(%d,%d) queue dev reinit enforced\n",
				    __func__, ac->id, dom);
			goto put_dev_and_continue;
		}
		spin_unlock_bh(&aq->lock);
put_dev_and_continue:
		put_device(dev);
	}
}

 
static inline void ap_scan_adapter(int ap)
{
	int rc, dom, depth, type, comp_type, ml;
	bool decfg, chkstop;
	struct ap_card *ac;
	struct device *dev;
	unsigned int func;
	ap_qid_t qid;

	 
	dev = bus_find_device(&ap_bus_type, NULL,
			      (void *)(long)ap,
			      __match_card_device_with_id);
	ac = dev ? to_ap_card(dev) : NULL;

	 
	if (!ap_test_config_card_id(ap)) {
		if (ac) {
			AP_DBF_INFO("%s(%d) ap not in config any more, rm card and queue devs\n",
				    __func__, ap);
			ap_scan_rm_card_dev_and_queue_devs(ac);
			put_device(dev);
		}
		return;
	}

	 

	for (dom = 0; dom <= ap_max_domain_id; dom++)
		if (ap_test_config_usage_domain(dom)) {
			qid = AP_MKQID(ap, dom);
			if (ap_queue_info(qid, &type, &func, &depth,
					  &ml, &decfg, &chkstop) > 0)
				break;
		}
	if (dom > ap_max_domain_id) {
		 
		if (ac) {
			AP_DBF_INFO("%s(%d) no type info (no APQN found), rm card and queue devs\n",
				    __func__, ap);
			ap_scan_rm_card_dev_and_queue_devs(ac);
			put_device(dev);
		} else {
			AP_DBF_DBG("%s(%d) no type info (no APQN found), ignored\n",
				   __func__, ap);
		}
		return;
	}
	if (!type) {
		 
		if (ac) {
			AP_DBF_INFO("%s(%d) no valid type (0) info, rm card and queue devs\n",
				    __func__, ap);
			ap_scan_rm_card_dev_and_queue_devs(ac);
			put_device(dev);
		} else {
			AP_DBF_DBG("%s(%d) no valid type (0) info, ignored\n",
				   __func__, ap);
		}
		return;
	}
	if (ac) {
		 
		if (ac->raw_hwtype != type) {
			AP_DBF_INFO("%s(%d) hwtype %d changed, rm card and queue devs\n",
				    __func__, ap, type);
			ap_scan_rm_card_dev_and_queue_devs(ac);
			put_device(dev);
			ac = NULL;
		} else if ((ac->functions & TAPQ_CARD_FUNC_CMP_MASK) !=
			   (func & TAPQ_CARD_FUNC_CMP_MASK)) {
			AP_DBF_INFO("%s(%d) functions 0x%08x changed, rm card and queue devs\n",
				    __func__, ap, func);
			ap_scan_rm_card_dev_and_queue_devs(ac);
			put_device(dev);
			ac = NULL;
		} else {
			 
			if (chkstop && !ac->chkstop) {
				 
				ac->chkstop = true;
				AP_DBF_INFO("%s(%d) card dev checkstop on\n",
					    __func__, ap);
			} else if (!chkstop && ac->chkstop) {
				 
				ac->chkstop = false;
				AP_DBF_INFO("%s(%d) card dev checkstop off\n",
					    __func__, ap);
			}
			 
			if (decfg && ac->config) {
				ac->config = false;
				AP_DBF_INFO("%s(%d) card dev config off\n",
					    __func__, ap);
				ap_send_config_uevent(&ac->ap_dev, ac->config);
			} else if (!decfg && !ac->config) {
				ac->config = true;
				AP_DBF_INFO("%s(%d) card dev config on\n",
					    __func__, ap);
				ap_send_config_uevent(&ac->ap_dev, ac->config);
			}
		}
	}

	if (!ac) {
		 
		comp_type = ap_get_compatible_type(qid, type, func);
		if (!comp_type) {
			AP_DBF_WARN("%s(%d) type %d, can't get compatibility type\n",
				    __func__, ap, type);
			return;
		}
		ac = ap_card_create(ap, depth, type, comp_type, func, ml);
		if (!ac) {
			AP_DBF_WARN("%s(%d) ap_card_create() failed\n",
				    __func__, ap);
			return;
		}
		ac->config = !decfg;
		ac->chkstop = chkstop;
		dev = &ac->ap_dev.device;
		dev->bus = &ap_bus_type;
		dev->parent = ap_root_device;
		dev_set_name(dev, "card%02x", ap);
		 
		if (ac->maxmsgsize > atomic_read(&ap_max_msg_size)) {
			atomic_set(&ap_max_msg_size, ac->maxmsgsize);
			AP_DBF_INFO("%s(%d) ap_max_msg_size update to %d byte\n",
				    __func__, ap,
				    atomic_read(&ap_max_msg_size));
		}
		 
		rc = device_register(dev);
		if (rc) {
			AP_DBF_WARN("%s(%d) device_register() failed\n",
				    __func__, ap);
			put_device(dev);
			return;
		}
		 
		get_device(dev);
		if (decfg)
			AP_DBF_INFO("%s(%d) new (decfg) card dev type=%d func=0x%08x created\n",
				    __func__, ap, type, func);
		else if (chkstop)
			AP_DBF_INFO("%s(%d) new (chkstop) card dev type=%d func=0x%08x created\n",
				    __func__, ap, type, func);
		else
			AP_DBF_INFO("%s(%d) new card dev type=%d func=0x%08x created\n",
				    __func__, ap, type, func);
	}

	 
	ap_scan_domains(ac);

	 
	put_device(&ac->ap_dev.device);
}

 
static bool ap_get_configuration(void)
{
	if (!ap_qci_info)	 
		return false;

	memcpy(ap_qci_info_old, ap_qci_info, sizeof(*ap_qci_info));
	ap_fetch_qci_info(ap_qci_info);

	return memcmp(ap_qci_info, ap_qci_info_old,
		      sizeof(struct ap_config_info)) != 0;
}

 
static void ap_scan_bus(struct work_struct *unused)
{
	int ap, config_changed = 0;

	 
	config_changed = ap_get_configuration();
	if (config_changed)
		notify_config_changed();
	ap_select_domain();

	AP_DBF_DBG("%s running\n", __func__);

	 
	for (ap = 0; ap <= ap_max_adapter_id; ap++)
		ap_scan_adapter(ap);

	 
	if (config_changed)
		notify_scan_complete();

	 
	if (ap_domain_index >= 0) {
		struct device *dev =
			bus_find_device(&ap_bus_type, NULL,
					(void *)(long)ap_domain_index,
					__match_queue_device_with_queue_id);
		if (dev)
			put_device(dev);
		else
			AP_DBF_INFO("%s no queue device with default domain %d available\n",
				    __func__, ap_domain_index);
	}

	if (atomic64_inc_return(&ap_scan_bus_count) == 1) {
		AP_DBF_DBG("%s init scan complete\n", __func__);
		ap_send_init_scan_done_uevent();
		ap_check_bindings_complete();
	}

	mod_timer(&ap_config_timer, jiffies + ap_config_time * HZ);
}

static void ap_config_timeout(struct timer_list *unused)
{
	queue_work(system_long_wq, &ap_scan_work);
}

static int __init ap_debug_init(void)
{
	ap_dbf_info = debug_register("ap", 2, 1,
				     DBF_MAX_SPRINTF_ARGS * sizeof(long));
	debug_register_view(ap_dbf_info, &debug_sprintf_view);
	debug_set_level(ap_dbf_info, DBF_ERR);

	return 0;
}

static void __init ap_perms_init(void)
{
	 
	memset(&ap_perms.ioctlm, 0xFF, sizeof(ap_perms.ioctlm));
	memset(&ap_perms.apm, 0xFF, sizeof(ap_perms.apm));
	memset(&ap_perms.aqm, 0xFF, sizeof(ap_perms.aqm));

	 
	if (apm_str) {
		memset(&ap_perms.apm, 0, sizeof(ap_perms.apm));
		ap_parse_mask_str(apm_str, ap_perms.apm, AP_DEVICES,
				  &ap_perms_mutex);
	}

	 
	if (aqm_str) {
		memset(&ap_perms.aqm, 0, sizeof(ap_perms.aqm));
		ap_parse_mask_str(aqm_str, ap_perms.aqm, AP_DOMAINS,
				  &ap_perms_mutex);
	}
}

 
static int __init ap_module_init(void)
{
	int rc;

	rc = ap_debug_init();
	if (rc)
		return rc;

	if (!ap_instructions_available()) {
		pr_warn("The hardware system does not support AP instructions\n");
		return -ENODEV;
	}

	 
	hash_init(ap_queues);

	 
	ap_perms_init();

	 
	ap_init_qci_info();

	 
	if (ap_domain_index < -1 || ap_domain_index > ap_max_domain_id ||
	    (ap_domain_index >= 0 &&
	     !test_bit_inv(ap_domain_index, ap_perms.aqm))) {
		pr_warn("%d is not a valid cryptographic domain\n",
			ap_domain_index);
		ap_domain_index = -1;
	}

	 
	if (ap_interrupts_available() && ap_useirq) {
		rc = register_adapter_interrupt(&ap_airq);
		ap_irq_flag = (rc == 0);
	}

	 
	rc = bus_register(&ap_bus_type);
	if (rc)
		goto out;

	 
	ap_root_device = root_device_register("ap");
	rc = PTR_ERR_OR_ZERO(ap_root_device);
	if (rc)
		goto out_bus;
	ap_root_device->bus = &ap_bus_type;

	 
	timer_setup(&ap_config_timer, ap_config_timeout, 0);

	 
	if (MACHINE_IS_VM)
		poll_high_timeout = 1500000;
	hrtimer_init(&ap_poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	ap_poll_timer.function = ap_poll_timeout;

	 
	if (ap_thread_flag) {
		rc = ap_poll_thread_start();
		if (rc)
			goto out_work;
	}

	queue_work(system_long_wq, &ap_scan_work);

	return 0;

out_work:
	hrtimer_cancel(&ap_poll_timer);
	root_device_unregister(ap_root_device);
out_bus:
	bus_unregister(&ap_bus_type);
out:
	if (ap_irq_flag)
		unregister_adapter_interrupt(&ap_airq);
	kfree(ap_qci_info);
	return rc;
}
device_initcall(ap_module_init);
