
 

#include <vmlinux.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_helpers.h>

#include "bpf_misc.h"
#include "cgrp_kfunc_common.h"

char _license[] SEC("license") = "GPL";

 

static struct __cgrps_kfunc_map_value *insert_lookup_cgrp(struct cgroup *cgrp)
{
	int status;

	status = cgrps_kfunc_map_insert(cgrp);
	if (status)
		return NULL;

	return cgrps_kfunc_map_value_lookup(cgrp);
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(cgrp_kfunc_acquire_untrusted, struct cgroup *cgrp, const char *path)
{
	struct cgroup *acquired;
	struct __cgrps_kfunc_map_value *v;

	v = insert_lookup_cgrp(cgrp);
	if (!v)
		return 0;

	 
	acquired = bpf_cgroup_acquire(v->cgrp);
	if (acquired)
		bpf_cgroup_release(acquired);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(cgrp_kfunc_acquire_no_null_check, struct cgroup *cgrp, const char *path)
{
	struct cgroup *acquired;

	acquired = bpf_cgroup_acquire(cgrp);
	 
	bpf_cgroup_release(acquired);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("arg#0 pointer type STRUCT cgroup must point")
int BPF_PROG(cgrp_kfunc_acquire_fp, struct cgroup *cgrp, const char *path)
{
	struct cgroup *acquired, *stack_cgrp = (struct cgroup *)&path;

	 
	acquired = bpf_cgroup_acquire((struct cgroup *)&stack_cgrp);
	if (acquired)
		bpf_cgroup_release(acquired);

	return 0;
}

SEC("kretprobe/cgroup_destroy_locked")
__failure __msg("reg type unsupported for arg#0 function")
int BPF_PROG(cgrp_kfunc_acquire_unsafe_kretprobe, struct cgroup *cgrp)
{
	struct cgroup *acquired;

	 
	acquired = bpf_cgroup_acquire(cgrp);
	if (acquired)
		bpf_cgroup_release(acquired);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("cgrp_kfunc_acquire_trusted_walked")
int BPF_PROG(cgrp_kfunc_acquire_trusted_walked, struct cgroup *cgrp, const char *path)
{
	struct cgroup *acquired;

	 
	acquired = bpf_cgroup_acquire(cgrp->old_dom_cgrp);
	if (acquired)
		bpf_cgroup_release(acquired);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(cgrp_kfunc_acquire_null, struct cgroup *cgrp, const char *path)
{
	struct cgroup *acquired;

	 
	acquired = bpf_cgroup_acquire(NULL);
	if (acquired)
		bpf_cgroup_release(acquired);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("Unreleased reference")
int BPF_PROG(cgrp_kfunc_acquire_unreleased, struct cgroup *cgrp, const char *path)
{
	struct cgroup *acquired;

	acquired = bpf_cgroup_acquire(cgrp);

	 
	__sink(acquired);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("Unreleased reference")
int BPF_PROG(cgrp_kfunc_xchg_unreleased, struct cgroup *cgrp, const char *path)
{
	struct cgroup *kptr;
	struct __cgrps_kfunc_map_value *v;

	v = insert_lookup_cgrp(cgrp);
	if (!v)
		return 0;

	kptr = bpf_kptr_xchg(&v->cgrp, NULL);
	if (!kptr)
		return 0;

	 

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("must be referenced or trusted")
int BPF_PROG(cgrp_kfunc_rcu_get_release, struct cgroup *cgrp, const char *path)
{
	struct cgroup *kptr;
	struct __cgrps_kfunc_map_value *v;

	v = insert_lookup_cgrp(cgrp);
	if (!v)
		return 0;

	bpf_rcu_read_lock();
	kptr = v->cgrp;
	if (kptr)
		 
		bpf_cgroup_release(kptr);
	bpf_rcu_read_unlock();

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(cgrp_kfunc_release_untrusted, struct cgroup *cgrp, const char *path)
{
	struct __cgrps_kfunc_map_value *v;

	v = insert_lookup_cgrp(cgrp);
	if (!v)
		return 0;

	 
	bpf_cgroup_release(v->cgrp);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("arg#0 pointer type STRUCT cgroup must point")
int BPF_PROG(cgrp_kfunc_release_fp, struct cgroup *cgrp, const char *path)
{
	struct cgroup *acquired = (struct cgroup *)&path;

	 
	bpf_cgroup_release(acquired);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("Possibly NULL pointer passed to trusted arg0")
int BPF_PROG(cgrp_kfunc_release_null, struct cgroup *cgrp, const char *path)
{
	struct __cgrps_kfunc_map_value local, *v;
	long status;
	struct cgroup *acquired, *old;
	s32 id;

	status = bpf_probe_read_kernel(&id, sizeof(id), &cgrp->self.id);
	if (status)
		return 0;

	local.cgrp = NULL;
	status = bpf_map_update_elem(&__cgrps_kfunc_map, &id, &local, BPF_NOEXIST);
	if (status)
		return status;

	v = bpf_map_lookup_elem(&__cgrps_kfunc_map, &id);
	if (!v)
		return -ENOENT;

	acquired = bpf_cgroup_acquire(cgrp);
	if (!acquired)
		return -ENOENT;

	old = bpf_kptr_xchg(&v->cgrp, acquired);

	 
	bpf_cgroup_release(old);

	return 0;
}

SEC("tp_btf/cgroup_mkdir")
__failure __msg("release kernel function bpf_cgroup_release expects")
int BPF_PROG(cgrp_kfunc_release_unacquired, struct cgroup *cgrp, const char *path)
{
	 
	bpf_cgroup_release(cgrp);

	return 0;
}
