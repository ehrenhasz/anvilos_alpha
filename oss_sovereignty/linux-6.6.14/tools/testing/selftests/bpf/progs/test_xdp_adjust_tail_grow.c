
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("xdp")
int _xdp_adjust_tail_grow(struct xdp_md *xdp)
{
	int data_len = bpf_xdp_get_buff_len(xdp);
	int offset = 0;
	 
#if defined(__TARGET_ARCH_s390)
	int tailroom = 512;
#else
	int tailroom = 320;
#endif

	 

	if (data_len == 54) {  
		offset = 4096;  
	} else if (data_len == 74) {  
		offset = 40;
	} else if (data_len == 64) {
		offset = 128;
	} else if (data_len == 128) {
		 
		offset = 4096 - 256 - tailroom - data_len;
	} else if (data_len == 9000) {
		offset = 10;
	} else if (data_len == 9001) {
		offset = 4096;
	} else {
		return XDP_ABORTED;  
	}

	if (bpf_xdp_adjust_tail(xdp, offset))
		return XDP_DROP;
	return XDP_TX;
}

char _license[] SEC("license") = "GPL";
