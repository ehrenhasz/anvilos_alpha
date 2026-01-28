#ifndef _ZFS_COMPILER_COMPAT_H
#define	_ZFS_COMPILER_COMPAT_H
#include <linux/compiler.h>
#if !defined(zfs_fallthrough)
#if defined(HAVE_IMPLICIT_FALLTHROUGH)
#define	zfs_fallthrough		__attribute__((__fallthrough__))
#else
#define	zfs_fallthrough		((void)0)
#endif
#endif
#if !defined(READ_ONCE)
#define	READ_ONCE(x)		ACCESS_ONCE(x)
#endif
#endif	 
