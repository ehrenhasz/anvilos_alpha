 

#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ip.h>
#include <linux/socket.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define DEBUG 1

SEC("sockops")
int bpf_iw(struct bpf_sock_ops *skops)
{
	int bufsize = 1500000;
	int rwnd_init = 40;
	int iw = 40;
	int rv = 0;
	int op;

	 
	if (bpf_ntohl(skops->remote_port) != 55601 &&
	    skops->local_port != 55601) {
		skops->reply = -1;
		return 1;
	}

	op = (int) skops->op;

#ifdef DEBUG
	bpf_printk("BPF command: %d\n", op);
#endif

	 
	switch (op) {
	case BPF_SOCK_OPS_RWND_INIT:
		rv = rwnd_init;
		break;
	case BPF_SOCK_OPS_TCP_CONNECT_CB:
		 
		rv = bpf_setsockopt(skops, SOL_SOCKET, SO_SNDBUF, &bufsize,
				    sizeof(bufsize));
		rv += bpf_setsockopt(skops, SOL_SOCKET, SO_RCVBUF,
				     &bufsize, sizeof(bufsize));
		break;
	case BPF_SOCK_OPS_ACTIVE_ESTABLISHED_CB:
		rv = bpf_setsockopt(skops, SOL_TCP, TCP_BPF_IW, &iw,
				    sizeof(iw));
		break;
	case BPF_SOCK_OPS_PASSIVE_ESTABLISHED_CB:
		 
		rv = bpf_setsockopt(skops, SOL_SOCKET, SO_SNDBUF, &bufsize,
				    sizeof(bufsize));
		rv +=  bpf_setsockopt(skops, SOL_SOCKET, SO_RCVBUF,
				      &bufsize, sizeof(bufsize));
		break;
	default:
		rv = -1;
	}
#ifdef DEBUG
	bpf_printk("Returning %d\n", rv);
#endif
	skops->reply = rv;
	return 1;
}
char _license[] SEC("license") = "GPL";
