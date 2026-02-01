
 

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/dfl.h>
#include <linux/gcd.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/spinlock.h>
#include <linux/units.h>

#define FME_FEATURE_ID_TOD		0x22

 
#define TOD_CLK_FREQ			0x038

 
#define TOD_SECONDSH			0x100
#define TOD_SECONDSL			0x104
#define TOD_NANOSEC			0x108
#define TOD_PERIOD			0x110
#define TOD_ADJUST_PERIOD		0x114
#define TOD_ADJUST_COUNT		0x118
#define TOD_DRIFT_ADJUST		0x11c
#define TOD_DRIFT_ADJUST_RATE		0x120
#define PERIOD_FRAC_OFFSET		16
#define SECONDS_MSB			GENMASK_ULL(47, 32)
#define SECONDS_LSB			GENMASK_ULL(31, 0)
#define TOD_SECONDSH_SEC_MSB		GENMASK_ULL(15, 0)

#define CAL_SECONDS(m, l)		((FIELD_GET(TOD_SECONDSH_SEC_MSB, (m)) << 32) | (l))

#define TOD_PERIOD_MASK		GENMASK_ULL(19, 0)
#define TOD_PERIOD_MAX			FIELD_MAX(TOD_PERIOD_MASK)
#define TOD_PERIOD_MIN			0
#define TOD_DRIFT_ADJUST_MASK		GENMASK_ULL(15, 0)
#define TOD_DRIFT_ADJUST_FNS_MAX	FIELD_MAX(TOD_DRIFT_ADJUST_MASK)
#define TOD_DRIFT_ADJUST_RATE_MAX	TOD_DRIFT_ADJUST_FNS_MAX
#define TOD_ADJUST_COUNT_MASK		GENMASK_ULL(19, 0)
#define TOD_ADJUST_COUNT_MAX		FIELD_MAX(TOD_ADJUST_COUNT_MASK)
#define TOD_ADJUST_INTERVAL_US		10
#define TOD_ADJUST_MS			\
		(((TOD_PERIOD_MAX >> 16) + 1) * (TOD_ADJUST_COUNT_MAX + 1))
#define TOD_ADJUST_MS_MAX		(TOD_ADJUST_MS / MICRO)
#define TOD_ADJUST_MAX_US		(TOD_ADJUST_MS_MAX * USEC_PER_MSEC)
#define TOD_MAX_ADJ			(500 * MEGA)

struct dfl_tod {
	struct ptp_clock_info ptp_clock_ops;
	struct device *dev;
	struct ptp_clock *ptp_clock;

	 
	void __iomem *tod_ctrl;

	 
	spinlock_t tod_lock;
};

 
static int fine_adjust_tod_clock(struct dfl_tod *dt, u32 adjust_period,
				 u32 adjust_count)
{
	void __iomem *base = dt->tod_ctrl;
	u32 val;

	writel(adjust_period, base + TOD_ADJUST_PERIOD);
	writel(adjust_count, base + TOD_ADJUST_COUNT);

	 
	return readl_poll_timeout_atomic(base + TOD_ADJUST_COUNT, val, !val, TOD_ADJUST_INTERVAL_US,
				  TOD_ADJUST_MAX_US);
}

 
static int coarse_adjust_tod_clock(struct dfl_tod *dt, s64 delta)
{
	u32 seconds_msb, seconds_lsb, nanosec;
	void __iomem *base = dt->tod_ctrl;
	u64 seconds, now;

	if (delta == 0)
		return 0;

	nanosec = readl(base + TOD_NANOSEC);
	seconds_lsb = readl(base + TOD_SECONDSL);
	seconds_msb = readl(base + TOD_SECONDSH);

	 
	seconds = CAL_SECONDS(seconds_msb, seconds_lsb);
	now = seconds * NSEC_PER_SEC + nanosec + delta;

	seconds = div_u64_rem(now, NSEC_PER_SEC, &nanosec);
	seconds_msb = FIELD_GET(SECONDS_MSB, seconds);
	seconds_lsb = FIELD_GET(SECONDS_LSB, seconds);

	writel(seconds_msb, base + TOD_SECONDSH);
	writel(seconds_lsb, base + TOD_SECONDSL);
	writel(nanosec, base + TOD_NANOSEC);

	return 0;
}

static int dfl_tod_adjust_fine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	struct dfl_tod *dt = container_of(ptp, struct dfl_tod, ptp_clock_ops);
	u32 tod_period, tod_rem, tod_drift_adjust_fns, tod_drift_adjust_rate;
	void __iomem *base = dt->tod_ctrl;
	unsigned long flags, rate;
	u64 ppb;

	 
	rate = readl(base + TOD_CLK_FREQ);

	 
	ppb = scaled_ppm_to_ppb(scaled_ppm) + GIGA;

	tod_period = div_u64_rem(ppb << PERIOD_FRAC_OFFSET, rate, &tod_rem);
	if (tod_period > TOD_PERIOD_MAX)
		return -ERANGE;

	 
	tod_drift_adjust_fns = tod_rem / gcd(tod_rem, rate);
	tod_drift_adjust_rate = rate / gcd(tod_rem, rate);

	while ((tod_drift_adjust_fns > TOD_DRIFT_ADJUST_FNS_MAX) ||
	       (tod_drift_adjust_rate > TOD_DRIFT_ADJUST_RATE_MAX)) {
		tod_drift_adjust_fns >>= 1;
		tod_drift_adjust_rate >>= 1;
	}

	if (tod_drift_adjust_fns == 0)
		tod_drift_adjust_rate = 0;

	spin_lock_irqsave(&dt->tod_lock, flags);
	writel(tod_period, base + TOD_PERIOD);
	writel(0, base + TOD_ADJUST_PERIOD);
	writel(0, base + TOD_ADJUST_COUNT);
	writel(tod_drift_adjust_fns, base + TOD_DRIFT_ADJUST);
	writel(tod_drift_adjust_rate, base + TOD_DRIFT_ADJUST_RATE);
	spin_unlock_irqrestore(&dt->tod_lock, flags);

	return 0;
}

static int dfl_tod_adjust_time(struct ptp_clock_info *ptp, s64 delta)
{
	struct dfl_tod *dt = container_of(ptp, struct dfl_tod, ptp_clock_ops);
	u32 period, diff, rem, rem_period, adj_period;
	void __iomem *base = dt->tod_ctrl;
	unsigned long flags;
	bool neg_adj;
	u64 count;
	int ret;

	neg_adj = delta < 0;
	if (neg_adj)
		delta = -delta;

	spin_lock_irqsave(&dt->tod_lock, flags);

	 
	period = readl(base + TOD_PERIOD);

	if (neg_adj) {
		diff = (period - TOD_PERIOD_MIN) >> PERIOD_FRAC_OFFSET;
		adj_period = period - (diff << PERIOD_FRAC_OFFSET);
		count = div_u64_rem(delta, diff, &rem);
		rem_period = period - (rem << PERIOD_FRAC_OFFSET);
	} else {
		diff = (TOD_PERIOD_MAX - period) >> PERIOD_FRAC_OFFSET;
		adj_period = period + (diff << PERIOD_FRAC_OFFSET);
		count = div_u64_rem(delta, diff, &rem);
		rem_period = period + (rem << PERIOD_FRAC_OFFSET);
	}

	ret = 0;

	if (count > TOD_ADJUST_COUNT_MAX) {
		ret = coarse_adjust_tod_clock(dt, delta);
	} else {
		 
		if (count)
			ret = fine_adjust_tod_clock(dt, adj_period, count);

		 
		if (rem)
			ret = fine_adjust_tod_clock(dt, rem_period, 1);
	}

	spin_unlock_irqrestore(&dt->tod_lock, flags);

	return ret;
}

static int dfl_tod_get_timex(struct ptp_clock_info *ptp, struct timespec64 *ts,
			     struct ptp_system_timestamp *sts)
{
	struct dfl_tod *dt = container_of(ptp, struct dfl_tod, ptp_clock_ops);
	u32 seconds_msb, seconds_lsb, nanosec;
	void __iomem *base = dt->tod_ctrl;
	unsigned long flags;
	u64 seconds;

	spin_lock_irqsave(&dt->tod_lock, flags);
	ptp_read_system_prets(sts);
	nanosec = readl(base + TOD_NANOSEC);
	seconds_lsb = readl(base + TOD_SECONDSL);
	seconds_msb = readl(base + TOD_SECONDSH);
	ptp_read_system_postts(sts);
	spin_unlock_irqrestore(&dt->tod_lock, flags);

	seconds = CAL_SECONDS(seconds_msb, seconds_lsb);

	ts->tv_nsec = nanosec;
	ts->tv_sec = seconds;

	return 0;
}

static int dfl_tod_set_time(struct ptp_clock_info *ptp,
			    const struct timespec64 *ts)
{
	struct dfl_tod *dt = container_of(ptp, struct dfl_tod, ptp_clock_ops);
	u32 seconds_msb = FIELD_GET(SECONDS_MSB, ts->tv_sec);
	u32 seconds_lsb = FIELD_GET(SECONDS_LSB, ts->tv_sec);
	u32 nanosec = FIELD_GET(SECONDS_LSB, ts->tv_nsec);
	void __iomem *base = dt->tod_ctrl;
	unsigned long flags;

	spin_lock_irqsave(&dt->tod_lock, flags);
	writel(seconds_msb, base + TOD_SECONDSH);
	writel(seconds_lsb, base + TOD_SECONDSL);
	writel(nanosec, base + TOD_NANOSEC);
	spin_unlock_irqrestore(&dt->tod_lock, flags);

	return 0;
}

static struct ptp_clock_info dfl_tod_clock_ops = {
	.owner = THIS_MODULE,
	.name = "dfl_tod",
	.max_adj = TOD_MAX_ADJ,
	.adjfine = dfl_tod_adjust_fine,
	.adjtime = dfl_tod_adjust_time,
	.gettimex64 = dfl_tod_get_timex,
	.settime64 = dfl_tod_set_time,
};

static int dfl_tod_probe(struct dfl_device *ddev)
{
	struct device *dev = &ddev->dev;
	struct dfl_tod *dt;

	dt = devm_kzalloc(dev, sizeof(*dt), GFP_KERNEL);
	if (!dt)
		return -ENOMEM;

	dt->tod_ctrl = devm_ioremap_resource(dev, &ddev->mmio_res);
	if (IS_ERR(dt->tod_ctrl))
		return PTR_ERR(dt->tod_ctrl);

	dt->dev = dev;
	spin_lock_init(&dt->tod_lock);
	dev_set_drvdata(dev, dt);

	dt->ptp_clock_ops = dfl_tod_clock_ops;

	dt->ptp_clock = ptp_clock_register(&dt->ptp_clock_ops, dev);
	if (IS_ERR(dt->ptp_clock))
		return dev_err_probe(dt->dev, PTR_ERR(dt->ptp_clock),
				     "Unable to register PTP clock\n");

	return 0;
}

static void dfl_tod_remove(struct dfl_device *ddev)
{
	struct dfl_tod *dt = dev_get_drvdata(&ddev->dev);

	ptp_clock_unregister(dt->ptp_clock);
}

static const struct dfl_device_id dfl_tod_ids[] = {
	{ FME_ID, FME_FEATURE_ID_TOD },
	{ }
};
MODULE_DEVICE_TABLE(dfl, dfl_tod_ids);

static struct dfl_driver dfl_tod_driver = {
	.drv = {
		.name = "dfl-tod",
	},
	.id_table = dfl_tod_ids,
	.probe = dfl_tod_probe,
	.remove = dfl_tod_remove,
};
module_dfl_driver(dfl_tod_driver);

MODULE_DESCRIPTION("FPGA DFL ToD driver");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");
