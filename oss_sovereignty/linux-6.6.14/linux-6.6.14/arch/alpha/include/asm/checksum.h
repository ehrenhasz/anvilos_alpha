#ifndef _ALPHA_CHECKSUM_H
#define _ALPHA_CHECKSUM_H
#include <linux/in6.h>
extern __sum16 ip_fast_csum(const void *iph, unsigned int ihl);
__sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
			  __u32 len, __u8 proto, __wsum sum);
__wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
			  __u32 len, __u8 proto, __wsum sum);
extern __wsum csum_partial(const void *buff, int len, __wsum sum);
#define _HAVE_ARCH_COPY_AND_CSUM_FROM_USER
#define _HAVE_ARCH_CSUM_AND_COPY
__wsum csum_and_copy_from_user(const void __user *src, void *dst, int len);
__wsum csum_partial_copy_nocheck(const void *src, void *dst, int len);
extern __sum16 ip_compute_csum(const void *buff, int len);
static inline __sum16 csum_fold(__wsum csum)
{
	u32 sum = (__force u32)csum;
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return (__force __sum16)~sum;
}
#define _HAVE_ARCH_IPV6_CSUM
extern __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
			       const struct in6_addr *daddr,
			       __u32 len, __u8 proto, __wsum sum);
#endif
