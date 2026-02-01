
 

#include <net/xfrm.h>

static struct xfrm_state_afinfo xfrm6_state_afinfo = {
	.family			= AF_INET6,
	.proto			= IPPROTO_IPV6,
	.output			= xfrm6_output,
	.transport_finish	= xfrm6_transport_finish,
	.local_error		= xfrm6_local_error,
};

int __init xfrm6_state_init(void)
{
	return xfrm_state_register_afinfo(&xfrm6_state_afinfo);
}

void xfrm6_state_fini(void)
{
	xfrm_state_unregister_afinfo(&xfrm6_state_afinfo);
}
