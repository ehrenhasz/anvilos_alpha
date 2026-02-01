

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char _license[] SEC("license") = "GPL";

SEC("lsm_cgroup/inet_csk_clone")
int BPF_PROG(nonvoid_socket_clone, struct sock *newsk, const struct request_sock *req)
{
	 
	return 0;
}
