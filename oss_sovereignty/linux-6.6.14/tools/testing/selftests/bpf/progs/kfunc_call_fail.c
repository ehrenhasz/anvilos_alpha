
 
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include "../bpf_testmod/bpf_testmod_kfunc.h"

struct syscall_test_args {
	__u8 data[16];
	size_t size;
};

SEC("?syscall")
int kfunc_syscall_test_fail(struct syscall_test_args *args)
{
	bpf_kfunc_call_test_mem_len_pass1(&args->data, sizeof(*args) + 1);

	return 0;
}

SEC("?syscall")
int kfunc_syscall_test_null_fail(struct syscall_test_args *args)
{
	 

	bpf_kfunc_call_test_mem_len_pass1(args, sizeof(*args));

	return 0;
}

SEC("?tc")
int kfunc_call_test_get_mem_fail_rdonly(struct __sk_buff *skb)
{
	struct prog_test_ref_kfunc *pt;
	unsigned long s = 0;
	int *p = NULL;
	int ret = 0;

	pt = bpf_kfunc_call_test_acquire(&s);
	if (pt) {
		p = bpf_kfunc_call_test_get_rdonly_mem(pt, 2 * sizeof(int));
		if (p)
			p[0] = 42;  
		else
			ret = -1;

		bpf_kfunc_call_test_release(pt);
	}
	return ret;
}

SEC("?tc")
int kfunc_call_test_get_mem_fail_use_after_free(struct __sk_buff *skb)
{
	struct prog_test_ref_kfunc *pt;
	unsigned long s = 0;
	int *p = NULL;
	int ret = 0;

	pt = bpf_kfunc_call_test_acquire(&s);
	if (pt) {
		p = bpf_kfunc_call_test_get_rdwr_mem(pt, 2 * sizeof(int));
		if (p) {
			p[0] = 42;
			ret = p[1];  
		} else {
			ret = -1;
		}

		bpf_kfunc_call_test_release(pt);
	}
	if (p)
		ret = p[0];  

	return ret;
}

SEC("?tc")
int kfunc_call_test_get_mem_fail_oob(struct __sk_buff *skb)
{
	struct prog_test_ref_kfunc *pt;
	unsigned long s = 0;
	int *p = NULL;
	int ret = 0;

	pt = bpf_kfunc_call_test_acquire(&s);
	if (pt) {
		p = bpf_kfunc_call_test_get_rdonly_mem(pt, 2 * sizeof(int));
		if (p)
			ret = p[2 * sizeof(int)];  
		else
			ret = -1;

		bpf_kfunc_call_test_release(pt);
	}
	return ret;
}

int not_const_size = 2 * sizeof(int);

SEC("?tc")
int kfunc_call_test_get_mem_fail_not_const(struct __sk_buff *skb)
{
	struct prog_test_ref_kfunc *pt;
	unsigned long s = 0;
	int *p = NULL;
	int ret = 0;

	pt = bpf_kfunc_call_test_acquire(&s);
	if (pt) {
		p = bpf_kfunc_call_test_get_rdonly_mem(pt, not_const_size);  
		if (p)
			ret = p[0];
		else
			ret = -1;

		bpf_kfunc_call_test_release(pt);
	}
	return ret;
}

SEC("?tc")
int kfunc_call_test_mem_acquire_fail(struct __sk_buff *skb)
{
	struct prog_test_ref_kfunc *pt;
	unsigned long s = 0;
	int *p = NULL;
	int ret = 0;

	pt = bpf_kfunc_call_test_acquire(&s);
	if (pt) {
		 
		p = bpf_kfunc_call_test_acq_rdonly_mem(pt, 2 * sizeof(int));
		if (p)
			ret = p[0];
		else
			ret = -1;

		bpf_kfunc_call_int_mem_release(p);

		bpf_kfunc_call_test_release(pt);
	}
	return ret;
}

char _license[] SEC("license") = "GPL";
