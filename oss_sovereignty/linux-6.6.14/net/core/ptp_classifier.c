
 

 

#include <linux/skbuff.h>
#include <linux/filter.h>
#include <linux/ptp_classify.h>

static struct bpf_prog *ptp_insns __read_mostly;

unsigned int ptp_classify_raw(const struct sk_buff *skb)
{
	return bpf_prog_run(ptp_insns, skb);
}
EXPORT_SYMBOL_GPL(ptp_classify_raw);

struct ptp_header *ptp_parse_header(struct sk_buff *skb, unsigned int type)
{
	u8 *ptr = skb_mac_header(skb);

	if (type & PTP_CLASS_VLAN)
		ptr += VLAN_HLEN;

	switch (type & PTP_CLASS_PMASK) {
	case PTP_CLASS_IPV4:
		ptr += IPV4_HLEN(ptr) + UDP_HLEN;
		break;
	case PTP_CLASS_IPV6:
		ptr += IP6_HLEN + UDP_HLEN;
		break;
	case PTP_CLASS_L2:
		break;
	default:
		return NULL;
	}

	ptr += ETH_HLEN;

	 
	if (ptr + sizeof(struct ptp_header) > skb->data + skb->len)
		return NULL;

	return (struct ptp_header *)ptr;
}
EXPORT_SYMBOL_GPL(ptp_parse_header);

bool ptp_msg_is_sync(struct sk_buff *skb, unsigned int type)
{
	struct ptp_header *hdr;

	hdr = ptp_parse_header(skb, type);
	if (!hdr)
		return false;

	return ptp_get_msgtype(hdr, type) == PTP_MSGTYPE_SYNC;
}
EXPORT_SYMBOL_GPL(ptp_msg_is_sync);

void __init ptp_classifier_init(void)
{
	static struct sock_filter ptp_filter[] __initdata = {
		{ 0x28,  0,  0, 0x0000000c },
		{ 0x15,  0, 12, 0x00000800 },
		{ 0x30,  0,  0, 0x00000017 },
		{ 0x15,  0,  9, 0x00000011 },
		{ 0x28,  0,  0, 0x00000014 },
		{ 0x45,  7,  0, 0x00001fff },
		{ 0xb1,  0,  0, 0x0000000e },
		{ 0x48,  0,  0, 0x00000010 },
		{ 0x15,  0,  4, 0x0000013f },
		{ 0x48,  0,  0, 0x00000016 },
		{ 0x54,  0,  0, 0x0000000f },
		{ 0x44,  0,  0, 0x00000010 },
		{ 0x16,  0,  0, 0x00000000 },
		{ 0x06,  0,  0, 0x00000000 },
		{ 0x15,  0,  9, 0x000086dd },
		{ 0x30,  0,  0, 0x00000014 },
		{ 0x15,  0,  6, 0x00000011 },
		{ 0x28,  0,  0, 0x00000038 },
		{ 0x15,  0,  4, 0x0000013f },
		{ 0x28,  0,  0, 0x0000003e },
		{ 0x54,  0,  0, 0x0000000f },
		{ 0x44,  0,  0, 0x00000020 },
		{ 0x16,  0,  0, 0x00000000 },
		{ 0x06,  0,  0, 0x00000000 },
		{ 0x15,  0, 32, 0x00008100 },
		{ 0x28,  0,  0, 0x00000010 },
		{ 0x15,  0,  7, 0x000088f7 },
		{ 0x30,  0,  0, 0x00000012 },
		{ 0x54,  0,  0, 0x00000008 },
		{ 0x15,  0, 35, 0x00000000 },
		{ 0x28,  0,  0, 0x00000012 },
		{ 0x54,  0,  0, 0x0000000f },
		{ 0x44,  0,  0, 0x000000c0 },
		{ 0x16,  0,  0, 0x00000000 },
		{ 0x15,  0, 12, 0x00000800 },
		{ 0x30,  0,  0, 0x0000001b },
		{ 0x15,  0,  9, 0x00000011 },
		{ 0x28,  0,  0, 0x00000018 },
		{ 0x45,  7,  0, 0x00001fff },
		{ 0xb1,  0,  0, 0x00000012 },
		{ 0x48,  0,  0, 0x00000014 },
		{ 0x15,  0,  4, 0x0000013f },
		{ 0x48,  0,  0, 0x0000001a },
		{ 0x54,  0,  0, 0x0000000f },
		{ 0x44,  0,  0, 0x00000090 },
		{ 0x16,  0,  0, 0x00000000 },
		{ 0x06,  0,  0, 0x00000000 },
		{ 0x15,  0,  8, 0x000086dd },
		{ 0x30,  0,  0, 0x00000018 },
		{ 0x15,  0,  6, 0x00000011 },
		{ 0x28,  0,  0, 0x0000003c },
		{ 0x15,  0,  4, 0x0000013f },
		{ 0x28,  0,  0, 0x00000042 },
		{ 0x54,  0,  0, 0x0000000f },
		{ 0x44,  0,  0, 0x000000a0 },
		{ 0x16,  0,  0, 0x00000000 },
		{ 0x06,  0,  0, 0x00000000 },
		{ 0x15,  0,  7, 0x000088f7 },
		{ 0x30,  0,  0, 0x0000000e },
		{ 0x54,  0,  0, 0x00000008 },
		{ 0x15,  0,  4, 0x00000000 },
		{ 0x28,  0,  0, 0x0000000e },
		{ 0x54,  0,  0, 0x0000000f },
		{ 0x44,  0,  0, 0x00000040 },
		{ 0x16,  0,  0, 0x00000000 },
		{ 0x06,  0,  0, 0x00000000 },
	};
	struct sock_fprog_kern ptp_prog;

	ptp_prog.len = ARRAY_SIZE(ptp_filter);
	ptp_prog.filter = ptp_filter;

	BUG_ON(bpf_prog_create(&ptp_insns, &ptp_prog));
}
