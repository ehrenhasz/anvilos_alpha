#ifndef _PARISC_CHECKSUM_H
#define _PARISC_CHECKSUM_H
#include <linux/in6.h>
extern __wsum csum_partial(const void *, int, __wsum);
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;
	unsigned long t0, t1, t2;
	__asm__ __volatile__ (
"	ldws,ma		4(%1), %0\n"
"	addib,<=	-4, %2, 2f\n"
"\n"
"	ldws		4(%1), %4\n"
"	ldws		8(%1), %5\n"
"	add		%0, %4, %0\n"
"	ldws,ma		12(%1), %3\n"
"	addc		%0, %5, %0\n"
"	addc		%0, %3, %0\n"
"1:	ldws,ma		4(%1), %3\n"
"	addib,<		0, %2, 1b\n"
"	addc		%0, %3, %0\n"
"\n"
"	extru		%0, 31, 16, %4\n"
"	extru		%0, 15, 16, %5\n"
"	addc		%4, %5, %0\n"
"	extru		%0, 15, 16, %5\n"
"	add		%0, %5, %0\n"
"	subi		-1, %0, %0\n"
"2:\n"
	: "=r" (sum), "=r" (iph), "=r" (ihl), "=r" (t0), "=r" (t1), "=r" (t2)
	: "1" (iph), "2" (ihl)
	: "memory");
	return (__force __sum16)sum;
}
static inline __sum16 csum_fold(__wsum csum)
{
	u32 sum = (__force u32)csum;
	sum += (sum << 16) + (sum >> 16);
	return (__force __sum16)(~sum >> 16);
}
static inline __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
					__u32 len, __u8 proto,
					__wsum sum)
{
	__asm__(
	"	add  %1, %0, %0\n"
	"	addc %2, %0, %0\n"
	"	addc %3, %0, %0\n"
	"	addc %%r0, %0, %0\n"
		: "=r" (sum)
		: "r" (daddr), "r"(saddr), "r"(proto+len), "0"(sum));
	return sum;
}
static inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
					__u32 len, __u8 proto,
					__wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}
static inline __sum16 ip_compute_csum(const void *buf, int len)
{
	 return csum_fold (csum_partial(buf, len, 0));
}
#define _HAVE_ARCH_IPV6_CSUM
static __inline__ __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
					  const struct in6_addr *daddr,
					  __u32 len, __u8 proto,
					  __wsum sum)
{
	unsigned long t0, t1, t2, t3;
	len += proto;	 
	__asm__ __volatile__ (
#if BITS_PER_LONG > 32
"	ldd,ma		8(%1), %4\n"	 
"	ldd,ma		8(%2), %5\n"	 
"	add		%4, %0, %0\n"
"	ldd,ma		8(%1), %6\n"	 
"	ldd,ma		8(%2), %7\n"	 
"	add,dc		%5, %0, %0\n"
"	add,dc		%6, %0, %0\n"
"	add,dc		%7, %0, %0\n"
"	add,dc		%3, %0, %0\n"   
"	extrd,u		%0, 31, 32, %4\n" 
"	depdi		0, 31, 32, %0\n" 
"	add		%4, %0, %0\n"	 
"	addc		0, %0, %0\n"	 
#else
"	ldw,ma		4(%1), %4\n"	 
"	ldw,ma		4(%2), %5\n"	 
"	add		%4, %0, %0\n"
"	ldw,ma		4(%1), %6\n"	 
"	addc		%5, %0, %0\n"
"	ldw,ma		4(%2), %7\n"	 
"	addc		%6, %0, %0\n"
"	ldw,ma		4(%1), %4\n"	 
"	addc		%7, %0, %0\n"
"	ldw,ma		4(%2), %5\n"	 
"	addc		%4, %0, %0\n"
"	ldw,ma		4(%1), %6\n"	 
"	addc		%5, %0, %0\n"
"	ldw,ma		4(%2), %7\n"	 
"	addc		%6, %0, %0\n"
"	addc		%7, %0, %0\n"
"	addc		%3, %0, %0\n"	 
#endif
	: "=r" (sum), "=r" (saddr), "=r" (daddr), "=r" (len),
	  "=r" (t0), "=r" (t1), "=r" (t2), "=r" (t3)
	: "0" (sum), "1" (saddr), "2" (daddr), "3" (len)
	: "memory");
	return csum_fold(sum);
}
#endif
