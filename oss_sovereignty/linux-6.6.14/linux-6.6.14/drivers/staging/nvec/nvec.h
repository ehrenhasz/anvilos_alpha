#ifndef __LINUX_MFD_NVEC
#define __LINUX_MFD_NVEC
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/reset.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#define NVEC_POOL_SIZE	64
#define NVEC_MSG_SIZE	34
enum nvec_event_size {
	NVEC_2BYTES,
	NVEC_3BYTES,
	NVEC_VAR_SIZE,
};
enum nvec_msg_type {
	NVEC_SYS = 1,
	NVEC_BAT,
	NVEC_GPIO,
	NVEC_SLEEP,
	NVEC_KBD,
	NVEC_PS2,
	NVEC_CNTL,
	NVEC_OEM0 = 0x0d,
	NVEC_KB_EVT = 0x80,
	NVEC_PS2_EVT,
};
struct nvec_msg {
	struct list_head node;
	unsigned char data[NVEC_MSG_SIZE];
	unsigned short size;
	unsigned short pos;
	atomic_t used;
};
struct nvec_chip {
	struct device *dev;
	struct gpio_desc *gpiod;
	int irq;
	u32 i2c_addr;
	void __iomem *base;
	struct clk *i2c_clk;
	struct reset_control *rst;
	struct atomic_notifier_head notifier_list;
	struct list_head rx_data, tx_data;
	struct notifier_block nvec_status_notifier;
	struct work_struct rx_work, tx_work;
	struct workqueue_struct *wq;
	struct nvec_msg msg_pool[NVEC_POOL_SIZE];
	struct nvec_msg *rx;
	struct nvec_msg *tx;
	struct nvec_msg tx_scratch;
	struct completion ec_transfer;
	spinlock_t tx_lock, rx_lock;
	struct mutex sync_write_mutex;
	struct completion sync_write;
	u16 sync_write_pending;
	struct nvec_msg *last_sync_msg;
	int state;
};
int nvec_write_async(struct nvec_chip *nvec, const unsigned char *data,
		     short size);
int nvec_write_sync(struct nvec_chip *nvec,
		    const unsigned char *data, short size,
		    struct nvec_msg **msg);
int nvec_register_notifier(struct nvec_chip *nvec,
			   struct notifier_block *nb,
			   unsigned int events);
int nvec_unregister_notifier(struct nvec_chip *dev, struct notifier_block *nb);
void nvec_msg_free(struct nvec_chip *nvec, struct nvec_msg *msg);
#endif
