#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

const char LICENSE[] SEC("license") = "GPL";

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, __u64);
} array SEC(".maps");

__noinline int sub1(int x)
{
	int key = 0;

	bpf_map_lookup_elem(&array, &key);
	return x + 1;
}

static __noinline int sub5(int v);

__noinline int sub2(int y)
{
	return sub5(y + 2);
}

static __noinline int sub3(int z)
{
	return z + 3 + sub1(4);
}

static __noinline int sub4(int w)
{
	int key = 0;

	bpf_map_lookup_elem(&array, &key);
	return w + sub3(5) + sub1(6);
}

 
static __noinline int sub5(int v)
{
	return sub1(v) - 1;  
}

 
__noinline int get_task_tgid(uintptr_t t)
{
	 
	return BPF_CORE_READ((struct task_struct *)(void *)t, tgid);
}

int res1 = 0;
int res2 = 0;
int res3 = 0;
int res4 = 0;

SEC("raw_tp/sys_enter")
int prog1(void *ctx)
{
	 
	struct task_struct *t = (void *)bpf_get_current_task();

	if (!BPF_CORE_READ(t, pid) || !get_task_tgid((uintptr_t)t))
		return 1;

	res1 = sub1(1) + sub3(2);  
	return 0;
}

SEC("raw_tp/sys_exit")
int prog2(void *ctx)
{
	struct task_struct *t = (void *)bpf_get_current_task();

	if (!BPF_CORE_READ(t, pid) || !get_task_tgid((uintptr_t)t))
		return 1;

	res2 = sub2(3) + sub3(4);  
	return 0;
}

static int empty_callback(__u32 index, void *data)
{
	return 0;
}

 
SEC("raw_tp/sys_enter")
int prog3(void *ctx)
{
	struct task_struct *t = (void *)bpf_get_current_task();

	if (!BPF_CORE_READ(t, pid) || !get_task_tgid((uintptr_t)t))
		return 1;

	 
	bpf_loop(1, empty_callback, NULL, 0);

	res3 = sub3(5) + 6;  
	return 0;
}

 
SEC("raw_tp/sys_exit")
int prog4(void *ctx)
{
	struct task_struct *t = (void *)bpf_get_current_task();

	if (!BPF_CORE_READ(t, pid) || !get_task_tgid((uintptr_t)t))
		return 1;

	res4 = sub4(7) + sub1(8);  
	return 0;
}
