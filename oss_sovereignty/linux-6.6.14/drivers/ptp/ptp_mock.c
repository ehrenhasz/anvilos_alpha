
 

#include <linux/ptp_clock_kernel.h>
#include <linux/ptp_mock.h>
#include <linux/timecounter.h>

 
#define MOCK_PHC_MAX_ADJ_PPB		32000000
 
#define MOCK_PHC_CC_SHIFT		31
#define MOCK_PHC_CC_MULT		(1 << MOCK_PHC_CC_SHIFT)
#define MOCK_PHC_FADJ_SHIFT		9
#define MOCK_PHC_FADJ_DENOMINATOR	15625ULL

 
#define MOCK_PHC_REFRESH_INTERVAL	(HZ * 5)

#define info_to_phc(d) container_of((d), struct mock_phc, info)

struct mock_phc {
	struct ptp_clock_info info;
	struct ptp_clock *clock;
	struct timecounter tc;
	struct cyclecounter cc;
	spinlock_t lock;
};

static u64 mock_phc_cc_read(const struct cyclecounter *cc)
{
	return ktime_get_raw_ns();
}

static int mock_phc_adjfine(struct ptp_clock_info *info, long scaled_ppm)
{
	struct mock_phc *phc = info_to_phc(info);
	s64 adj;

	adj = (s64)scaled_ppm << MOCK_PHC_FADJ_SHIFT;
	adj = div_s64(adj, MOCK_PHC_FADJ_DENOMINATOR);

	spin_lock(&phc->lock);
	timecounter_read(&phc->tc);
	phc->cc.mult = MOCK_PHC_CC_MULT + adj;
	spin_unlock(&phc->lock);

	return 0;
}

static int mock_phc_adjtime(struct ptp_clock_info *info, s64 delta)
{
	struct mock_phc *phc = info_to_phc(info);

	spin_lock(&phc->lock);
	timecounter_adjtime(&phc->tc, delta);
	spin_unlock(&phc->lock);

	return 0;
}

static int mock_phc_settime64(struct ptp_clock_info *info,
			      const struct timespec64 *ts)
{
	struct mock_phc *phc = info_to_phc(info);
	u64 ns = timespec64_to_ns(ts);

	spin_lock(&phc->lock);
	timecounter_init(&phc->tc, &phc->cc, ns);
	spin_unlock(&phc->lock);

	return 0;
}

static int mock_phc_gettime64(struct ptp_clock_info *info, struct timespec64 *ts)
{
	struct mock_phc *phc = info_to_phc(info);
	u64 ns;

	spin_lock(&phc->lock);
	ns = timecounter_read(&phc->tc);
	spin_unlock(&phc->lock);

	*ts = ns_to_timespec64(ns);

	return 0;
}

static long mock_phc_refresh(struct ptp_clock_info *info)
{
	struct timespec64 ts;

	mock_phc_gettime64(info, &ts);

	return MOCK_PHC_REFRESH_INTERVAL;
}

int mock_phc_index(struct mock_phc *phc)
{
	return ptp_clock_index(phc->clock);
}
EXPORT_SYMBOL_GPL(mock_phc_index);

struct mock_phc *mock_phc_create(struct device *dev)
{
	struct mock_phc *phc;
	int err;

	phc = kzalloc(sizeof(*phc), GFP_KERNEL);
	if (!phc) {
		err = -ENOMEM;
		goto out;
	}

	phc->info = (struct ptp_clock_info) {
		.owner		= THIS_MODULE,
		.name		= "Mock-up PTP clock",
		.max_adj	= MOCK_PHC_MAX_ADJ_PPB,
		.adjfine	= mock_phc_adjfine,
		.adjtime	= mock_phc_adjtime,
		.gettime64	= mock_phc_gettime64,
		.settime64	= mock_phc_settime64,
		.do_aux_work	= mock_phc_refresh,
	};

	phc->cc = (struct cyclecounter) {
		.read	= mock_phc_cc_read,
		.mask	= CYCLECOUNTER_MASK(64),
		.mult	= MOCK_PHC_CC_MULT,
		.shift	= MOCK_PHC_CC_SHIFT,
	};

	spin_lock_init(&phc->lock);
	timecounter_init(&phc->tc, &phc->cc, 0);

	phc->clock = ptp_clock_register(&phc->info, dev);
	if (IS_ERR(phc->clock)) {
		err = PTR_ERR(phc->clock);
		goto out_free_phc;
	}

	ptp_schedule_worker(phc->clock, MOCK_PHC_REFRESH_INTERVAL);

	return phc;

out_free_phc:
	kfree(phc);
out:
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(mock_phc_create);

void mock_phc_destroy(struct mock_phc *phc)
{
	ptp_clock_unregister(phc->clock);
	kfree(phc);
}
EXPORT_SYMBOL_GPL(mock_phc_destroy);

MODULE_DESCRIPTION("Mock-up PTP Hardware Clock driver");
MODULE_LICENSE("GPL");
