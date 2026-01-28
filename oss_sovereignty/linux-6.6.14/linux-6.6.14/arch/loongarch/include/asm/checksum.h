#ifndef __ASM_CHECKSUM_H
#define __ASM_CHECKSUM_H
#include <linux/bitops.h>
#include <linux/in6.h>
#define _HAVE_ARCH_IPV6_CSUM
__sum16 csum_ipv6_magic(const struct in6_addr *saddr,
			const struct in6_addr *daddr,
			__u32 len, __u8 proto, __wsum sum);
static inline __sum16 csum_fold(__wsum sum)
{
	u32 tmp = (__force u32)sum;
	return (__force __sum16)(~(tmp + rol32(tmp, 16)) >> 16);
}
#define csum_fold csum_fold
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	u64 sum;
	__uint128_t tmp;
	int n = ihl;  
	tmp = *(const __uint128_t *)iph;
	iph += 16;
	n -= 4;
	tmp += ((tmp >> 64) | (tmp << 64));
	sum = tmp >> 64;
	do {
		sum += *(const u32 *)iph;
		iph += 4;
	} while (--n > 0);
	sum += ror64(sum, 32);
	return csum_fold((__force __wsum)(sum >> 32));
}
#define ip_fast_csum ip_fast_csum
extern unsigned int do_csum(const unsigned char *buff, int len);
#define do_csum do_csum
#include <asm-generic/checksum.h>
#endif	 
