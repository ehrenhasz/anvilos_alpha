


#ifndef _NET_SUNRPC_FAIL_H_
#define _NET_SUNRPC_FAIL_H_

#include <linux/fault-inject.h>

#if IS_ENABLED(CONFIG_FAULT_INJECTION)

struct fail_sunrpc_attr {
	struct fault_attr	attr;

	bool			ignore_client_disconnect;
	bool			ignore_server_disconnect;
	bool			ignore_cache_wait;
};

extern struct fail_sunrpc_attr fail_sunrpc;

#endif 

#endif 
