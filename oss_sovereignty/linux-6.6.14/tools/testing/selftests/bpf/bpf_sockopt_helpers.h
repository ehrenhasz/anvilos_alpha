 

#include <sys/socket.h>
#include <bpf/bpf_helpers.h>

int get_set_sk_priority(void *ctx)
{
	int prio;

	 

	if (bpf_getsockopt(ctx, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
		return 0;
	if (bpf_setsockopt(ctx, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
		return 0;

	return 1;
}
