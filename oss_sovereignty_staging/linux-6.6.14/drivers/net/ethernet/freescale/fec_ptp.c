
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/fec.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_net.h>

#include "fec.h"

 
#define FEC_T_CTRL_SLAVE                0x00002000
#define FEC_T_CTRL_CAPTURE              0x00000800
#define FEC_T_CTRL_RESTART              0x00000200
#define FEC_T_CTRL_PERIOD_RST           0x00000030
#define FEC_T_CTRL_PERIOD_EN		0x00000010
#define FEC_T_CTRL_ENABLE               0x00000001

#define FEC_T_INC_MASK                  0x0000007f
#define FEC_T_INC_OFFSET                0
#define FEC_T_INC_CORR_MASK             0x00007f00
#define FEC_T_INC_CORR_OFFSET           8

#define FEC_T_CTRL_PINPER		0x00000080
#define FEC_T_TF0_MASK			0x00000001
#define FEC_T_TF0_OFFSET		0
#define FEC_T_TF1_MASK			0x00000002
#define FEC_T_TF1_OFFSET		1
#define FEC_T_TF2_MASK			0x00000004
#define FEC_T_TF2_OFFSET		2
#define FEC_T_TF3_MASK			0x00000008
#define FEC_T_TF3_OFFSET		3
#define FEC_T_TDRE_MASK			0x00000001
#define FEC_T_TDRE_OFFSET		0
#define FEC_T_TMODE_MASK		0x0000003C
#define FEC_T_TMODE_OFFSET		2
#define FEC_T_TIE_MASK			0x00000040
#define FEC_T_TIE_OFFSET		6
#define FEC_T_TF_MASK			0x00000080
#define FEC_T_TF_OFFSET			7

#define FEC_ATIME_CTRL		0x400
#define FEC_ATIME		0x404
#define FEC_ATIME_EVT_OFFSET	0x408
#define FEC_ATIME_EVT_PERIOD	0x40c
#define FEC_ATIME_CORR		0x410
#define FEC_ATIME_INC		0x414
#define FEC_TS_TIMESTAMP	0x418

#define FEC_TGSR		0x604
#define FEC_TCSR(n)		(0x608 + n * 0x08)
#define FEC_TCCR(n)		(0x60C + n * 0x08)
#define MAX_TIMER_CHANNEL	3
#define FEC_TMODE_TOGGLE	0x05
#define FEC_HIGH_PULSE		0x0F

#define FEC_CC_MULT	(1 << 31)
#define FEC_COUNTER_PERIOD	(1 << 31)
#define PPS_OUPUT_RELOAD_PERIOD	NSEC_PER_SEC
#define FEC_CHANNLE_0		0
#define DEFAULT_PPS_CHANNEL	FEC_CHANNLE_0

#define FEC_PTP_MAX_NSEC_PERIOD		4000000000ULL
#define FEC_PTP_MAX_NSEC_COUNTER	0x80000000ULL

 
static int fec_ptp_enable_pps(struct fec_enet_private *fep, uint enable)
{
	unsigned long flags;
	u32 val, tempval;
	struct timespec64 ts;
	u64 ns;

	if (fep->pps_enable == enable)
		return 0;

	fep->pps_channel = DEFAULT_PPS_CHANNEL;
	fep->reload_period = PPS_OUPUT_RELOAD_PERIOD;

	spin_lock_irqsave(&fep->tmreg_lock, flags);

	if (enable) {
		 
		writel(FEC_T_TF_MASK, fep->hwp + FEC_TCSR(fep->pps_channel));

		 
		val = readl(fep->hwp + FEC_TCSR(fep->pps_channel));
		do {
			val &= ~(FEC_T_TMODE_MASK);
			writel(val, fep->hwp + FEC_TCSR(fep->pps_channel));
			val = readl(fep->hwp + FEC_TCSR(fep->pps_channel));
		} while (val & FEC_T_TMODE_MASK);

		 
		timecounter_read(&fep->tc);
		 
		tempval = fep->cc.read(&fep->cc);
		 
		ns = timecounter_cyc2time(&fep->tc, tempval);
		ts = ns_to_timespec64(ns);

		 
		val = NSEC_PER_SEC - (u32)ts.tv_nsec + tempval;

		 
		val += NSEC_PER_SEC;

		 
		val &= fep->cc.mask;
		writel(val, fep->hwp + FEC_TCCR(fep->pps_channel));

		 
		fep->next_counter = (val + fep->reload_period) & fep->cc.mask;

		 
		val = readl(fep->hwp + FEC_ATIME_CTRL);
		val |= FEC_T_CTRL_PINPER;
		writel(val, fep->hwp + FEC_ATIME_CTRL);

		 
		val = readl(fep->hwp + FEC_TCSR(fep->pps_channel));
		val |= (1 << FEC_T_TF_OFFSET | 1 << FEC_T_TIE_OFFSET);
		val &= ~(1 << FEC_T_TDRE_OFFSET);
		val &= ~(FEC_T_TMODE_MASK);
		val |= (FEC_HIGH_PULSE << FEC_T_TMODE_OFFSET);
		writel(val, fep->hwp + FEC_TCSR(fep->pps_channel));

		 
		writel(fep->next_counter, fep->hwp + FEC_TCCR(fep->pps_channel));
		fep->next_counter = (fep->next_counter + fep->reload_period) & fep->cc.mask;
	} else {
		writel(0, fep->hwp + FEC_TCSR(fep->pps_channel));
	}

	fep->pps_enable = enable;
	spin_unlock_irqrestore(&fep->tmreg_lock, flags);

	return 0;
}

static int fec_ptp_pps_perout(struct fec_enet_private *fep)
{
	u32 compare_val, ptp_hc, temp_val;
	u64 curr_time;
	unsigned long flags;

	spin_lock_irqsave(&fep->tmreg_lock, flags);

	 
	timecounter_read(&fep->tc);

	 
	temp_val = readl(fep->hwp + FEC_ATIME_CTRL);
	temp_val |= FEC_T_CTRL_CAPTURE;
	writel(temp_val, fep->hwp + FEC_ATIME_CTRL);
	if (fep->quirks & FEC_QUIRK_BUG_CAPTURE)
		udelay(1);

	ptp_hc = readl(fep->hwp + FEC_ATIME);

	 
	curr_time = timecounter_cyc2time(&fep->tc, ptp_hc);

	 
	if (fep->perout_stime < curr_time + 100 * NSEC_PER_MSEC) {
		dev_err(&fep->pdev->dev, "Current time is too close to the start time!\n");
		spin_unlock_irqrestore(&fep->tmreg_lock, flags);
		return -1;
	}

	compare_val = fep->perout_stime - curr_time + ptp_hc;
	compare_val &= fep->cc.mask;

	writel(compare_val, fep->hwp + FEC_TCCR(fep->pps_channel));
	fep->next_counter = (compare_val + fep->reload_period) & fep->cc.mask;

	 
	temp_val = readl(fep->hwp + FEC_ATIME_CTRL);
	temp_val |= FEC_T_CTRL_PINPER;
	writel(temp_val, fep->hwp + FEC_ATIME_CTRL);

	 
	temp_val = readl(fep->hwp + FEC_TCSR(fep->pps_channel));
	temp_val |= (1 << FEC_T_TF_OFFSET | 1 << FEC_T_TIE_OFFSET);
	temp_val &= ~(1 << FEC_T_TDRE_OFFSET);
	temp_val &= ~(FEC_T_TMODE_MASK);
	temp_val |= (FEC_TMODE_TOGGLE << FEC_T_TMODE_OFFSET);
	writel(temp_val, fep->hwp + FEC_TCSR(fep->pps_channel));

	 
	writel(fep->next_counter, fep->hwp + FEC_TCCR(fep->pps_channel));
	fep->next_counter = (fep->next_counter + fep->reload_period) & fep->cc.mask;
	spin_unlock_irqrestore(&fep->tmreg_lock, flags);

	return 0;
}

static enum hrtimer_restart fec_ptp_pps_perout_handler(struct hrtimer *timer)
{
	struct fec_enet_private *fep = container_of(timer,
					struct fec_enet_private, perout_timer);

	fec_ptp_pps_perout(fep);

	return HRTIMER_NORESTART;
}

 
static u64 fec_ptp_read(const struct cyclecounter *cc)
{
	struct fec_enet_private *fep =
		container_of(cc, struct fec_enet_private, cc);
	u32 tempval;

	tempval = readl(fep->hwp + FEC_ATIME_CTRL);
	tempval |= FEC_T_CTRL_CAPTURE;
	writel(tempval, fep->hwp + FEC_ATIME_CTRL);

	if (fep->quirks & FEC_QUIRK_BUG_CAPTURE)
		udelay(1);

	return readl(fep->hwp + FEC_ATIME);
}

 
void fec_ptp_start_cyclecounter(struct net_device *ndev)
{
	struct fec_enet_private *fep = netdev_priv(ndev);
	unsigned long flags;
	int inc;

	inc = 1000000000 / fep->cycle_speed;

	 
	spin_lock_irqsave(&fep->tmreg_lock, flags);

	 
	writel(inc << FEC_T_INC_OFFSET, fep->hwp + FEC_ATIME_INC);

	 
	writel(FEC_COUNTER_PERIOD, fep->hwp + FEC_ATIME_EVT_PERIOD);

	writel(FEC_T_CTRL_ENABLE | FEC_T_CTRL_PERIOD_RST,
		fep->hwp + FEC_ATIME_CTRL);

	memset(&fep->cc, 0, sizeof(fep->cc));
	fep->cc.read = fec_ptp_read;
	fep->cc.mask = CLOCKSOURCE_MASK(31);
	fep->cc.shift = 31;
	fep->cc.mult = FEC_CC_MULT;

	 
	timecounter_init(&fep->tc, &fep->cc, 0);

	spin_unlock_irqrestore(&fep->tmreg_lock, flags);
}

 
static int fec_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	s32 ppb = scaled_ppm_to_ppb(scaled_ppm);
	unsigned long flags;
	int neg_adj = 0;
	u32 i, tmp;
	u32 corr_inc, corr_period;
	u32 corr_ns;
	u64 lhs, rhs;

	struct fec_enet_private *fep =
	    container_of(ptp, struct fec_enet_private, ptp_caps);

	if (ppb == 0)
		return 0;

	if (ppb < 0) {
		ppb = -ppb;
		neg_adj = 1;
	}

	 
	lhs = NSEC_PER_SEC;
	rhs = (u64)ppb * (u64)fep->ptp_inc;
	for (i = 1; i <= fep->ptp_inc; i++) {
		if (lhs >= rhs) {
			corr_inc = i;
			corr_period = div_u64(lhs, rhs);
			break;
		}
		lhs += NSEC_PER_SEC;
	}
	 
	if (i > fep->ptp_inc) {
		corr_inc = fep->ptp_inc;
		corr_period = 1;
	}

	if (neg_adj)
		corr_ns = fep->ptp_inc - corr_inc;
	else
		corr_ns = fep->ptp_inc + corr_inc;

	spin_lock_irqsave(&fep->tmreg_lock, flags);

	tmp = readl(fep->hwp + FEC_ATIME_INC) & FEC_T_INC_MASK;
	tmp |= corr_ns << FEC_T_INC_CORR_OFFSET;
	writel(tmp, fep->hwp + FEC_ATIME_INC);
	corr_period = corr_period > 1 ? corr_period - 1 : corr_period;
	writel(corr_period, fep->hwp + FEC_ATIME_CORR);
	 
	timecounter_read(&fep->tc);

	spin_unlock_irqrestore(&fep->tmreg_lock, flags);

	return 0;
}

 
static int fec_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct fec_enet_private *fep =
	    container_of(ptp, struct fec_enet_private, ptp_caps);
	unsigned long flags;

	spin_lock_irqsave(&fep->tmreg_lock, flags);
	timecounter_adjtime(&fep->tc, delta);
	spin_unlock_irqrestore(&fep->tmreg_lock, flags);

	return 0;
}

 
static int fec_ptp_gettime(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
	struct fec_enet_private *fep =
	    container_of(ptp, struct fec_enet_private, ptp_caps);
	u64 ns;
	unsigned long flags;

	mutex_lock(&fep->ptp_clk_mutex);
	 
	if (!fep->ptp_clk_on) {
		mutex_unlock(&fep->ptp_clk_mutex);
		return -EINVAL;
	}
	spin_lock_irqsave(&fep->tmreg_lock, flags);
	ns = timecounter_read(&fep->tc);
	spin_unlock_irqrestore(&fep->tmreg_lock, flags);
	mutex_unlock(&fep->ptp_clk_mutex);

	*ts = ns_to_timespec64(ns);

	return 0;
}

 
static int fec_ptp_settime(struct ptp_clock_info *ptp,
			   const struct timespec64 *ts)
{
	struct fec_enet_private *fep =
	    container_of(ptp, struct fec_enet_private, ptp_caps);

	u64 ns;
	unsigned long flags;
	u32 counter;

	mutex_lock(&fep->ptp_clk_mutex);
	 
	if (!fep->ptp_clk_on) {
		mutex_unlock(&fep->ptp_clk_mutex);
		return -EINVAL;
	}

	ns = timespec64_to_ns(ts);
	 
	counter = ns & fep->cc.mask;

	spin_lock_irqsave(&fep->tmreg_lock, flags);
	writel(counter, fep->hwp + FEC_ATIME);
	timecounter_init(&fep->tc, &fep->cc, ns);
	spin_unlock_irqrestore(&fep->tmreg_lock, flags);
	mutex_unlock(&fep->ptp_clk_mutex);
	return 0;
}

static int fec_ptp_pps_disable(struct fec_enet_private *fep, uint channel)
{
	unsigned long flags;

	spin_lock_irqsave(&fep->tmreg_lock, flags);
	writel(0, fep->hwp + FEC_TCSR(channel));
	spin_unlock_irqrestore(&fep->tmreg_lock, flags);

	return 0;
}

 
static int fec_ptp_enable(struct ptp_clock_info *ptp,
			  struct ptp_clock_request *rq, int on)
{
	struct fec_enet_private *fep =
	    container_of(ptp, struct fec_enet_private, ptp_caps);
	ktime_t timeout;
	struct timespec64 start_time, period;
	u64 curr_time, delta, period_ns;
	unsigned long flags;
	int ret = 0;

	if (rq->type == PTP_CLK_REQ_PPS) {
		ret = fec_ptp_enable_pps(fep, on);

		return ret;
	} else if (rq->type == PTP_CLK_REQ_PEROUT) {
		 
		if (rq->perout.flags)
			return -EOPNOTSUPP;

		if (rq->perout.index != DEFAULT_PPS_CHANNEL)
			return -EOPNOTSUPP;

		fep->pps_channel = DEFAULT_PPS_CHANNEL;
		period.tv_sec = rq->perout.period.sec;
		period.tv_nsec = rq->perout.period.nsec;
		period_ns = timespec64_to_ns(&period);

		 
		if (period_ns > FEC_PTP_MAX_NSEC_PERIOD) {
			dev_err(&fep->pdev->dev, "The period must equal to or less than 4s!\n");
			return -EOPNOTSUPP;
		}

		fep->reload_period = div_u64(period_ns, 2);
		if (on && fep->reload_period) {
			 
			start_time.tv_sec = rq->perout.start.sec;
			start_time.tv_nsec = rq->perout.start.nsec;
			fep->perout_stime = timespec64_to_ns(&start_time);

			mutex_lock(&fep->ptp_clk_mutex);
			if (!fep->ptp_clk_on) {
				dev_err(&fep->pdev->dev, "Error: PTP clock is closed!\n");
				mutex_unlock(&fep->ptp_clk_mutex);
				return -EOPNOTSUPP;
			}
			spin_lock_irqsave(&fep->tmreg_lock, flags);
			 
			curr_time = timecounter_read(&fep->tc);
			spin_unlock_irqrestore(&fep->tmreg_lock, flags);
			mutex_unlock(&fep->ptp_clk_mutex);

			 
			delta = fep->perout_stime - curr_time;

			if (fep->perout_stime <= curr_time) {
				dev_err(&fep->pdev->dev, "Start time must larger than current time!\n");
				return -EINVAL;
			}

			 
			if (delta > FEC_PTP_MAX_NSEC_COUNTER) {
				timeout = ns_to_ktime(delta - NSEC_PER_SEC);
				hrtimer_start(&fep->perout_timer, timeout, HRTIMER_MODE_REL);
			} else {
				return fec_ptp_pps_perout(fep);
			}
		} else {
			fec_ptp_pps_disable(fep, fep->pps_channel);
		}

		return 0;
	} else {
		return -EOPNOTSUPP;
	}
}

int fec_ptp_set(struct net_device *ndev, struct kernel_hwtstamp_config *config,
		struct netlink_ext_ack *extack)
{
	struct fec_enet_private *fep = netdev_priv(ndev);

	switch (config->tx_type) {
	case HWTSTAMP_TX_OFF:
		fep->hwts_tx_en = 0;
		break;
	case HWTSTAMP_TX_ON:
		fep->hwts_tx_en = 1;
		break;
	default:
		return -ERANGE;
	}

	switch (config->rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		fep->hwts_rx_en = 0;
		break;

	default:
		fep->hwts_rx_en = 1;
		config->rx_filter = HWTSTAMP_FILTER_ALL;
		break;
	}

	return 0;
}

void fec_ptp_get(struct net_device *ndev, struct kernel_hwtstamp_config *config)
{
	struct fec_enet_private *fep = netdev_priv(ndev);

	config->flags = 0;
	config->tx_type = fep->hwts_tx_en ? HWTSTAMP_TX_ON : HWTSTAMP_TX_OFF;
	config->rx_filter = (fep->hwts_rx_en ?
			     HWTSTAMP_FILTER_ALL : HWTSTAMP_FILTER_NONE);
}

 
static void fec_time_keep(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct fec_enet_private *fep = container_of(dwork, struct fec_enet_private, time_keep);
	unsigned long flags;

	mutex_lock(&fep->ptp_clk_mutex);
	if (fep->ptp_clk_on) {
		spin_lock_irqsave(&fep->tmreg_lock, flags);
		timecounter_read(&fep->tc);
		spin_unlock_irqrestore(&fep->tmreg_lock, flags);
	}
	mutex_unlock(&fep->ptp_clk_mutex);

	schedule_delayed_work(&fep->time_keep, HZ);
}

 
static irqreturn_t fec_pps_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct fec_enet_private *fep = netdev_priv(ndev);
	u32 val;
	u8 channel = fep->pps_channel;
	struct ptp_clock_event event;

	val = readl(fep->hwp + FEC_TCSR(channel));
	if (val & FEC_T_TF_MASK) {
		 
		writel(fep->next_counter, fep->hwp + FEC_TCCR(channel));
		do {
			writel(val, fep->hwp + FEC_TCSR(channel));
		} while (readl(fep->hwp + FEC_TCSR(channel)) & FEC_T_TF_MASK);

		 
		fep->next_counter = (fep->next_counter + fep->reload_period) &
				fep->cc.mask;

		event.type = PTP_CLOCK_PPS;
		ptp_clock_event(fep->ptp_clock, &event);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

 

void fec_ptp_init(struct platform_device *pdev, int irq_idx)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct fec_enet_private *fep = netdev_priv(ndev);
	int irq;
	int ret;

	fep->ptp_caps.owner = THIS_MODULE;
	strscpy(fep->ptp_caps.name, "fec ptp", sizeof(fep->ptp_caps.name));

	fep->ptp_caps.max_adj = 250000000;
	fep->ptp_caps.n_alarm = 0;
	fep->ptp_caps.n_ext_ts = 0;
	fep->ptp_caps.n_per_out = 1;
	fep->ptp_caps.n_pins = 0;
	fep->ptp_caps.pps = 1;
	fep->ptp_caps.adjfine = fec_ptp_adjfine;
	fep->ptp_caps.adjtime = fec_ptp_adjtime;
	fep->ptp_caps.gettime64 = fec_ptp_gettime;
	fep->ptp_caps.settime64 = fec_ptp_settime;
	fep->ptp_caps.enable = fec_ptp_enable;

	fep->cycle_speed = clk_get_rate(fep->clk_ptp);
	if (!fep->cycle_speed) {
		fep->cycle_speed = NSEC_PER_SEC;
		dev_err(&fep->pdev->dev, "clk_ptp clock rate is zero\n");
	}
	fep->ptp_inc = NSEC_PER_SEC / fep->cycle_speed;

	spin_lock_init(&fep->tmreg_lock);

	fec_ptp_start_cyclecounter(ndev);

	INIT_DELAYED_WORK(&fep->time_keep, fec_time_keep);

	hrtimer_init(&fep->perout_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	fep->perout_timer.function = fec_ptp_pps_perout_handler;

	irq = platform_get_irq_byname_optional(pdev, "pps");
	if (irq < 0)
		irq = platform_get_irq_optional(pdev, irq_idx);
	 
	if (irq >= 0) {
		ret = devm_request_irq(&pdev->dev, irq, fec_pps_interrupt,
				       0, pdev->name, ndev);
		if (ret < 0)
			dev_warn(&pdev->dev, "request for pps irq failed(%d)\n",
				 ret);
	}

	fep->ptp_clock = ptp_clock_register(&fep->ptp_caps, &pdev->dev);
	if (IS_ERR(fep->ptp_clock)) {
		fep->ptp_clock = NULL;
		dev_err(&pdev->dev, "ptp_clock_register failed\n");
	}

	schedule_delayed_work(&fep->time_keep, HZ);
}

void fec_ptp_stop(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct fec_enet_private *fep = netdev_priv(ndev);

	cancel_delayed_work_sync(&fep->time_keep);
	hrtimer_cancel(&fep->perout_timer);
	if (fep->ptp_clock)
		ptp_clock_unregister(fep->ptp_clock);
}
