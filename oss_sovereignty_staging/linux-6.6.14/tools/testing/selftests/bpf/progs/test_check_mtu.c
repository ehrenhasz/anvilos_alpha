
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>

#include <stddef.h>
#include <stdint.h>

char _license[] SEC("license") = "GPL";

 
volatile const int GLOBAL_USER_MTU;
volatile const __u32 GLOBAL_USER_IFINDEX;

 
__u32 global_bpf_mtu_xdp = 0;
__u32 global_bpf_mtu_tc  = 0;

SEC("xdp")
int xdp_use_helper_basic(struct xdp_md *ctx)
{
	__u32 mtu_len = 0;

	if (bpf_check_mtu(ctx, 0, &mtu_len, 0, 0))
		return XDP_ABORTED;

	return XDP_PASS;
}

SEC("xdp")
int xdp_use_helper(struct xdp_md *ctx)
{
	int retval = XDP_PASS;  
	__u32 mtu_len = 0;
	__u32 ifindex = 0;
	int delta = 0;

	 
	if (GLOBAL_USER_IFINDEX > 0)
		ifindex = GLOBAL_USER_IFINDEX;

	if (bpf_check_mtu(ctx, ifindex, &mtu_len, delta, 0)) {
		 
		retval = XDP_ABORTED;
		goto out;
	}

	if (mtu_len != GLOBAL_USER_MTU)
		retval = XDP_DROP;

out:
	global_bpf_mtu_xdp = mtu_len;
	return retval;
}

SEC("xdp")
int xdp_exceed_mtu(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	__u32 data_len = data_end - data;
	int retval = XDP_ABORTED;  
	__u32 mtu_len = 0;
	int delta;
	int err;

	 
	delta = GLOBAL_USER_MTU - (data_len - ETH_HLEN) + 1;

	err = bpf_check_mtu(ctx, ifindex, &mtu_len, delta, 0);
	if (err) {
		retval = XDP_PASS;  
		if (err != BPF_MTU_CHK_RET_FRAG_NEEDED)
			retval = XDP_DROP;
	}

	global_bpf_mtu_xdp = mtu_len;
	return retval;
}

SEC("xdp")
int xdp_minus_delta(struct xdp_md *ctx)
{
	int retval = XDP_PASS;  
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	__u32 data_len = data_end - data;
	__u32 mtu_len = 0;
	int delta;

	 
	delta = -((data_len - ETH_HLEN) + 1);

	 
	if (bpf_check_mtu(ctx, ifindex, &mtu_len, delta, 0))
		retval = XDP_ABORTED;

	global_bpf_mtu_xdp = mtu_len;
	return retval;
}

SEC("xdp")
int xdp_input_len(struct xdp_md *ctx)
{
	int retval = XDP_PASS;  
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	__u32 data_len = data_end - data;

	 
	__u32 mtu_len = data_len - ETH_HLEN;

	if (bpf_check_mtu(ctx, ifindex, &mtu_len, 0, 0))
		retval = XDP_ABORTED;

	global_bpf_mtu_xdp = mtu_len;
	return retval;
}

SEC("xdp")
int xdp_input_len_exceed(struct xdp_md *ctx)
{
	int retval = XDP_ABORTED;  
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	int err;

	 
	__u32 mtu_len = GLOBAL_USER_MTU;

	mtu_len += 1;  

	err = bpf_check_mtu(ctx, ifindex, &mtu_len, 0, 0);
	if (err == BPF_MTU_CHK_RET_FRAG_NEEDED)
		retval = XDP_PASS ;  

	global_bpf_mtu_xdp = mtu_len;
	return retval;
}

SEC("tc")
int tc_use_helper(struct __sk_buff *ctx)
{
	int retval = BPF_OK;  
	__u32 mtu_len = 0;
	int delta = 0;

	if (bpf_check_mtu(ctx, 0, &mtu_len, delta, 0)) {
		retval = BPF_DROP;
		goto out;
	}

	if (mtu_len != GLOBAL_USER_MTU)
		retval = BPF_REDIRECT;
out:
	global_bpf_mtu_tc = mtu_len;
	return retval;
}

SEC("tc")
int tc_exceed_mtu(struct __sk_buff *ctx)
{
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	int retval = BPF_DROP;  
	__u32 skb_len = ctx->len;
	__u32 mtu_len = 0;
	int delta;
	int err;

	 
	delta = GLOBAL_USER_MTU - (skb_len - ETH_HLEN) + 1;

	err = bpf_check_mtu(ctx, ifindex, &mtu_len, delta, 0);
	if (err) {
		retval = BPF_OK;  
		if (err != BPF_MTU_CHK_RET_FRAG_NEEDED)
			retval = BPF_DROP;
	}

	global_bpf_mtu_tc = mtu_len;
	return retval;
}

SEC("tc")
int tc_exceed_mtu_da(struct __sk_buff *ctx)
{
	 
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	__u32 data_len = data_end - data;
	int retval = BPF_DROP;  
	__u32 mtu_len = 0;
	int delta;
	int err;

	 
	delta = GLOBAL_USER_MTU - (data_len - ETH_HLEN) + 1;

	err = bpf_check_mtu(ctx, ifindex, &mtu_len, delta, 0);
	if (err) {
		retval = BPF_OK;  
		if (err != BPF_MTU_CHK_RET_FRAG_NEEDED)
			retval = BPF_DROP;
	}

	global_bpf_mtu_tc = mtu_len;
	return retval;
}

SEC("tc")
int tc_minus_delta(struct __sk_buff *ctx)
{
	int retval = BPF_OK;  
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	__u32 skb_len = ctx->len;
	__u32 mtu_len = 0;
	int delta;

	 
	delta = -((skb_len - ETH_HLEN) + 1);

	 
	if (bpf_check_mtu(ctx, ifindex, &mtu_len, delta, 0))
		retval = BPF_DROP;

	global_bpf_mtu_xdp = mtu_len;
	return retval;
}

SEC("tc")
int tc_input_len(struct __sk_buff *ctx)
{
	int retval = BPF_OK;  
	__u32 ifindex = GLOBAL_USER_IFINDEX;

	 
	__u32 mtu_len = GLOBAL_USER_MTU;

	if (bpf_check_mtu(ctx, ifindex, &mtu_len, 0, 0))
		retval = BPF_DROP;

	global_bpf_mtu_xdp = mtu_len;
	return retval;
}

SEC("tc")
int tc_input_len_exceed(struct __sk_buff *ctx)
{
	int retval = BPF_DROP;  
	__u32 ifindex = GLOBAL_USER_IFINDEX;
	int err;

	 
	__u32 mtu_len = GLOBAL_USER_MTU;

	mtu_len += 1;  

	err = bpf_check_mtu(ctx, ifindex, &mtu_len, 0, 0);
	if (err == BPF_MTU_CHK_RET_FRAG_NEEDED)
		retval = BPF_OK;  

	global_bpf_mtu_xdp = mtu_len;
	return retval;
}
