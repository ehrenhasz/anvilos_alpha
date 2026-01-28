#ifndef _SPL_TYPES_H
#define	_SPL_TYPES_H
#include <linux/types.h>
typedef enum {
	B_FALSE = 0,
	B_TRUE = 1
} boolean_t;
typedef unsigned char		uchar_t;
typedef unsigned short		ushort_t;
typedef unsigned int		uint_t;
typedef unsigned long		ulong_t;
typedef unsigned long long	u_longlong_t;
typedef long long		longlong_t;
typedef long			intptr_t;
typedef unsigned long long	rlim64_t;
typedef struct task_struct	kthread_t;
typedef struct task_struct	proc_t;
typedef int			id_t;
typedef short			pri_t;
typedef short			index_t;
typedef longlong_t		offset_t;
typedef u_longlong_t		u_offset_t;
typedef ulong_t			pgcnt_t;
typedef int			major_t;
typedef int			minor_t;
struct user_namespace;
#ifdef HAVE_IOPS_CREATE_IDMAP
#include <linux/refcount.h>
struct mnt_idmap {
	struct user_namespace *owner;
	refcount_t count;
};
typedef struct mnt_idmap	zidmap_t;
#define	idmap_owner(p)	(((struct mnt_idmap *)p)->owner)
#else
typedef struct user_namespace	zidmap_t;
#define	idmap_owner(p)	((struct user_namespace *)p)
#endif
extern zidmap_t *zfs_init_idmap;
#endif	 
