

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#define ATTR __attribute__((noinline))
#include "test_jhash.h"

SEC("tc")
int balancer_ingress(struct __sk_buff *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	void *ptr;
	int nh_off, i = 0;

	nh_off = 32;

	 

#define C do { \
	ptr = data + i; \
	if (ptr + nh_off > data_end) \
		break; \
	ctx->tc_index = jhash(ptr, nh_off, ctx->cb[0] + i++); \
	} while (0);
#define C30 C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;C;
	C30;C30;C30;  
	return 0;
}
char _license[] SEC("license") = "GPL";
