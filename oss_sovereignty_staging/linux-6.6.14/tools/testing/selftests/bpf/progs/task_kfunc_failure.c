
 

#include <vmlinux.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_helpers.h>

#include "bpf_misc.h"
#include "task_kfunc_common.h"

char _license[] SEC("license") = "GPL";

 

static struct __tasks_kfunc_map_value *insert_lookup_task(struct task_struct *task)
{
	int status;

	status = tasks_kfunc_map_insert(task);
	if (status)
		return NULL;

	return tasks_kfunc_map_value_lookup(task);
}

SEC("tp_btf/task_newtask")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(task_kfunc_acquire_untrusted, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired;
	struct __tasks_kfunc_map_value *v;

	v = insert_lookup_task(task);
	if (!v)
		return 0;

	 
	acquired = bpf_task_acquire(v->task);
	if (!acquired)
		return 0;

	bpf_task_release(acquired);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("arg#0 pointer type STRUCT task_struct must point")
int BPF_PROG(task_kfunc_acquire_fp, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired, *stack_task = (struct task_struct *)&clone_flags;

	 
	acquired = bpf_task_acquire((struct task_struct *)&stack_task);
	if (!acquired)
		return 0;

	bpf_task_release(acquired);

	return 0;
}

SEC("kretprobe/free_task")
__failure __msg("calling kernel function bpf_task_acquire is not allowed")
int BPF_PROG(task_kfunc_acquire_unsafe_kretprobe, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired;

	 
	acquired = bpf_task_acquire(task);
	if (!acquired)
		return 0;
	bpf_task_release(acquired);

	return 0;
}

SEC("kretprobe/free_task")
__failure __msg("calling kernel function bpf_task_acquire is not allowed")
int BPF_PROG(task_kfunc_acquire_unsafe_kretprobe_rcu, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired;

	bpf_rcu_read_lock();
	if (!task) {
		bpf_rcu_read_unlock();
		return 0;
	}
	 
	acquired = bpf_task_acquire(task);
	if (acquired)
		bpf_task_release(acquired);
	bpf_rcu_read_unlock();

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(task_kfunc_acquire_null, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired;

	 
	acquired = bpf_task_acquire(NULL);
	if (!acquired)
		return 0;
	bpf_task_release(acquired);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("Unreleased reference")
int BPF_PROG(task_kfunc_acquire_unreleased, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired;

	acquired = bpf_task_acquire(task);

	 
	__sink(acquired);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("Unreleased reference")
int BPF_PROG(task_kfunc_xchg_unreleased, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *kptr;
	struct __tasks_kfunc_map_value *v;

	v = insert_lookup_task(task);
	if (!v)
		return 0;

	kptr = bpf_kptr_xchg(&v->task, NULL);
	if (!kptr)
		return 0;

	 

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(task_kfunc_acquire_release_no_null_check, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired;

	acquired = bpf_task_acquire(task);
	 
	bpf_task_release(acquired);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(task_kfunc_release_untrusted, struct task_struct *task, u64 clone_flags)
{
	struct __tasks_kfunc_map_value *v;

	v = insert_lookup_task(task);
	if (!v)
		return 0;

	 
	bpf_task_release(v->task);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("arg#0 pointer type STRUCT task_struct must point")
int BPF_PROG(task_kfunc_release_fp, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired = (struct task_struct *)&clone_flags;

	 
	bpf_task_release(acquired);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(task_kfunc_release_null, struct task_struct *task, u64 clone_flags)
{
	struct __tasks_kfunc_map_value local, *v;
	long status;
	struct task_struct *acquired, *old;
	s32 pid;

	status = bpf_probe_read_kernel(&pid, sizeof(pid), &task->pid);
	if (status)
		return 0;

	local.task = NULL;
	status = bpf_map_update_elem(&__tasks_kfunc_map, &pid, &local, BPF_NOEXIST);
	if (status)
		return status;

	v = bpf_map_lookup_elem(&__tasks_kfunc_map, &pid);
	if (!v)
		return -ENOENT;

	acquired = bpf_task_acquire(task);
	if (!acquired)
		return -EEXIST;

	old = bpf_kptr_xchg(&v->task, acquired);

	 
	bpf_task_release(old);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("release kernel function bpf_task_release expects")
int BPF_PROG(task_kfunc_release_unacquired, struct task_struct *task, u64 clone_flags)
{
	 
	bpf_task_release(task);

	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(task_kfunc_from_pid_no_null_check, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *acquired;

	acquired = bpf_task_from_pid(task->pid);

	 
	bpf_task_release(acquired);

	return 0;
}

SEC("lsm/task_free")
__failure __msg("reg type unsupported for arg#0 function")
int BPF_PROG(task_kfunc_from_lsm_task_free, struct task_struct *task)
{
	struct task_struct *acquired;

	 
	acquired = bpf_task_acquire(task);
	if (!acquired)
		return 0;

	bpf_task_release(acquired);
	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("access beyond the end of member comm")
int BPF_PROG(task_access_comm1, struct task_struct *task, u64 clone_flags)
{
	bpf_strncmp(task->comm, 17, "foo");
	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("access beyond the end of member comm")
int BPF_PROG(task_access_comm2, struct task_struct *task, u64 clone_flags)
{
	bpf_strncmp(task->comm + 1, 16, "foo");
	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("write into memory")
int BPF_PROG(task_access_comm3, struct task_struct *task, u64 clone_flags)
{
	bpf_probe_read_kernel(task->comm, 16, task->comm);
	return 0;
}

SEC("fentry/__set_task_comm")
__failure __msg("R1 type=ptr_ expected")
int BPF_PROG(task_access_comm4, struct task_struct *task, const char *buf, bool exec)
{
	 
	bpf_strncmp(task->comm, 16, "foo");
	return 0;
}

SEC("tp_btf/task_newtask")
__failure __msg("R1 must be referenced or trusted")
int BPF_PROG(task_kfunc_release_in_map, struct task_struct *task, u64 clone_flags)
{
	struct task_struct *local;
	struct __tasks_kfunc_map_value *v;

	if (tasks_kfunc_map_insert(task))
		return 0;

	v = tasks_kfunc_map_value_lookup(task);
	if (!v)
		return 0;

	bpf_rcu_read_lock();
	local = v->task;
	if (!local) {
		bpf_rcu_read_unlock();
		return 0;
	}
	 
	bpf_task_release(local);
	bpf_rcu_read_unlock();

	return 0;
}
