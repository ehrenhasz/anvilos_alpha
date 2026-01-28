#ifndef __6LOWPAN_NHC_H
#define __6LOWPAN_NHC_H
#include <linux/skbuff.h>
#include <linux/rbtree.h>
#include <linux/module.h>
#include <net/6lowpan.h>
#include <net/ipv6.h>
#define LOWPAN_NHC(__nhc, _name, _nexthdr,	\
		   _hdrlen, _id, _idmask,	\
		   _uncompress, _compress)	\
static const struct lowpan_nhc __nhc = {	\
	.name		= _name,		\
	.nexthdr	= _nexthdr,		\
	.nexthdrlen	= _hdrlen,		\
	.id		= _id,			\
	.idmask		= _idmask,		\
	.uncompress	= _uncompress,		\
	.compress	= _compress,		\
}
#define module_lowpan_nhc(__nhc)		\
static int __init __nhc##_init(void)		\
{						\
	return lowpan_nhc_add(&(__nhc));	\
}						\
module_init(__nhc##_init);			\
static void __exit __nhc##_exit(void)		\
{						\
	lowpan_nhc_del(&(__nhc));		\
}						\
module_exit(__nhc##_exit);
struct lowpan_nhc {
	const char	*name;
	u8		nexthdr;
	size_t		nexthdrlen;
	u8		id;
	u8		idmask;
	int		(*uncompress)(struct sk_buff *skb, size_t needed);
	int		(*compress)(struct sk_buff *skb, u8 **hc_ptr);
};
struct lowpan_nhc *lowpan_nhc_by_nexthdr(u8 nexthdr);
int lowpan_nhc_check_compression(struct sk_buff *skb,
				 const struct ipv6hdr *hdr, u8 **hc_ptr);
int lowpan_nhc_do_compression(struct sk_buff *skb, const struct ipv6hdr *hdr,
			      u8 **hc_ptr);
int lowpan_nhc_do_uncompression(struct sk_buff *skb,
				const struct net_device *dev,
				struct ipv6hdr *hdr);
int lowpan_nhc_add(const struct lowpan_nhc *nhc);
void lowpan_nhc_del(const struct lowpan_nhc *nhc);
void lowpan_nhc_init(void);
#endif  
