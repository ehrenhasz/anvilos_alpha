

#ifndef __USDT_BPF_H__
#define __USDT_BPF_H__

#include <linux/errno.h>
#include "bpf_helpers.h"
#include "bpf_tracing.h"




#ifndef BPF_USDT_MAX_SPEC_CNT
#define BPF_USDT_MAX_SPEC_CNT 256
#endif

#ifndef BPF_USDT_MAX_IP_CNT
#define BPF_USDT_MAX_IP_CNT (4 * BPF_USDT_MAX_SPEC_CNT)
#endif

enum __bpf_usdt_arg_type {
	BPF_USDT_ARG_CONST,
	BPF_USDT_ARG_REG,
	BPF_USDT_ARG_REG_DEREF,
};

struct __bpf_usdt_arg_spec {
	
	__u64 val_off;
	
	enum __bpf_usdt_arg_type arg_type;
	
	short reg_off;
	
	bool arg_signed;
	
	char arg_bitshift;
};


#define BPF_USDT_MAX_ARG_CNT 12
struct __bpf_usdt_spec {
	struct __bpf_usdt_arg_spec args[BPF_USDT_MAX_ARG_CNT];
	__u64 usdt_cookie;
	short arg_cnt;
};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, BPF_USDT_MAX_SPEC_CNT);
	__type(key, int);
	__type(value, struct __bpf_usdt_spec);
} __bpf_usdt_specs SEC(".maps") __weak;

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, BPF_USDT_MAX_IP_CNT);
	__type(key, long);
	__type(value, __u32);
} __bpf_usdt_ip_to_spec_id SEC(".maps") __weak;

extern const _Bool LINUX_HAS_BPF_COOKIE __kconfig;

static __always_inline
int __bpf_usdt_spec_id(struct pt_regs *ctx)
{
	if (!LINUX_HAS_BPF_COOKIE) {
		long ip = PT_REGS_IP(ctx);
		int *spec_id_ptr;

		spec_id_ptr = bpf_map_lookup_elem(&__bpf_usdt_ip_to_spec_id, &ip);
		return spec_id_ptr ? *spec_id_ptr : -ESRCH;
	}

	return bpf_get_attach_cookie(ctx);
}


__weak __hidden
int bpf_usdt_arg_cnt(struct pt_regs *ctx)
{
	struct __bpf_usdt_spec *spec;
	int spec_id;

	spec_id = __bpf_usdt_spec_id(ctx);
	if (spec_id < 0)
		return -ESRCH;

	spec = bpf_map_lookup_elem(&__bpf_usdt_specs, &spec_id);
	if (!spec)
		return -ESRCH;

	return spec->arg_cnt;
}


__weak __hidden
int bpf_usdt_arg(struct pt_regs *ctx, __u64 arg_num, long *res)
{
	struct __bpf_usdt_spec *spec;
	struct __bpf_usdt_arg_spec *arg_spec;
	unsigned long val;
	int err, spec_id;

	*res = 0;

	spec_id = __bpf_usdt_spec_id(ctx);
	if (spec_id < 0)
		return -ESRCH;

	spec = bpf_map_lookup_elem(&__bpf_usdt_specs, &spec_id);
	if (!spec)
		return -ESRCH;

	if (arg_num >= BPF_USDT_MAX_ARG_CNT)
		return -ENOENT;
	barrier_var(arg_num);
	if (arg_num >= spec->arg_cnt)
		return -ENOENT;

	arg_spec = &spec->args[arg_num];
	switch (arg_spec->arg_type) {
	case BPF_USDT_ARG_CONST:
		
		val = arg_spec->val_off;
		break;
	case BPF_USDT_ARG_REG:
		
		err = bpf_probe_read_kernel(&val, sizeof(val), (void *)ctx + arg_spec->reg_off);
		if (err)
			return err;
		break;
	case BPF_USDT_ARG_REG_DEREF:
		
		err = bpf_probe_read_kernel(&val, sizeof(val), (void *)ctx + arg_spec->reg_off);
		if (err)
			return err;
		err = bpf_probe_read_user(&val, sizeof(val), (void *)val + arg_spec->val_off);
		if (err)
			return err;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		val >>= arg_spec->arg_bitshift;
#endif
		break;
	default:
		return -EINVAL;
	}

	
	val <<= arg_spec->arg_bitshift;
	if (arg_spec->arg_signed)
		val = ((long)val) >> arg_spec->arg_bitshift;
	else
		val = val >> arg_spec->arg_bitshift;
	*res = val;
	return 0;
}


__weak __hidden
long bpf_usdt_cookie(struct pt_regs *ctx)
{
	struct __bpf_usdt_spec *spec;
	int spec_id;

	spec_id = __bpf_usdt_spec_id(ctx);
	if (spec_id < 0)
		return 0;

	spec = bpf_map_lookup_elem(&__bpf_usdt_specs, &spec_id);
	if (!spec)
		return 0;

	return spec->usdt_cookie;
}


#define ___bpf_usdt_args0() ctx
#define ___bpf_usdt_args1(x) ___bpf_usdt_args0(), ({ long _x; bpf_usdt_arg(ctx, 0, &_x); (void *)_x; })
#define ___bpf_usdt_args2(x, args...) ___bpf_usdt_args1(args), ({ long _x; bpf_usdt_arg(ctx, 1, &_x); (void *)_x; })
#define ___bpf_usdt_args3(x, args...) ___bpf_usdt_args2(args), ({ long _x; bpf_usdt_arg(ctx, 2, &_x); (void *)_x; })
#define ___bpf_usdt_args4(x, args...) ___bpf_usdt_args3(args), ({ long _x; bpf_usdt_arg(ctx, 3, &_x); (void *)_x; })
#define ___bpf_usdt_args5(x, args...) ___bpf_usdt_args4(args), ({ long _x; bpf_usdt_arg(ctx, 4, &_x); (void *)_x; })
#define ___bpf_usdt_args6(x, args...) ___bpf_usdt_args5(args), ({ long _x; bpf_usdt_arg(ctx, 5, &_x); (void *)_x; })
#define ___bpf_usdt_args7(x, args...) ___bpf_usdt_args6(args), ({ long _x; bpf_usdt_arg(ctx, 6, &_x); (void *)_x; })
#define ___bpf_usdt_args8(x, args...) ___bpf_usdt_args7(args), ({ long _x; bpf_usdt_arg(ctx, 7, &_x); (void *)_x; })
#define ___bpf_usdt_args9(x, args...) ___bpf_usdt_args8(args), ({ long _x; bpf_usdt_arg(ctx, 8, &_x); (void *)_x; })
#define ___bpf_usdt_args10(x, args...) ___bpf_usdt_args9(args), ({ long _x; bpf_usdt_arg(ctx, 9, &_x); (void *)_x; })
#define ___bpf_usdt_args11(x, args...) ___bpf_usdt_args10(args), ({ long _x; bpf_usdt_arg(ctx, 10, &_x); (void *)_x; })
#define ___bpf_usdt_args12(x, args...) ___bpf_usdt_args11(args), ({ long _x; bpf_usdt_arg(ctx, 11, &_x); (void *)_x; })
#define ___bpf_usdt_args(args...) ___bpf_apply(___bpf_usdt_args, ___bpf_narg(args))(args)


#define BPF_USDT(name, args...)						    \
name(struct pt_regs *ctx);						    \
static __always_inline typeof(name(0))					    \
____##name(struct pt_regs *ctx, ##args);				    \
typeof(name(0)) name(struct pt_regs *ctx)				    \
{									    \
        _Pragma("GCC diagnostic push")					    \
        _Pragma("GCC diagnostic ignored \"-Wint-conversion\"")		    \
        return ____##name(___bpf_usdt_args(args));			    \
        _Pragma("GCC diagnostic pop")					    \
}									    \
static __always_inline typeof(name(0))					    \
____##name(struct pt_regs *ctx, ##args)

#endif 
