#ifndef _S390_CHECKSUM_H
#define _S390_CHECKSUM_H
#include <linux/kasan-checks.h>
#include <linux/in6.h>
static inline __wsum csum_partial(const void *buff, int len, __wsum sum)
{
	union register_pair rp = {
		.even = (unsigned long) buff,
		.odd = (unsigned long) len,
	};
	kasan_check_read(buff, len);
	asm volatile(
		"0:	cksm	%[sum],%[rp]\n"
		"	jo	0b\n"
		: [sum] "+&d" (sum), [rp] "+&d" (rp.pair) : : "cc", "memory");
	return sum;
}
static inline __sum16 csum_fold(__wsum sum)
{
	u32 csum = (__force u32) sum;
	csum += (csum >> 16) | (csum << 16);
	csum >>= 16;
	return (__force __sum16) ~csum;
}
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	__u64 csum = 0;
	__u32 *ptr = (u32 *)iph;
	csum += *ptr++;
	csum += *ptr++;
	csum += *ptr++;
	csum += *ptr++;
	ihl -= 4;
	while (ihl--)
		csum += *ptr++;
	csum += (csum >> 32) | (csum << 32);
	return csum_fold((__force __wsum)(csum >> 32));
}
static inline __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr, __u32 len,
					__u8 proto, __wsum sum)
{
	__u64 csum = (__force __u64)sum;
	csum += (__force __u32)saddr;
	csum += (__force __u32)daddr;
	csum += len;
	csum += proto;
	csum += (csum >> 32) | (csum << 32);
	return (__force __wsum)(csum >> 32);
}
static inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr, __u32 len,
					__u8 proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}
static inline __sum16 ip_compute_csum(const void *buff, int len)
{
	return csum_fold(csum_partial(buff, len, 0));
}
#define _HAVE_ARCH_IPV6_CSUM
static inline __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
				      const struct in6_addr *daddr,
				      __u32 len, __u8 proto, __wsum csum)
{
	__u64 sum = (__force __u64)csum;
	sum += (__force __u32)saddr->s6_addr32[0];
	sum += (__force __u32)saddr->s6_addr32[1];
	sum += (__force __u32)saddr->s6_addr32[2];
	sum += (__force __u32)saddr->s6_addr32[3];
	sum += (__force __u32)daddr->s6_addr32[0];
	sum += (__force __u32)daddr->s6_addr32[1];
	sum += (__force __u32)daddr->s6_addr32[2];
	sum += (__force __u32)daddr->s6_addr32[3];
	sum += len;
	sum += proto;
	sum += (sum >> 32) | (sum << 32);
	return csum_fold((__force __wsum)(sum >> 32));
}
#endif  
