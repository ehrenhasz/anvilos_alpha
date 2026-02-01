
 

 

#include <linux/export.h>
#include <net/checksum.h>

#include <asm/byteorder.h>

#ifndef do_csum
static inline unsigned short from32to16(unsigned int x)
{
	 
	x = (x & 0xffff) + (x >> 16);
	 
	x = (x & 0xffff) + (x >> 16);
	return x;
}

static unsigned int do_csum(const unsigned char *buff, int len)
{
	int odd;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long) buff;
	if (odd) {
#ifdef __LITTLE_ENDIAN
		result += (*buff << 8);
#else
		result = *buff;
#endif
		len--;
		buff++;
	}
	if (len >= 2) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			len -= 2;
			buff += 2;
		}
		if (len >= 4) {
			const unsigned char *end = buff + ((unsigned)len & ~3);
			unsigned int carry = 0;
			do {
				unsigned int w = *(unsigned int *) buff;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (buff < end);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
#ifdef __LITTLE_ENDIAN
		result += *buff;
#else
		result += (*buff << 8);
#endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}
#endif

#ifndef ip_fast_csum
 
__sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	return (__force __sum16)~do_csum(iph, ihl*4);
}
EXPORT_SYMBOL(ip_fast_csum);
#endif

 
__wsum csum_partial(const void *buff, int len, __wsum wsum)
{
	unsigned int sum = (__force unsigned int)wsum;
	unsigned int result = do_csum(buff, len);

	 
	result += sum;
	if (sum > result)
		result += 1;
	return (__force __wsum)result;
}
EXPORT_SYMBOL(csum_partial);

 
__sum16 ip_compute_csum(const void *buff, int len)
{
	return (__force __sum16)~do_csum(buff, len);
}
EXPORT_SYMBOL(ip_compute_csum);

#ifndef csum_tcpudp_nofold
static inline u32 from64to32(u64 x)
{
	 
	x = (x & 0xffffffff) + (x >> 32);
	 
	x = (x & 0xffffffff) + (x >> 32);
	return (u32)x;
}

__wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
			  __u32 len, __u8 proto, __wsum sum)
{
	unsigned long long s = (__force u32)sum;

	s += (__force u32)saddr;
	s += (__force u32)daddr;
#ifdef __BIG_ENDIAN
	s += proto + len;
#else
	s += (proto + len) << 8;
#endif
	return (__force __wsum)from64to32(s);
}
EXPORT_SYMBOL(csum_tcpudp_nofold);
#endif
