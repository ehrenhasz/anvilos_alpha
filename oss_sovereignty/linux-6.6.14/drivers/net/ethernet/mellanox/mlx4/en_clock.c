 

#include <linux/mlx4/device.h>
#include <linux/clocksource.h>

#include "mlx4_en.h"

 
static u64 mlx4_en_read_clock(const struct cyclecounter *tc)
{
	struct mlx4_en_dev *mdev =
		container_of(tc, struct mlx4_en_dev, cycles);
	struct mlx4_dev *dev = mdev->dev;

	return mlx4_read_clock(dev) & tc->mask;
}

u64 mlx4_en_get_cqe_ts(struct mlx4_cqe *cqe)
{
	u64 hi, lo;
	struct mlx4_ts_cqe *ts_cqe = (struct mlx4_ts_cqe *)cqe;

	lo = (u64)be16_to_cpu(ts_cqe->timestamp_lo);
	hi = ((u64)be32_to_cpu(ts_cqe->timestamp_hi) + !lo) << 16;

	return hi | lo;
}

u64 mlx4_en_get_hwtstamp(struct mlx4_en_dev *mdev, u64 timestamp)
{
	unsigned int seq;
	u64 nsec;

	do {
		seq = read_seqbegin(&mdev->clock_lock);
		nsec = timecounter_cyc2time(&mdev->clock, timestamp);
	} while (read_seqretry(&mdev->clock_lock, seq));

	return ns_to_ktime(nsec);
}

void mlx4_en_fill_hwtstamps(struct mlx4_en_dev *mdev,
			    struct skb_shared_hwtstamps *hwts,
			    u64 timestamp)
{
	memset(hwts, 0, sizeof(struct skb_shared_hwtstamps));
	hwts->hwtstamp = mlx4_en_get_hwtstamp(mdev, timestamp);
}

 
void mlx4_en_remove_timestamp(struct mlx4_en_dev *mdev)
{
	if (mdev->ptp_clock) {
		ptp_clock_unregister(mdev->ptp_clock);
		mdev->ptp_clock = NULL;
		mlx4_info(mdev, "removed PHC\n");
	}
}

#define MLX4_EN_WRAP_AROUND_SEC	10UL
 
#define MLX4_EN_OVERFLOW_PERIOD (MLX4_EN_WRAP_AROUND_SEC * HZ / 2)

void mlx4_en_ptp_overflow_check(struct mlx4_en_dev *mdev)
{
	bool timeout = time_is_before_jiffies(mdev->last_overflow_check +
					      MLX4_EN_OVERFLOW_PERIOD);
	unsigned long flags;

	if (timeout) {
		write_seqlock_irqsave(&mdev->clock_lock, flags);
		timecounter_read(&mdev->clock);
		write_sequnlock_irqrestore(&mdev->clock_lock, flags);
		mdev->last_overflow_check = jiffies;
	}
}

 
static int mlx4_en_phc_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	u32 mult;
	unsigned long flags;
	struct mlx4_en_dev *mdev = container_of(ptp, struct mlx4_en_dev,
						ptp_clock_info);

	mult = (u32)adjust_by_scaled_ppm(mdev->nominal_c_mult, scaled_ppm);

	write_seqlock_irqsave(&mdev->clock_lock, flags);
	timecounter_read(&mdev->clock);
	mdev->cycles.mult = mult;
	write_sequnlock_irqrestore(&mdev->clock_lock, flags);

	return 0;
}

 
static int mlx4_en_phc_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct mlx4_en_dev *mdev = container_of(ptp, struct mlx4_en_dev,
						ptp_clock_info);
	unsigned long flags;

	write_seqlock_irqsave(&mdev->clock_lock, flags);
	timecounter_adjtime(&mdev->clock, delta);
	write_sequnlock_irqrestore(&mdev->clock_lock, flags);

	return 0;
}

 
static int mlx4_en_phc_gettime(struct ptp_clock_info *ptp,
			       struct timespec64 *ts)
{
	struct mlx4_en_dev *mdev = container_of(ptp, struct mlx4_en_dev,
						ptp_clock_info);
	unsigned long flags;
	u64 ns;

	write_seqlock_irqsave(&mdev->clock_lock, flags);
	ns = timecounter_read(&mdev->clock);
	write_sequnlock_irqrestore(&mdev->clock_lock, flags);

	*ts = ns_to_timespec64(ns);

	return 0;
}

 
static int mlx4_en_phc_settime(struct ptp_clock_info *ptp,
			       const struct timespec64 *ts)
{
	struct mlx4_en_dev *mdev = container_of(ptp, struct mlx4_en_dev,
						ptp_clock_info);
	u64 ns = timespec64_to_ns(ts);
	unsigned long flags;

	 
	write_seqlock_irqsave(&mdev->clock_lock, flags);
	timecounter_init(&mdev->clock, &mdev->cycles, ns);
	write_sequnlock_irqrestore(&mdev->clock_lock, flags);

	return 0;
}

 
static int mlx4_en_phc_enable(struct ptp_clock_info __always_unused *ptp,
			      struct ptp_clock_request __always_unused *request,
			      int __always_unused on)
{
	return -EOPNOTSUPP;
}

static const struct ptp_clock_info mlx4_en_ptp_clock_info = {
	.owner		= THIS_MODULE,
	.max_adj	= 100000000,
	.n_alarm	= 0,
	.n_ext_ts	= 0,
	.n_per_out	= 0,
	.n_pins		= 0,
	.pps		= 0,
	.adjfine	= mlx4_en_phc_adjfine,
	.adjtime	= mlx4_en_phc_adjtime,
	.gettime64	= mlx4_en_phc_gettime,
	.settime64	= mlx4_en_phc_settime,
	.enable		= mlx4_en_phc_enable,
};


 
static u32 freq_to_shift(u16 freq)
{
	u32 freq_khz = freq * 1000;
	u64 max_val_cycles = freq_khz * 1000 * MLX4_EN_WRAP_AROUND_SEC;
	u64 max_val_cycles_rounded = 1ULL << fls64(max_val_cycles - 1);
	 
	u64 max_mul = div64_u64(ULLONG_MAX, max_val_cycles_rounded);

	 
	return ilog2(div_u64(max_mul * freq_khz, 1000000));
}

void mlx4_en_init_timestamp(struct mlx4_en_dev *mdev)
{
	struct mlx4_dev *dev = mdev->dev;
	unsigned long flags;

	 
	if (mdev->ptp_clock)
		return;

	seqlock_init(&mdev->clock_lock);

	memset(&mdev->cycles, 0, sizeof(mdev->cycles));
	mdev->cycles.read = mlx4_en_read_clock;
	mdev->cycles.mask = CLOCKSOURCE_MASK(48);
	mdev->cycles.shift = freq_to_shift(dev->caps.hca_core_clock);
	mdev->cycles.mult =
		clocksource_khz2mult(1000 * dev->caps.hca_core_clock, mdev->cycles.shift);
	mdev->nominal_c_mult = mdev->cycles.mult;

	write_seqlock_irqsave(&mdev->clock_lock, flags);
	timecounter_init(&mdev->clock, &mdev->cycles,
			 ktime_to_ns(ktime_get_real()));
	write_sequnlock_irqrestore(&mdev->clock_lock, flags);

	 
	mdev->ptp_clock_info = mlx4_en_ptp_clock_info;
	snprintf(mdev->ptp_clock_info.name, 16, "mlx4 ptp");

	mdev->ptp_clock = ptp_clock_register(&mdev->ptp_clock_info,
					     &mdev->pdev->dev);
	if (IS_ERR(mdev->ptp_clock)) {
		mdev->ptp_clock = NULL;
		mlx4_err(mdev, "ptp_clock_register failed\n");
	} else if (mdev->ptp_clock) {
		mlx4_info(mdev, "registered PHC clock\n");
	}

}
