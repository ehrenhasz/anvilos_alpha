#ifndef _ASM_IA64_CHECKSUM_H
#define _ASM_IA64_CHECKSUM_H
extern __sum16 ip_fast_csum(const void *iph, unsigned int ihl);
extern __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
				 __u32 len, __u8 proto, __wsum sum);
extern __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
				 __u32 len, __u8 proto, __wsum sum);
extern __wsum csum_partial(const void *buff, int len, __wsum sum);
extern __sum16 ip_compute_csum(const void *buff, int len);
static inline __sum16 csum_fold(__wsum csum)
{
	u32 sum = (__force u32)csum;
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return (__force __sum16)~sum;
}
#define _HAVE_ARCH_IPV6_CSUM	1
struct in6_addr;
extern __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
			       const struct in6_addr *daddr,
			       __u32 len, __u8 proto, __wsum csum);
#endif  
