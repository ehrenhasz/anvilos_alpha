
 

#include <vmlinux.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "bpf_experimental.h"

 
struct node_data {
	int key;
	int data;
	struct bpf_list_node node;
};

#define private(name) SEC(".data." #name) __hidden __attribute__((aligned(8)))
private(A) struct bpf_spin_lock glock;
private(A) struct bpf_rb_root groot __contains(node_data, node);

SEC("tc")
long rbtree_api_add__wrong_node_type(void *ctx)
{
	struct node_data *n;

	n = bpf_obj_new(typeof(*n));
	if (!n)
		return 1;

	bpf_spin_lock(&glock);
	bpf_rbtree_first(&groot);
	bpf_spin_unlock(&glock);
	return 0;
}

char _license[] SEC("license") = "GPL";
