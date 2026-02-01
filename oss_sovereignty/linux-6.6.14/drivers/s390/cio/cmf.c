
 

#define KMSG_COMPONENT "cio"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/memblock.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/export.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/timex.h>	 

#include <asm/ccwdev.h>
#include <asm/cio.h>
#include <asm/cmb.h>
#include <asm/div64.h>

#include "cio.h"
#include "css.h"
#include "device.h"
#include "ioasm.h"
#include "chsc.h"

 
#define ARGSTRING "s390cmf"

 
enum cmb_index {
	avg_utilization = -1,
  
	cmb_ssch_rsch_count = 0,
	cmb_sample_count,
	cmb_device_connect_time,
	cmb_function_pending_time,
	cmb_device_disconnect_time,
	cmb_control_unit_queuing_time,
	cmb_device_active_only_time,
  
	cmb_device_busy_time,
	cmb_initial_command_response_time,
};

 
enum cmb_format {
	CMF_BASIC,
	CMF_EXTENDED,
	CMF_AUTODETECT = -1,
};

 
static int format = CMF_AUTODETECT;
module_param(format, bint, 0444);

 
struct cmb_operations {
	int  (*alloc)  (struct ccw_device *);
	void (*free)   (struct ccw_device *);
	int  (*set)    (struct ccw_device *, u32);
	u64  (*read)   (struct ccw_device *, int);
	int  (*readall)(struct ccw_device *, struct cmbdata *);
	void (*reset)  (struct ccw_device *);
 
	struct attribute_group *attr_group;
};
static struct cmb_operations *cmbops;

struct cmb_data {
	void *hw_block;    
	void *last_block;  
	int size;	   
	unsigned long long last_update;   
};

 
static inline u64 time_to_nsec(u32 value)
{
	return ((u64)value) * 128000ull;
}

 
static inline u64 time_to_avg_nsec(u32 value, u32 count)
{
	u64 ret;

	 
	if (count == 0)
		return 0;

	 
	ret = time_to_nsec(value);
	do_div(ret, count);

	return ret;
}

#define CMF_OFF 0
#define CMF_ON	2

 
static inline void cmf_activate(void *area, unsigned int onoff)
{
	 
	asm volatile(
		"	lgr	1,%[r1]\n"
		"	lgr	2,%[mbo]\n"
		"	schm\n"
		:
		: [r1] "d" ((unsigned long)onoff), [mbo] "d" (area)
		: "1", "2");
}

static int set_schib(struct ccw_device *cdev, u32 mme, int mbfc,
		     unsigned long address)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	int ret;

	sch->config.mme = mme;
	sch->config.mbfc = mbfc;
	 
	if (mbfc)
		sch->config.mba = address;
	else
		sch->config.mbi = address;

	ret = cio_commit_config(sch);
	if (!mme && ret == -ENODEV) {
		 
		ret = 0;
	}
	return ret;
}

struct set_schib_struct {
	u32 mme;
	int mbfc;
	unsigned long address;
	wait_queue_head_t wait;
	int ret;
};

#define CMF_PENDING 1
#define SET_SCHIB_TIMEOUT (10 * HZ)

static int set_schib_wait(struct ccw_device *cdev, u32 mme,
			  int mbfc, unsigned long address)
{
	struct set_schib_struct set_data;
	int ret = -ENODEV;

	spin_lock_irq(cdev->ccwlock);
	if (!cdev->private->cmb)
		goto out;

	ret = set_schib(cdev, mme, mbfc, address);
	if (ret != -EBUSY)
		goto out;

	 
	if (cdev->private->state != DEV_STATE_ONLINE)
		goto out;

	init_waitqueue_head(&set_data.wait);
	set_data.mme = mme;
	set_data.mbfc = mbfc;
	set_data.address = address;
	set_data.ret = CMF_PENDING;

	cdev->private->state = DEV_STATE_CMFCHANGE;
	cdev->private->cmb_wait = &set_data;
	spin_unlock_irq(cdev->ccwlock);

	ret = wait_event_interruptible_timeout(set_data.wait,
					       set_data.ret != CMF_PENDING,
					       SET_SCHIB_TIMEOUT);
	spin_lock_irq(cdev->ccwlock);
	if (ret <= 0) {
		if (set_data.ret == CMF_PENDING) {
			set_data.ret = (ret == 0) ? -ETIME : ret;
			if (cdev->private->state == DEV_STATE_CMFCHANGE)
				cdev->private->state = DEV_STATE_ONLINE;
		}
	}
	cdev->private->cmb_wait = NULL;
	ret = set_data.ret;
out:
	spin_unlock_irq(cdev->ccwlock);
	return ret;
}

void retry_set_schib(struct ccw_device *cdev)
{
	struct set_schib_struct *set_data = cdev->private->cmb_wait;

	if (!set_data)
		return;

	set_data->ret = set_schib(cdev, set_data->mme, set_data->mbfc,
				  set_data->address);
	wake_up(&set_data->wait);
}

static int cmf_copy_block(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	struct cmb_data *cmb_data;
	void *hw_block;

	if (cio_update_schib(sch))
		return -ENODEV;

	if (scsw_fctl(&sch->schib.scsw) & SCSW_FCTL_START_FUNC) {
		 
		if ((!(scsw_actl(&sch->schib.scsw) & SCSW_ACTL_SUSPENDED)) &&
		    (scsw_actl(&sch->schib.scsw) &
		     (SCSW_ACTL_DEVACT | SCSW_ACTL_SCHACT)) &&
		    (!(scsw_stctl(&sch->schib.scsw) & SCSW_STCTL_SEC_STATUS)))
			return -EBUSY;
	}
	cmb_data = cdev->private->cmb;
	hw_block = cmb_data->hw_block;
	memcpy(cmb_data->last_block, hw_block, cmb_data->size);
	cmb_data->last_update = get_tod_clock();
	return 0;
}

struct copy_block_struct {
	wait_queue_head_t wait;
	int ret;
};

static int cmf_cmb_copy_wait(struct ccw_device *cdev)
{
	struct copy_block_struct copy_block;
	int ret = -ENODEV;

	spin_lock_irq(cdev->ccwlock);
	if (!cdev->private->cmb)
		goto out;

	ret = cmf_copy_block(cdev);
	if (ret != -EBUSY)
		goto out;

	if (cdev->private->state != DEV_STATE_ONLINE)
		goto out;

	init_waitqueue_head(&copy_block.wait);
	copy_block.ret = CMF_PENDING;

	cdev->private->state = DEV_STATE_CMFUPDATE;
	cdev->private->cmb_wait = &copy_block;
	spin_unlock_irq(cdev->ccwlock);

	ret = wait_event_interruptible(copy_block.wait,
				       copy_block.ret != CMF_PENDING);
	spin_lock_irq(cdev->ccwlock);
	if (ret) {
		if (copy_block.ret == CMF_PENDING) {
			copy_block.ret = -ERESTARTSYS;
			if (cdev->private->state == DEV_STATE_CMFUPDATE)
				cdev->private->state = DEV_STATE_ONLINE;
		}
	}
	cdev->private->cmb_wait = NULL;
	ret = copy_block.ret;
out:
	spin_unlock_irq(cdev->ccwlock);
	return ret;
}

void cmf_retry_copy_block(struct ccw_device *cdev)
{
	struct copy_block_struct *copy_block = cdev->private->cmb_wait;

	if (!copy_block)
		return;

	copy_block->ret = cmf_copy_block(cdev);
	wake_up(&copy_block->wait);
}

static void cmf_generic_reset(struct ccw_device *cdev)
{
	struct cmb_data *cmb_data;

	spin_lock_irq(cdev->ccwlock);
	cmb_data = cdev->private->cmb;
	if (cmb_data) {
		memset(cmb_data->last_block, 0, cmb_data->size);
		 
		memset(cmb_data->hw_block, 0, cmb_data->size);
		cmb_data->last_update = 0;
	}
	cdev->private->cmb_start_time = get_tod_clock();
	spin_unlock_irq(cdev->ccwlock);
}

 
struct cmb_area {
	struct cmb *mem;
	struct list_head list;
	int num_channels;
	spinlock_t lock;
};

static struct cmb_area cmb_area = {
	.lock = __SPIN_LOCK_UNLOCKED(cmb_area.lock),
	.list = LIST_HEAD_INIT(cmb_area.list),
	.num_channels  = 1024,
};

 

 

module_param_named(maxchannels, cmb_area.num_channels, uint, 0444);

 
struct cmb {
	u16 ssch_rsch_count;
	u16 sample_count;
	u32 device_connect_time;
	u32 function_pending_time;
	u32 device_disconnect_time;
	u32 control_unit_queuing_time;
	u32 device_active_only_time;
	u32 reserved[2];
};

 
static int alloc_cmb_single(struct ccw_device *cdev,
			    struct cmb_data *cmb_data)
{
	struct cmb *cmb;
	struct ccw_device_private *node;
	int ret;

	spin_lock_irq(cdev->ccwlock);
	if (!list_empty(&cdev->private->cmb_list)) {
		ret = -EBUSY;
		goto out;
	}

	 
	cmb = cmb_area.mem;
	list_for_each_entry(node, &cmb_area.list, cmb_list) {
		struct cmb_data *data;
		data = node->cmb;
		if ((struct cmb*)data->hw_block > cmb)
			break;
		cmb++;
	}
	if (cmb - cmb_area.mem >= cmb_area.num_channels) {
		ret = -ENOMEM;
		goto out;
	}

	 
	list_add_tail(&cdev->private->cmb_list, &node->cmb_list);
	cmb_data->hw_block = cmb;
	cdev->private->cmb = cmb_data;
	ret = 0;
out:
	spin_unlock_irq(cdev->ccwlock);
	return ret;
}

static int alloc_cmb(struct ccw_device *cdev)
{
	int ret;
	struct cmb *mem;
	ssize_t size;
	struct cmb_data *cmb_data;

	 
	cmb_data = kzalloc(sizeof(struct cmb_data), GFP_KERNEL);
	if (!cmb_data)
		return -ENOMEM;

	cmb_data->last_block = kzalloc(sizeof(struct cmb), GFP_KERNEL);
	if (!cmb_data->last_block) {
		kfree(cmb_data);
		return -ENOMEM;
	}
	cmb_data->size = sizeof(struct cmb);
	spin_lock(&cmb_area.lock);

	if (!cmb_area.mem) {
		 
		size = sizeof(struct cmb) * cmb_area.num_channels;
		WARN_ON(!list_empty(&cmb_area.list));

		spin_unlock(&cmb_area.lock);
		mem = (void*)__get_free_pages(GFP_KERNEL | GFP_DMA,
				 get_order(size));
		spin_lock(&cmb_area.lock);

		if (cmb_area.mem) {
			 
			free_pages((unsigned long)mem, get_order(size));
		} else if (!mem) {
			 
			ret = -ENOMEM;
			goto out;
		} else {
			 
			memset(mem, 0, size);
			cmb_area.mem = mem;
			cmf_activate(cmb_area.mem, CMF_ON);
		}
	}

	 
	ret = alloc_cmb_single(cdev, cmb_data);
out:
	spin_unlock(&cmb_area.lock);
	if (ret) {
		kfree(cmb_data->last_block);
		kfree(cmb_data);
	}
	return ret;
}

static void free_cmb(struct ccw_device *cdev)
{
	struct ccw_device_private *priv;
	struct cmb_data *cmb_data;

	spin_lock(&cmb_area.lock);
	spin_lock_irq(cdev->ccwlock);

	priv = cdev->private;
	cmb_data = priv->cmb;
	priv->cmb = NULL;
	if (cmb_data)
		kfree(cmb_data->last_block);
	kfree(cmb_data);
	list_del_init(&priv->cmb_list);

	if (list_empty(&cmb_area.list)) {
		ssize_t size;
		size = sizeof(struct cmb) * cmb_area.num_channels;
		cmf_activate(NULL, CMF_OFF);
		free_pages((unsigned long)cmb_area.mem, get_order(size));
		cmb_area.mem = NULL;
	}
	spin_unlock_irq(cdev->ccwlock);
	spin_unlock(&cmb_area.lock);
}

static int set_cmb(struct ccw_device *cdev, u32 mme)
{
	u16 offset;
	struct cmb_data *cmb_data;
	unsigned long flags;

	spin_lock_irqsave(cdev->ccwlock, flags);
	if (!cdev->private->cmb) {
		spin_unlock_irqrestore(cdev->ccwlock, flags);
		return -EINVAL;
	}
	cmb_data = cdev->private->cmb;
	offset = mme ? (struct cmb *)cmb_data->hw_block - cmb_area.mem : 0;
	spin_unlock_irqrestore(cdev->ccwlock, flags);

	return set_schib_wait(cdev, mme, 0, offset);
}

 
static u64 __cmb_utilization(u64 device_connect_time, u64 function_pending_time,
			     u64 device_disconnect_time, u64 start_time)
{
	u64 utilization, elapsed_time;

	utilization = time_to_nsec(device_connect_time +
				   function_pending_time +
				   device_disconnect_time);

	elapsed_time = get_tod_clock() - start_time;
	elapsed_time = tod_to_ns(elapsed_time);
	elapsed_time /= 1000;

	return elapsed_time ? (utilization / elapsed_time) : 0;
}

static u64 read_cmb(struct ccw_device *cdev, int index)
{
	struct cmb_data *cmb_data;
	unsigned long flags;
	struct cmb *cmb;
	u64 ret = 0;
	u32 val;

	spin_lock_irqsave(cdev->ccwlock, flags);
	cmb_data = cdev->private->cmb;
	if (!cmb_data)
		goto out;

	cmb = cmb_data->hw_block;
	switch (index) {
	case avg_utilization:
		ret = __cmb_utilization(cmb->device_connect_time,
					cmb->function_pending_time,
					cmb->device_disconnect_time,
					cdev->private->cmb_start_time);
		goto out;
	case cmb_ssch_rsch_count:
		ret = cmb->ssch_rsch_count;
		goto out;
	case cmb_sample_count:
		ret = cmb->sample_count;
		goto out;
	case cmb_device_connect_time:
		val = cmb->device_connect_time;
		break;
	case cmb_function_pending_time:
		val = cmb->function_pending_time;
		break;
	case cmb_device_disconnect_time:
		val = cmb->device_disconnect_time;
		break;
	case cmb_control_unit_queuing_time:
		val = cmb->control_unit_queuing_time;
		break;
	case cmb_device_active_only_time:
		val = cmb->device_active_only_time;
		break;
	default:
		goto out;
	}
	ret = time_to_avg_nsec(val, cmb->sample_count);
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static int readall_cmb(struct ccw_device *cdev, struct cmbdata *data)
{
	struct cmb *cmb;
	struct cmb_data *cmb_data;
	u64 time;
	unsigned long flags;
	int ret;

	ret = cmf_cmb_copy_wait(cdev);
	if (ret < 0)
		return ret;
	spin_lock_irqsave(cdev->ccwlock, flags);
	cmb_data = cdev->private->cmb;
	if (!cmb_data) {
		ret = -ENODEV;
		goto out;
	}
	if (cmb_data->last_update == 0) {
		ret = -EAGAIN;
		goto out;
	}
	cmb = cmb_data->last_block;
	time = cmb_data->last_update - cdev->private->cmb_start_time;

	memset(data, 0, sizeof(struct cmbdata));

	 
	data->size = offsetof(struct cmbdata, device_busy_time);

	data->elapsed_time = tod_to_ns(time);

	 
	data->ssch_rsch_count = cmb->ssch_rsch_count;
	data->sample_count = cmb->sample_count;

	 
	data->device_connect_time = time_to_nsec(cmb->device_connect_time);
	data->function_pending_time = time_to_nsec(cmb->function_pending_time);
	data->device_disconnect_time =
		time_to_nsec(cmb->device_disconnect_time);
	data->control_unit_queuing_time
		= time_to_nsec(cmb->control_unit_queuing_time);
	data->device_active_only_time
		= time_to_nsec(cmb->device_active_only_time);
	ret = 0;
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static void reset_cmb(struct ccw_device *cdev)
{
	cmf_generic_reset(cdev);
}

static int cmf_enabled(struct ccw_device *cdev)
{
	int enabled;

	spin_lock_irq(cdev->ccwlock);
	enabled = !!cdev->private->cmb;
	spin_unlock_irq(cdev->ccwlock);

	return enabled;
}

static struct attribute_group cmf_attr_group;

static struct cmb_operations cmbops_basic = {
	.alloc	= alloc_cmb,
	.free	= free_cmb,
	.set	= set_cmb,
	.read	= read_cmb,
	.readall    = readall_cmb,
	.reset	    = reset_cmb,
	.attr_group = &cmf_attr_group,
};

 

 
struct cmbe {
	u32 ssch_rsch_count;
	u32 sample_count;
	u32 device_connect_time;
	u32 function_pending_time;
	u32 device_disconnect_time;
	u32 control_unit_queuing_time;
	u32 device_active_only_time;
	u32 device_busy_time;
	u32 initial_command_response_time;
	u32 reserved[7];
} __packed __aligned(64);

static struct kmem_cache *cmbe_cache;

static int alloc_cmbe(struct ccw_device *cdev)
{
	struct cmb_data *cmb_data;
	struct cmbe *cmbe;
	int ret = -ENOMEM;

	cmbe = kmem_cache_zalloc(cmbe_cache, GFP_KERNEL);
	if (!cmbe)
		return ret;

	cmb_data = kzalloc(sizeof(*cmb_data), GFP_KERNEL);
	if (!cmb_data)
		goto out_free;

	cmb_data->last_block = kzalloc(sizeof(struct cmbe), GFP_KERNEL);
	if (!cmb_data->last_block)
		goto out_free;

	cmb_data->size = sizeof(*cmbe);
	cmb_data->hw_block = cmbe;

	spin_lock(&cmb_area.lock);
	spin_lock_irq(cdev->ccwlock);
	if (cdev->private->cmb)
		goto out_unlock;

	cdev->private->cmb = cmb_data;

	 
	if (list_empty(&cmb_area.list))
		cmf_activate(NULL, CMF_ON);
	list_add_tail(&cdev->private->cmb_list, &cmb_area.list);

	spin_unlock_irq(cdev->ccwlock);
	spin_unlock(&cmb_area.lock);
	return 0;

out_unlock:
	spin_unlock_irq(cdev->ccwlock);
	spin_unlock(&cmb_area.lock);
	ret = -EBUSY;
out_free:
	if (cmb_data)
		kfree(cmb_data->last_block);
	kfree(cmb_data);
	kmem_cache_free(cmbe_cache, cmbe);

	return ret;
}

static void free_cmbe(struct ccw_device *cdev)
{
	struct cmb_data *cmb_data;

	spin_lock(&cmb_area.lock);
	spin_lock_irq(cdev->ccwlock);
	cmb_data = cdev->private->cmb;
	cdev->private->cmb = NULL;
	if (cmb_data) {
		kfree(cmb_data->last_block);
		kmem_cache_free(cmbe_cache, cmb_data->hw_block);
	}
	kfree(cmb_data);

	 
	list_del_init(&cdev->private->cmb_list);
	if (list_empty(&cmb_area.list))
		cmf_activate(NULL, CMF_OFF);
	spin_unlock_irq(cdev->ccwlock);
	spin_unlock(&cmb_area.lock);
}

static int set_cmbe(struct ccw_device *cdev, u32 mme)
{
	unsigned long mba;
	struct cmb_data *cmb_data;
	unsigned long flags;

	spin_lock_irqsave(cdev->ccwlock, flags);
	if (!cdev->private->cmb) {
		spin_unlock_irqrestore(cdev->ccwlock, flags);
		return -EINVAL;
	}
	cmb_data = cdev->private->cmb;
	mba = mme ? (unsigned long) cmb_data->hw_block : 0;
	spin_unlock_irqrestore(cdev->ccwlock, flags);

	return set_schib_wait(cdev, mme, 1, mba);
}

static u64 read_cmbe(struct ccw_device *cdev, int index)
{
	struct cmb_data *cmb_data;
	unsigned long flags;
	struct cmbe *cmb;
	u64 ret = 0;
	u32 val;

	spin_lock_irqsave(cdev->ccwlock, flags);
	cmb_data = cdev->private->cmb;
	if (!cmb_data)
		goto out;

	cmb = cmb_data->hw_block;
	switch (index) {
	case avg_utilization:
		ret = __cmb_utilization(cmb->device_connect_time,
					cmb->function_pending_time,
					cmb->device_disconnect_time,
					cdev->private->cmb_start_time);
		goto out;
	case cmb_ssch_rsch_count:
		ret = cmb->ssch_rsch_count;
		goto out;
	case cmb_sample_count:
		ret = cmb->sample_count;
		goto out;
	case cmb_device_connect_time:
		val = cmb->device_connect_time;
		break;
	case cmb_function_pending_time:
		val = cmb->function_pending_time;
		break;
	case cmb_device_disconnect_time:
		val = cmb->device_disconnect_time;
		break;
	case cmb_control_unit_queuing_time:
		val = cmb->control_unit_queuing_time;
		break;
	case cmb_device_active_only_time:
		val = cmb->device_active_only_time;
		break;
	case cmb_device_busy_time:
		val = cmb->device_busy_time;
		break;
	case cmb_initial_command_response_time:
		val = cmb->initial_command_response_time;
		break;
	default:
		goto out;
	}
	ret = time_to_avg_nsec(val, cmb->sample_count);
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static int readall_cmbe(struct ccw_device *cdev, struct cmbdata *data)
{
	struct cmbe *cmb;
	struct cmb_data *cmb_data;
	u64 time;
	unsigned long flags;
	int ret;

	ret = cmf_cmb_copy_wait(cdev);
	if (ret < 0)
		return ret;
	spin_lock_irqsave(cdev->ccwlock, flags);
	cmb_data = cdev->private->cmb;
	if (!cmb_data) {
		ret = -ENODEV;
		goto out;
	}
	if (cmb_data->last_update == 0) {
		ret = -EAGAIN;
		goto out;
	}
	time = cmb_data->last_update - cdev->private->cmb_start_time;

	memset (data, 0, sizeof(struct cmbdata));

	 
	data->size = offsetof(struct cmbdata, device_busy_time);

	data->elapsed_time = tod_to_ns(time);

	cmb = cmb_data->last_block;
	 
	data->ssch_rsch_count = cmb->ssch_rsch_count;
	data->sample_count = cmb->sample_count;

	 
	data->device_connect_time = time_to_nsec(cmb->device_connect_time);
	data->function_pending_time = time_to_nsec(cmb->function_pending_time);
	data->device_disconnect_time =
		time_to_nsec(cmb->device_disconnect_time);
	data->control_unit_queuing_time
		= time_to_nsec(cmb->control_unit_queuing_time);
	data->device_active_only_time
		= time_to_nsec(cmb->device_active_only_time);
	data->device_busy_time = time_to_nsec(cmb->device_busy_time);
	data->initial_command_response_time
		= time_to_nsec(cmb->initial_command_response_time);

	ret = 0;
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static void reset_cmbe(struct ccw_device *cdev)
{
	cmf_generic_reset(cdev);
}

static struct attribute_group cmf_attr_group_ext;

static struct cmb_operations cmbops_extended = {
	.alloc	    = alloc_cmbe,
	.free	    = free_cmbe,
	.set	    = set_cmbe,
	.read	    = read_cmbe,
	.readall    = readall_cmbe,
	.reset	    = reset_cmbe,
	.attr_group = &cmf_attr_group_ext,
};

static ssize_t cmb_show_attr(struct device *dev, char *buf, enum cmb_index idx)
{
	return sprintf(buf, "%lld\n",
		(unsigned long long) cmf_read(to_ccwdev(dev), idx));
}

static ssize_t cmb_show_avg_sample_interval(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	unsigned long count;
	long interval;

	count = cmf_read(cdev, cmb_sample_count);
	spin_lock_irq(cdev->ccwlock);
	if (count) {
		interval = get_tod_clock() - cdev->private->cmb_start_time;
		interval = tod_to_ns(interval);
		interval /= count;
	} else
		interval = -1;
	spin_unlock_irq(cdev->ccwlock);
	return sprintf(buf, "%ld\n", interval);
}

static ssize_t cmb_show_avg_utilization(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned long u = cmf_read(to_ccwdev(dev), avg_utilization);

	return sprintf(buf, "%02lu.%01lu%%\n", u / 10, u % 10);
}

#define cmf_attr(name) \
static ssize_t show_##name(struct device *dev, \
			   struct device_attribute *attr, char *buf)	\
{ return cmb_show_attr((dev), buf, cmb_##name); } \
static DEVICE_ATTR(name, 0444, show_##name, NULL);

#define cmf_attr_avg(name) \
static ssize_t show_avg_##name(struct device *dev, \
			       struct device_attribute *attr, char *buf) \
{ return cmb_show_attr((dev), buf, cmb_##name); } \
static DEVICE_ATTR(avg_##name, 0444, show_avg_##name, NULL);

cmf_attr(ssch_rsch_count);
cmf_attr(sample_count);
cmf_attr_avg(device_connect_time);
cmf_attr_avg(function_pending_time);
cmf_attr_avg(device_disconnect_time);
cmf_attr_avg(control_unit_queuing_time);
cmf_attr_avg(device_active_only_time);
cmf_attr_avg(device_busy_time);
cmf_attr_avg(initial_command_response_time);

static DEVICE_ATTR(avg_sample_interval, 0444, cmb_show_avg_sample_interval,
		   NULL);
static DEVICE_ATTR(avg_utilization, 0444, cmb_show_avg_utilization, NULL);

static struct attribute *cmf_attributes[] = {
	&dev_attr_avg_sample_interval.attr,
	&dev_attr_avg_utilization.attr,
	&dev_attr_ssch_rsch_count.attr,
	&dev_attr_sample_count.attr,
	&dev_attr_avg_device_connect_time.attr,
	&dev_attr_avg_function_pending_time.attr,
	&dev_attr_avg_device_disconnect_time.attr,
	&dev_attr_avg_control_unit_queuing_time.attr,
	&dev_attr_avg_device_active_only_time.attr,
	NULL,
};

static struct attribute_group cmf_attr_group = {
	.name  = "cmf",
	.attrs = cmf_attributes,
};

static struct attribute *cmf_attributes_ext[] = {
	&dev_attr_avg_sample_interval.attr,
	&dev_attr_avg_utilization.attr,
	&dev_attr_ssch_rsch_count.attr,
	&dev_attr_sample_count.attr,
	&dev_attr_avg_device_connect_time.attr,
	&dev_attr_avg_function_pending_time.attr,
	&dev_attr_avg_device_disconnect_time.attr,
	&dev_attr_avg_control_unit_queuing_time.attr,
	&dev_attr_avg_device_active_only_time.attr,
	&dev_attr_avg_device_busy_time.attr,
	&dev_attr_avg_initial_command_response_time.attr,
	NULL,
};

static struct attribute_group cmf_attr_group_ext = {
	.name  = "cmf",
	.attrs = cmf_attributes_ext,
};

static ssize_t cmb_enable_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct ccw_device *cdev = to_ccwdev(dev);

	return sprintf(buf, "%d\n", cmf_enabled(cdev));
}

static ssize_t cmb_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t c)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	switch (val) {
	case 0:
		ret = disable_cmf(cdev);
		break;
	case 1:
		ret = enable_cmf(cdev);
		break;
	default:
		ret = -EINVAL;
	}

	return ret ? ret : c;
}
DEVICE_ATTR_RW(cmb_enable);

 
int enable_cmf(struct ccw_device *cdev)
{
	int ret = 0;

	device_lock(&cdev->dev);
	if (cmf_enabled(cdev)) {
		cmbops->reset(cdev);
		goto out_unlock;
	}
	get_device(&cdev->dev);
	ret = cmbops->alloc(cdev);
	if (ret)
		goto out;
	cmbops->reset(cdev);
	ret = sysfs_create_group(&cdev->dev.kobj, cmbops->attr_group);
	if (ret) {
		cmbops->free(cdev);
		goto out;
	}
	ret = cmbops->set(cdev, 2);
	if (ret) {
		sysfs_remove_group(&cdev->dev.kobj, cmbops->attr_group);
		cmbops->free(cdev);
	}
out:
	if (ret)
		put_device(&cdev->dev);
out_unlock:
	device_unlock(&cdev->dev);
	return ret;
}

 
int __disable_cmf(struct ccw_device *cdev)
{
	int ret;

	ret = cmbops->set(cdev, 0);
	if (ret)
		return ret;

	sysfs_remove_group(&cdev->dev.kobj, cmbops->attr_group);
	cmbops->free(cdev);
	put_device(&cdev->dev);

	return ret;
}

 
int disable_cmf(struct ccw_device *cdev)
{
	int ret;

	device_lock(&cdev->dev);
	ret = __disable_cmf(cdev);
	device_unlock(&cdev->dev);

	return ret;
}

 
u64 cmf_read(struct ccw_device *cdev, int index)
{
	return cmbops->read(cdev, index);
}

 
int cmf_readall(struct ccw_device *cdev, struct cmbdata *data)
{
	return cmbops->readall(cdev, data);
}

 
int cmf_reenable(struct ccw_device *cdev)
{
	cmbops->reset(cdev);
	return cmbops->set(cdev, 2);
}

 
void cmf_reactivate(void)
{
	spin_lock(&cmb_area.lock);
	if (!list_empty(&cmb_area.list))
		cmf_activate(cmb_area.mem, CMF_ON);
	spin_unlock(&cmb_area.lock);
}

static int __init init_cmbe(void)
{
	cmbe_cache = kmem_cache_create("cmbe_cache", sizeof(struct cmbe),
				       __alignof__(struct cmbe), 0, NULL);

	return cmbe_cache ? 0 : -ENOMEM;
}

static int __init init_cmf(void)
{
	char *format_string;
	char *detect_string;
	int ret;

	 
	if (format == CMF_AUTODETECT) {
		if (!css_general_characteristics.ext_mb) {
			format = CMF_BASIC;
		} else {
			format = CMF_EXTENDED;
		}
		detect_string = "autodetected";
	} else {
		detect_string = "parameter";
	}

	switch (format) {
	case CMF_BASIC:
		format_string = "basic";
		cmbops = &cmbops_basic;
		break;
	case CMF_EXTENDED:
		format_string = "extended";
		cmbops = &cmbops_extended;

		ret = init_cmbe();
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}
	pr_info("Channel measurement facility initialized using format "
		"%s (mode %s)\n", format_string, detect_string);
	return 0;
}
device_initcall(init_cmf);

EXPORT_SYMBOL_GPL(enable_cmf);
EXPORT_SYMBOL_GPL(disable_cmf);
EXPORT_SYMBOL_GPL(cmf_read);
EXPORT_SYMBOL_GPL(cmf_readall);
