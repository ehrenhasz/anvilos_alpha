#ifndef _CEC_PRIV_H
#define _CEC_PRIV_H
#include <linux/cec-funcs.h>
#include <media/cec-notifier.h>
#define dprintk(lvl, fmt, arg...)					\
	do {								\
		if (lvl <= cec_debug)					\
			pr_info("cec-%s: " fmt, adap->name, ## arg);	\
	} while (0)
#define call_op(adap, op, arg...)					\
	((adap->ops->op && !adap->devnode.unregistered) ?		\
	 adap->ops->op(adap, ## arg) : 0)
#define call_void_op(adap, op, arg...)					\
	do {								\
		if (adap->ops->op && !adap->devnode.unregistered)	\
			adap->ops->op(adap, ## arg);			\
	} while (0)
#define to_cec_adapter(node) container_of(node, struct cec_adapter, devnode)
static inline bool msg_is_raw(const struct cec_msg *msg)
{
	return msg->flags & CEC_MSG_FL_RAW;
}
extern int cec_debug;
int cec_get_device(struct cec_devnode *devnode);
void cec_put_device(struct cec_devnode *devnode);
int cec_monitor_all_cnt_inc(struct cec_adapter *adap);
void cec_monitor_all_cnt_dec(struct cec_adapter *adap);
int cec_monitor_pin_cnt_inc(struct cec_adapter *adap);
void cec_monitor_pin_cnt_dec(struct cec_adapter *adap);
int cec_adap_status(struct seq_file *file, void *priv);
int cec_thread_func(void *_adap);
int cec_adap_enable(struct cec_adapter *adap);
void __cec_s_phys_addr(struct cec_adapter *adap, u16 phys_addr, bool block);
int __cec_s_log_addrs(struct cec_adapter *adap,
		      struct cec_log_addrs *log_addrs, bool block);
int cec_transmit_msg_fh(struct cec_adapter *adap, struct cec_msg *msg,
			struct cec_fh *fh, bool block);
void cec_queue_event_fh(struct cec_fh *fh,
			const struct cec_event *new_ev, u64 ts);
extern const struct file_operations cec_devnode_fops;
#endif
