


#ifndef _HELLCREEK_PTP_H_
#define _HELLCREEK_PTP_H_

#include <linux/bitops.h>
#include <linux/ptp_clock_kernel.h>

#include "hellcreek.h"


#define MAX_NS_PER_STEP			7L


#define MIN_CLK_CYCLES_BETWEEN_STEPS	0


#define MAX_SLOW_OFFSET_ADJ					\
	((unsigned long long)((1 << 30) - 1) * MAX_NS_PER_STEP)


#define HELLCREEK_OVERFLOW_PERIOD	(HZ / 4)


#define PR_SETTINGS_C			(0x09 * 2)
#define PR_SETTINGS_C_RES3TS		BIT(4)
#define PR_SETTINGS_C_TS_SRC_TK_SHIFT	8
#define PR_SETTINGS_C_TS_SRC_TK_MASK	GENMASK(9, 8)
#define PR_COMMAND_C			(0x0a * 2)
#define PR_COMMAND_C_SS			BIT(0)

#define PR_CLOCK_STATUS_C		(0x0c * 2)
#define PR_CLOCK_STATUS_C_ENA_DRIFT	BIT(12)
#define PR_CLOCK_STATUS_C_OFS_ACT	BIT(13)
#define PR_CLOCK_STATUS_C_ENA_OFS	BIT(14)

#define PR_CLOCK_READ_C			(0x0d * 2)
#define PR_CLOCK_WRITE_C		(0x0e * 2)
#define PR_CLOCK_OFFSET_C		(0x0f * 2)
#define PR_CLOCK_DRIFT_C		(0x10 * 2)

#define PR_SS_FREE_DATA_C		(0x12 * 2)
#define PR_SS_SYNT_DATA_C		(0x14 * 2)
#define PR_SS_SYNC_DATA_C		(0x16 * 2)
#define PR_SS_DRAC_DATA_C		(0x18 * 2)

#define STATUS_OUT			(0x60 * 2)
#define STATUS_OUT_SYNC_GOOD		BIT(0)
#define STATUS_OUT_IS_GM		BIT(1)

int hellcreek_ptp_setup(struct hellcreek *hellcreek);
void hellcreek_ptp_free(struct hellcreek *hellcreek);
u16 hellcreek_ptp_read(struct hellcreek *hellcreek, unsigned int offset);
void hellcreek_ptp_write(struct hellcreek *hellcreek, u16 data,
			 unsigned int offset);
u64 hellcreek_ptp_gettime_seconds(struct hellcreek *hellcreek, u64 ns);

#define ptp_to_hellcreek(ptp)					\
	container_of(ptp, struct hellcreek, ptp_clock_info)

#define dw_overflow_to_hellcreek(dw)				\
	container_of(dw, struct hellcreek, overflow_work)

#define led_to_hellcreek(ldev, led)				\
	container_of(ldev, struct hellcreek, led)

#endif 
