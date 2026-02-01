
 

#include "tag.h"

#define NONE_NAME	"none"

static struct sk_buff *dsa_slave_notag_xmit(struct sk_buff *skb,
					    struct net_device *dev)
{
	 
	return skb;
}

static const struct dsa_device_ops none_ops = {
	.name	= NONE_NAME,
	.proto	= DSA_TAG_PROTO_NONE,
	.xmit	= dsa_slave_notag_xmit,
};

module_dsa_tag_driver(none_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_NONE, NONE_NAME);
MODULE_LICENSE("GPL");
