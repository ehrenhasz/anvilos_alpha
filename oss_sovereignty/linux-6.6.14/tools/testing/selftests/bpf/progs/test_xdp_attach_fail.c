
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define ERRMSG_LEN 64

struct xdp_errmsg {
	char msg[ERRMSG_LEN];
};

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__type(key, int);
	__type(value, int);
} xdp_errmsg_pb SEC(".maps");

struct xdp_attach_error_ctx {
	unsigned long unused;

	 
	__u32 msg; 
};

 

SEC("tp/xdp/bpf_xdp_link_attach_failed")
int tp__xdp__bpf_xdp_link_attach_failed(struct xdp_attach_error_ctx *ctx)
{
	char *msg = (void *)(__u64) ((void *) ctx + (__u16) ctx->msg);
	struct xdp_errmsg errmsg = {};

	bpf_probe_read_kernel_str(&errmsg.msg, ERRMSG_LEN, msg);
	bpf_perf_event_output(ctx, &xdp_errmsg_pb, BPF_F_CURRENT_CPU, &errmsg,
			      ERRMSG_LEN);
	return 0;
}

 

char LICENSE[] SEC("license") = "GPL";
