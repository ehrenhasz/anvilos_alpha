
 

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/seq_file.h>
#include <linux/coresight.h>
#include <linux/amba/bus.h>
#include <linux/clk.h>
#include <linux/circ_buf.h>
#include <linux/mm.h>
#include <linux/perf_event.h>


#include "coresight-priv.h"
#include "coresight-etm-perf.h"

#define ETB_RAM_DEPTH_REG	0x004
#define ETB_STATUS_REG		0x00c
#define ETB_RAM_READ_DATA_REG	0x010
#define ETB_RAM_READ_POINTER	0x014
#define ETB_RAM_WRITE_POINTER	0x018
#define ETB_TRG			0x01c
#define ETB_CTL_REG		0x020
#define ETB_RWD_REG		0x024
#define ETB_FFSR		0x300
#define ETB_FFCR		0x304
#define ETB_ITMISCOP0		0xee0
#define ETB_ITTRFLINACK		0xee4
#define ETB_ITTRFLIN		0xee8
#define ETB_ITATBDATA0		0xeeC
#define ETB_ITATBCTR2		0xef0
#define ETB_ITATBCTR1		0xef4
#define ETB_ITATBCTR0		0xef8

 
 
#define ETB_STATUS_RAM_FULL	BIT(0)
 
#define ETB_CTL_CAPT_EN		BIT(0)
 
#define ETB_FFCR_EN_FTC		BIT(0)
#define ETB_FFCR_FON_MAN	BIT(6)
#define ETB_FFCR_STOP_FI	BIT(12)
#define ETB_FFCR_STOP_TRIGGER	BIT(13)

#define ETB_FFCR_BIT		6
#define ETB_FFSR_BIT		1
#define ETB_FRAME_SIZE_WORDS	4

DEFINE_CORESIGHT_DEVLIST(etb_devs, "etb");

 
struct etb_drvdata {
	void __iomem		*base;
	struct clk		*atclk;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	spinlock_t		spinlock;
	local_t			reading;
	pid_t			pid;
	u8			*buf;
	u32			mode;
	u32			buffer_depth;
	u32			trigger_cntr;
};

static int etb_set_buffer(struct coresight_device *csdev,
			  struct perf_output_handle *handle);

static inline unsigned int etb_get_buffer_depth(struct etb_drvdata *drvdata)
{
	return readl_relaxed(drvdata->base + ETB_RAM_DEPTH_REG);
}

static void __etb_enable_hw(struct etb_drvdata *drvdata)
{
	int i;
	u32 depth;

	CS_UNLOCK(drvdata->base);

	depth = drvdata->buffer_depth;
	 
	writel_relaxed(0x0, drvdata->base + ETB_RAM_WRITE_POINTER);
	 
	for (i = 0; i < depth; i++)
		writel_relaxed(0x0, drvdata->base + ETB_RWD_REG);

	 
	writel_relaxed(0x0, drvdata->base + ETB_RAM_WRITE_POINTER);
	 
	writel_relaxed(0x0, drvdata->base + ETB_RAM_READ_POINTER);

	writel_relaxed(drvdata->trigger_cntr, drvdata->base + ETB_TRG);
	writel_relaxed(ETB_FFCR_EN_FTC | ETB_FFCR_STOP_TRIGGER,
		       drvdata->base + ETB_FFCR);
	 
	writel_relaxed(ETB_CTL_CAPT_EN, drvdata->base + ETB_CTL_REG);

	CS_LOCK(drvdata->base);
}

static int etb_enable_hw(struct etb_drvdata *drvdata)
{
	int rc = coresight_claim_device(drvdata->csdev);

	if (rc)
		return rc;

	__etb_enable_hw(drvdata);
	return 0;
}

static int etb_enable_sysfs(struct coresight_device *csdev)
{
	int ret = 0;
	unsigned long flags;
	struct etb_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock_irqsave(&drvdata->spinlock, flags);

	 
	if (drvdata->mode == CS_MODE_PERF) {
		ret = -EBUSY;
		goto out;
	}

	if (drvdata->mode == CS_MODE_DISABLED) {
		ret = etb_enable_hw(drvdata);
		if (ret)
			goto out;

		drvdata->mode = CS_MODE_SYSFS;
	}

	atomic_inc(&csdev->refcnt);
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	return ret;
}

static int etb_enable_perf(struct coresight_device *csdev, void *data)
{
	int ret = 0;
	pid_t pid;
	unsigned long flags;
	struct etb_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct perf_output_handle *handle = data;
	struct cs_buffers *buf = etm_perf_sink_config(handle);

	spin_lock_irqsave(&drvdata->spinlock, flags);

	 
	if (drvdata->mode == CS_MODE_SYSFS) {
		ret = -EBUSY;
		goto out;
	}

	 
	pid = buf->pid;

	if (drvdata->pid != -1 && drvdata->pid != pid) {
		ret = -EBUSY;
		goto out;
	}

	 
	if (drvdata->pid == pid) {
		atomic_inc(&csdev->refcnt);
		goto out;
	}

	 
	ret = etb_set_buffer(csdev, handle);
	if (ret)
		goto out;

	ret = etb_enable_hw(drvdata);
	if (!ret) {
		 
		drvdata->pid = pid;
		drvdata->mode = CS_MODE_PERF;
		atomic_inc(&csdev->refcnt);
	}

out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	return ret;
}

static int etb_enable(struct coresight_device *csdev, enum cs_mode mode,
		      void *data)
{
	int ret;

	switch (mode) {
	case CS_MODE_SYSFS:
		ret = etb_enable_sysfs(csdev);
		break;
	case CS_MODE_PERF:
		ret = etb_enable_perf(csdev, data);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret)
		return ret;

	dev_dbg(&csdev->dev, "ETB enabled\n");
	return 0;
}

static void __etb_disable_hw(struct etb_drvdata *drvdata)
{
	u32 ffcr;
	struct device *dev = &drvdata->csdev->dev;
	struct csdev_access *csa = &drvdata->csdev->access;

	CS_UNLOCK(drvdata->base);

	ffcr = readl_relaxed(drvdata->base + ETB_FFCR);
	 
	ffcr |= ETB_FFCR_STOP_FI;
	writel_relaxed(ffcr, drvdata->base + ETB_FFCR);
	 
	ffcr |= ETB_FFCR_FON_MAN;
	writel_relaxed(ffcr, drvdata->base + ETB_FFCR);

	if (coresight_timeout(csa, ETB_FFCR, ETB_FFCR_BIT, 0)) {
		dev_err(dev,
		"timeout while waiting for completion of Manual Flush\n");
	}

	 
	writel_relaxed(0x0, drvdata->base + ETB_CTL_REG);

	if (coresight_timeout(csa, ETB_FFSR, ETB_FFSR_BIT, 1)) {
		dev_err(dev,
			"timeout while waiting for Formatter to Stop\n");
	}

	CS_LOCK(drvdata->base);
}

static void etb_dump_hw(struct etb_drvdata *drvdata)
{
	bool lost = false;
	int i;
	u8 *buf_ptr;
	u32 read_data, depth;
	u32 read_ptr, write_ptr;
	u32 frame_off, frame_endoff;
	struct device *dev = &drvdata->csdev->dev;

	CS_UNLOCK(drvdata->base);

	read_ptr = readl_relaxed(drvdata->base + ETB_RAM_READ_POINTER);
	write_ptr = readl_relaxed(drvdata->base + ETB_RAM_WRITE_POINTER);

	frame_off = write_ptr % ETB_FRAME_SIZE_WORDS;
	frame_endoff = ETB_FRAME_SIZE_WORDS - frame_off;
	if (frame_off) {
		dev_err(dev,
			"write_ptr: %lu not aligned to formatter frame size\n",
			(unsigned long)write_ptr);
		dev_err(dev, "frameoff: %lu, frame_endoff: %lu\n",
			(unsigned long)frame_off, (unsigned long)frame_endoff);
		write_ptr += frame_endoff;
	}

	if ((readl_relaxed(drvdata->base + ETB_STATUS_REG)
		      & ETB_STATUS_RAM_FULL) == 0) {
		writel_relaxed(0x0, drvdata->base + ETB_RAM_READ_POINTER);
	} else {
		writel_relaxed(write_ptr, drvdata->base + ETB_RAM_READ_POINTER);
		lost = true;
	}

	depth = drvdata->buffer_depth;
	buf_ptr = drvdata->buf;
	for (i = 0; i < depth; i++) {
		read_data = readl_relaxed(drvdata->base +
					  ETB_RAM_READ_DATA_REG);
		*(u32 *)buf_ptr = read_data;
		buf_ptr += 4;
	}

	if (lost)
		coresight_insert_barrier_packet(drvdata->buf);

	if (frame_off) {
		buf_ptr -= (frame_endoff * 4);
		for (i = 0; i < frame_endoff; i++) {
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
		}
	}

	writel_relaxed(read_ptr, drvdata->base + ETB_RAM_READ_POINTER);

	CS_LOCK(drvdata->base);
}

static void etb_disable_hw(struct etb_drvdata *drvdata)
{
	__etb_disable_hw(drvdata);
	etb_dump_hw(drvdata);
	coresight_disclaim_device(drvdata->csdev);
}

static int etb_disable(struct coresight_device *csdev)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	if (atomic_dec_return(&csdev->refcnt)) {
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		return -EBUSY;
	}

	 
	WARN_ON_ONCE(drvdata->mode == CS_MODE_DISABLED);
	etb_disable_hw(drvdata);
	 
	drvdata->pid = -1;
	drvdata->mode = CS_MODE_DISABLED;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_dbg(&csdev->dev, "ETB disabled\n");
	return 0;
}

static void *etb_alloc_buffer(struct coresight_device *csdev,
			      struct perf_event *event, void **pages,
			      int nr_pages, bool overwrite)
{
	int node;
	struct cs_buffers *buf;

	node = (event->cpu == -1) ? NUMA_NO_NODE : cpu_to_node(event->cpu);

	buf = kzalloc_node(sizeof(struct cs_buffers), GFP_KERNEL, node);
	if (!buf)
		return NULL;

	buf->pid = task_pid_nr(event->owner);
	buf->snapshot = overwrite;
	buf->nr_pages = nr_pages;
	buf->data_pages = pages;

	return buf;
}

static void etb_free_buffer(void *config)
{
	struct cs_buffers *buf = config;

	kfree(buf);
}

static int etb_set_buffer(struct coresight_device *csdev,
			  struct perf_output_handle *handle)
{
	int ret = 0;
	unsigned long head;
	struct cs_buffers *buf = etm_perf_sink_config(handle);

	if (!buf)
		return -EINVAL;

	 
	head = handle->head & ((buf->nr_pages << PAGE_SHIFT) - 1);

	 
	buf->cur = head / PAGE_SIZE;

	 
	buf->offset = head % PAGE_SIZE;

	local_set(&buf->data_size, 0);

	return ret;
}

static unsigned long etb_update_buffer(struct coresight_device *csdev,
			      struct perf_output_handle *handle,
			      void *sink_config)
{
	bool lost = false;
	int i, cur;
	u8 *buf_ptr;
	const u32 *barrier;
	u32 read_ptr, write_ptr, capacity;
	u32 status, read_data;
	unsigned long offset, to_read = 0, flags;
	struct cs_buffers *buf = sink_config;
	struct etb_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	if (!buf)
		return 0;

	capacity = drvdata->buffer_depth * ETB_FRAME_SIZE_WORDS;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	 
	if (atomic_read(&csdev->refcnt) != 1)
		goto out;

	__etb_disable_hw(drvdata);
	CS_UNLOCK(drvdata->base);

	 
	read_ptr = readl_relaxed(drvdata->base + ETB_RAM_READ_POINTER);
	write_ptr = readl_relaxed(drvdata->base + ETB_RAM_WRITE_POINTER);

	 
	if (write_ptr % ETB_FRAME_SIZE_WORDS) {
		dev_err(&csdev->dev,
			"write_ptr: %lu not aligned to formatter frame size\n",
			(unsigned long)write_ptr);

		write_ptr &= ~(ETB_FRAME_SIZE_WORDS - 1);
		lost = true;
	}

	 
	status = readl_relaxed(drvdata->base + ETB_STATUS_REG);
	if (status & ETB_STATUS_RAM_FULL) {
		lost = true;
		to_read = capacity;
		read_ptr = write_ptr;
	} else {
		to_read = CIRC_CNT(write_ptr, read_ptr, drvdata->buffer_depth);
		to_read *= ETB_FRAME_SIZE_WORDS;
	}

	 
	if (!buf->snapshot && to_read > handle->size) {
		u32 mask = ~(ETB_FRAME_SIZE_WORDS - 1);

		 
		to_read = handle->size & mask;
		 
		read_ptr = (write_ptr + drvdata->buffer_depth) -
					to_read / ETB_FRAME_SIZE_WORDS;
		 
		if (read_ptr > (drvdata->buffer_depth - 1))
			read_ptr -= drvdata->buffer_depth;
		 
		lost = true;
	}

	 
	if (!buf->snapshot && lost)
		perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED);

	 
	writel_relaxed(read_ptr, drvdata->base + ETB_RAM_READ_POINTER);

	cur = buf->cur;
	offset = buf->offset;
	barrier = coresight_barrier_pkt;

	for (i = 0; i < to_read; i += 4) {
		buf_ptr = buf->data_pages[cur] + offset;
		read_data = readl_relaxed(drvdata->base +
					  ETB_RAM_READ_DATA_REG);
		if (lost && i < CORESIGHT_BARRIER_PKT_SIZE) {
			read_data = *barrier;
			barrier++;
		}

		*(u32 *)buf_ptr = read_data;
		buf_ptr += 4;

		offset += 4;
		if (offset >= PAGE_SIZE) {
			offset = 0;
			cur++;
			 
			cur &= buf->nr_pages - 1;
		}
	}

	 
	writel_relaxed(0x0, drvdata->base + ETB_RAM_READ_POINTER);
	writel_relaxed(0x0, drvdata->base + ETB_RAM_WRITE_POINTER);

	 
	if (buf->snapshot)
		handle->head += to_read;

	__etb_enable_hw(drvdata);
	CS_LOCK(drvdata->base);
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return to_read;
}

static const struct coresight_ops_sink etb_sink_ops = {
	.enable		= etb_enable,
	.disable	= etb_disable,
	.alloc_buffer	= etb_alloc_buffer,
	.free_buffer	= etb_free_buffer,
	.update_buffer	= etb_update_buffer,
};

static const struct coresight_ops etb_cs_ops = {
	.sink_ops	= &etb_sink_ops,
};

static void etb_dump(struct etb_drvdata *drvdata)
{
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->mode == CS_MODE_SYSFS) {
		__etb_disable_hw(drvdata);
		etb_dump_hw(drvdata);
		__etb_enable_hw(drvdata);
	}
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_dbg(&drvdata->csdev->dev, "ETB dumped\n");
}

static int etb_open(struct inode *inode, struct file *file)
{
	struct etb_drvdata *drvdata = container_of(file->private_data,
						   struct etb_drvdata, miscdev);

	if (local_cmpxchg(&drvdata->reading, 0, 1))
		return -EBUSY;

	dev_dbg(&drvdata->csdev->dev, "%s: successfully opened\n", __func__);
	return 0;
}

static ssize_t etb_read(struct file *file, char __user *data,
				size_t len, loff_t *ppos)
{
	u32 depth;
	struct etb_drvdata *drvdata = container_of(file->private_data,
						   struct etb_drvdata, miscdev);
	struct device *dev = &drvdata->csdev->dev;

	etb_dump(drvdata);

	depth = drvdata->buffer_depth;
	if (*ppos + len > depth * 4)
		len = depth * 4 - *ppos;

	if (copy_to_user(data, drvdata->buf + *ppos, len)) {
		dev_dbg(dev,
			"%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(dev, "%s: %zu bytes copied, %d bytes left\n",
		__func__, len, (int)(depth * 4 - *ppos));
	return len;
}

static int etb_release(struct inode *inode, struct file *file)
{
	struct etb_drvdata *drvdata = container_of(file->private_data,
						   struct etb_drvdata, miscdev);
	local_set(&drvdata->reading, 0);

	dev_dbg(&drvdata->csdev->dev, "%s: released\n", __func__);
	return 0;
}

static const struct file_operations etb_fops = {
	.owner		= THIS_MODULE,
	.open		= etb_open,
	.read		= etb_read,
	.release	= etb_release,
	.llseek		= no_llseek,
};

static struct attribute *coresight_etb_mgmt_attrs[] = {
	coresight_simple_reg32(rdp, ETB_RAM_DEPTH_REG),
	coresight_simple_reg32(sts, ETB_STATUS_REG),
	coresight_simple_reg32(rrp, ETB_RAM_READ_POINTER),
	coresight_simple_reg32(rwp, ETB_RAM_WRITE_POINTER),
	coresight_simple_reg32(trg, ETB_TRG),
	coresight_simple_reg32(ctl, ETB_CTL_REG),
	coresight_simple_reg32(ffsr, ETB_FFSR),
	coresight_simple_reg32(ffcr, ETB_FFCR),
	NULL,
};

static ssize_t trigger_cntr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->trigger_cntr;

	return sprintf(buf, "%#lx\n", val);
}

static ssize_t trigger_cntr_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct etb_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	drvdata->trigger_cntr = val;
	return size;
}
static DEVICE_ATTR_RW(trigger_cntr);

static struct attribute *coresight_etb_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	NULL,
};

static const struct attribute_group coresight_etb_group = {
	.attrs = coresight_etb_attrs,
};

static const struct attribute_group coresight_etb_mgmt_group = {
	.attrs = coresight_etb_mgmt_attrs,
	.name = "mgmt",
};

static const struct attribute_group *coresight_etb_groups[] = {
	&coresight_etb_group,
	&coresight_etb_mgmt_group,
	NULL,
};

static int etb_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret;
	void __iomem *base;
	struct device *dev = &adev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct etb_drvdata *drvdata;
	struct resource *res = &adev->res;
	struct coresight_desc desc = { 0 };

	desc.name = coresight_alloc_device_name(&etb_devs, dev);
	if (!desc.name)
		return -ENOMEM;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->atclk = devm_clk_get(&adev->dev, "atclk");  
	if (!IS_ERR(drvdata->atclk)) {
		ret = clk_prepare_enable(drvdata->atclk);
		if (ret)
			return ret;
	}
	dev_set_drvdata(dev, drvdata);

	 
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	drvdata->base = base;
	desc.access = CSDEV_ACCESS_IOMEM(base);

	spin_lock_init(&drvdata->spinlock);

	drvdata->buffer_depth = etb_get_buffer_depth(drvdata);

	if (drvdata->buffer_depth & 0x80000000)
		return -EINVAL;

	drvdata->buf = devm_kcalloc(dev,
				    drvdata->buffer_depth, 4, GFP_KERNEL);
	if (!drvdata->buf)
		return -ENOMEM;

	 
	drvdata->pid = -1;

	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	adev->dev.platform_data = pdata;

	desc.type = CORESIGHT_DEV_TYPE_SINK;
	desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
	desc.ops = &etb_cs_ops;
	desc.pdata = pdata;
	desc.dev = dev;
	desc.groups = coresight_etb_groups;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	drvdata->miscdev.name = desc.name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &etb_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		goto err_misc_register;

	pm_runtime_put(&adev->dev);
	return 0;

err_misc_register:
	coresight_unregister(drvdata->csdev);
	return ret;
}

static void etb_remove(struct amba_device *adev)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(&adev->dev);

	 
	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
}

#ifdef CONFIG_PM
static int etb_runtime_suspend(struct device *dev)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);

	return 0;
}

static int etb_runtime_resume(struct device *dev)
{
	struct etb_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_prepare_enable(drvdata->atclk);

	return 0;
}
#endif

static const struct dev_pm_ops etb_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(etb_runtime_suspend, etb_runtime_resume, NULL)
};

static const struct amba_id etb_ids[] = {
	{
		.id	= 0x000bb907,
		.mask	= 0x000fffff,
	},
	{ 0, 0},
};

MODULE_DEVICE_TABLE(amba, etb_ids);

static struct amba_driver etb_driver = {
	.drv = {
		.name	= "coresight-etb10",
		.owner	= THIS_MODULE,
		.pm	= &etb_dev_pm_ops,
		.suppress_bind_attrs = true,

	},
	.probe		= etb_probe,
	.remove		= etb_remove,
	.id_table	= etb_ids,
};

module_amba_driver(etb_driver);

MODULE_AUTHOR("Pratik Patel <pratikp@codeaurora.org>");
MODULE_AUTHOR("Mathieu Poirier <mathieu.poirier@linaro.org>");
MODULE_DESCRIPTION("Arm CoreSight Embedded Trace Buffer driver");
MODULE_LICENSE("GPL v2");
