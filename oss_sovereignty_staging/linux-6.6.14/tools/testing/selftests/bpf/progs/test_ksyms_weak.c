
 

#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

int out__existing_typed = -1;
__u64 out__existing_typeless = -1;

__u64 out__non_existent_typeless = -1;
__u64 out__non_existent_typed = -1;

 

 
extern const struct rq runqueues __ksym __weak;  
extern const void bpf_prog_active __ksym __weak;  
struct task_struct *bpf_task_acquire(struct task_struct *p) __ksym __weak;
void bpf_testmod_test_mod_kfunc(int i) __ksym __weak;


 

 
extern const void bpf_link_fops1 __ksym __weak;

 
extern const int bpf_link_fops2 __ksym __weak;
void invalid_kfunc(void) __ksym __weak;

SEC("raw_tp/sys_enter")
int pass_handler(const void *ctx)
{
	struct rq *rq;

	 
	rq = (struct rq *)bpf_per_cpu_ptr(&runqueues, 0);
	if (rq && bpf_ksym_exists(&runqueues))
		out__existing_typed = rq->cpu;
	out__existing_typeless = (__u64)&bpf_prog_active;

	 
	out__non_existent_typeless = (__u64)&bpf_link_fops1;

	 
	out__non_existent_typed = (__u64)&bpf_link_fops2;

	if (&bpf_link_fops2)  
		out__non_existent_typed = (__u64)bpf_per_cpu_ptr(&bpf_link_fops2, 0);

	if (!bpf_ksym_exists(bpf_task_acquire))
		 
		bpf_task_acquire(0);

	if (!bpf_ksym_exists(bpf_testmod_test_mod_kfunc))
		 
		bpf_testmod_test_mod_kfunc(0);

	if (bpf_ksym_exists(invalid_kfunc))
		 
		invalid_kfunc();

	return 0;
}

char _license[] SEC("license") = "GPL";
