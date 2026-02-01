
 

#include <linux/device.h>
#include <linux/module.h>
#include <linux/timecounter.h>
#include <linux/pci.h>

#include "cavium_ptp.h"

#define DRV_NAME "cavium_ptp"

#define PCI_DEVICE_ID_CAVIUM_PTP	0xA00C
#define PCI_SUBSYS_DEVID_88XX_PTP	0xA10C
#define PCI_SUBSYS_DEVID_81XX_PTP	0XA20C
#define PCI_SUBSYS_DEVID_83XX_PTP	0xA30C
#define PCI_DEVICE_ID_CAVIUM_RST	0xA00E

#define PCI_PTP_BAR_NO	0
#define PCI_RST_BAR_NO	0

#define PTP_CLOCK_CFG		0xF00ULL
#define  PTP_CLOCK_CFG_PTP_EN	BIT(0)
#define PTP_CLOCK_LO		0xF08ULL
#define PTP_CLOCK_HI		0xF10ULL
#define PTP_CLOCK_COMP		0xF18ULL

#define RST_BOOT	0x1600ULL
#define CLOCK_BASE_RATE	50000000ULL

static u64 ptp_cavium_clock_get(void)
{
	struct pci_dev *pdev;
	void __iomem *base;
	u64 ret = CLOCK_BASE_RATE * 16;

	pdev = pci_get_device(PCI_VENDOR_ID_CAVIUM,
			      PCI_DEVICE_ID_CAVIUM_RST, NULL);
	if (!pdev)
		goto error;

	base = pci_ioremap_bar(pdev, PCI_RST_BAR_NO);
	if (!base)
		goto error_put_pdev;

	ret = CLOCK_BASE_RATE * ((readq(base + RST_BOOT) >> 33) & 0x3f);

	iounmap(base);

error_put_pdev:
	pci_dev_put(pdev);

error:
	return ret;
}

struct cavium_ptp *cavium_ptp_get(void)
{
	struct cavium_ptp *ptp;
	struct pci_dev *pdev;

	pdev = pci_get_device(PCI_VENDOR_ID_CAVIUM,
			      PCI_DEVICE_ID_CAVIUM_PTP, NULL);
	if (!pdev)
		return ERR_PTR(-ENODEV);

	ptp = pci_get_drvdata(pdev);
	if (!ptp)
		ptp = ERR_PTR(-EPROBE_DEFER);
	if (IS_ERR(ptp))
		pci_dev_put(pdev);

	return ptp;
}
EXPORT_SYMBOL(cavium_ptp_get);

void cavium_ptp_put(struct cavium_ptp *ptp)
{
	if (!ptp)
		return;
	pci_dev_put(ptp->pdev);
}
EXPORT_SYMBOL(cavium_ptp_put);

 
static int cavium_ptp_adjfine(struct ptp_clock_info *ptp_info, long scaled_ppm)
{
	struct cavium_ptp *clock =
		container_of(ptp_info, struct cavium_ptp, ptp_info);
	unsigned long flags;
	u64 comp;
	u64 adj;
	bool neg_adj = false;

	if (scaled_ppm < 0) {
		neg_adj = true;
		scaled_ppm = -scaled_ppm;
	}

	 
	comp = ((u64)1000000000ull << 32) / clock->clock_rate;
	adj = comp * scaled_ppm;
	adj >>= 16;
	adj = div_u64(adj, 1000000ull);
	comp = neg_adj ? comp - adj : comp + adj;

	spin_lock_irqsave(&clock->spin_lock, flags);
	writeq(comp, clock->reg_base + PTP_CLOCK_COMP);
	spin_unlock_irqrestore(&clock->spin_lock, flags);

	return 0;
}

 
static int cavium_ptp_adjtime(struct ptp_clock_info *ptp_info, s64 delta)
{
	struct cavium_ptp *clock =
		container_of(ptp_info, struct cavium_ptp, ptp_info);
	unsigned long flags;

	spin_lock_irqsave(&clock->spin_lock, flags);
	timecounter_adjtime(&clock->time_counter, delta);
	spin_unlock_irqrestore(&clock->spin_lock, flags);

	 
	smp_mb();

	return 0;
}

 
static int cavium_ptp_gettime(struct ptp_clock_info *ptp_info,
			      struct timespec64 *ts)
{
	struct cavium_ptp *clock =
		container_of(ptp_info, struct cavium_ptp, ptp_info);
	unsigned long flags;
	u64 nsec;

	spin_lock_irqsave(&clock->spin_lock, flags);
	nsec = timecounter_read(&clock->time_counter);
	spin_unlock_irqrestore(&clock->spin_lock, flags);

	*ts = ns_to_timespec64(nsec);

	return 0;
}

 
static int cavium_ptp_settime(struct ptp_clock_info *ptp_info,
			      const struct timespec64 *ts)
{
	struct cavium_ptp *clock =
		container_of(ptp_info, struct cavium_ptp, ptp_info);
	unsigned long flags;
	u64 nsec;

	nsec = timespec64_to_ns(ts);

	spin_lock_irqsave(&clock->spin_lock, flags);
	timecounter_init(&clock->time_counter, &clock->cycle_counter, nsec);
	spin_unlock_irqrestore(&clock->spin_lock, flags);

	return 0;
}

 
static int cavium_ptp_enable(struct ptp_clock_info *ptp_info,
			     struct ptp_clock_request *rq, int on)
{
	return -EOPNOTSUPP;
}

static u64 cavium_ptp_cc_read(const struct cyclecounter *cc)
{
	struct cavium_ptp *clock =
		container_of(cc, struct cavium_ptp, cycle_counter);

	return readq(clock->reg_base + PTP_CLOCK_HI);
}

static int cavium_ptp_probe(struct pci_dev *pdev,
			    const struct pci_device_id *ent)
{
	struct device *dev = &pdev->dev;
	struct cavium_ptp *clock;
	struct cyclecounter *cc;
	u64 clock_cfg;
	u64 clock_comp;
	int err;

	clock = devm_kzalloc(dev, sizeof(*clock), GFP_KERNEL);
	if (!clock) {
		err = -ENOMEM;
		goto error;
	}

	clock->pdev = pdev;

	err = pcim_enable_device(pdev);
	if (err)
		goto error_free;

	err = pcim_iomap_regions(pdev, 1 << PCI_PTP_BAR_NO, pci_name(pdev));
	if (err)
		goto error_free;

	clock->reg_base = pcim_iomap_table(pdev)[PCI_PTP_BAR_NO];

	spin_lock_init(&clock->spin_lock);

	cc = &clock->cycle_counter;
	cc->read = cavium_ptp_cc_read;
	cc->mask = CYCLECOUNTER_MASK(64);
	cc->mult = 1;
	cc->shift = 0;

	timecounter_init(&clock->time_counter, &clock->cycle_counter,
			 ktime_to_ns(ktime_get_real()));

	clock->clock_rate = ptp_cavium_clock_get();

	clock->ptp_info = (struct ptp_clock_info) {
		.owner		= THIS_MODULE,
		.name		= "ThunderX PTP",
		.max_adj	= 1000000000ull,
		.n_ext_ts	= 0,
		.n_pins		= 0,
		.pps		= 0,
		.adjfine	= cavium_ptp_adjfine,
		.adjtime	= cavium_ptp_adjtime,
		.gettime64	= cavium_ptp_gettime,
		.settime64	= cavium_ptp_settime,
		.enable		= cavium_ptp_enable,
	};

	clock_cfg = readq(clock->reg_base + PTP_CLOCK_CFG);
	clock_cfg |= PTP_CLOCK_CFG_PTP_EN;
	writeq(clock_cfg, clock->reg_base + PTP_CLOCK_CFG);

	clock_comp = ((u64)1000000000ull << 32) / clock->clock_rate;
	writeq(clock_comp, clock->reg_base + PTP_CLOCK_COMP);

	clock->ptp_clock = ptp_clock_register(&clock->ptp_info, dev);
	if (IS_ERR(clock->ptp_clock)) {
		err = PTR_ERR(clock->ptp_clock);
		goto error_stop;
	}

	pci_set_drvdata(pdev, clock);
	return 0;

error_stop:
	clock_cfg = readq(clock->reg_base + PTP_CLOCK_CFG);
	clock_cfg &= ~PTP_CLOCK_CFG_PTP_EN;
	writeq(clock_cfg, clock->reg_base + PTP_CLOCK_CFG);
	pcim_iounmap_regions(pdev, 1 << PCI_PTP_BAR_NO);

error_free:
	devm_kfree(dev, clock);

error:
	 
	pci_set_drvdata(pdev, ERR_PTR(err));
	return 0;
}

static void cavium_ptp_remove(struct pci_dev *pdev)
{
	struct cavium_ptp *clock = pci_get_drvdata(pdev);
	u64 clock_cfg;

	if (IS_ERR_OR_NULL(clock))
		return;

	ptp_clock_unregister(clock->ptp_clock);

	clock_cfg = readq(clock->reg_base + PTP_CLOCK_CFG);
	clock_cfg &= ~PTP_CLOCK_CFG_PTP_EN;
	writeq(clock_cfg, clock->reg_base + PTP_CLOCK_CFG);
}

static const struct pci_device_id cavium_ptp_id_table[] = {
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_CAVIUM, PCI_DEVICE_ID_CAVIUM_PTP,
			PCI_VENDOR_ID_CAVIUM, PCI_SUBSYS_DEVID_88XX_PTP) },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_CAVIUM, PCI_DEVICE_ID_CAVIUM_PTP,
			PCI_VENDOR_ID_CAVIUM, PCI_SUBSYS_DEVID_81XX_PTP) },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_CAVIUM, PCI_DEVICE_ID_CAVIUM_PTP,
			PCI_VENDOR_ID_CAVIUM, PCI_SUBSYS_DEVID_83XX_PTP) },
	{ 0, }
};

static struct pci_driver cavium_ptp_driver = {
	.name = DRV_NAME,
	.id_table = cavium_ptp_id_table,
	.probe = cavium_ptp_probe,
	.remove = cavium_ptp_remove,
};

module_pci_driver(cavium_ptp_driver);

MODULE_DESCRIPTION(DRV_NAME);
MODULE_AUTHOR("Cavium Networks <support@cavium.com>");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(pci, cavium_ptp_id_table);
