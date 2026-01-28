

#ifndef _SPL_SYS_TYPES_H_
#define	_SPL_SYS_TYPES_H_

#pragma once

#include_next <sys/types.h>

#ifdef __ILP32__
typedef __uint64_t u_longlong_t;
typedef __int64_t longlong_t;
#else
typedef unsigned long long	u_longlong_t;
typedef long long		longlong_t;
#endif
#include <sys/stdint.h>

#define	_CLOCK_T_DECLARED

#include <sys/types32.h>
#include <sys/_stdarg.h>
#include <linux/types.h>

#define	MAXNAMELEN	256



typedef	void zfs_kernel_param_t;

typedef	struct timespec	timestruc_t;
typedef	struct timespec	timespec_t;
typedef struct timespec inode_timespec_t;

typedef u_int		uint_t;
typedef u_char		uchar_t;
typedef u_short		ushort_t;
typedef u_long		ulong_t;

typedef	int		minor_t;
#ifndef	_OFF64_T_DECLARED
#define	_OFF64_T_DECLARED
typedef off_t		off64_t;
#endif
typedef id_t		taskid_t;
typedef id_t		projid_t;
typedef id_t		poolid_t;
typedef uint_t		zoneid_t;
typedef id_t		ctid_t;
typedef	mode_t		o_mode_t;
typedef	uint64_t	pgcnt_t;

typedef	short		index_t;
typedef	off_t		offset_t;
#ifndef _PTRDIFF_T_DECLARED
typedef	__ptrdiff_t		ptrdiff_t;	
#define	_PTRDIFF_T_DECLARED
#endif
typedef	int64_t		rlim64_t;
typedef	int		major_t;

#ifdef NEED_SOLARIS_BOOLEAN
#if defined(__XOPEN_OR_POSIX)
typedef enum { _B_FALSE, _B_TRUE }	boolean_t;
#else
typedef enum { B_FALSE, B_TRUE }	boolean_t;
#endif 
#else

#define	B_FALSE	0
#define	B_TRUE	1

#endif

typedef	u_longlong_t	u_offset_t;
typedef	u_longlong_t	len_t;

typedef	longlong_t	diskaddr_t;

typedef void		zidmap_t;

#include <sys/debug.h>
#endif	
