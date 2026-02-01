
 

#include <linux/bpf.h>
#include <linux/btf_ids.h>

#include <net/dst_metadata.h>
#include <net/fou.h>

struct bpf_fou_encap {
	__be16 sport;
	__be16 dport;
};

enum bpf_fou_encap_type {
	FOU_BPF_ENCAP_FOU,
	FOU_BPF_ENCAP_GUE,
};

__diag_push();
__diag_ignore_all("-Wmissing-prototypes",
		  "Global functions as their definitions will be in BTF");

 
__bpf_kfunc int bpf_skb_set_fou_encap(struct __sk_buff *skb_ctx,
				      struct bpf_fou_encap *encap, int type)
{
	struct sk_buff *skb = (struct sk_buff *)skb_ctx;
	struct ip_tunnel_info *info = skb_tunnel_info(skb);

	if (unlikely(!encap))
		return -EINVAL;

	if (unlikely(!info || !(info->mode & IP_TUNNEL_INFO_TX)))
		return -EINVAL;

	switch (type) {
	case FOU_BPF_ENCAP_FOU:
		info->encap.type = TUNNEL_ENCAP_FOU;
		break;
	case FOU_BPF_ENCAP_GUE:
		info->encap.type = TUNNEL_ENCAP_GUE;
		break;
	default:
		info->encap.type = TUNNEL_ENCAP_NONE;
	}

	if (info->key.tun_flags & TUNNEL_CSUM)
		info->encap.flags |= TUNNEL_ENCAP_FLAG_CSUM;

	info->encap.sport = encap->sport;
	info->encap.dport = encap->dport;

	return 0;
}

 
__bpf_kfunc int bpf_skb_get_fou_encap(struct __sk_buff *skb_ctx,
				      struct bpf_fou_encap *encap)
{
	struct sk_buff *skb = (struct sk_buff *)skb_ctx;
	struct ip_tunnel_info *info = skb_tunnel_info(skb);

	if (unlikely(!info))
		return -EINVAL;

	encap->sport = info->encap.sport;
	encap->dport = info->encap.dport;

	return 0;
}

__diag_pop()

BTF_SET8_START(fou_kfunc_set)
BTF_ID_FLAGS(func, bpf_skb_set_fou_encap)
BTF_ID_FLAGS(func, bpf_skb_get_fou_encap)
BTF_SET8_END(fou_kfunc_set)

static const struct btf_kfunc_id_set fou_bpf_kfunc_set = {
	.owner = THIS_MODULE,
	.set   = &fou_kfunc_set,
};

int register_fou_bpf(void)
{
	return register_btf_kfunc_id_set(BPF_PROG_TYPE_SCHED_CLS,
					 &fou_bpf_kfunc_set);
}
