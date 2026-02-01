
 

#include <linux/raid/pq.h>
#include "loongarch.h"

 

#ifdef CONFIG_CPU_HAS_LSX
static int raid6_has_lsx(void)
{
	return cpu_has_lsx;
}

static void raid6_2data_recov_lsx(int disks, size_t bytes, int faila,
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

	 
	ptrs[faila] = dp;
	ptrs[failb] = dq;
	ptrs[disks - 2] = p;
	ptrs[disks - 1] = q;

	 
	pbmul = raid6_vgfmul[raid6_gfexi[failb - faila]];
	qmul  = raid6_vgfmul[raid6_gfinv[raid6_gfexp[faila] ^ raid6_gfexp[failb]]];

	kernel_fpu_begin();

	 
	asm volatile("vld $vr20, %0" : : "m" (qmul[0]));
	asm volatile("vld $vr21, %0" : : "m" (qmul[16]));
	asm volatile("vld $vr22, %0" : : "m" (pbmul[0]));
	asm volatile("vld $vr23, %0" : : "m" (pbmul[16]));

	while (bytes) {
		 
		asm volatile("vld $vr4, %0" : : "m" (q[0]));
		asm volatile("vld $vr5, %0" : : "m" (q[16]));
		asm volatile("vld $vr6, %0" : : "m" (q[32]));
		asm volatile("vld $vr7, %0" : : "m" (q[48]));
		 
		asm volatile("vld $vr8, %0" : : "m" (dq[0]));
		asm volatile("vld $vr9, %0" : : "m" (dq[16]));
		asm volatile("vld $vr10, %0" : : "m" (dq[32]));
		asm volatile("vld $vr11, %0" : : "m" (dq[48]));
		asm volatile("vxor.v $vr4, $vr4, $vr8");
		asm volatile("vxor.v $vr5, $vr5, $vr9");
		asm volatile("vxor.v $vr6, $vr6, $vr10");
		asm volatile("vxor.v $vr7, $vr7, $vr11");
		 
		asm volatile("vld $vr0, %0" : : "m" (p[0]));
		asm volatile("vld $vr1, %0" : : "m" (p[16]));
		asm volatile("vld $vr2, %0" : : "m" (p[32]));
		asm volatile("vld $vr3, %0" : : "m" (p[48]));
		 
		asm volatile("vld $vr8, %0" : : "m" (dp[0]));
		asm volatile("vld $vr9, %0" : : "m" (dp[16]));
		asm volatile("vld $vr10, %0" : : "m" (dp[32]));
		asm volatile("vld $vr11, %0" : : "m" (dp[48]));
		asm volatile("vxor.v $vr0, $vr0, $vr8");
		asm volatile("vxor.v $vr1, $vr1, $vr9");
		asm volatile("vxor.v $vr2, $vr2, $vr10");
		asm volatile("vxor.v $vr3, $vr3, $vr11");

		 
		asm volatile("vsrli.b $vr8, $vr4, 4");
		asm volatile("vsrli.b $vr9, $vr5, 4");
		asm volatile("vsrli.b $vr10, $vr6, 4");
		asm volatile("vsrli.b $vr11, $vr7, 4");
		 
		asm volatile("vandi.b $vr4, $vr4, 0x0f");
		asm volatile("vandi.b $vr5, $vr5, 0x0f");
		asm volatile("vandi.b $vr6, $vr6, 0x0f");
		asm volatile("vandi.b $vr7, $vr7, 0x0f");
		 
		asm volatile("vshuf.b $vr4, $vr20, $vr20, $vr4");
		asm volatile("vshuf.b $vr5, $vr20, $vr20, $vr5");
		asm volatile("vshuf.b $vr6, $vr20, $vr20, $vr6");
		asm volatile("vshuf.b $vr7, $vr20, $vr20, $vr7");
		 
		asm volatile("vshuf.b $vr8, $vr21, $vr21, $vr8");
		asm volatile("vshuf.b $vr9, $vr21, $vr21, $vr9");
		asm volatile("vshuf.b $vr10, $vr21, $vr21, $vr10");
		asm volatile("vshuf.b $vr11, $vr21, $vr21, $vr11");
		 
		asm volatile("vxor.v $vr16, $vr8, $vr4");
		asm volatile("vxor.v $vr17, $vr9, $vr5");
		asm volatile("vxor.v $vr18, $vr10, $vr6");
		asm volatile("vxor.v $vr19, $vr11, $vr7");

		 
		asm volatile("vsrli.b $vr4, $vr0, 4");
		asm volatile("vsrli.b $vr5, $vr1, 4");
		asm volatile("vsrli.b $vr6, $vr2, 4");
		asm volatile("vsrli.b $vr7, $vr3, 4");
		 
		asm volatile("vandi.b $vr12, $vr0, 0x0f");
		asm volatile("vandi.b $vr13, $vr1, 0x0f");
		asm volatile("vandi.b $vr14, $vr2, 0x0f");
		asm volatile("vandi.b $vr15, $vr3, 0x0f");
		 
		asm volatile("vshuf.b $vr12, $vr22, $vr22, $vr12");
		asm volatile("vshuf.b $vr13, $vr22, $vr22, $vr13");
		asm volatile("vshuf.b $vr14, $vr22, $vr22, $vr14");
		asm volatile("vshuf.b $vr15, $vr22, $vr22, $vr15");
		 
		asm volatile("vshuf.b $vr4, $vr23, $vr23, $vr4");
		asm volatile("vshuf.b $vr5, $vr23, $vr23, $vr5");
		asm volatile("vshuf.b $vr6, $vr23, $vr23, $vr6");
		asm volatile("vshuf.b $vr7, $vr23, $vr23, $vr7");
		 
		asm volatile("vxor.v $vr4, $vr4, $vr12");
		asm volatile("vxor.v $vr5, $vr5, $vr13");
		asm volatile("vxor.v $vr6, $vr6, $vr14");
		asm volatile("vxor.v $vr7, $vr7, $vr15");

		 
		asm volatile("vxor.v $vr4, $vr4, $vr16");
		asm volatile("vxor.v $vr5, $vr5, $vr17");
		asm volatile("vxor.v $vr6, $vr6, $vr18");
		asm volatile("vxor.v $vr7, $vr7, $vr19");
		asm volatile("vst $vr4, %0" : "=m" (dq[0]));
		asm volatile("vst $vr5, %0" : "=m" (dq[16]));
		asm volatile("vst $vr6, %0" : "=m" (dq[32]));
		asm volatile("vst $vr7, %0" : "=m" (dq[48]));

		 
		asm volatile("vxor.v $vr0, $vr0, $vr4");
		asm volatile("vxor.v $vr1, $vr1, $vr5");
		asm volatile("vxor.v $vr2, $vr2, $vr6");
		asm volatile("vxor.v $vr3, $vr3, $vr7");
		asm volatile("vst $vr0, %0" : "=m" (dp[0]));
		asm volatile("vst $vr1, %0" : "=m" (dp[16]));
		asm volatile("vst $vr2, %0" : "=m" (dp[32]));
		asm volatile("vst $vr3, %0" : "=m" (dp[48]));

		bytes -= 64;
		p += 64;
		q += 64;
		dp += 64;
		dq += 64;
	}

	kernel_fpu_end();
}

static void raid6_datap_recov_lsx(int disks, size_t bytes, int faila,
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

	 
	ptrs[faila] = dq;
	ptrs[disks - 1] = q;

	 
	qmul  = raid6_vgfmul[raid6_gfinv[raid6_gfexp[faila]]];

	kernel_fpu_begin();

	 
	asm volatile("vld $vr22, %0" : : "m" (qmul[0]));
	asm volatile("vld $vr23, %0" : : "m" (qmul[16]));

	while (bytes) {
		 
		asm volatile("vld $vr0, %0" : : "m" (p[0]));
		asm volatile("vld $vr1, %0" : : "m" (p[16]));
		asm volatile("vld $vr2, %0" : : "m" (p[32]));
		asm volatile("vld $vr3, %0" : : "m" (p[48]));
		 
		asm volatile("vld $vr4, %0" : : "m" (dq[0]));
		asm volatile("vld $vr5, %0" : : "m" (dq[16]));
		asm volatile("vld $vr6, %0" : : "m" (dq[32]));
		asm volatile("vld $vr7, %0" : : "m" (dq[48]));
		 
		asm volatile("vld $vr8, %0" : : "m" (q[0]));
		asm volatile("vld $vr9, %0" : : "m" (q[16]));
		asm volatile("vld $vr10, %0" : : "m" (q[32]));
		asm volatile("vld $vr11, %0" : : "m" (q[48]));
		asm volatile("vxor.v $vr4, $vr4, $vr8");
		asm volatile("vxor.v $vr5, $vr5, $vr9");
		asm volatile("vxor.v $vr6, $vr6, $vr10");
		asm volatile("vxor.v $vr7, $vr7, $vr11");

		 
		asm volatile("vsrli.b $vr8, $vr4, 4");
		asm volatile("vsrli.b $vr9, $vr5, 4");
		asm volatile("vsrli.b $vr10, $vr6, 4");
		asm volatile("vsrli.b $vr11, $vr7, 4");
		 
		asm volatile("vandi.b $vr4, $vr4, 0x0f");
		asm volatile("vandi.b $vr5, $vr5, 0x0f");
		asm volatile("vandi.b $vr6, $vr6, 0x0f");
		asm volatile("vandi.b $vr7, $vr7, 0x0f");
		 
		asm volatile("vshuf.b $vr4, $vr22, $vr22, $vr4");
		asm volatile("vshuf.b $vr5, $vr22, $vr22, $vr5");
		asm volatile("vshuf.b $vr6, $vr22, $vr22, $vr6");
		asm volatile("vshuf.b $vr7, $vr22, $vr22, $vr7");
		 
		asm volatile("vshuf.b $vr8, $vr23, $vr23, $vr8");
		asm volatile("vshuf.b $vr9, $vr23, $vr23, $vr9");
		asm volatile("vshuf.b $vr10, $vr23, $vr23, $vr10");
		asm volatile("vshuf.b $vr11, $vr23, $vr23, $vr11");
		 
		asm volatile("vxor.v $vr4, $vr4, $vr8");
		asm volatile("vxor.v $vr5, $vr5, $vr9");
		asm volatile("vxor.v $vr6, $vr6, $vr10");
		asm volatile("vxor.v $vr7, $vr7, $vr11");
		asm volatile("vst $vr4, %0" : "=m" (dq[0]));
		asm volatile("vst $vr5, %0" : "=m" (dq[16]));
		asm volatile("vst $vr6, %0" : "=m" (dq[32]));
		asm volatile("vst $vr7, %0" : "=m" (dq[48]));

		 
		asm volatile("vxor.v $vr0, $vr0, $vr4");
		asm volatile("vxor.v $vr1, $vr1, $vr5");
		asm volatile("vxor.v $vr2, $vr2, $vr6");
		asm volatile("vxor.v $vr3, $vr3, $vr7");
		asm volatile("vst $vr0, %0" : "=m" (p[0]));
		asm volatile("vst $vr1, %0" : "=m" (p[16]));
		asm volatile("vst $vr2, %0" : "=m" (p[32]));
		asm volatile("vst $vr3, %0" : "=m" (p[48]));

		bytes -= 64;
		p += 64;
		q += 64;
		dq += 64;
	}

	kernel_fpu_end();
}

const struct raid6_recov_calls raid6_recov_lsx = {
	.data2 = raid6_2data_recov_lsx,
	.datap = raid6_datap_recov_lsx,
	.valid = raid6_has_lsx,
	.name = "lsx",
	.priority = 1,
};
#endif  

#ifdef CONFIG_CPU_HAS_LASX
static int raid6_has_lasx(void)
{
	return cpu_has_lasx;
}

static void raid6_2data_recov_lasx(int disks, size_t bytes, int faila,
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

	 
	ptrs[faila] = dp;
	ptrs[failb] = dq;
	ptrs[disks - 2] = p;
	ptrs[disks - 1] = q;

	 
	pbmul = raid6_vgfmul[raid6_gfexi[failb - faila]];
	qmul  = raid6_vgfmul[raid6_gfinv[raid6_gfexp[faila] ^ raid6_gfexp[failb]]];

	kernel_fpu_begin();

	 
	asm volatile("vld $vr20, %0" : : "m" (qmul[0]));
	asm volatile("vld $vr21, %0" : : "m" (qmul[16]));
	asm volatile("vld $vr22, %0" : : "m" (pbmul[0]));
	asm volatile("vld $vr23, %0" : : "m" (pbmul[16]));
	asm volatile("xvreplve0.q $xr20, $xr20");
	asm volatile("xvreplve0.q $xr21, $xr21");
	asm volatile("xvreplve0.q $xr22, $xr22");
	asm volatile("xvreplve0.q $xr23, $xr23");

	while (bytes) {
		 
		asm volatile("xvld $xr0, %0" : : "m" (q[0]));
		asm volatile("xvld $xr1, %0" : : "m" (q[32]));
		 
		asm volatile("xvld $xr4, %0" : : "m" (dq[0]));
		asm volatile("xvld $xr5, %0" : : "m" (dq[32]));
		asm volatile("xvxor.v $xr0, $xr0, $xr4");
		asm volatile("xvxor.v $xr1, $xr1, $xr5");
		 
		asm volatile("xvld $xr2, %0" : : "m" (p[0]));
		asm volatile("xvld $xr3, %0" : : "m" (p[32]));
		 
		asm volatile("xvld $xr4, %0" : : "m" (dp[0]));
		asm volatile("xvld $xr5, %0" : : "m" (dp[32]));
		asm volatile("xvxor.v $xr2, $xr2, $xr4");
		asm volatile("xvxor.v $xr3, $xr3, $xr5");

		 
		asm volatile("xvsrli.b $xr4, $xr0, 4");
		asm volatile("xvsrli.b $xr5, $xr1, 4");
		 
		asm volatile("xvandi.b $xr0, $xr0, 0x0f");
		asm volatile("xvandi.b $xr1, $xr1, 0x0f");
		 
		asm volatile("xvshuf.b $xr0, $xr20, $xr20, $xr0");
		asm volatile("xvshuf.b $xr1, $xr20, $xr20, $xr1");
		 
		asm volatile("xvshuf.b $xr4, $xr21, $xr21, $xr4");
		asm volatile("xvshuf.b $xr5, $xr21, $xr21, $xr5");
		 
		asm volatile("xvxor.v $xr6, $xr4, $xr0");
		asm volatile("xvxor.v $xr7, $xr5, $xr1");

		 
		asm volatile("xvsrli.b $xr4, $xr2, 4");
		asm volatile("xvsrli.b $xr5, $xr3, 4");
		 
		asm volatile("xvandi.b $xr0, $xr2, 0x0f");
		asm volatile("xvandi.b $xr1, $xr3, 0x0f");
		 
		asm volatile("xvshuf.b $xr0, $xr22, $xr22, $xr0");
		asm volatile("xvshuf.b $xr1, $xr22, $xr22, $xr1");
		 
		asm volatile("xvshuf.b $xr4, $xr23, $xr23, $xr4");
		asm volatile("xvshuf.b $xr5, $xr23, $xr23, $xr5");
		 
		asm volatile("xvxor.v $xr0, $xr0, $xr4");
		asm volatile("xvxor.v $xr1, $xr1, $xr5");

		 
		asm volatile("xvxor.v $xr0, $xr0, $xr6");
		asm volatile("xvxor.v $xr1, $xr1, $xr7");

		 
		asm volatile("xvxor.v $xr2, $xr2, $xr0");
		asm volatile("xvxor.v $xr3, $xr3, $xr1");

		asm volatile("xvst $xr0, %0" : "=m" (dq[0]));
		asm volatile("xvst $xr1, %0" : "=m" (dq[32]));
		asm volatile("xvst $xr2, %0" : "=m" (dp[0]));
		asm volatile("xvst $xr3, %0" : "=m" (dp[32]));

		bytes -= 64;
		p += 64;
		q += 64;
		dp += 64;
		dq += 64;
	}

	kernel_fpu_end();
}

static void raid6_datap_recov_lasx(int disks, size_t bytes, int faila,
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

	 
	ptrs[faila] = dq;
	ptrs[disks - 1] = q;

	 
	qmul  = raid6_vgfmul[raid6_gfinv[raid6_gfexp[faila]]];

	kernel_fpu_begin();

	 
	asm volatile("vld $vr22, %0" : : "m" (qmul[0]));
	asm volatile("xvreplve0.q $xr22, $xr22");
	asm volatile("vld $vr23, %0" : : "m" (qmul[16]));
	asm volatile("xvreplve0.q $xr23, $xr23");

	while (bytes) {
		 
		asm volatile("xvld $xr0, %0" : : "m" (p[0]));
		asm volatile("xvld $xr1, %0" : : "m" (p[32]));
		 
		asm volatile("xvld $xr2, %0" : : "m" (dq[0]));
		asm volatile("xvld $xr3, %0" : : "m" (dq[32]));
		 
		asm volatile("xvld $xr4, %0" : : "m" (q[0]));
		asm volatile("xvld $xr5, %0" : : "m" (q[32]));
		asm volatile("xvxor.v $xr2, $xr2, $xr4");
		asm volatile("xvxor.v $xr3, $xr3, $xr5");

		 
		asm volatile("xvsrli.b $xr4, $xr2, 4");
		asm volatile("xvsrli.b $xr5, $xr3, 4");
		 
		asm volatile("xvandi.b $xr2, $xr2, 0x0f");
		asm volatile("xvandi.b $xr3, $xr3, 0x0f");
		 
		asm volatile("xvshuf.b $xr2, $xr22, $xr22, $xr2");
		asm volatile("xvshuf.b $xr3, $xr22, $xr22, $xr3");
		 
		asm volatile("xvshuf.b $xr4, $xr23, $xr23, $xr4");
		asm volatile("xvshuf.b $xr5, $xr23, $xr23, $xr5");
		 
		asm volatile("xvxor.v $xr2, $xr2, $xr4");
		asm volatile("xvxor.v $xr3, $xr3, $xr5");

		 
		asm volatile("xvxor.v $xr0, $xr0, $xr2");
		asm volatile("xvxor.v $xr1, $xr1, $xr3");

		asm volatile("xvst $xr2, %0" : "=m" (dq[0]));
		asm volatile("xvst $xr3, %0" : "=m" (dq[32]));
		asm volatile("xvst $xr0, %0" : "=m" (p[0]));
		asm volatile("xvst $xr1, %0" : "=m" (p[32]));

		bytes -= 64;
		p += 64;
		q += 64;
		dq += 64;
	}

	kernel_fpu_end();
}

const struct raid6_recov_calls raid6_recov_lasx = {
	.data2 = raid6_2data_recov_lasx,
	.datap = raid6_datap_recov_lasx,
	.valid = raid6_has_lasx,
	.name = "lasx",
	.priority = 2,
};
#endif  
