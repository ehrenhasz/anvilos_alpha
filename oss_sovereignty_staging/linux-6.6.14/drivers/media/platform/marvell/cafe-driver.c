
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/i2c/ov7670.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clkdev.h>

#include "mcam-core.h"

#define CAFE_VERSION 0x000002


 
MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("Marvell 88ALP01 CMOS Camera Controller driver");
MODULE_LICENSE("GPL");

struct cafe_camera {
	int registered;			 
	struct mcam_camera mcam;
	struct pci_dev *pdev;
	struct i2c_adapter *i2c_adapter;
	wait_queue_head_t smbus_wait;	 
};

 

 
#define REG_GPR		0xb4
#define	  GPR_C1EN	  0x00000020	 
#define	  GPR_C0EN	  0x00000010	 
#define	  GPR_C1	  0x00000002	 
 
#define	  GPR_C0	  0x00000001	 

 
#define REG_TWSIC0	0xb8	 
#define	  TWSIC0_EN	  0x00000001	 
#define	  TWSIC0_MODE	  0x00000002	 
#define	  TWSIC0_SID	  0x000003fc	 
 
#define	  TWSIC0_SID_SHIFT 3
#define	  TWSIC0_CLKDIV	  0x0007fc00	 
#define	  TWSIC0_MASKACK  0x00400000	 
#define	  TWSIC0_OVMAGIC  0x00800000	 

#define REG_TWSIC1	0xbc	 
#define	  TWSIC1_DATA	  0x0000ffff	 
#define	  TWSIC1_ADDR	  0x00ff0000	 
#define	  TWSIC1_ADDR_SHIFT 16
#define	  TWSIC1_READ	  0x01000000	 
#define	  TWSIC1_WSTAT	  0x02000000	 
#define	  TWSIC1_RVALID	  0x04000000	 
#define	  TWSIC1_ERROR	  0x08000000	 

 
#define REG_GL_CSR     0x3004   
#define	  GCSR_SRS	 0x00000001	 
#define	  GCSR_SRC	 0x00000002	 
#define	  GCSR_MRS	 0x00000004	 
#define	  GCSR_MRC	 0x00000008	 
#define	  GCSR_CCIC_EN	 0x00004000     
#define REG_GL_IMASK   0x300c   
#define	  GIMSK_CCIC_EN		 0x00000004     

#define REG_GL_FCR	0x3038	 
#define	  GFCR_GPIO_ON	  0x08		 
#define REG_GL_GPIOR	0x315c	 
#define	  GGPIO_OUT		0x80000	 
#define	  GGPIO_VAL		0x00008	 

#define REG_LEN		       (REG_GL_IMASK + 4)


 
#define cam_err(cam, fmt, arg...) \
	dev_err(&(cam)->pdev->dev, fmt, ##arg);
#define cam_warn(cam, fmt, arg...) \
	dev_warn(&(cam)->pdev->dev, fmt, ##arg);

 
 
#define CAFE_SMBUS_TIMEOUT (HZ)   

static int cafe_smbus_write_done(struct mcam_camera *mcam)
{
	unsigned long flags;
	int c1;

	 
	udelay(20);
	spin_lock_irqsave(&mcam->dev_lock, flags);
	c1 = mcam_reg_read(mcam, REG_TWSIC1);
	spin_unlock_irqrestore(&mcam->dev_lock, flags);
	return (c1 & (TWSIC1_WSTAT|TWSIC1_ERROR)) != TWSIC1_WSTAT;
}

static int cafe_smbus_write_data(struct cafe_camera *cam,
		u16 addr, u8 command, u8 value)
{
	unsigned int rval;
	unsigned long flags;
	struct mcam_camera *mcam = &cam->mcam;

	spin_lock_irqsave(&mcam->dev_lock, flags);
	rval = TWSIC0_EN | ((addr << TWSIC0_SID_SHIFT) & TWSIC0_SID);
	rval |= TWSIC0_OVMAGIC;   
	 
	rval |= TWSIC0_CLKDIV;
	mcam_reg_write(mcam, REG_TWSIC0, rval);
	(void) mcam_reg_read(mcam, REG_TWSIC1);  
	rval = value | ((command << TWSIC1_ADDR_SHIFT) & TWSIC1_ADDR);
	mcam_reg_write(mcam, REG_TWSIC1, rval);
	spin_unlock_irqrestore(&mcam->dev_lock, flags);

	 
	mdelay(2);

	 
	wait_event_timeout(cam->smbus_wait, cafe_smbus_write_done(mcam),
			CAFE_SMBUS_TIMEOUT);

	spin_lock_irqsave(&mcam->dev_lock, flags);
	rval = mcam_reg_read(mcam, REG_TWSIC1);
	spin_unlock_irqrestore(&mcam->dev_lock, flags);

	if (rval & TWSIC1_WSTAT) {
		cam_err(cam, "SMBUS write (%02x/%02x/%02x) timed out\n", addr,
				command, value);
		return -EIO;
	}
	if (rval & TWSIC1_ERROR) {
		cam_err(cam, "SMBUS write (%02x/%02x/%02x) error\n", addr,
				command, value);
		return -EIO;
	}
	return 0;
}



static int cafe_smbus_read_done(struct mcam_camera *mcam)
{
	unsigned long flags;
	int c1;

	 
	udelay(20);
	spin_lock_irqsave(&mcam->dev_lock, flags);
	c1 = mcam_reg_read(mcam, REG_TWSIC1);
	spin_unlock_irqrestore(&mcam->dev_lock, flags);
	return c1 & (TWSIC1_RVALID|TWSIC1_ERROR);
}



static int cafe_smbus_read_data(struct cafe_camera *cam,
		u16 addr, u8 command, u8 *value)
{
	unsigned int rval;
	unsigned long flags;
	struct mcam_camera *mcam = &cam->mcam;

	spin_lock_irqsave(&mcam->dev_lock, flags);
	rval = TWSIC0_EN | ((addr << TWSIC0_SID_SHIFT) & TWSIC0_SID);
	rval |= TWSIC0_OVMAGIC;  
	 
	rval |= TWSIC0_CLKDIV;
	mcam_reg_write(mcam, REG_TWSIC0, rval);
	(void) mcam_reg_read(mcam, REG_TWSIC1);  
	rval = TWSIC1_READ | ((command << TWSIC1_ADDR_SHIFT) & TWSIC1_ADDR);
	mcam_reg_write(mcam, REG_TWSIC1, rval);
	spin_unlock_irqrestore(&mcam->dev_lock, flags);

	wait_event_timeout(cam->smbus_wait,
			cafe_smbus_read_done(mcam), CAFE_SMBUS_TIMEOUT);
	spin_lock_irqsave(&mcam->dev_lock, flags);
	rval = mcam_reg_read(mcam, REG_TWSIC1);
	spin_unlock_irqrestore(&mcam->dev_lock, flags);

	if (rval & TWSIC1_ERROR) {
		cam_err(cam, "SMBUS read (%02x/%02x) error\n", addr, command);
		return -EIO;
	}
	if (!(rval & TWSIC1_RVALID)) {
		cam_err(cam, "SMBUS read (%02x/%02x) timed out\n", addr,
				command);
		return -EIO;
	}
	*value = rval & 0xff;
	return 0;
}

 
static int cafe_smbus_xfer(struct i2c_adapter *adapter, u16 addr,
		unsigned short flags, char rw, u8 command,
		int size, union i2c_smbus_data *data)
{
	struct cafe_camera *cam = i2c_get_adapdata(adapter);
	int ret = -EINVAL;

	 
	if (size != I2C_SMBUS_BYTE_DATA) {
		cam_err(cam, "funky xfer size %d\n", size);
		return -EINVAL;
	}

	if (rw == I2C_SMBUS_WRITE)
		ret = cafe_smbus_write_data(cam, addr, command, data->byte);
	else if (rw == I2C_SMBUS_READ)
		ret = cafe_smbus_read_data(cam, addr, command, &data->byte);
	return ret;
}


static void cafe_smbus_enable_irq(struct cafe_camera *cam)
{
	unsigned long flags;

	spin_lock_irqsave(&cam->mcam.dev_lock, flags);
	mcam_reg_set_bit(&cam->mcam, REG_IRQMASK, TWSIIRQS);
	spin_unlock_irqrestore(&cam->mcam.dev_lock, flags);
}

static u32 cafe_smbus_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_READ_BYTE_DATA  |
	       I2C_FUNC_SMBUS_WRITE_BYTE_DATA;
}

static const struct i2c_algorithm cafe_smbus_algo = {
	.smbus_xfer = cafe_smbus_xfer,
	.functionality = cafe_smbus_func
};

static int cafe_smbus_setup(struct cafe_camera *cam)
{
	struct i2c_adapter *adap;
	int ret;

	adap = kzalloc(sizeof(*adap), GFP_KERNEL);
	if (adap == NULL)
		return -ENOMEM;
	adap->owner = THIS_MODULE;
	adap->algo = &cafe_smbus_algo;
	strscpy(adap->name, "cafe_ccic", sizeof(adap->name));
	adap->dev.parent = &cam->pdev->dev;
	i2c_set_adapdata(adap, cam);
	ret = i2c_add_adapter(adap);
	if (ret) {
		printk(KERN_ERR "Unable to register cafe i2c adapter\n");
		kfree(adap);
		return ret;
	}

	cam->i2c_adapter = adap;
	cafe_smbus_enable_irq(cam);
	return 0;
}

static void cafe_smbus_shutdown(struct cafe_camera *cam)
{
	i2c_del_adapter(cam->i2c_adapter);
	kfree(cam->i2c_adapter);
}


 

static void cafe_ctlr_init(struct mcam_camera *mcam)
{
	unsigned long flags;

	spin_lock_irqsave(&mcam->dev_lock, flags);
	 
	mcam_reg_write(mcam, 0x3038, 0x8);
	mcam_reg_write(mcam, 0x315c, 0x80008);
	 
	mcam_reg_write(mcam, REG_GL_CSR, GCSR_SRS|GCSR_MRS);  
	mcam_reg_write(mcam, REG_GL_CSR, GCSR_SRC|GCSR_MRC);
	mcam_reg_write(mcam, REG_GL_CSR, GCSR_SRC|GCSR_MRS);
	 
	spin_unlock_irqrestore(&mcam->dev_lock, flags);
	msleep(5);
	spin_lock_irqsave(&mcam->dev_lock, flags);

	mcam_reg_write(mcam, REG_GL_CSR, GCSR_CCIC_EN|GCSR_SRC|GCSR_MRC);
	mcam_reg_set_bit(mcam, REG_GL_IMASK, GIMSK_CCIC_EN);
	 
	mcam_reg_write(mcam, REG_IRQMASK, 0);
	spin_unlock_irqrestore(&mcam->dev_lock, flags);
}


static int cafe_ctlr_power_up(struct mcam_camera *mcam)
{
	 
	mcam_reg_write(mcam, REG_GL_FCR, GFCR_GPIO_ON);
	mcam_reg_write(mcam, REG_GL_GPIOR, GGPIO_OUT|GGPIO_VAL);
	 
	mcam_reg_write(mcam, REG_GPR, GPR_C1EN|GPR_C0EN);  
	mcam_reg_write(mcam, REG_GPR, GPR_C1EN|GPR_C0EN|GPR_C0);

	return 0;
}

static void cafe_ctlr_power_down(struct mcam_camera *mcam)
{
	mcam_reg_write(mcam, REG_GPR, GPR_C1EN|GPR_C0EN|GPR_C1);
	mcam_reg_write(mcam, REG_GL_FCR, GFCR_GPIO_ON);
	mcam_reg_write(mcam, REG_GL_GPIOR, GGPIO_OUT);
}



 
static irqreturn_t cafe_irq(int irq, void *data)
{
	struct cafe_camera *cam = data;
	struct mcam_camera *mcam = &cam->mcam;
	unsigned int irqs, handled;

	spin_lock(&mcam->dev_lock);
	irqs = mcam_reg_read(mcam, REG_IRQSTAT);
	handled = cam->registered && mccic_irq(mcam, irqs);
	if (irqs & TWSIIRQS) {
		mcam_reg_write(mcam, REG_IRQSTAT, TWSIIRQS);
		wake_up(&cam->smbus_wait);
		handled = 1;
	}
	spin_unlock(&mcam->dev_lock);
	return IRQ_RETVAL(handled);
}

 

static struct ov7670_config sensor_cfg = {
	 
	.min_width = 320,
	.min_height = 240,

	 
	.clock_speed = 45,
	.use_smbus = 1,
};

static struct i2c_board_info ov7670_info = {
	.type = "ov7670",
	.addr = 0x42 >> 1,
	.platform_data = &sensor_cfg,
};

 
 

static int cafe_pci_probe(struct pci_dev *pdev,
		const struct pci_device_id *id)
{
	int ret;
	struct cafe_camera *cam;
	struct mcam_camera *mcam;
	struct v4l2_async_connection *asd;
	struct i2c_client *i2c_dev;

	 
	ret = -ENOMEM;
	cam = kzalloc(sizeof(struct cafe_camera), GFP_KERNEL);
	if (cam == NULL)
		goto out;
	pci_set_drvdata(pdev, cam);
	cam->pdev = pdev;
	mcam = &cam->mcam;
	mcam->chip_id = MCAM_CAFE;
	spin_lock_init(&mcam->dev_lock);
	init_waitqueue_head(&cam->smbus_wait);
	mcam->plat_power_up = cafe_ctlr_power_up;
	mcam->plat_power_down = cafe_ctlr_power_down;
	mcam->dev = &pdev->dev;
	 
	mcam->buffer_mode = B_vmalloc;
	 
	ret = pci_enable_device(pdev);
	if (ret)
		goto out_free;
	pci_set_master(pdev);

	ret = -EIO;
	mcam->regs = pci_iomap(pdev, 0, 0);
	if (!mcam->regs) {
		printk(KERN_ERR "Unable to ioremap cafe-ccic regs\n");
		goto out_disable;
	}
	mcam->regs_size = pci_resource_len(pdev, 0);
	ret = request_irq(pdev->irq, cafe_irq, IRQF_SHARED, "cafe-ccic", cam);
	if (ret)
		goto out_iounmap;

	 
	cafe_ctlr_init(mcam);

	 
	ret = cafe_smbus_setup(cam);
	if (ret)
		goto out_pdown;

	ret = v4l2_device_register(mcam->dev, &mcam->v4l2_dev);
	if (ret)
		goto out_smbus_shutdown;

	v4l2_async_nf_init(&mcam->notifier, &mcam->v4l2_dev);

	asd = v4l2_async_nf_add_i2c(&mcam->notifier,
				    i2c_adapter_id(cam->i2c_adapter),
				    ov7670_info.addr,
				    struct v4l2_async_connection);
	if (IS_ERR(asd)) {
		ret = PTR_ERR(asd);
		goto out_v4l2_device_unregister;
	}

	ret = mccic_register(mcam);
	if (ret)
		goto out_v4l2_device_unregister;

	clkdev_create(mcam->mclk, "xclk", "%d-%04x",
		i2c_adapter_id(cam->i2c_adapter), ov7670_info.addr);

	i2c_dev = i2c_new_client_device(cam->i2c_adapter, &ov7670_info);
	if (IS_ERR(i2c_dev)) {
		ret = PTR_ERR(i2c_dev);
		goto out_mccic_shutdown;
	}

	cam->registered = 1;
	return 0;

out_mccic_shutdown:
	mccic_shutdown(mcam);
out_v4l2_device_unregister:
	v4l2_device_unregister(&mcam->v4l2_dev);
out_smbus_shutdown:
	cafe_smbus_shutdown(cam);
out_pdown:
	cafe_ctlr_power_down(mcam);
	free_irq(pdev->irq, cam);
out_iounmap:
	pci_iounmap(pdev, mcam->regs);
out_disable:
	pci_disable_device(pdev);
out_free:
	kfree(cam);
out:
	return ret;
}


 
static void cafe_shutdown(struct cafe_camera *cam)
{
	mccic_shutdown(&cam->mcam);
	v4l2_device_unregister(&cam->mcam.v4l2_dev);
	cafe_smbus_shutdown(cam);
	free_irq(cam->pdev->irq, cam);
	pci_iounmap(cam->pdev, cam->mcam.regs);
}


static void cafe_pci_remove(struct pci_dev *pdev)
{
	struct cafe_camera *cam = pci_get_drvdata(pdev);

	if (cam == NULL) {
		printk(KERN_WARNING "pci_remove on unknown pdev %p\n", pdev);
		return;
	}
	cafe_shutdown(cam);
	kfree(cam);
}


 
static int __maybe_unused cafe_pci_suspend(struct device *dev)
{
	struct cafe_camera *cam = dev_get_drvdata(dev);

	mccic_suspend(&cam->mcam);
	return 0;
}


static int __maybe_unused cafe_pci_resume(struct device *dev)
{
	struct cafe_camera *cam = dev_get_drvdata(dev);

	cafe_ctlr_init(&cam->mcam);
	return mccic_resume(&cam->mcam);
}

static const struct pci_device_id cafe_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL,
		     PCI_DEVICE_ID_MARVELL_88ALP01_CCIC) },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, cafe_ids);

static SIMPLE_DEV_PM_OPS(cafe_pci_pm_ops, cafe_pci_suspend, cafe_pci_resume);

static struct pci_driver cafe_pci_driver = {
	.name = "cafe1000-ccic",
	.id_table = cafe_ids,
	.probe = cafe_pci_probe,
	.remove = cafe_pci_remove,
	.driver.pm = &cafe_pci_pm_ops,
};




static int __init cafe_init(void)
{
	int ret;

	printk(KERN_NOTICE "Marvell M88ALP01 'CAFE' Camera Controller version %d\n",
			CAFE_VERSION);
	ret = pci_register_driver(&cafe_pci_driver);
	if (ret) {
		printk(KERN_ERR "Unable to register cafe_ccic driver\n");
		goto out;
	}
	ret = 0;

out:
	return ret;
}


static void __exit cafe_exit(void)
{
	pci_unregister_driver(&cafe_pci_driver);
}

module_init(cafe_init);
module_exit(cafe_exit);
