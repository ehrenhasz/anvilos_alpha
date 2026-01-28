


#ifndef _SYS_FEATURE_TESTS_H
#define	_SYS_FEATURE_TESTS_H

#define	____cacheline_aligned

#if !defined(zfs_fallthrough) && !defined(_LIBCPP_VERSION)
#if defined(HAVE_IMPLICIT_FALLTHROUGH)
#define	zfs_fallthrough		__attribute__((__fallthrough__))
#else
#define	zfs_fallthrough		((void)0)
#endif
#endif

#endif
