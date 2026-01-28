







#ifndef	_TIRPC_NETTYPE_H
#define	_TIRPC_NETTYPE_H

#include <netconfig.h>

#define	_RPC_NONE	0
#define	_RPC_NETPATH	1
#define	_RPC_VISIBLE	2
#define	_RPC_CIRCUIT_V	3
#define	_RPC_DATAGRAM_V	4
#define	_RPC_CIRCUIT_N	5
#define	_RPC_DATAGRAM_N	6
#define	_RPC_TCP	7
#define	_RPC_UDP	8

#ifdef __cplusplus
extern "C" {
#endif
extern void *__rpc_setconf(const char *);
extern void __rpc_endconf(void *);
extern struct netconfig *__rpc_getconf(void *);
extern struct netconfig *__rpc_getconfip(const char *);
#ifdef __cplusplus
}
#endif

#endif	
