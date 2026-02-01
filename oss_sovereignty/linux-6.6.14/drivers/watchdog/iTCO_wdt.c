
 

 

 
#define DRV_NAME	"iTCO_wdt"
#define DRV_VERSION	"1.11"

 
#include <linux/acpi.h>			 
#include <linux/bits.h>			 
#include <linux/module.h>		 
#include <linux/moduleparam.h>		 
#include <linux/types.h>		 
#include <linux/errno.h>		 
#include <linux/kernel.h>		 
#include <linux/watchdog.h>		 
#include <linux/init.h>			 
#include <linux/fs.h>			 
#include <linux/platform_device.h>	 
#include <linux/pci.h>			 
#include <linux/ioport.h>		 
#include <linux/spinlock.h>		 
#include <linux/uaccess.h>		 
#include <linux/io.h>			 
#include <linux/platform_data/itco_wdt.h>
#include <linux/mfd/intel_pmc_bxt.h>

#include "iTCO_vendor.h"

 
 
#define TCOBASE(p)	((p)->tco_res->start)
 
#define SMI_EN(p)	((p)->smi_res->start)

#define TCO_RLD(p)	(TCOBASE(p) + 0x00)  
#define TCOv1_TMR(p)	(TCOBASE(p) + 0x01)  
#define TCO_DAT_IN(p)	(TCOBASE(p) + 0x02)  
#define TCO_DAT_OUT(p)	(TCOBASE(p) + 0x03)  
#define TCO1_STS(p)	(TCOBASE(p) + 0x04)  
#define TCO2_STS(p)	(TCOBASE(p) + 0x06)  
#define TCO1_CNT(p)	(TCOBASE(p) + 0x08)  
#define TCO2_CNT(p)	(TCOBASE(p) + 0x0a)  
#define TCOv2_TMR(p)	(TCOBASE(p) + 0x12)  

 
struct iTCO_wdt_private {
	struct watchdog_device wddev;

	 
	unsigned int iTCO_version;
	struct resource *tco_res;
	struct resource *smi_res;
	 
	unsigned long __iomem *gcs_pmc;
	 
	spinlock_t io_lock;
	 
	struct pci_dev *pci_dev;
	 
	bool suspended;
	 
	void *no_reboot_priv;
	 
	int (*update_no_reboot_bit)(void *p, bool set);
};

 
#define WATCHDOG_TIMEOUT 30	 
static int heartbeat = WATCHDOG_TIMEOUT;   
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog timeout in seconds. "
	"5..76 (TCO v1) or 3..614 (TCO v2), default="
				__MODULE_STRING(WATCHDOG_TIMEOUT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static int turn_SMI_watchdog_clear_off = 1;
module_param(turn_SMI_watchdog_clear_off, int, 0);
MODULE_PARM_DESC(turn_SMI_watchdog_clear_off,
	"Turn off SMI clearing watchdog (depends on TCO-version)(default=1)");

 

 
static inline unsigned int seconds_to_ticks(struct iTCO_wdt_private *p,
					    int secs)
{
	return p->iTCO_version == 3 ? secs : (secs * 10) / 6;
}

static inline unsigned int ticks_to_seconds(struct iTCO_wdt_private *p,
					    int ticks)
{
	return p->iTCO_version == 3 ? ticks : (ticks * 6) / 10;
}

static inline u32 no_reboot_bit(struct iTCO_wdt_private *p)
{
	u32 enable_bit;

	switch (p->iTCO_version) {
	case 5:
	case 3:
		enable_bit = 0x00000010;
		break;
	case 2:
		enable_bit = 0x00000020;
		break;
	case 4:
	case 1:
	default:
		enable_bit = 0x00000002;
		break;
	}

	return enable_bit;
}

static int update_no_reboot_bit_def(void *priv, bool set)
{
	return 0;
}

static int update_no_reboot_bit_pci(void *priv, bool set)
{
	struct iTCO_wdt_private *p = priv;
	u32 val32 = 0, newval32 = 0;

	pci_read_config_dword(p->pci_dev, 0xd4, &val32);
	if (set)
		val32 |= no_reboot_bit(p);
	else
		val32 &= ~no_reboot_bit(p);
	pci_write_config_dword(p->pci_dev, 0xd4, val32);
	pci_read_config_dword(p->pci_dev, 0xd4, &newval32);

	 
	if (val32 != newval32)
		return -EIO;

	return 0;
}

static int update_no_reboot_bit_mem(void *priv, bool set)
{
	struct iTCO_wdt_private *p = priv;
	u32 val32 = 0, newval32 = 0;

	val32 = readl(p->gcs_pmc);
	if (set)
		val32 |= no_reboot_bit(p);
	else
		val32 &= ~no_reboot_bit(p);
	writel(val32, p->gcs_pmc);
	newval32 = readl(p->gcs_pmc);

	 
	if (val32 != newval32)
		return -EIO;

	return 0;
}

static int update_no_reboot_bit_cnt(void *priv, bool set)
{
	struct iTCO_wdt_private *p = priv;
	u16 val, newval;

	val = inw(TCO1_CNT(p));
	if (set)
		val |= BIT(0);
	else
		val &= ~BIT(0);
	outw(val, TCO1_CNT(p));
	newval = inw(TCO1_CNT(p));

	 
	return val != newval ? -EIO : 0;
}

static int update_no_reboot_bit_pmc(void *priv, bool set)
{
	struct intel_pmc_dev *pmc = priv;
	u32 bits = PMC_CFG_NO_REBOOT_EN;
	u32 value = set ? bits : 0;

	return intel_pmc_gcr_update(pmc, PMC_GCR_PMC_CFG_REG, bits, value);
}

static void iTCO_wdt_no_reboot_bit_setup(struct iTCO_wdt_private *p,
					 struct platform_device *pdev,
					 struct itco_wdt_platform_data *pdata)
{
	if (pdata->no_reboot_use_pmc) {
		struct intel_pmc_dev *pmc = dev_get_drvdata(pdev->dev.parent);

		p->update_no_reboot_bit = update_no_reboot_bit_pmc;
		p->no_reboot_priv = pmc;
		return;
	}

	if (p->iTCO_version >= 6)
		p->update_no_reboot_bit = update_no_reboot_bit_cnt;
	else if (p->iTCO_version >= 2)
		p->update_no_reboot_bit = update_no_reboot_bit_mem;
	else if (p->iTCO_version == 1)
		p->update_no_reboot_bit = update_no_reboot_bit_pci;
	else
		p->update_no_reboot_bit = update_no_reboot_bit_def;

	p->no_reboot_priv = p;
}

static int iTCO_wdt_start(struct watchdog_device *wd_dev)
{
	struct iTCO_wdt_private *p = watchdog_get_drvdata(wd_dev);
	unsigned int val;

	spin_lock(&p->io_lock);

	iTCO_vendor_pre_start(p->smi_res, wd_dev->timeout);

	 
	if (p->update_no_reboot_bit(p->no_reboot_priv, false)) {
		spin_unlock(&p->io_lock);
		dev_err(wd_dev->parent, "failed to reset NO_REBOOT flag, reboot disabled by hardware/BIOS\n");
		return -EIO;
	}

	 
	if (p->iTCO_version >= 2)
		outw(0x01, TCO_RLD(p));
	else if (p->iTCO_version == 1)
		outb(0x01, TCO_RLD(p));

	 
	val = inw(TCO1_CNT(p));
	val &= 0xf7ff;
	outw(val, TCO1_CNT(p));
	val = inw(TCO1_CNT(p));
	spin_unlock(&p->io_lock);

	if (val & 0x0800)
		return -1;
	return 0;
}

static int iTCO_wdt_stop(struct watchdog_device *wd_dev)
{
	struct iTCO_wdt_private *p = watchdog_get_drvdata(wd_dev);
	unsigned int val;

	spin_lock(&p->io_lock);

	iTCO_vendor_pre_stop(p->smi_res);

	 
	val = inw(TCO1_CNT(p));
	val |= 0x0800;
	outw(val, TCO1_CNT(p));
	val = inw(TCO1_CNT(p));

	 
	p->update_no_reboot_bit(p->no_reboot_priv, true);

	spin_unlock(&p->io_lock);

	if ((val & 0x0800) == 0)
		return -1;
	return 0;
}

static int iTCO_wdt_ping(struct watchdog_device *wd_dev)
{
	struct iTCO_wdt_private *p = watchdog_get_drvdata(wd_dev);

	spin_lock(&p->io_lock);

	 
	if (p->iTCO_version >= 2) {
		outw(0x01, TCO_RLD(p));
	} else if (p->iTCO_version == 1) {
		 
		outw(0x0008, TCO1_STS(p));	 

		outb(0x01, TCO_RLD(p));
	}

	spin_unlock(&p->io_lock);
	return 0;
}

static int iTCO_wdt_set_timeout(struct watchdog_device *wd_dev, unsigned int t)
{
	struct iTCO_wdt_private *p = watchdog_get_drvdata(wd_dev);
	unsigned int val16;
	unsigned char val8;
	unsigned int tmrval;

	tmrval = seconds_to_ticks(p, t);

	 
	if (p->iTCO_version == 1)
		tmrval /= 2;

	 
	 
	if (tmrval < 0x04)
		return -EINVAL;
	if ((p->iTCO_version >= 2 && tmrval > 0x3ff) ||
	    (p->iTCO_version == 1 && tmrval > 0x03f))
		return -EINVAL;

	 
	if (p->iTCO_version >= 2) {
		spin_lock(&p->io_lock);
		val16 = inw(TCOv2_TMR(p));
		val16 &= 0xfc00;
		val16 |= tmrval;
		outw(val16, TCOv2_TMR(p));
		val16 = inw(TCOv2_TMR(p));
		spin_unlock(&p->io_lock);

		if ((val16 & 0x3ff) != tmrval)
			return -EINVAL;
	} else if (p->iTCO_version == 1) {
		spin_lock(&p->io_lock);
		val8 = inb(TCOv1_TMR(p));
		val8 &= 0xc0;
		val8 |= (tmrval & 0xff);
		outb(val8, TCOv1_TMR(p));
		val8 = inb(TCOv1_TMR(p));
		spin_unlock(&p->io_lock);

		if ((val8 & 0x3f) != tmrval)
			return -EINVAL;
	}

	wd_dev->timeout = t;
	return 0;
}

static unsigned int iTCO_wdt_get_timeleft(struct watchdog_device *wd_dev)
{
	struct iTCO_wdt_private *p = watchdog_get_drvdata(wd_dev);
	unsigned int val16;
	unsigned char val8;
	unsigned int time_left = 0;

	 
	if (p->iTCO_version >= 2) {
		spin_lock(&p->io_lock);
		val16 = inw(TCO_RLD(p));
		val16 &= 0x3ff;
		spin_unlock(&p->io_lock);

		time_left = ticks_to_seconds(p, val16);
	} else if (p->iTCO_version == 1) {
		spin_lock(&p->io_lock);
		val8 = inb(TCO_RLD(p));
		val8 &= 0x3f;
		if (!(inw(TCO1_STS(p)) & 0x0008))
			val8 += (inb(TCOv1_TMR(p)) & 0x3f);
		spin_unlock(&p->io_lock);

		time_left = ticks_to_seconds(p, val8);
	}
	return time_left;
}

 
static bool iTCO_wdt_set_running(struct iTCO_wdt_private *p)
{
	u16 val;

	 
	val = inw(TCO1_CNT(p));
	if (!(val & BIT(11))) {
		set_bit(WDOG_HW_RUNNING, &p->wddev.status);
		return true;
	}
	return false;
}

 

static struct watchdog_info ident = {
	.options =		WDIOF_SETTIMEOUT |
				WDIOF_KEEPALIVEPING |
				WDIOF_MAGICCLOSE,
	.identity =		DRV_NAME,
};

static const struct watchdog_ops iTCO_wdt_ops = {
	.owner =		THIS_MODULE,
	.start =		iTCO_wdt_start,
	.stop =			iTCO_wdt_stop,
	.ping =			iTCO_wdt_ping,
	.set_timeout =		iTCO_wdt_set_timeout,
	.get_timeleft =		iTCO_wdt_get_timeleft,
};

 

static int iTCO_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct itco_wdt_platform_data *pdata = dev_get_platdata(dev);
	struct iTCO_wdt_private *p;
	unsigned long val32;
	int ret;

	if (!pdata)
		return -ENODEV;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	spin_lock_init(&p->io_lock);

	p->tco_res = platform_get_resource(pdev, IORESOURCE_IO, ICH_RES_IO_TCO);
	if (!p->tco_res)
		return -ENODEV;

	p->iTCO_version = pdata->version;
	p->pci_dev = to_pci_dev(dev->parent);

	p->smi_res = platform_get_resource(pdev, IORESOURCE_IO, ICH_RES_IO_SMI);
	if (p->smi_res) {
		 
		if (!devm_request_region(dev, p->smi_res->start,
					 resource_size(p->smi_res),
					 pdev->name)) {
			dev_err(dev, "I/O address 0x%04llx already in use, device disabled\n",
			       (u64)SMI_EN(p));
			return -EBUSY;
		}
	} else if (iTCO_vendorsupport ||
		   turn_SMI_watchdog_clear_off >= p->iTCO_version) {
		dev_err(dev, "SMI I/O resource is missing\n");
		return -ENODEV;
	}

	iTCO_wdt_no_reboot_bit_setup(p, pdev, pdata);

	 
	if (p->iTCO_version >= 2 && p->iTCO_version < 6 &&
	    !pdata->no_reboot_use_pmc) {
		p->gcs_pmc = devm_platform_ioremap_resource(pdev, ICH_RES_MEM_GCS_PMC);
		if (IS_ERR(p->gcs_pmc))
			return PTR_ERR(p->gcs_pmc);
	}

	 
	if (p->update_no_reboot_bit(p->no_reboot_priv, false) &&
	    iTCO_vendor_check_noreboot_on()) {
		dev_info(dev, "unable to reset NO_REBOOT flag, device disabled by hardware/BIOS\n");
		return -ENODEV;	 
	}

	if (turn_SMI_watchdog_clear_off >= p->iTCO_version) {
		 
		val32 = inl(SMI_EN(p));
		val32 &= 0xffffdfff;	 
		outl(val32, SMI_EN(p));
	}

	if (!devm_request_region(dev, p->tco_res->start,
				 resource_size(p->tco_res),
				 pdev->name)) {
		dev_err(dev, "I/O address 0x%04llx already in use, device disabled\n",
		       (u64)TCOBASE(p));
		return -EBUSY;
	}

	dev_info(dev, "Found a %s TCO device (Version=%d, TCOBASE=0x%04llx)\n",
		pdata->name, pdata->version, (u64)TCOBASE(p));

	 
	switch (p->iTCO_version) {
	case 6:
	case 5:
	case 4:
		outw(0x0008, TCO1_STS(p));  
		outw(0x0002, TCO2_STS(p));  
		break;
	case 3:
		outl(0x20008, TCO1_STS(p));
		break;
	case 2:
	case 1:
	default:
		outw(0x0008, TCO1_STS(p));  
		outw(0x0002, TCO2_STS(p));  
		outw(0x0004, TCO2_STS(p));  
		break;
	}

	ident.firmware_version = p->iTCO_version;
	p->wddev.info = &ident,
	p->wddev.ops = &iTCO_wdt_ops,
	p->wddev.bootstatus = 0;
	p->wddev.timeout = WATCHDOG_TIMEOUT;
	watchdog_set_nowayout(&p->wddev, nowayout);
	p->wddev.parent = dev;

	watchdog_set_drvdata(&p->wddev, p);
	platform_set_drvdata(pdev, p);

	if (!iTCO_wdt_set_running(p)) {
		 
		p->update_no_reboot_bit(p->no_reboot_priv, true);
	}

	 
	if (iTCO_wdt_set_timeout(&p->wddev, heartbeat)) {
		iTCO_wdt_set_timeout(&p->wddev, WATCHDOG_TIMEOUT);
		dev_info(dev, "timeout value out of range, using %d\n",
			WATCHDOG_TIMEOUT);
	}

	watchdog_stop_on_reboot(&p->wddev);
	watchdog_stop_on_unregister(&p->wddev);
	ret = devm_watchdog_register_device(dev, &p->wddev);
	if (ret != 0) {
		dev_err(dev, "cannot register watchdog device (err=%d)\n", ret);
		return ret;
	}

	dev_info(dev, "initialized. heartbeat=%d sec (nowayout=%d)\n",
		heartbeat, nowayout);

	return 0;
}

 

#ifdef CONFIG_ACPI
static inline bool __maybe_unused need_suspend(void)
{
	return acpi_target_system_state() == ACPI_STATE_S0;
}
#else
static inline bool __maybe_unused need_suspend(void) { return true; }
#endif

static int __maybe_unused iTCO_wdt_suspend_noirq(struct device *dev)
{
	struct iTCO_wdt_private *p = dev_get_drvdata(dev);
	int ret = 0;

	p->suspended = false;
	if (watchdog_active(&p->wddev) && need_suspend()) {
		ret = iTCO_wdt_stop(&p->wddev);
		if (!ret)
			p->suspended = true;
	}
	return ret;
}

static int __maybe_unused iTCO_wdt_resume_noirq(struct device *dev)
{
	struct iTCO_wdt_private *p = dev_get_drvdata(dev);

	if (p->suspended)
		iTCO_wdt_start(&p->wddev);

	return 0;
}

static const struct dev_pm_ops iTCO_wdt_pm = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(iTCO_wdt_suspend_noirq,
				      iTCO_wdt_resume_noirq)
};

static struct platform_driver iTCO_wdt_driver = {
	.probe          = iTCO_wdt_probe,
	.driver         = {
		.name   = DRV_NAME,
		.pm     = &iTCO_wdt_pm,
	},
};

module_platform_driver(iTCO_wdt_driver);

MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>");
MODULE_DESCRIPTION("Intel TCO WatchDog Timer Driver");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
