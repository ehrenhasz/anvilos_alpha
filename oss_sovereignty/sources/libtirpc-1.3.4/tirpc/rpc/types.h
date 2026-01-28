




#ifndef _TIRPC_TYPES_H
#define _TIRPC_TYPES_H

#include <sys/types.h>

typedef int32_t bool_t;
typedef int32_t enum_t;

typedef u_int32_t rpcprog_t;
typedef u_int32_t rpcvers_t;
typedef u_int32_t rpcproc_t;
typedef u_int32_t rpcprot_t;
typedef u_int32_t rpcport_t;
typedef   int32_t rpc_inline_t;

#ifndef NULL
#	define NULL	0
#endif
#define __dontcare__	-1

#ifndef FALSE
#	define FALSE	(0)
#endif
#ifndef TRUE
#	define TRUE	(1)
#endif

#define mem_alloc(bsize)	calloc(1, bsize)
#define mem_free(ptr, bsize)	free(ptr)


#if defined __APPLE_CC__ || defined __FreeBSD__ || !defined (__GLIBC__)
# define __u_char_defined
# define __daddr_t_defined
#endif

#if defined __BIONIC__
typedef int64_t quad_t;
typedef uint64_t u_quad_t;
#endif

#ifndef __u_char_defined
typedef __u_char u_char;
typedef __u_short u_short;
typedef __u_int u_int;
typedef __u_long u_long;
typedef __quad_t quad_t;
typedef __u_quad_t u_quad_t;
typedef __fsid_t fsid_t;
# define __u_char_defined
#endif
#ifndef __daddr_t_defined
typedef __daddr_t daddr_t;
typedef __caddr_t caddr_t;
# define __daddr_t_defined
#endif

#include <sys/time.h>
#include <sys/param.h>
#include <stdlib.h>
#include <netconfig.h>




struct netbuf {
  unsigned int maxlen;
  unsigned int len;
  void *buf;
};



struct t_bind {
  struct netbuf   addr;
  unsigned int    qlen;
};


struct __rpc_sockinfo {
	int si_af; 
	int si_proto;
	int si_socktype;
	int si_alen;
};

#endif 
