


#ifndef	_SYS_CCOMPILE_H
#define	_SYS_CCOMPILE_H



#ifdef	__cplusplus
extern "C" {
#endif

#if defined(INVARIANTS) && !defined(ZFS_DEBUG)
#define	ZFS_DEBUG
#undef 	NDEBUG
#endif

#define	EXPORT_SYMBOL(x)
#define	module_param(a, b, c)
#define	module_param_call(a, b, c, d, e)
#define	module_param_named(a, b, c, d)
#define	MODULE_PARM_DESC(a, b)
#define	asm __asm
#ifdef ZFS_DEBUG
#undef NDEBUG
#endif
#if !defined(ZFS_DEBUG) && !defined(NDEBUG)
#define	NDEBUG
#endif

#ifndef EINTEGRITY
#define	EINTEGRITY 97 
#endif


#define	ECKSUM	EINTEGRITY
#define	EFRAGS	ENOSPC


#define	ENOTACTIVE	ECANCELED

#define	EREMOTEIO EREMOTE
#define	ECHRNG ENXIO
#define	ETIME ETIMEDOUT

#ifndef LOCORE
#ifndef HAVE_RPC_TYPES
#ifndef _KERNEL
typedef int bool_t;
typedef int enum_t;
#endif
#endif
#endif

#ifndef __cplusplus
#define	__init
#define	__exit
#endif

#if defined(_KERNEL) || defined(_STANDALONE)
#define	param_set_charp(a, b) (0)
#define	ATTR_UID AT_UID
#define	ATTR_GID AT_GID
#define	ATTR_MODE AT_MODE
#define	ATTR_XVATTR	AT_XVATTR
#define	ATTR_CTIME	AT_CTIME
#define	ATTR_MTIME	AT_MTIME
#define	ATTR_ATIME	AT_ATIME
#if defined(_STANDALONE)
#define	vmem_free kmem_free
#define	vmem_zalloc kmem_zalloc
#define	vmem_alloc kmem_zalloc
#else
#define	vmem_free zfs_kmem_free
#define	vmem_zalloc(size, flags) zfs_kmem_alloc(size, flags | M_ZERO)
#define	vmem_alloc zfs_kmem_alloc
#endif
#define	MUTEX_NOLOCKDEP 0
#define	RW_NOLOCKDEP 0

#else
#define	FALSE 0
#define	TRUE 1
	
#define	ENOSTR ENOTCONN
#define	ENODATA EINVAL


#define	__BSD_VISIBLE 1
#ifndef	IN_BASE
#define	__POSIX_VISIBLE 201808
#define	__XSI_VISIBLE 1000
#endif
#define	ARRAY_SIZE(a) (sizeof (a) / sizeof (a[0]))
#define	mmap64 mmap

#if defined(__FreeBSD__)
#define	open64 open
#define	pwrite64 pwrite
#define	ftruncate64 ftruncate
#define	lseek64 lseek
#define	pread64 pread
#define	stat64 stat
#define	lstat64 lstat
#define	statfs64 statfs
#define	readdir64 readdir
#define	dirent64 dirent
#endif
#define	P2ALIGN(x, align)		((x) & -(align))
#define	P2CROSS(x, y, align)	(((x) ^ (y)) > (align) - 1)
#define	P2ROUNDUP(x, align)		((((x) - 1) | ((align) - 1)) + 1)
#define	P2PHASE(x, align)		((x) & ((align) - 1))
#define	P2NPHASE(x, align)		(-(x) & ((align) - 1))
#define	ISP2(x)			(((x) & ((x) - 1)) == 0)
#define	IS_P2ALIGNED(v, a)	((((uintptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)
#define	P2BOUNDARY(off, len, align) \
	(((off) ^ ((off) + (len) - 1)) > (align) - 1)


#define	P2ALIGN_TYPED(x, align, type)   \
	((type)(x) & -(type)(align))
#define	P2PHASE_TYPED(x, align, type)   \
	((type)(x) & ((type)(align) - 1))
#define	P2NPHASE_TYPED(x, align, type)  \
	(-(type)(x) & ((type)(align) - 1))
#define	P2ROUNDUP_TYPED(x, align, type) \
	((((type)(x) - 1) | ((type)(align) - 1)) + 1)
#define	P2END_TYPED(x, align, type)     \
	(-(~(type)(x) & -(type)(align)))
#define	P2PHASEUP_TYPED(x, align, phase, type)  \
	((type)(phase) - (((type)(phase) - (type)(x)) & -(type)(align)))
#define	P2CROSS_TYPED(x, y, align, type)        \
	(((type)(x) ^ (type)(y)) > (type)(align) - 1)
#define	P2SAMEHIGHBIT_TYPED(x, y, type) \
	(((type)(x) ^ (type)(y)) < ((type)(x) & (type)(y)))

#define	DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define	RLIM64_INFINITY RLIM_INFINITY
#ifndef HAVE_ERESTART
#define	ERESTART EAGAIN
#endif
#define	ABS(a)	((a) < 0 ? -(a) : (a))

#endif
#ifdef	__cplusplus
}
#endif

#endif	
