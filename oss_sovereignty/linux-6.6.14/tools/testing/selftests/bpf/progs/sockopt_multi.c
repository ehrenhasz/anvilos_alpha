
#include <netinet/in.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

char _license[] SEC("license") = "GPL";

__u32 page_size = 0;

SEC("cgroup/getsockopt")
int _getsockopt_child(struct bpf_sockopt *ctx)
{
	__u8 *optval_end = ctx->optval_end;
	__u8 *optval = ctx->optval;

	if (ctx->level != SOL_IP || ctx->optname != IP_TOS)
		goto out;

	if (optval + 1 > optval_end)
		return 0;  

	if (optval[0] != 0x80)
		return 0;  

	ctx->retval = 0;  

	optval[0] = 0x90;
	ctx->optlen = 1;

	return 1;

out:
	 
	if (ctx->optlen > page_size)
		ctx->optlen = 0;
	return 1;
}

SEC("cgroup/getsockopt")
int _getsockopt_parent(struct bpf_sockopt *ctx)
{
	__u8 *optval_end = ctx->optval_end;
	__u8 *optval = ctx->optval;

	if (ctx->level != SOL_IP || ctx->optname != IP_TOS)
		goto out;

	if (optval + 1 > optval_end)
		return 0;  

	if (optval[0] != 0x90)
		return 0;  

	ctx->retval = 0;  

	optval[0] = 0xA0;
	ctx->optlen = 1;

	return 1;

out:
	 
	if (ctx->optlen > page_size)
		ctx->optlen = 0;
	return 1;
}

SEC("cgroup/setsockopt")
int _setsockopt(struct bpf_sockopt *ctx)
{
	__u8 *optval_end = ctx->optval_end;
	__u8 *optval = ctx->optval;

	if (ctx->level != SOL_IP || ctx->optname != IP_TOS)
		goto out;

	if (optval + 1 > optval_end)
		return 0;  

	optval[0] += 0x10;
	ctx->optlen = 1;

	return 1;

out:
	 
	if (ctx->optlen > page_size)
		ctx->optlen = 0;
	return 1;
}
