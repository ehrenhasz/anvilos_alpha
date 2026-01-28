#ifndef _ASM_CHECKSUM_H
#define _ASM_CHECKSUM_H
#define do_csum	do_csum
unsigned int do_csum(const void *voidptr, int len);
#define csum_tcpudp_nofold csum_tcpudp_nofold
__wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
			  __u32 len, __u8 proto, __wsum sum);
#define csum_tcpudp_magic csum_tcpudp_magic
__sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
			  __u32 len, __u8 proto, __wsum sum);
#include <asm-generic/checksum.h>
#endif
