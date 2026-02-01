
#include <string.h>
#include <linux/tcp.h>
#include <linux/bpf.h>
#include <netinet/in.h>
#include <bpf/bpf_helpers.h>

char _license[] SEC("license") = "GPL";

int page_size = 0;  

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#define SOL_CUSTOM			0xdeadbeef

struct sockopt_sk {
	__u8 val;
};

struct {
	__uint(type, BPF_MAP_TYPE_SK_STORAGE);
	__uint(map_flags, BPF_F_NO_PREALLOC);
	__type(key, int);
	__type(value, struct sockopt_sk);
} socket_storage_map SEC(".maps");

SEC("cgroup/getsockopt")
int _getsockopt(struct bpf_sockopt *ctx)
{
	__u8 *optval_end = ctx->optval_end;
	__u8 *optval = ctx->optval;
	struct sockopt_sk *storage;
	struct bpf_sock *sk;

	 
	sk = ctx->sk;
	if (sk && sk->family == AF_NETLINK)
		goto out;

	 
	if (bpf_get_netns_cookie(NULL) == 0)
		return 0;

	if (bpf_get_netns_cookie(ctx) == 0)
		return 0;

	if (ctx->level == SOL_IP && ctx->optname == IP_TOS) {
		 
		goto out;
	}

	if (ctx->level == SOL_SOCKET && ctx->optname == SO_SNDBUF) {
		 
		goto out;
	}

	if (ctx->level == SOL_TCP && ctx->optname == TCP_CONGESTION) {
		 
		goto out;
	}

	if (ctx->level == SOL_TCP && ctx->optname == TCP_ZEROCOPY_RECEIVE) {
		 

		 
		if (optval + sizeof(__u64) > optval_end)
			return 0;  

		if (((struct tcp_zerocopy_receive *)optval)->address != 0)
			return 0;  

		goto out;
	}

	if (ctx->level == SOL_IP && ctx->optname == IP_FREEBIND) {
		if (optval + 1 > optval_end)
			return 0;  

		ctx->retval = 0;  

		 
		optval[0] = 0x55;
		ctx->optlen = 1;

		 
		if (optval_end - optval != page_size)
			return 0;  

		return 1;
	}

	if (ctx->level != SOL_CUSTOM)
		return 0;  

	if (optval + 1 > optval_end)
		return 0;  

	storage = bpf_sk_storage_get(&socket_storage_map, ctx->sk, 0,
				     BPF_SK_STORAGE_GET_F_CREATE);
	if (!storage)
		return 0;  

	if (!ctx->retval)
		return 0;  
	ctx->retval = 0;  

	optval[0] = storage->val;
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
	struct sockopt_sk *storage;
	struct bpf_sock *sk;

	 
	sk = ctx->sk;
	if (sk && sk->family == AF_NETLINK)
		goto out;

	 
	if (bpf_get_netns_cookie(NULL) == 0)
		return 0;

	if (bpf_get_netns_cookie(ctx) == 0)
		return 0;

	if (ctx->level == SOL_IP && ctx->optname == IP_TOS) {
		 
		ctx->optlen = 0;  
		return 1;
	}

	if (ctx->level == SOL_SOCKET && ctx->optname == SO_SNDBUF) {
		 

		if (optval + sizeof(__u32) > optval_end)
			return 0;  

		*(__u32 *)optval = 0x55AA;
		ctx->optlen = 4;

		return 1;
	}

	if (ctx->level == SOL_TCP && ctx->optname == TCP_CONGESTION) {
		 

		if (optval + 5 > optval_end)
			return 0;  

		memcpy(optval, "cubic", 5);
		ctx->optlen = 5;

		return 1;
	}

	if (ctx->level == SOL_IP && ctx->optname == IP_FREEBIND) {
		 
		if (ctx->optlen != page_size * 2)
			return 0;  

		if (optval + 1 > optval_end)
			return 0;  

		 
		optval[0] = 0;
		ctx->optlen = 1;

		 
		if (optval_end - optval != page_size)
			return 0;  

		return 1;
	}

	if (ctx->level != SOL_CUSTOM)
		return 0;  

	if (optval + 1 > optval_end)
		return 0;  

	storage = bpf_sk_storage_get(&socket_storage_map, ctx->sk, 0,
				     BPF_SK_STORAGE_GET_F_CREATE);
	if (!storage)
		return 0;  

	storage->val = optval[0];
	ctx->optlen = -1;  

	return 1;

out:
	 
	if (ctx->optlen > page_size)
		ctx->optlen = 0;
	return 1;
}
