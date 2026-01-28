#ifndef _ASM_ARC_CHECKSUM_H
#define _ASM_ARC_CHECKSUM_H
static inline __sum16 csum_fold(__wsum s)
{
	unsigned int r = s << 16 | s >> 16;	 
	s = ~s;
	s -= r;
	return s >> 16;
}
static inline __sum16
ip_fast_csum(const void *iph, unsigned int ihl)
{
	const void *ptr = iph;
	unsigned int tmp, tmp2, sum;
	__asm__(
	"	ld.ab  %0, [%3, 4]		\n"
	"	ld.ab  %2, [%3, 4]		\n"
	"	sub    %1, %4, 2		\n"
	"	lsr.f  lp_count, %1, 1		\n"
	"	bcc    0f			\n"
	"	add.f  %0, %0, %2		\n"
	"	ld.ab  %2, [%3, 4]		\n"
	"0:	lp     1f			\n"
	"	ld.ab  %1, [%3, 4]		\n"
	"	adc.f  %0, %0, %2		\n"
	"	ld.ab  %2, [%3, 4]		\n"
	"	adc.f  %0, %0, %1		\n"
	"1:	adc.f  %0, %0, %2		\n"
	"	add.cs %0,%0,1			\n"
	: "=&r"(sum), "=r"(tmp), "=&r"(tmp2), "+&r" (ptr)
	: "r"(ihl)
	: "cc", "lp_count", "memory");
	return csum_fold(sum);
}
static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, __u32 len,
		   __u8 proto, __wsum sum)
{
	__asm__ __volatile__(
	"	add.f %0, %0, %1	\n"
	"	adc.f %0, %0, %2	\n"
	"	adc.f %0, %0, %3	\n"
	"	adc.f %0, %0, %4	\n"
	"	adc   %0, %0, 0		\n"
	: "+&r"(sum)
	: "r"(saddr), "r"(daddr),
#ifdef CONFIG_CPU_BIG_ENDIAN
	  "r"(len),
#else
	  "r"(len << 8),
#endif
	  "r"(htons(proto))
	: "cc");
	return sum;
}
#define csum_fold csum_fold
#define ip_fast_csum ip_fast_csum
#define csum_tcpudp_nofold csum_tcpudp_nofold
#include <asm-generic/checksum.h>
#endif  
