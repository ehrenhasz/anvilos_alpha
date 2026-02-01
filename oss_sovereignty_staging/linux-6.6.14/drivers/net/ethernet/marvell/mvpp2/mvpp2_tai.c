
 
#include <linux/io.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/slab.h>

#include "mvpp2.h"

#define CR0_SW_NRESET			BIT(0)

#define TCFCR0_PHASE_UPDATE_ENABLE	BIT(8)
#define TCFCR0_TCF_MASK			(7 << 2)
#define TCFCR0_TCF_UPDATE		(0 << 2)
#define TCFCR0_TCF_FREQUPDATE		(1 << 2)
#define TCFCR0_TCF_INCREMENT		(2 << 2)
#define TCFCR0_TCF_DECREMENT		(3 << 2)
#define TCFCR0_TCF_CAPTURE		(4 << 2)
#define TCFCR0_TCF_NOP			(7 << 2)
#define TCFCR0_TCF_TRIGGER		BIT(0)

#define TCSR_CAPTURE_1_VALID		BIT(1)
#define TCSR_CAPTURE_0_VALID		BIT(0)

struct mvpp2_tai {
	struct ptp_clock_info caps;
	struct ptp_clock *ptp_clock;
	void __iomem *base;
	spinlock_t lock;
	u64 period;		 
	 
	struct timespec64 stamp;
};

static void mvpp2_tai_modify(void __iomem *reg, u32 mask, u32 set)
{
	u32 val;

	val = readl_relaxed(reg) & ~mask;
	val |= set & mask;
	writel(val, reg);
}

static void mvpp2_tai_write(u32 val, void __iomem *reg)
{
	writel_relaxed(val & 0xffff, reg);
}

static u32 mvpp2_tai_read(void __iomem *reg)
{
	return readl_relaxed(reg) & 0xffff;
}

static struct mvpp2_tai *ptp_to_tai(struct ptp_clock_info *ptp)
{
	return container_of(ptp, struct mvpp2_tai, caps);
}

static void mvpp22_tai_read_ts(struct timespec64 *ts, void __iomem *base)
{
	ts->tv_sec = (u64)mvpp2_tai_read(base + 0) << 32 |
		     mvpp2_tai_read(base + 4) << 16 |
		     mvpp2_tai_read(base + 8);

	ts->tv_nsec = mvpp2_tai_read(base + 12) << 16 |
		      mvpp2_tai_read(base + 16);

	 
	readl_relaxed(base + 20);
	readl_relaxed(base + 24);
}

static void mvpp2_tai_write_tlv(const struct timespec64 *ts, u32 frac,
			        void __iomem *base)
{
	mvpp2_tai_write(ts->tv_sec >> 32, base + MVPP22_TAI_TLV_SEC_HIGH);
	mvpp2_tai_write(ts->tv_sec >> 16, base + MVPP22_TAI_TLV_SEC_MED);
	mvpp2_tai_write(ts->tv_sec, base + MVPP22_TAI_TLV_SEC_LOW);
	mvpp2_tai_write(ts->tv_nsec >> 16, base + MVPP22_TAI_TLV_NANO_HIGH);
	mvpp2_tai_write(ts->tv_nsec, base + MVPP22_TAI_TLV_NANO_LOW);
	mvpp2_tai_write(frac >> 16, base + MVPP22_TAI_TLV_FRAC_HIGH);
	mvpp2_tai_write(frac, base + MVPP22_TAI_TLV_FRAC_LOW);
}

static void mvpp2_tai_op(u32 op, void __iomem *base)
{
	 
	mvpp2_tai_modify(base + MVPP22_TAI_TCFCR0,
			 TCFCR0_TCF_MASK | TCFCR0_TCF_TRIGGER,
			 op | TCFCR0_TCF_TRIGGER);
	mvpp2_tai_modify(base + MVPP22_TAI_TCFCR0, TCFCR0_TCF_MASK,
			 TCFCR0_TCF_NOP);
}

 
static u64 mvpp22_calc_frac_ppm(struct mvpp2_tai *tai, long abs_scaled_ppm)
{
	u64 val = tai->period * abs_scaled_ppm >> 4;

	return div_u64(val, (1000000 << 12) + (abs_scaled_ppm >> 4));
}

static s32 mvpp22_calc_max_adj(struct mvpp2_tai *tai)
{
	return 1000000;
}

static int mvpp22_tai_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	struct mvpp2_tai *tai = ptp_to_tai(ptp);
	unsigned long flags;
	void __iomem *base;
	bool neg_adj;
	s32 frac;
	u64 val;

	neg_adj = scaled_ppm < 0;
	if (neg_adj)
		scaled_ppm = -scaled_ppm;

	val = mvpp22_calc_frac_ppm(tai, scaled_ppm);

	 
	if (neg_adj) {
		 
		if (val > 0x80000000)
			return -ERANGE;

		frac = -val;
	} else {
		if (val > S32_MAX)
			return -ERANGE;

		frac = val;
	}

	base = tai->base;
	spin_lock_irqsave(&tai->lock, flags);
	mvpp2_tai_write(frac >> 16, base + MVPP22_TAI_TLV_FRAC_HIGH);
	mvpp2_tai_write(frac, base + MVPP22_TAI_TLV_FRAC_LOW);
	mvpp2_tai_op(TCFCR0_TCF_FREQUPDATE, base);
	spin_unlock_irqrestore(&tai->lock, flags);

	return 0;
}

static int mvpp22_tai_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct mvpp2_tai *tai = ptp_to_tai(ptp);
	struct timespec64 ts;
	unsigned long flags;
	void __iomem *base;
	u32 tcf;

	 
	if (delta == S64_MIN)
		return -ERANGE;

	if (delta < 0) {
		delta = -delta;
		tcf = TCFCR0_TCF_DECREMENT;
	} else {
		tcf = TCFCR0_TCF_INCREMENT;
	}

	ts = ns_to_timespec64(delta);

	base = tai->base;
	spin_lock_irqsave(&tai->lock, flags);
	mvpp2_tai_write_tlv(&ts, 0, base);
	mvpp2_tai_op(tcf, base);
	spin_unlock_irqrestore(&tai->lock, flags);

	return 0;
}

static int mvpp22_tai_gettimex64(struct ptp_clock_info *ptp,
				 struct timespec64 *ts,
				 struct ptp_system_timestamp *sts)
{
	struct mvpp2_tai *tai = ptp_to_tai(ptp);
	unsigned long flags;
	void __iomem *base;
	u32 tcsr;
	int ret;

	base = tai->base;
	spin_lock_irqsave(&tai->lock, flags);
	 
	ptp_read_system_prets(sts);
	mvpp2_tai_modify(base + MVPP22_TAI_TCFCR0,
			 TCFCR0_TCF_MASK | TCFCR0_TCF_TRIGGER,
			 TCFCR0_TCF_CAPTURE | TCFCR0_TCF_TRIGGER);
	ptp_read_system_postts(sts);
	mvpp2_tai_modify(base + MVPP22_TAI_TCFCR0, TCFCR0_TCF_MASK,
			 TCFCR0_TCF_NOP);

	tcsr = readl(base + MVPP22_TAI_TCSR);
	if (tcsr & TCSR_CAPTURE_1_VALID) {
		mvpp22_tai_read_ts(ts, base + MVPP22_TAI_TCV1_SEC_HIGH);
		ret = 0;
	} else if (tcsr & TCSR_CAPTURE_0_VALID) {
		mvpp22_tai_read_ts(ts, base + MVPP22_TAI_TCV0_SEC_HIGH);
		ret = 0;
	} else {
		 
		ret = -EBUSY;
	}
	spin_unlock_irqrestore(&tai->lock, flags);

	return ret;
}

static int mvpp22_tai_settime64(struct ptp_clock_info *ptp,
				const struct timespec64 *ts)
{
	struct mvpp2_tai *tai = ptp_to_tai(ptp);
	unsigned long flags;
	void __iomem *base;

	base = tai->base;
	spin_lock_irqsave(&tai->lock, flags);
	mvpp2_tai_write_tlv(ts, 0, base);

	 
	mvpp2_tai_modify(base + MVPP22_TAI_TCFCR0,
			 TCFCR0_PHASE_UPDATE_ENABLE |
			 TCFCR0_TCF_MASK | TCFCR0_TCF_TRIGGER,
			 TCFCR0_TCF_UPDATE | TCFCR0_TCF_TRIGGER);
	mvpp2_tai_modify(base + MVPP22_TAI_TCFCR0, TCFCR0_TCF_MASK,
			 TCFCR0_TCF_NOP);
	spin_unlock_irqrestore(&tai->lock, flags);

	return 0;
}

static long mvpp22_tai_aux_work(struct ptp_clock_info *ptp)
{
	struct mvpp2_tai *tai = ptp_to_tai(ptp);

	mvpp22_tai_gettimex64(ptp, &tai->stamp, NULL);

	return msecs_to_jiffies(2000);
}

static void mvpp22_tai_set_step(struct mvpp2_tai *tai)
{
	void __iomem *base = tai->base;
	u32 nano, frac;

	nano = upper_32_bits(tai->period);
	frac = lower_32_bits(tai->period);

	 
	if (frac >= 0x80000000)
		nano += 1;

	mvpp2_tai_write(nano, base + MVPP22_TAI_TOD_STEP_NANO_CR);
	mvpp2_tai_write(frac >> 16, base + MVPP22_TAI_TOD_STEP_FRAC_HIGH);
	mvpp2_tai_write(frac, base + MVPP22_TAI_TOD_STEP_FRAC_LOW);
}

static void mvpp22_tai_init(struct mvpp2_tai *tai)
{
	void __iomem *base = tai->base;

	mvpp22_tai_set_step(tai);

	 
	mvpp2_tai_modify(base + MVPP22_TAI_CR0, CR0_SW_NRESET, CR0_SW_NRESET);
}

int mvpp22_tai_ptp_clock_index(struct mvpp2_tai *tai)
{
	return ptp_clock_index(tai->ptp_clock);
}

void mvpp22_tai_tstamp(struct mvpp2_tai *tai, u32 tstamp,
		       struct skb_shared_hwtstamps *hwtstamp)
{
	struct timespec64 ts;
	int delta;

	 
	ts.tv_sec = READ_ONCE(tai->stamp.tv_sec);
	ts.tv_nsec = tstamp & 0x3fffffff;

	 
	delta = ((tstamp >> 30) - (ts.tv_sec & 3)) & 3;
	if (delta == 3)
		delta -= 4;
	ts.tv_sec += delta;

	memset(hwtstamp, 0, sizeof(*hwtstamp));
	hwtstamp->hwtstamp = timespec64_to_ktime(ts);
}

void mvpp22_tai_start(struct mvpp2_tai *tai)
{
	long delay;

	delay = mvpp22_tai_aux_work(&tai->caps);

	ptp_schedule_worker(tai->ptp_clock, delay);
}

void mvpp22_tai_stop(struct mvpp2_tai *tai)
{
	ptp_cancel_worker_sync(tai->ptp_clock);
}

static void mvpp22_tai_remove(void *priv)
{
	struct mvpp2_tai *tai = priv;

	if (!IS_ERR(tai->ptp_clock))
		ptp_clock_unregister(tai->ptp_clock);
}

int mvpp22_tai_probe(struct device *dev, struct mvpp2 *priv)
{
	struct mvpp2_tai *tai;
	int ret;

	tai = devm_kzalloc(dev, sizeof(*tai), GFP_KERNEL);
	if (!tai)
		return -ENOMEM;

	spin_lock_init(&tai->lock);

	tai->base = priv->iface_base;

	 
	tai->period = 3ULL << 32;

	mvpp22_tai_init(tai);

	tai->caps.owner = THIS_MODULE;
	strscpy(tai->caps.name, "Marvell PP2.2", sizeof(tai->caps.name));
	tai->caps.max_adj = mvpp22_calc_max_adj(tai);
	tai->caps.adjfine = mvpp22_tai_adjfine;
	tai->caps.adjtime = mvpp22_tai_adjtime;
	tai->caps.gettimex64 = mvpp22_tai_gettimex64;
	tai->caps.settime64 = mvpp22_tai_settime64;
	tai->caps.do_aux_work = mvpp22_tai_aux_work;

	ret = devm_add_action(dev, mvpp22_tai_remove, tai);
	if (ret)
		return ret;

	tai->ptp_clock = ptp_clock_register(&tai->caps, dev);
	if (IS_ERR(tai->ptp_clock))
		return PTR_ERR(tai->ptp_clock);

	priv->tai = tai;

	return 0;
}
