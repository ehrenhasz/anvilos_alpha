
 

 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/uaccess.h>
#include <linux/io.h>

 
#define ESB_MODULE_NAME "i6300ESB timer"

 
#define ESB_CONFIG_REG  0x60             
#define ESB_LOCK_REG    0x68             

 
#define ESB_TIMER1_REG(w) ((w)->base + 0x00) 
#define ESB_TIMER2_REG(w) ((w)->base + 0x04) 
#define ESB_GINTSR_REG(w) ((w)->base + 0x08) 
#define ESB_RELOAD_REG(w) ((w)->base + 0x0c) 

 
#define ESB_WDT_FUNC    (0x01 << 2)    
#define ESB_WDT_ENABLE  (0x01 << 1)    
#define ESB_WDT_LOCK    (0x01 << 0)    

 
#define ESB_WDT_REBOOT  (0x01 << 5)    
#define ESB_WDT_FREQ    (0x01 << 2)    
#define ESB_WDT_INTTYPE (0x03 << 0)    

 
#define ESB_WDT_TIMEOUT (0x01 << 9)     
#define ESB_WDT_RELOAD  (0x01 << 8)     

 
#define ESB_UNLOCK1     0x80             
#define ESB_UNLOCK2     0x86             

 
 
#define ESB_HEARTBEAT_MIN	1
#define ESB_HEARTBEAT_MAX	2046
#define ESB_HEARTBEAT_DEFAULT	30
#define ESB_HEARTBEAT_RANGE __MODULE_STRING(ESB_HEARTBEAT_MIN) \
	"<heartbeat<" __MODULE_STRING(ESB_HEARTBEAT_MAX)
static int heartbeat;  
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat,
	"Watchdog heartbeat in seconds. (" ESB_HEARTBEAT_RANGE
	", default=" __MODULE_STRING(ESB_HEARTBEAT_DEFAULT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

 
struct esb_dev {
	struct watchdog_device wdd;
	void __iomem *base;
	struct pci_dev *pdev;
};

#define to_esb_dev(wptr) container_of(wptr, struct esb_dev, wdd)

 

 
static inline void esb_unlock_registers(struct esb_dev *edev)
{
	writew(ESB_UNLOCK1, ESB_RELOAD_REG(edev));
	writew(ESB_UNLOCK2, ESB_RELOAD_REG(edev));
}

static int esb_timer_start(struct watchdog_device *wdd)
{
	struct esb_dev *edev = to_esb_dev(wdd);
	int _wdd_nowayout = test_bit(WDOG_NO_WAY_OUT, &wdd->status);
	u8 val;

	esb_unlock_registers(edev);
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG(edev));
	 
	val = ESB_WDT_ENABLE | (_wdd_nowayout ? ESB_WDT_LOCK : 0x00);
	pci_write_config_byte(edev->pdev, ESB_LOCK_REG, val);
	return 0;
}

static int esb_timer_stop(struct watchdog_device *wdd)
{
	struct esb_dev *edev = to_esb_dev(wdd);
	u8 val;

	 
	esb_unlock_registers(edev);
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG(edev));
	 
	pci_write_config_byte(edev->pdev, ESB_LOCK_REG, 0x0);
	pci_read_config_byte(edev->pdev, ESB_LOCK_REG, &val);

	 
	return val & ESB_WDT_ENABLE;
}

static int esb_timer_keepalive(struct watchdog_device *wdd)
{
	struct esb_dev *edev = to_esb_dev(wdd);

	esb_unlock_registers(edev);
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG(edev));
	 
	return 0;
}

static int esb_timer_set_heartbeat(struct watchdog_device *wdd,
		unsigned int time)
{
	struct esb_dev *edev = to_esb_dev(wdd);
	u32 val;

	 
	val = time << 9;

	 
	esb_unlock_registers(edev);
	writel(val, ESB_TIMER1_REG(edev));

	 
	esb_unlock_registers(edev);
	writel(val, ESB_TIMER2_REG(edev));

	 
	esb_unlock_registers(edev);
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG(edev));

	 

	 
	wdd->timeout = time;
	return 0;
}

 

static struct watchdog_info esb_info = {
	.identity = ESB_MODULE_NAME,
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops esb_ops = {
	.owner = THIS_MODULE,
	.start = esb_timer_start,
	.stop = esb_timer_stop,
	.set_timeout = esb_timer_set_heartbeat,
	.ping = esb_timer_keepalive,
};

 
static const struct pci_device_id esb_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ESB_9), },
	{ 0, },                  
};
MODULE_DEVICE_TABLE(pci, esb_pci_tbl);

 

static unsigned char esb_getdevice(struct esb_dev *edev)
{
	if (pci_enable_device(edev->pdev)) {
		dev_err(&edev->pdev->dev, "failed to enable device\n");
		goto err_devput;
	}

	if (pci_request_region(edev->pdev, 0, ESB_MODULE_NAME)) {
		dev_err(&edev->pdev->dev, "failed to request region\n");
		goto err_disable;
	}

	edev->base = pci_ioremap_bar(edev->pdev, 0);
	if (edev->base == NULL) {
		 
		dev_err(&edev->pdev->dev, "failed to get BASEADDR\n");
		goto err_release;
	}

	 
	dev_set_drvdata(&edev->pdev->dev, edev);
	return 1;

err_release:
	pci_release_region(edev->pdev, 0);
err_disable:
	pci_disable_device(edev->pdev);
err_devput:
	return 0;
}

static void esb_initdevice(struct esb_dev *edev)
{
	u8 val1;
	u16 val2;

	 
	pci_write_config_word(edev->pdev, ESB_CONFIG_REG, 0x0003);

	 
	pci_read_config_byte(edev->pdev, ESB_LOCK_REG, &val1);
	if (val1 & ESB_WDT_LOCK)
		dev_warn(&edev->pdev->dev, "nowayout already set\n");

	 
	pci_write_config_byte(edev->pdev, ESB_LOCK_REG, 0x00);

	 
	esb_unlock_registers(edev);
	val2 = readw(ESB_RELOAD_REG(edev));
	if (val2 & ESB_WDT_TIMEOUT)
		edev->wdd.bootstatus = WDIOF_CARDRESET;

	 
	esb_unlock_registers(edev);
	writew((ESB_WDT_TIMEOUT | ESB_WDT_RELOAD), ESB_RELOAD_REG(edev));

	 
	esb_timer_set_heartbeat(&edev->wdd, edev->wdd.timeout);
}

static int esb_probe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	struct esb_dev *edev;
	int ret;

	edev = devm_kzalloc(&pdev->dev, sizeof(*edev), GFP_KERNEL);
	if (!edev)
		return -ENOMEM;

	 
	edev->pdev = pdev;
	if (!esb_getdevice(edev))
		return -ENODEV;

	 
	edev->wdd.info = &esb_info;
	edev->wdd.ops = &esb_ops;
	edev->wdd.min_timeout = ESB_HEARTBEAT_MIN;
	edev->wdd.max_timeout = ESB_HEARTBEAT_MAX;
	edev->wdd.timeout = ESB_HEARTBEAT_DEFAULT;
	watchdog_init_timeout(&edev->wdd, heartbeat, NULL);
	watchdog_set_nowayout(&edev->wdd, nowayout);
	watchdog_stop_on_reboot(&edev->wdd);
	watchdog_stop_on_unregister(&edev->wdd);
	esb_initdevice(edev);

	 
	ret = watchdog_register_device(&edev->wdd);
	if (ret != 0)
		goto err_unmap;
	dev_info(&pdev->dev,
		"initialized. heartbeat=%d sec (nowayout=%d)\n",
		edev->wdd.timeout, nowayout);
	return 0;

err_unmap:
	iounmap(edev->base);
	pci_release_region(edev->pdev, 0);
	pci_disable_device(edev->pdev);
	return ret;
}

static void esb_remove(struct pci_dev *pdev)
{
	struct esb_dev *edev = dev_get_drvdata(&pdev->dev);

	watchdog_unregister_device(&edev->wdd);
	iounmap(edev->base);
	pci_release_region(edev->pdev, 0);
	pci_disable_device(edev->pdev);
}

static struct pci_driver esb_driver = {
	.name		= ESB_MODULE_NAME,
	.id_table	= esb_pci_tbl,
	.probe          = esb_probe,
	.remove         = esb_remove,
};

module_pci_driver(esb_driver);

MODULE_AUTHOR("Ross Biro and David Härdeman");
MODULE_DESCRIPTION("Watchdog driver for Intel 6300ESB chipsets");
MODULE_LICENSE("GPL");
