
 

#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/ptp_clock_kernel.h>
#include "common.h"
#include "stmmac_ptp.h"
#include "dwmac4.h"
#include "stmmac.h"

static void config_hw_tstamping(void __iomem *ioaddr, u32 data)
{
	writel(data, ioaddr + PTP_TCR);
}

static void config_sub_second_increment(void __iomem *ioaddr,
		u32 ptp_clock, int gmac4, u32 *ssinc)
{
	u32 value = readl(ioaddr + PTP_TCR);
	unsigned long data;
	u32 reg_value;

	 
	if (value & PTP_TCR_TSCFUPDT)
		data = (2000000000ULL / ptp_clock);
	else
		data = (1000000000ULL / ptp_clock);

	 
	if (!(value & PTP_TCR_TSCTRLSSR))
		data = (data * 1000) / 465;

	if (data > PTP_SSIR_SSINC_MAX)
		data = PTP_SSIR_SSINC_MAX;

	reg_value = data;
	if (gmac4)
		reg_value <<= GMAC4_PTP_SSIR_SSINC_SHIFT;

	writel(reg_value, ioaddr + PTP_SSIR);

	if (ssinc)
		*ssinc = data;
}

static void hwtstamp_correct_latency(struct stmmac_priv *priv)
{
	void __iomem *ioaddr = priv->ptpaddr;
	u32 reg_tsic, reg_tsicsns;
	u32 reg_tsec, reg_tsecsns;
	u64 scaled_ns;
	u32 val;

	 
	scaled_ns = readl(ioaddr + PTP_TS_INGR_LAT);

	 
	val = readl(ioaddr + PTP_TCR);
	if (val & PTP_TCR_TSCTRLSSR)
		 
		scaled_ns = ((u64)NSEC_PER_SEC << 16) - scaled_ns;
	else
		 
		scaled_ns = ((1ULL << 31) << 16) -
			DIV_U64_ROUND_CLOSEST(scaled_ns * PSEC_PER_NSEC, 466U);

	reg_tsic = scaled_ns >> 16;
	reg_tsicsns = scaled_ns & 0xff00;

	 
	reg_tsic |= BIT(31);

	writel(reg_tsic, ioaddr + PTP_TS_INGR_CORR_NS);
	writel(reg_tsicsns, ioaddr + PTP_TS_INGR_CORR_SNS);

	 
	scaled_ns = readl(ioaddr + PTP_TS_EGR_LAT);

	reg_tsec = scaled_ns >> 16;
	reg_tsecsns = scaled_ns & 0xff00;

	writel(reg_tsec, ioaddr + PTP_TS_EGR_CORR_NS);
	writel(reg_tsecsns, ioaddr + PTP_TS_EGR_CORR_SNS);
}

static int init_systime(void __iomem *ioaddr, u32 sec, u32 nsec)
{
	u32 value;

	writel(sec, ioaddr + PTP_STSUR);
	writel(nsec, ioaddr + PTP_STNSUR);
	 
	value = readl(ioaddr + PTP_TCR);
	value |= PTP_TCR_TSINIT;
	writel(value, ioaddr + PTP_TCR);

	 
	return readl_poll_timeout_atomic(ioaddr + PTP_TCR, value,
				 !(value & PTP_TCR_TSINIT),
				 10, 100000);
}

static int config_addend(void __iomem *ioaddr, u32 addend)
{
	u32 value;
	int limit;

	writel(addend, ioaddr + PTP_TAR);
	 
	value = readl(ioaddr + PTP_TCR);
	value |= PTP_TCR_TSADDREG;
	writel(value, ioaddr + PTP_TCR);

	 
	limit = 10;
	while (limit--) {
		if (!(readl(ioaddr + PTP_TCR) & PTP_TCR_TSADDREG))
			break;
		mdelay(10);
	}
	if (limit < 0)
		return -EBUSY;

	return 0;
}

static int adjust_systime(void __iomem *ioaddr, u32 sec, u32 nsec,
		int add_sub, int gmac4)
{
	u32 value;
	int limit;

	if (add_sub) {
		 
		if (gmac4)
			sec = -sec;

		value = readl(ioaddr + PTP_TCR);
		if (value & PTP_TCR_TSCTRLSSR)
			nsec = (PTP_DIGITAL_ROLLOVER_MODE - nsec);
		else
			nsec = (PTP_BINARY_ROLLOVER_MODE - nsec);
	}

	writel(sec, ioaddr + PTP_STSUR);
	value = (add_sub << PTP_STNSUR_ADDSUB_SHIFT) | nsec;
	writel(value, ioaddr + PTP_STNSUR);

	 
	value = readl(ioaddr + PTP_TCR);
	value |= PTP_TCR_TSUPDT;
	writel(value, ioaddr + PTP_TCR);

	 
	limit = 10;
	while (limit--) {
		if (!(readl(ioaddr + PTP_TCR) & PTP_TCR_TSUPDT))
			break;
		mdelay(10);
	}
	if (limit < 0)
		return -EBUSY;

	return 0;
}

static void get_systime(void __iomem *ioaddr, u64 *systime)
{
	u64 ns, sec0, sec1;

	 
	sec1 = readl_relaxed(ioaddr + PTP_STSR);
	do {
		sec0 = sec1;
		 
		ns = readl_relaxed(ioaddr + PTP_STNSR);
		 
		sec1 = readl_relaxed(ioaddr + PTP_STSR);
	} while (sec0 != sec1);

	if (systime)
		*systime = ns + (sec1 * 1000000000ULL);
}

static void get_ptptime(void __iomem *ptpaddr, u64 *ptp_time)
{
	u64 ns;

	ns = readl(ptpaddr + PTP_ATNR);
	ns += readl(ptpaddr + PTP_ATSR) * NSEC_PER_SEC;

	*ptp_time = ns;
}

static void timestamp_interrupt(struct stmmac_priv *priv)
{
	u32 num_snapshot, ts_status, tsync_int;
	struct ptp_clock_event event;
	unsigned long flags;
	u64 ptp_time;
	int i;

	if (priv->plat->flags & STMMAC_FLAG_INT_SNAPSHOT_EN) {
		wake_up(&priv->tstamp_busy_wait);
		return;
	}

	tsync_int = readl(priv->ioaddr + GMAC_INT_STATUS) & GMAC_INT_TSIE;

	if (!tsync_int)
		return;

	 
	ts_status = readl(priv->ioaddr + GMAC_TIMESTAMP_STATUS);

	if (!(priv->plat->flags & STMMAC_FLAG_EXT_SNAPSHOT_EN))
		return;

	num_snapshot = (ts_status & GMAC_TIMESTAMP_ATSNS_MASK) >>
		       GMAC_TIMESTAMP_ATSNS_SHIFT;

	for (i = 0; i < num_snapshot; i++) {
		read_lock_irqsave(&priv->ptp_lock, flags);
		get_ptptime(priv->ptpaddr, &ptp_time);
		read_unlock_irqrestore(&priv->ptp_lock, flags);
		event.type = PTP_CLOCK_EXTTS;
		event.index = 0;
		event.timestamp = ptp_time;
		ptp_clock_event(priv->ptp_clock, &event);
	}
}

const struct stmmac_hwtimestamp stmmac_ptp = {
	.config_hw_tstamping = config_hw_tstamping,
	.init_systime = init_systime,
	.config_sub_second_increment = config_sub_second_increment,
	.config_addend = config_addend,
	.adjust_systime = adjust_systime,
	.get_systime = get_systime,
	.get_ptptime = get_ptptime,
	.timestamp_interrupt = timestamp_interrupt,
	.hwtstamp_correct_latency = hwtstamp_correct_latency,
};
