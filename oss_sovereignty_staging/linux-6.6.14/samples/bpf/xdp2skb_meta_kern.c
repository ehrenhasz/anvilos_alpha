 
#include <uapi/linux/bpf.h>
#include <uapi/linux/pkt_cls.h>

#include <bpf/bpf_helpers.h>

 
struct meta_info {
	__u32 mark;
} __attribute__((aligned(4)));

SEC("xdp_mark")
int _xdp_mark(struct xdp_md *ctx)
{
	struct meta_info *meta;
	void *data, *data_end;
	int ret;

	 
	ret = bpf_xdp_adjust_meta(ctx, -(int)sizeof(*meta));
	if (ret < 0)
		return XDP_ABORTED;

	 
	data = (void *)(unsigned long)ctx->data;

	 
	meta = (void *)(unsigned long)ctx->data_meta;
	if (meta + 1 > data)
		return XDP_ABORTED;

	meta->mark = 42;

	return XDP_PASS;
}

SEC("tc_mark")
int _tc_mark(struct __sk_buff *ctx)
{
	void *data      = (void *)(unsigned long)ctx->data;
	void *data_end  = (void *)(unsigned long)ctx->data_end;
	void *data_meta = (void *)(unsigned long)ctx->data_meta;
	struct meta_info *meta = data_meta;

	 
	if (meta + 1 > data) {
		ctx->mark = 41;
		  
		return TC_ACT_OK;
	}

	 
	ctx->mark = meta->mark;  

	return TC_ACT_OK;
}

 
