
 
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("xdp")
int xdpdm_devlog(struct xdp_md *ctx)
{
	char fmt[] = "devmap redirect: dev %u -> dev %u len %u\n";
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	unsigned int len = data_end - data;

	bpf_trace_printk(fmt, sizeof(fmt),
			 ctx->ingress_ifindex, ctx->egress_ifindex, len);

	return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
