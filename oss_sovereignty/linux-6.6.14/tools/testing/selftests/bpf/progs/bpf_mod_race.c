
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

const volatile struct {
	 
	pid_t tgid;
	 
	int inject_error;
	 
	void *fault_addr;
} bpf_mod_race_config = { -1 };

int bpf_blocking = 0;
int res_try_get_module = -1;

static __always_inline bool check_thread_id(void)
{
	struct task_struct *task = bpf_get_current_task_btf();

	return task->tgid == bpf_mod_race_config.tgid;
}

 

SEC("fmod_ret.s/bpf_fentry_test1")
int BPF_PROG(widen_race, int a, int ret)
{
	char dst;

	if (!check_thread_id())
		return 0;
	 
	bpf_blocking = 1;
	bpf_copy_from_user(&dst, 1, bpf_mod_race_config.fault_addr);
	return bpf_mod_race_config.inject_error;
}

SEC("fexit/do_init_module")
int BPF_PROG(fexit_init_module, struct module *mod, int ret)
{
	if (!check_thread_id())
		return 0;
	 
	bpf_blocking = 2;
	return 0;
}

SEC("fexit/btf_try_get_module")
int BPF_PROG(fexit_module_get, const struct btf *btf, struct module *mod)
{
	res_try_get_module = !!mod;
	return 0;
}

char _license[] SEC("license") = "GPL";
