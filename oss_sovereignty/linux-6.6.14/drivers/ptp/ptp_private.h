#ifndef _PTP_PRIVATE_H_
#define _PTP_PRIVATE_H_
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/posix-clock.h>
#include <linux/ptp_clock.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/time.h>
#define PTP_MAX_TIMESTAMPS 128
#define PTP_BUF_TIMESTAMPS 30
#define PTP_DEFAULT_MAX_VCLOCKS 20
struct timestamp_event_queue {
	struct ptp_extts_event buf[PTP_MAX_TIMESTAMPS];
	int head;
	int tail;
	spinlock_t lock;
};
struct ptp_clock {
	struct posix_clock clock;
	struct device dev;
	struct ptp_clock_info *info;
	dev_t devid;
	int index;  
	struct pps_device *pps_source;
	long dialed_frequency;  
	struct timestamp_event_queue tsevq;  
	struct mutex tsevq_mux;  
	struct mutex pincfg_mux;  
	wait_queue_head_t tsev_wq;
	int defunct;  
	struct device_attribute *pin_dev_attr;
	struct attribute **pin_attr;
	struct attribute_group pin_attr_group;
	const struct attribute_group *pin_attr_groups[2];
	struct kthread_worker *kworker;
	struct kthread_delayed_work aux_work;
	unsigned int max_vclocks;
	unsigned int n_vclocks;
	int *vclock_index;
	struct mutex n_vclocks_mux;  
	bool is_virtual_clock;
	bool has_cycles;
};
#define info_to_vclock(d) container_of((d), struct ptp_vclock, info)
#define cc_to_vclock(d) container_of((d), struct ptp_vclock, cc)
#define dw_to_vclock(d) container_of((d), struct ptp_vclock, refresh_work)
struct ptp_vclock {
	struct ptp_clock *pclock;
	struct ptp_clock_info info;
	struct ptp_clock *clock;
	struct hlist_node vclock_hash_node;
	struct cyclecounter cc;
	struct timecounter tc;
	struct mutex lock;	 
};
static inline int queue_cnt(const struct timestamp_event_queue *q)
{
	int cnt = READ_ONCE(q->tail) - READ_ONCE(q->head);
	return cnt < 0 ? PTP_MAX_TIMESTAMPS + cnt : cnt;
}
static inline bool ptp_vclock_in_use(struct ptp_clock *ptp)
{
	bool in_use = false;
	if (mutex_lock_interruptible(&ptp->n_vclocks_mux))
		return true;
	if (!ptp->is_virtual_clock && ptp->n_vclocks)
		in_use = true;
	mutex_unlock(&ptp->n_vclocks_mux);
	return in_use;
}
static inline bool ptp_clock_freerun(struct ptp_clock *ptp)
{
	if (ptp->has_cycles)
		return false;
	return ptp_vclock_in_use(ptp);
}
extern struct class *ptp_class;
int ptp_set_pinfunc(struct ptp_clock *ptp, unsigned int pin,
		    enum ptp_pin_function func, unsigned int chan);
long ptp_ioctl(struct posix_clock *pc,
	       unsigned int cmd, unsigned long arg);
int ptp_open(struct posix_clock *pc, fmode_t fmode);
ssize_t ptp_read(struct posix_clock *pc,
		 uint flags, char __user *buf, size_t cnt);
__poll_t ptp_poll(struct posix_clock *pc,
	      struct file *fp, poll_table *wait);
extern const struct attribute_group *ptp_groups[];
int ptp_populate_pin_groups(struct ptp_clock *ptp);
void ptp_cleanup_pin_groups(struct ptp_clock *ptp);
struct ptp_vclock *ptp_vclock_register(struct ptp_clock *pclock);
void ptp_vclock_unregister(struct ptp_vclock *vclock);
#endif
