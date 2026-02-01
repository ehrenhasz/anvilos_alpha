
 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <asm/intel_scu_ipc.h>

 
#define IPCMSG_PCNTRL         0xff  

 
#define IPC_CMD_PCNTRL_W      0  
#define IPC_CMD_PCNTRL_R      1  
#define IPC_CMD_PCNTRL_M      2  

 

#define IPC_WWBUF_SIZE    20		 
#define IPC_RWBUF_SIZE    20		 
#define IPC_IOC	          0x100		 

struct intel_scu_ipc_dev {
	struct device dev;
	struct resource mem;
	struct module *owner;
	int irq;
	void __iomem *ipc_base;
	struct completion cmd_complete;
};

#define IPC_STATUS		0x04
#define IPC_STATUS_IRQ		BIT(2)
#define IPC_STATUS_ERR		BIT(1)
#define IPC_STATUS_BUSY		BIT(0)

 
#define IPC_WRITE_BUFFER	0x80
#define IPC_READ_BUFFER		0x90

 
#define IPC_TIMEOUT		(10 * HZ)

static struct intel_scu_ipc_dev *ipcdev;  
static DEFINE_MUTEX(ipclock);  

static struct class intel_scu_ipc_class = {
	.name = "intel_scu_ipc",
};

 
struct intel_scu_ipc_dev *intel_scu_ipc_dev_get(void)
{
	struct intel_scu_ipc_dev *scu = NULL;

	mutex_lock(&ipclock);
	if (ipcdev) {
		get_device(&ipcdev->dev);
		 
		if (!try_module_get(ipcdev->owner))
			put_device(&ipcdev->dev);
		else
			scu = ipcdev;
	}

	mutex_unlock(&ipclock);
	return scu;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_dev_get);

 
void intel_scu_ipc_dev_put(struct intel_scu_ipc_dev *scu)
{
	if (scu) {
		module_put(scu->owner);
		put_device(&scu->dev);
	}
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_dev_put);

struct intel_scu_ipc_devres {
	struct intel_scu_ipc_dev *scu;
};

static void devm_intel_scu_ipc_dev_release(struct device *dev, void *res)
{
	struct intel_scu_ipc_devres *dr = res;
	struct intel_scu_ipc_dev *scu = dr->scu;

	intel_scu_ipc_dev_put(scu);
}

 
struct intel_scu_ipc_dev *devm_intel_scu_ipc_dev_get(struct device *dev)
{
	struct intel_scu_ipc_devres *dr;
	struct intel_scu_ipc_dev *scu;

	dr = devres_alloc(devm_intel_scu_ipc_dev_release, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return NULL;

	scu = intel_scu_ipc_dev_get();
	if (!scu) {
		devres_free(dr);
		return NULL;
	}

	dr->scu = scu;
	devres_add(dev, dr);

	return scu;
}
EXPORT_SYMBOL_GPL(devm_intel_scu_ipc_dev_get);

 
static inline void ipc_command(struct intel_scu_ipc_dev *scu, u32 cmd)
{
	reinit_completion(&scu->cmd_complete);
	writel(cmd | IPC_IOC, scu->ipc_base);
}

 
static inline void ipc_data_writel(struct intel_scu_ipc_dev *scu, u32 data, u32 offset)
{
	writel(data, scu->ipc_base + IPC_WRITE_BUFFER + offset);
}

 
static inline u8 ipc_read_status(struct intel_scu_ipc_dev *scu)
{
	return __raw_readl(scu->ipc_base + IPC_STATUS);
}

 
static inline u8 ipc_data_readb(struct intel_scu_ipc_dev *scu, u32 offset)
{
	return readb(scu->ipc_base + IPC_READ_BUFFER + offset);
}

 
static inline u32 ipc_data_readl(struct intel_scu_ipc_dev *scu, u32 offset)
{
	return readl(scu->ipc_base + IPC_READ_BUFFER + offset);
}

 
static inline int busy_loop(struct intel_scu_ipc_dev *scu)
{
	u8 status;
	int err;

	err = readx_poll_timeout(ipc_read_status, scu, status, !(status & IPC_STATUS_BUSY),
				 100, jiffies_to_usecs(IPC_TIMEOUT));
	if (err)
		return err;

	return (status & IPC_STATUS_ERR) ? -EIO : 0;
}

 
static inline int ipc_wait_for_interrupt(struct intel_scu_ipc_dev *scu)
{
	int status;

	wait_for_completion_timeout(&scu->cmd_complete, IPC_TIMEOUT);

	status = ipc_read_status(scu);
	if (status & IPC_STATUS_BUSY)
		return -ETIMEDOUT;

	if (status & IPC_STATUS_ERR)
		return -EIO;

	return 0;
}

static int intel_scu_ipc_check_status(struct intel_scu_ipc_dev *scu)
{
	return scu->irq > 0 ? ipc_wait_for_interrupt(scu) : busy_loop(scu);
}

static struct intel_scu_ipc_dev *intel_scu_ipc_get(struct intel_scu_ipc_dev *scu)
{
	u8 status;

	if (!scu)
		scu = ipcdev;
	if (!scu)
		return ERR_PTR(-ENODEV);

	status = ipc_read_status(scu);
	if (status & IPC_STATUS_BUSY) {
		dev_dbg(&scu->dev, "device is busy\n");
		return ERR_PTR(-EBUSY);
	}

	return scu;
}

 
static int pwr_reg_rdwr(struct intel_scu_ipc_dev *scu, u16 *addr, u8 *data,
			u32 count, u32 op, u32 id)
{
	int nc;
	u32 offset = 0;
	int err;
	u8 cbuf[IPC_WWBUF_SIZE];
	u32 *wbuf = (u32 *)&cbuf;

	memset(cbuf, 0, sizeof(cbuf));

	mutex_lock(&ipclock);
	scu = intel_scu_ipc_get(scu);
	if (IS_ERR(scu)) {
		mutex_unlock(&ipclock);
		return PTR_ERR(scu);
	}

	for (nc = 0; nc < count; nc++, offset += 2) {
		cbuf[offset] = addr[nc];
		cbuf[offset + 1] = addr[nc] >> 8;
	}

	if (id == IPC_CMD_PCNTRL_R) {
		for (nc = 0, offset = 0; nc < count; nc++, offset += 4)
			ipc_data_writel(scu, wbuf[nc], offset);
		ipc_command(scu, (count * 2) << 16 | id << 12 | 0 << 8 | op);
	} else if (id == IPC_CMD_PCNTRL_W) {
		for (nc = 0; nc < count; nc++, offset += 1)
			cbuf[offset] = data[nc];
		for (nc = 0, offset = 0; nc < count; nc++, offset += 4)
			ipc_data_writel(scu, wbuf[nc], offset);
		ipc_command(scu, (count * 3) << 16 | id << 12 | 0 << 8 | op);
	} else if (id == IPC_CMD_PCNTRL_M) {
		cbuf[offset] = data[0];
		cbuf[offset + 1] = data[1];
		ipc_data_writel(scu, wbuf[0], 0);  
		ipc_command(scu, 4 << 16 | id << 12 | 0 << 8 | op);
	}

	err = intel_scu_ipc_check_status(scu);
	if (!err && id == IPC_CMD_PCNTRL_R) {  
		 
		memcpy_fromio(cbuf, scu->ipc_base + 0x90, 16);
		for (nc = 0; nc < count; nc++)
			data[nc] = ipc_data_readb(scu, nc);
	}
	mutex_unlock(&ipclock);
	return err;
}

 
int intel_scu_ipc_dev_ioread8(struct intel_scu_ipc_dev *scu, u16 addr, u8 *data)
{
	return pwr_reg_rdwr(scu, &addr, data, 1, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_R);
}
EXPORT_SYMBOL(intel_scu_ipc_dev_ioread8);

 
int intel_scu_ipc_dev_iowrite8(struct intel_scu_ipc_dev *scu, u16 addr, u8 data)
{
	return pwr_reg_rdwr(scu, &addr, &data, 1, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_W);
}
EXPORT_SYMBOL(intel_scu_ipc_dev_iowrite8);

 
int intel_scu_ipc_dev_readv(struct intel_scu_ipc_dev *scu, u16 *addr, u8 *data,
			    size_t len)
{
	return pwr_reg_rdwr(scu, addr, data, len, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_R);
}
EXPORT_SYMBOL(intel_scu_ipc_dev_readv);

 
int intel_scu_ipc_dev_writev(struct intel_scu_ipc_dev *scu, u16 *addr, u8 *data,
			     size_t len)
{
	return pwr_reg_rdwr(scu, addr, data, len, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_W);
}
EXPORT_SYMBOL(intel_scu_ipc_dev_writev);

 
int intel_scu_ipc_dev_update(struct intel_scu_ipc_dev *scu, u16 addr, u8 data,
			     u8 mask)
{
	u8 tmp[2] = { data, mask };
	return pwr_reg_rdwr(scu, &addr, tmp, 1, IPCMSG_PCNTRL, IPC_CMD_PCNTRL_M);
}
EXPORT_SYMBOL(intel_scu_ipc_dev_update);

 
int intel_scu_ipc_dev_simple_command(struct intel_scu_ipc_dev *scu, int cmd,
				     int sub)
{
	u32 cmdval;
	int err;

	mutex_lock(&ipclock);
	scu = intel_scu_ipc_get(scu);
	if (IS_ERR(scu)) {
		mutex_unlock(&ipclock);
		return PTR_ERR(scu);
	}

	cmdval = sub << 12 | cmd;
	ipc_command(scu, cmdval);
	err = intel_scu_ipc_check_status(scu);
	mutex_unlock(&ipclock);
	if (err)
		dev_err(&scu->dev, "IPC command %#x failed with %d\n", cmdval, err);
	return err;
}
EXPORT_SYMBOL(intel_scu_ipc_dev_simple_command);

 
int intel_scu_ipc_dev_command_with_size(struct intel_scu_ipc_dev *scu, int cmd,
					int sub, const void *in, size_t inlen,
					size_t size, void *out, size_t outlen)
{
	size_t outbuflen = DIV_ROUND_UP(outlen, sizeof(u32));
	size_t inbuflen = DIV_ROUND_UP(inlen, sizeof(u32));
	u32 cmdval, inbuf[4] = {};
	int i, err;

	if (inbuflen > 4 || outbuflen > 4)
		return -EINVAL;

	mutex_lock(&ipclock);
	scu = intel_scu_ipc_get(scu);
	if (IS_ERR(scu)) {
		mutex_unlock(&ipclock);
		return PTR_ERR(scu);
	}

	memcpy(inbuf, in, inlen);
	for (i = 0; i < inbuflen; i++)
		ipc_data_writel(scu, inbuf[i], 4 * i);

	cmdval = (size << 16) | (sub << 12) | cmd;
	ipc_command(scu, cmdval);
	err = intel_scu_ipc_check_status(scu);

	if (!err) {
		u32 outbuf[4] = {};

		for (i = 0; i < outbuflen; i++)
			outbuf[i] = ipc_data_readl(scu, 4 * i);

		memcpy(out, outbuf, outlen);
	}

	mutex_unlock(&ipclock);
	if (err)
		dev_err(&scu->dev, "IPC command %#x failed with %d\n", cmdval, err);
	return err;
}
EXPORT_SYMBOL(intel_scu_ipc_dev_command_with_size);

 
static irqreturn_t ioc(int irq, void *dev_id)
{
	struct intel_scu_ipc_dev *scu = dev_id;
	int status = ipc_read_status(scu);

	writel(status | IPC_STATUS_IRQ, scu->ipc_base + IPC_STATUS);
	complete(&scu->cmd_complete);

	return IRQ_HANDLED;
}

static void intel_scu_ipc_release(struct device *dev)
{
	struct intel_scu_ipc_dev *scu;

	scu = container_of(dev, struct intel_scu_ipc_dev, dev);
	if (scu->irq > 0)
		free_irq(scu->irq, scu);
	iounmap(scu->ipc_base);
	release_mem_region(scu->mem.start, resource_size(&scu->mem));
	kfree(scu);
}

 
struct intel_scu_ipc_dev *
__intel_scu_ipc_register(struct device *parent,
			 const struct intel_scu_ipc_data *scu_data,
			 struct module *owner)
{
	int err;
	struct intel_scu_ipc_dev *scu;
	void __iomem *ipc_base;

	mutex_lock(&ipclock);
	 
	if (ipcdev) {
		err = -EBUSY;
		goto err_unlock;
	}

	scu = kzalloc(sizeof(*scu), GFP_KERNEL);
	if (!scu) {
		err = -ENOMEM;
		goto err_unlock;
	}

	scu->owner = owner;
	scu->dev.parent = parent;
	scu->dev.class = &intel_scu_ipc_class;
	scu->dev.release = intel_scu_ipc_release;

	if (!request_mem_region(scu_data->mem.start, resource_size(&scu_data->mem),
				"intel_scu_ipc")) {
		err = -EBUSY;
		goto err_free;
	}

	ipc_base = ioremap(scu_data->mem.start, resource_size(&scu_data->mem));
	if (!ipc_base) {
		err = -ENOMEM;
		goto err_release;
	}

	scu->ipc_base = ipc_base;
	scu->mem = scu_data->mem;
	scu->irq = scu_data->irq;
	init_completion(&scu->cmd_complete);

	if (scu->irq > 0) {
		err = request_irq(scu->irq, ioc, 0, "intel_scu_ipc", scu);
		if (err)
			goto err_unmap;
	}

	 
	dev_set_name(&scu->dev, "intel_scu_ipc");
	err = device_register(&scu->dev);
	if (err) {
		put_device(&scu->dev);
		goto err_unlock;
	}

	 
	ipcdev = scu;
	mutex_unlock(&ipclock);

	return scu;

err_unmap:
	iounmap(ipc_base);
err_release:
	release_mem_region(scu_data->mem.start, resource_size(&scu_data->mem));
err_free:
	kfree(scu);
err_unlock:
	mutex_unlock(&ipclock);

	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(__intel_scu_ipc_register);

 
void intel_scu_ipc_unregister(struct intel_scu_ipc_dev *scu)
{
	mutex_lock(&ipclock);
	if (!WARN_ON(!ipcdev)) {
		ipcdev = NULL;
		device_unregister(&scu->dev);
	}
	mutex_unlock(&ipclock);
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_unregister);

static void devm_intel_scu_ipc_unregister(struct device *dev, void *res)
{
	struct intel_scu_ipc_devres *dr = res;
	struct intel_scu_ipc_dev *scu = dr->scu;

	intel_scu_ipc_unregister(scu);
}

 
struct intel_scu_ipc_dev *
__devm_intel_scu_ipc_register(struct device *parent,
			      const struct intel_scu_ipc_data *scu_data,
			      struct module *owner)
{
	struct intel_scu_ipc_devres *dr;
	struct intel_scu_ipc_dev *scu;

	dr = devres_alloc(devm_intel_scu_ipc_unregister, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return NULL;

	scu = __intel_scu_ipc_register(parent, scu_data, owner);
	if (IS_ERR(scu)) {
		devres_free(dr);
		return scu;
	}

	dr->scu = scu;
	devres_add(parent, dr);

	return scu;
}
EXPORT_SYMBOL_GPL(__devm_intel_scu_ipc_register);

static int __init intel_scu_ipc_init(void)
{
	return class_register(&intel_scu_ipc_class);
}
subsys_initcall(intel_scu_ipc_init);

static void __exit intel_scu_ipc_exit(void)
{
	class_unregister(&intel_scu_ipc_class);
}
module_exit(intel_scu_ipc_exit);
