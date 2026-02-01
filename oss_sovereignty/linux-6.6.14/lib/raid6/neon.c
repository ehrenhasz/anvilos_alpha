
 

#include <linux/raid/pq.h>

#ifdef __KERNEL__
#include <asm/neon.h>
#else
#define kernel_neon_begin()
#define kernel_neon_end()
#define cpu_has_neon()		(1)
#endif

 

#define RAID6_NEON_WRAPPER(_n)						\
	static void raid6_neon ## _n ## _gen_syndrome(int disks,	\
					size_t bytes, void **ptrs)	\
	{								\
		void raid6_neon ## _n  ## _gen_syndrome_real(int,	\
						unsigned long, void**);	\
		kernel_neon_begin();					\
		raid6_neon ## _n ## _gen_syndrome_real(disks,		\
					(unsigned long)bytes, ptrs);	\
		kernel_neon_end();					\
	}								\
	static void raid6_neon ## _n ## _xor_syndrome(int disks,	\
					int start, int stop, 		\
					size_t bytes, void **ptrs)	\
	{								\
		void raid6_neon ## _n  ## _xor_syndrome_real(int,	\
				int, int, unsigned long, void**);	\
		kernel_neon_begin();					\
		raid6_neon ## _n ## _xor_syndrome_real(disks,		\
			start, stop, (unsigned long)bytes, ptrs);	\
		kernel_neon_end();					\
	}								\
	struct raid6_calls const raid6_neonx ## _n = {			\
		raid6_neon ## _n ## _gen_syndrome,			\
		raid6_neon ## _n ## _xor_syndrome,			\
		raid6_have_neon,					\
		"neonx" #_n,						\
		0							\
	}

static int raid6_have_neon(void)
{
	return cpu_has_neon();
}

RAID6_NEON_WRAPPER(1);
RAID6_NEON_WRAPPER(2);
RAID6_NEON_WRAPPER(4);
RAID6_NEON_WRAPPER(8);
