
 

#include <linux/raid/pq.h>

#ifdef __KERNEL__
#include <asm/neon.h>
#include "neon.h"
#else
#define kernel_neon_begin()
#define kernel_neon_end()
#define cpu_has_neon()		(1)
#endif

static int raid6_has_neon(void)
{
	return cpu_has_neon();
}

static void raid6_2data_recov_neon(int disks, size_t bytes, int faila,
		int failb, void **ptrs)
{
	u8 *p, *q, *dp, *dq;
	const u8 *pbmul;	 
	const u8 *qmul;		 

	p = (u8 *)ptrs[disks - 2];
	q = (u8 *)ptrs[disks - 1];

	 
	dp = (u8 *)ptrs[faila];
	ptrs[faila] = (void *)raid6_empty_zero_page;
	ptrs[disks - 2] = dp;
	dq = (u8 *)ptrs[failb];
	ptrs[failb] = (void *)raid6_empty_zero_page;
	ptrs[disks - 1] = dq;

	raid6_call.gen_syndrome(disks, bytes, ptrs);

	 
	ptrs[faila]     = dp;
	ptrs[failb]     = dq;
	ptrs[disks - 2] = p;
	ptrs[disks - 1] = q;

	 
	pbmul = raid6_vgfmul[raid6_gfexi[failb-faila]];
	qmul  = raid6_vgfmul[raid6_gfinv[raid6_gfexp[faila] ^
					 raid6_gfexp[failb]]];

	kernel_neon_begin();
	__raid6_2data_recov_neon(bytes, p, q, dp, dq, pbmul, qmul);
	kernel_neon_end();
}

static void raid6_datap_recov_neon(int disks, size_t bytes, int faila,
		void **ptrs)
{
	u8 *p, *q, *dq;
	const u8 *qmul;		 

	p = (u8 *)ptrs[disks - 2];
	q = (u8 *)ptrs[disks - 1];

	 
	dq = (u8 *)ptrs[faila];
	ptrs[faila] = (void *)raid6_empty_zero_page;
	ptrs[disks - 1] = dq;

	raid6_call.gen_syndrome(disks, bytes, ptrs);

	 
	ptrs[faila]     = dq;
	ptrs[disks - 1] = q;

	 
	qmul = raid6_vgfmul[raid6_gfinv[raid6_gfexp[faila]]];

	kernel_neon_begin();
	__raid6_datap_recov_neon(bytes, p, q, dq, qmul);
	kernel_neon_end();
}

const struct raid6_recov_calls raid6_recov_neon = {
	.data2		= raid6_2data_recov_neon,
	.datap		= raid6_datap_recov_neon,
	.valid		= raid6_has_neon,
	.name		= "neon",
	.priority	= 10,
};
