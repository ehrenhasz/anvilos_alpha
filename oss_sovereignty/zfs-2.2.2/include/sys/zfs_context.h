 
 

#ifndef _SYS_ZFS_CONTEXT_H
#define	_SYS_ZFS_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

 
#if defined(__KERNEL__) || defined(_STANDALONE)
#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/sysmacros.h>
#include <sys/vmsystm.h>
#include <sys/condvar.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/kmem_cache.h>
#include <sys/vmem.h>
#include <sys/misc.h>
#include <sys/taskq.h>
#include <sys/param.h>
#include <sys/disp.h>
#include <sys/debug.h>
#include <sys/random.h>
#include <sys/string.h>
#include <sys/byteorder.h>
#include <sys/list.h>
#include <sys/time.h>
#include <sys/zone.h>
#include <sys/kstat.h>
#include <sys/zfs_debug.h>
#include <sys/sysevent.h>
#include <sys/sysevent/eventdefs.h>
#include <sys/zfs_delay.h>
#include <sys/sunddi.h>
#include <sys/ctype.h>
#include <sys/disp.h>
#include <sys/trace.h>
#include <sys/procfs_list.h>
#include <sys/mod.h>
#include <sys/uio_impl.h>
#include <sys/zfs_context_os.h>
#else  

#define	_SYS_MUTEX_H
#define	_SYS_RWLOCK_H
#define	_SYS_CONDVAR_H
#define	_SYS_VNODE_H
#define	_SYS_VFS_H
#define	_SYS_SUNDDI_H
#define	_SYS_CALLB_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <setjmp.h>
#include <assert.h>
#include <umem.h>
#include <limits.h>
#include <atomic.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/byteorder.h>
#include <sys/list.h>
#include <sys/mod.h>
#include <sys/uio.h>
#include <sys/zfs_debug.h>
#include <sys/kstat.h>
#include <sys/u8_textprep.h>
#include <sys/sysevent.h>
#include <sys/sysevent/eventdefs.h>
#include <sys/sunddi.h>
#include <sys/debug.h>
#include <sys/utsname.h>
#include <sys/trace_zfs.h>

#include <sys/zfs_context_os.h>

 

#define	noinline	__attribute__((noinline))
#define	likely(x)	__builtin_expect((x), 1)
#define	unlikely(x)	__builtin_expect((x), 0)

 

 

#define	CE_CONT		0	 
#define	CE_NOTE		1	 
#define	CE_WARN		2	 
#define	CE_PANIC	3	 
#define	CE_IGNORE	4	 

 

extern void dprintf_setup(int *argc, char **argv);

extern void cmn_err(int, const char *, ...)
    __attribute__((format(printf, 2, 3)));
extern void vcmn_err(int, const char *, va_list)
    __attribute__((format(printf, 2, 0)));
extern void panic(const char *, ...)
    __attribute__((format(printf, 1, 2), noreturn));
extern void vpanic(const char *, va_list)
    __attribute__((format(printf, 1, 0), noreturn));

#define	fm_panic	panic

 

#ifdef DTRACE_PROBE
#undef	DTRACE_PROBE
#endif	 
#define	DTRACE_PROBE(a)

#ifdef DTRACE_PROBE1
#undef	DTRACE_PROBE1
#endif	 
#define	DTRACE_PROBE1(a, b, c)

#ifdef DTRACE_PROBE2
#undef	DTRACE_PROBE2
#endif	 
#define	DTRACE_PROBE2(a, b, c, d, e)

#ifdef DTRACE_PROBE3
#undef	DTRACE_PROBE3
#endif	 
#define	DTRACE_PROBE3(a, b, c, d, e, f, g)

#ifdef DTRACE_PROBE4
#undef	DTRACE_PROBE4
#endif	 
#define	DTRACE_PROBE4(a, b, c, d, e, f, g, h, i)

 
typedef struct zfs_kernel_param {
	const char *name;	 
} zfs_kernel_param_t;

#define	ZFS_MODULE_PARAM(scope_prefix, name_prefix, name, type, perm, desc)
#define	ZFS_MODULE_PARAM_ARGS void
#define	ZFS_MODULE_PARAM_CALL(scope_prefix, name_prefix, name, setfunc, \
	getfunc, perm, desc)

 
typedef pthread_t	kthread_t;

#define	TS_RUN		0x00000002
#define	TS_JOINABLE	0x00000004

#define	curthread	((void *)(uintptr_t)pthread_self())
#define	getcomm()	"unknown"

#define	thread_create_named(name, stk, stksize, func, arg, len, \
    pp, state, pri)	\
	zk_thread_create(func, arg, stksize, state)
#define	thread_create(stk, stksize, func, arg, len, pp, state, pri)	\
	zk_thread_create(func, arg, stksize, state)
#define	thread_exit()	pthread_exit(NULL)
#define	thread_join(t)	pthread_join((pthread_t)(t), NULL)

#define	newproc(f, a, cid, pri, ctp, pid)	(ENOSYS)

 
typedef struct proc {
	uintptr_t	this_is_never_used_dont_dereference_it;
} proc_t;

extern struct proc p0;
#define	curproc		(&p0)

#define	PS_NONE		-1

extern kthread_t *zk_thread_create(void (*func)(void *), void *arg,
    size_t stksize, int state);

#define	issig(why)	(FALSE)
#define	ISSIG(thr, why)	(FALSE)

#define	KPREEMPT_SYNC		(-1)

#define	kpreempt(x)		sched_yield()
#define	kpreempt_disable()	((void)0)
#define	kpreempt_enable()	((void)0)

 
typedef struct kmutex {
	pthread_mutex_t		m_lock;
	pthread_t		m_owner;
} kmutex_t;

#define	MUTEX_DEFAULT		0
#define	MUTEX_NOLOCKDEP		MUTEX_DEFAULT
#define	MUTEX_HELD(mp)		pthread_equal((mp)->m_owner, pthread_self())
#define	MUTEX_NOT_HELD(mp)	!MUTEX_HELD(mp)

extern void mutex_init(kmutex_t *mp, char *name, int type, void *cookie);
extern void mutex_destroy(kmutex_t *mp);
extern void mutex_enter(kmutex_t *mp);
extern int mutex_enter_check_return(kmutex_t *mp);
extern void mutex_exit(kmutex_t *mp);
extern int mutex_tryenter(kmutex_t *mp);

#define	NESTED_SINGLE 1
#define	mutex_enter_nested(mp, class) mutex_enter(mp)
#define	mutex_enter_interruptible(mp) mutex_enter_check_return(mp)
 
typedef struct krwlock {
	pthread_rwlock_t	rw_lock;
	pthread_t		rw_owner;
	uint_t			rw_readers;
} krwlock_t;

typedef int krw_t;

#define	RW_READER		0
#define	RW_WRITER		1
#define	RW_DEFAULT		RW_READER
#define	RW_NOLOCKDEP		RW_READER

#define	RW_READ_HELD(rw)	((rw)->rw_readers > 0)
#define	RW_WRITE_HELD(rw)	pthread_equal((rw)->rw_owner, pthread_self())
#define	RW_LOCK_HELD(rw)	(RW_READ_HELD(rw) || RW_WRITE_HELD(rw))

extern void rw_init(krwlock_t *rwlp, char *name, int type, void *arg);
extern void rw_destroy(krwlock_t *rwlp);
extern void rw_enter(krwlock_t *rwlp, krw_t rw);
extern int rw_tryenter(krwlock_t *rwlp, krw_t rw);
extern int rw_tryupgrade(krwlock_t *rwlp);
extern void rw_exit(krwlock_t *rwlp);
#define	rw_downgrade(rwlp) do { } while (0)

 
extern uid_t crgetuid(cred_t *cr);
extern uid_t crgetruid(cred_t *cr);
extern gid_t crgetgid(cred_t *cr);
extern int crgetngroups(cred_t *cr);
extern gid_t *crgetgroups(cred_t *cr);

 
typedef pthread_cond_t		kcondvar_t;

#define	CV_DEFAULT		0
#define	CALLOUT_FLAG_ABSOLUTE	0x2

extern void cv_init(kcondvar_t *cv, char *name, int type, void *arg);
extern void cv_destroy(kcondvar_t *cv);
extern void cv_wait(kcondvar_t *cv, kmutex_t *mp);
extern int cv_wait_sig(kcondvar_t *cv, kmutex_t *mp);
extern int cv_timedwait(kcondvar_t *cv, kmutex_t *mp, clock_t abstime);
extern int cv_timedwait_hires(kcondvar_t *cvp, kmutex_t *mp, hrtime_t tim,
    hrtime_t res, int flag);
extern void cv_signal(kcondvar_t *cv);
extern void cv_broadcast(kcondvar_t *cv);

#define	cv_timedwait_io(cv, mp, at)		cv_timedwait(cv, mp, at)
#define	cv_timedwait_idle(cv, mp, at)		cv_timedwait(cv, mp, at)
#define	cv_timedwait_sig(cv, mp, at)		cv_timedwait(cv, mp, at)
#define	cv_wait_io(cv, mp)			cv_wait(cv, mp)
#define	cv_wait_idle(cv, mp)			cv_wait(cv, mp)
#define	cv_wait_io_sig(cv, mp)			cv_wait_sig(cv, mp)
#define	cv_timedwait_sig_hires(cv, mp, t, r, f) \
	cv_timedwait_hires(cv, mp, t, r, f)
#define	cv_timedwait_idle_hires(cv, mp, t, r, f) \
	cv_timedwait_hires(cv, mp, t, r, f)

 
#define	tsd_get(k) pthread_getspecific(k)
#define	tsd_set(k, v) pthread_setspecific(k, v)
#define	tsd_create(kp, d) pthread_key_create((pthread_key_t *)kp, d)
#define	tsd_destroy(kp)  
#ifdef __FreeBSD__
typedef off_t loff_t;
#endif

 
extern kstat_t *kstat_create(const char *, int,
    const char *, const char *, uchar_t, ulong_t, uchar_t);
extern void kstat_install(kstat_t *);
extern void kstat_delete(kstat_t *);
extern void kstat_set_raw_ops(kstat_t *ksp,
    int (*headers)(char *buf, size_t size),
    int (*data)(char *buf, size_t size, void *data),
    void *(*addr)(kstat_t *ksp, loff_t index));

 

typedef struct procfs_list {
	void		*pl_private;
	kmutex_t	pl_lock;
	list_t		pl_list;
	uint64_t	pl_next_id;
	size_t		pl_node_offset;
} procfs_list_t;

#ifndef __cplusplus
struct seq_file { };
void seq_printf(struct seq_file *m, const char *fmt, ...);

typedef struct procfs_list_node {
	list_node_t	pln_link;
	uint64_t	pln_id;
} procfs_list_node_t;

void procfs_list_install(const char *module,
    const char *submodule,
    const char *name,
    mode_t mode,
    procfs_list_t *procfs_list,
    int (*show)(struct seq_file *f, void *p),
    int (*show_header)(struct seq_file *f),
    int (*clear)(procfs_list_t *procfs_list),
    size_t procfs_list_node_off);
void procfs_list_uninstall(procfs_list_t *procfs_list);
void procfs_list_destroy(procfs_list_t *procfs_list);
void procfs_list_add(procfs_list_t *procfs_list, void *p);
#endif

 
#define	KM_SLEEP		UMEM_NOFAIL
#define	KM_PUSHPAGE		KM_SLEEP
#define	KM_NOSLEEP		UMEM_DEFAULT
#define	KM_NORMALPRI		0	 
#define	KMC_NODEBUG		UMC_NODEBUG
#define	KMC_KVMEM		0x0
#define	kmem_alloc(_s, _f)	umem_alloc(_s, _f)
#define	kmem_zalloc(_s, _f)	umem_zalloc(_s, _f)
#define	kmem_free(_b, _s)	umem_free(_b, _s)
#define	vmem_alloc(_s, _f)	kmem_alloc(_s, _f)
#define	vmem_zalloc(_s, _f)	kmem_zalloc(_s, _f)
#define	vmem_free(_b, _s)	kmem_free(_b, _s)
#define	kmem_cache_create(_a, _b, _c, _d, _e, _f, _g, _h, _i) \
	umem_cache_create(_a, _b, _c, _d, _e, _f, _g, _h, _i)
#define	kmem_cache_destroy(_c)	umem_cache_destroy(_c)
#define	kmem_cache_alloc(_c, _f) umem_cache_alloc(_c, _f)
#define	kmem_cache_free(_c, _b)	umem_cache_free(_c, _b)
#define	kmem_debugging()	0
#define	kmem_cache_reap_now(_c)	umem_cache_reap_now(_c);
#define	kmem_cache_set_move(_c, _cb)	 
#define	POINTER_INVALIDATE(_pp)		 
#define	POINTER_IS_VALID(_p)	0

typedef umem_cache_t kmem_cache_t;

typedef enum kmem_cbrc {
	KMEM_CBRC_YES,
	KMEM_CBRC_NO,
	KMEM_CBRC_LATER,
	KMEM_CBRC_DONT_NEED,
	KMEM_CBRC_DONT_KNOW
} kmem_cbrc_t;

 

#define	TASKQ_NAMELEN	31

typedef uintptr_t taskqid_t;
typedef void (task_func_t)(void *);

typedef struct taskq_ent {
	struct taskq_ent	*tqent_next;
	struct taskq_ent	*tqent_prev;
	task_func_t		*tqent_func;
	void			*tqent_arg;
	uintptr_t		tqent_flags;
} taskq_ent_t;

typedef struct taskq {
	char		tq_name[TASKQ_NAMELEN + 1];
	kmutex_t	tq_lock;
	krwlock_t	tq_threadlock;
	kcondvar_t	tq_dispatch_cv;
	kcondvar_t	tq_wait_cv;
	kthread_t	**tq_threadlist;
	int		tq_flags;
	int		tq_active;
	int		tq_nthreads;
	int		tq_nalloc;
	int		tq_minalloc;
	int		tq_maxalloc;
	kcondvar_t	tq_maxalloc_cv;
	int		tq_maxalloc_wait;
	taskq_ent_t	*tq_freelist;
	taskq_ent_t	tq_task;
} taskq_t;

#define	TQENT_FLAG_PREALLOC	0x1	 

#define	TASKQ_PREPOPULATE	0x0001
#define	TASKQ_CPR_SAFE		0x0002	 
#define	TASKQ_DYNAMIC		0x0004	 
#define	TASKQ_THREADS_CPU_PCT	0x0008	 
#define	TASKQ_DC_BATCH		0x0010	 

#define	TQ_SLEEP	KM_SLEEP	 
#define	TQ_NOSLEEP	KM_NOSLEEP	 
#define	TQ_NOQUEUE	0x02		 
#define	TQ_FRONT	0x08		 

#define	TASKQID_INVALID		((taskqid_t)0)

extern taskq_t *system_taskq;
extern taskq_t *system_delay_taskq;

extern taskq_t	*taskq_create(const char *, int, pri_t, int, int, uint_t);
#define	taskq_create_proc(a, b, c, d, e, p, f) \
	    (taskq_create(a, b, c, d, e, f))
#define	taskq_create_sysdc(a, b, d, e, p, dc, f) \
	    ((void) sizeof (dc), taskq_create(a, b, maxclsyspri, d, e, f))
extern taskqid_t taskq_dispatch(taskq_t *, task_func_t, void *, uint_t);
extern taskqid_t taskq_dispatch_delay(taskq_t *, task_func_t, void *, uint_t,
    clock_t);
extern void	taskq_dispatch_ent(taskq_t *, task_func_t, void *, uint_t,
    taskq_ent_t *);
extern int	taskq_empty_ent(taskq_ent_t *);
extern void	taskq_init_ent(taskq_ent_t *);
extern void	taskq_destroy(taskq_t *);
extern void	taskq_wait(taskq_t *);
extern void	taskq_wait_id(taskq_t *, taskqid_t);
extern void	taskq_wait_outstanding(taskq_t *, taskqid_t);
extern int	taskq_member(taskq_t *, kthread_t *);
extern taskq_t	*taskq_of_curthread(void);
extern int	taskq_cancel_id(taskq_t *, taskqid_t);
extern void	system_taskq_init(void);
extern void	system_taskq_fini(void);

#define	XVA_MAPSIZE	3
#define	XVA_MAGIC	0x78766174

extern char *vn_dumpdir;
#define	AV_SCANSTAMP_SZ	32		 

typedef struct xoptattr {
	inode_timespec_t xoa_createtime;	 
	uint8_t		xoa_archive;
	uint8_t		xoa_system;
	uint8_t		xoa_readonly;
	uint8_t		xoa_hidden;
	uint8_t		xoa_nounlink;
	uint8_t		xoa_immutable;
	uint8_t		xoa_appendonly;
	uint8_t		xoa_nodump;
	uint8_t		xoa_settable;
	uint8_t		xoa_opaque;
	uint8_t		xoa_av_quarantined;
	uint8_t		xoa_av_modified;
	uint8_t		xoa_av_scanstamp[AV_SCANSTAMP_SZ];
	uint8_t		xoa_reparse;
	uint8_t		xoa_offline;
	uint8_t		xoa_sparse;
} xoptattr_t;

typedef struct vattr {
	uint_t		va_mask;	 
	u_offset_t	va_size;	 
} vattr_t;


typedef struct xvattr {
	vattr_t		xva_vattr;	 
	uint32_t	xva_magic;	 
	uint32_t	xva_mapsize;	 
	uint32_t	*xva_rtnattrmapp;	 
	uint32_t	xva_reqattrmap[XVA_MAPSIZE];	 
	uint32_t	xva_rtnattrmap[XVA_MAPSIZE];	 
	xoptattr_t	xva_xoptattrs;	 
} xvattr_t;

typedef struct vsecattr {
	uint_t		vsa_mask;	 
	int		vsa_aclcnt;	 
	void		*vsa_aclentp;	 
	int		vsa_dfaclcnt;	 
	void		*vsa_dfaclentp;	 
	size_t		vsa_aclentsz;	 
} vsecattr_t;

#define	AT_MODE		0x00002
#define	AT_UID		0x00004
#define	AT_GID		0x00008
#define	AT_FSID		0x00010
#define	AT_NODEID	0x00020
#define	AT_NLINK	0x00040
#define	AT_SIZE		0x00080
#define	AT_ATIME	0x00100
#define	AT_MTIME	0x00200
#define	AT_CTIME	0x00400
#define	AT_RDEV		0x00800
#define	AT_BLKSIZE	0x01000
#define	AT_NBLOCKS	0x02000
#define	AT_SEQ		0x08000
#define	AT_XVATTR	0x10000

#define	CRCREAT		0

#define	F_FREESP	11
#define	FIGNORECASE	0x80000  

 
#define	ddi_get_lbolt()		(gethrtime() >> 23)
#define	ddi_get_lbolt64()	(gethrtime() >> 23)
#define	hz	119	 

#define	ddi_time_before(a, b)		(a < b)
#define	ddi_time_after(a, b)		ddi_time_before(b, a)
#define	ddi_time_before_eq(a, b)	(!ddi_time_after(a, b))
#define	ddi_time_after_eq(a, b)		ddi_time_before_eq(b, a)

#define	ddi_time_before64(a, b)		(a < b)
#define	ddi_time_after64(a, b)		ddi_time_before64(b, a)
#define	ddi_time_before_eq64(a, b)	(!ddi_time_after64(a, b))
#define	ddi_time_after_eq64(a, b)	ddi_time_before_eq64(b, a)

extern void delay(clock_t ticks);

#define	SEC_TO_TICK(sec)	((sec) * hz)
#define	MSEC_TO_TICK(msec)	(howmany((hrtime_t)(msec) * hz, MILLISEC))
#define	USEC_TO_TICK(usec)	(howmany((hrtime_t)(usec) * hz, MICROSEC))
#define	NSEC_TO_TICK(nsec)	(howmany((hrtime_t)(nsec) * hz, NANOSEC))

#define	max_ncpus	64
#define	boot_ncpus	(sysconf(_SC_NPROCESSORS_ONLN))

 
#define	minclsyspri	19
#define	maxclsyspri	-20
#define	defclsyspri	0

#define	CPU_SEQID	((uintptr_t)pthread_self() & (max_ncpus - 1))
#define	CPU_SEQID_UNSTABLE	CPU_SEQID

#define	kcred		NULL
#define	CRED()		NULL

#define	ptob(x)		((x) * PAGESIZE)

#define	NN_DIVISOR_1000	(1U << 0)
#define	NN_NUMBUF_SZ	(6)

extern uint64_t physmem;
extern const char *random_path;
extern const char *urandom_path;

extern int highbit64(uint64_t i);
extern int lowbit64(uint64_t i);
extern int random_get_bytes(uint8_t *ptr, size_t len);
extern int random_get_pseudo_bytes(uint8_t *ptr, size_t len);

static __inline__ uint32_t
random_in_range(uint32_t range)
{
	uint32_t r;

	ASSERT(range != 0);

	if (range == 1)
		return (0);

	(void) random_get_pseudo_bytes((uint8_t *)&r, sizeof (r));

	return (r % range);
}

extern void kernel_init(int mode);
extern void kernel_fini(void);
extern void random_init(void);
extern void random_fini(void);

struct spa;
extern void show_pool_stats(struct spa *);
extern int set_global_var(char const *arg);

typedef struct callb_cpr {
	kmutex_t	*cc_lockp;
} callb_cpr_t;

#define	CALLB_CPR_INIT(cp, lockp, func, name)	{		\
	(cp)->cc_lockp = lockp;					\
}

#define	CALLB_CPR_SAFE_BEGIN(cp) {				\
	ASSERT(MUTEX_HELD((cp)->cc_lockp));			\
}

#define	CALLB_CPR_SAFE_END(cp, lockp) {				\
	ASSERT(MUTEX_HELD((cp)->cc_lockp));			\
}

#define	CALLB_CPR_EXIT(cp) {					\
	ASSERT(MUTEX_HELD((cp)->cc_lockp));			\
	mutex_exit((cp)->cc_lockp);				\
}

#define	zone_dataset_visible(x, y)	(1)
#define	INGLOBALZONE(z)			(1)
extern uint32_t zone_get_hostid(void *zonep);

extern char *kmem_vasprintf(const char *fmt, va_list adx);
extern char *kmem_asprintf(const char *fmt, ...);
#define	kmem_strfree(str) kmem_free((str), strlen(str) + 1)
#define	kmem_strdup(s)  strdup(s)

#ifndef __cplusplus
extern int kmem_scnprintf(char *restrict str, size_t size,
    const char *restrict fmt, ...);
#endif

 
extern int ddi_strtoull(const char *str, char **nptr, int base,
    u_longlong_t *result);

typedef struct utsname	utsname_t;
extern utsname_t *utsname(void);

 

struct _buf {
	intptr_t	_fd;
};

struct bootstat {
	uint64_t st_size;
};

typedef struct ace_object {
	uid_t		a_who;
	uint32_t	a_access_mask;
	uint16_t	a_flags;
	uint16_t	a_type;
	uint8_t		a_obj_type[16];
	uint8_t		a_inherit_obj_type[16];
} ace_object_t;


#define	ACE_ACCESS_ALLOWED_OBJECT_ACE_TYPE	0x05
#define	ACE_ACCESS_DENIED_OBJECT_ACE_TYPE	0x06
#define	ACE_SYSTEM_AUDIT_OBJECT_ACE_TYPE	0x07
#define	ACE_SYSTEM_ALARM_OBJECT_ACE_TYPE	0x08

extern int zfs_secpolicy_snapshot_perms(const char *name, cred_t *cr);
extern int zfs_secpolicy_rename_perms(const char *from, const char *to,
    cred_t *cr);
extern int zfs_secpolicy_destroy_perms(const char *name, cred_t *cr);
extern int secpolicy_zfs(const cred_t *cr);
extern int secpolicy_zfs_proc(const cred_t *cr, proc_t *proc);
extern zoneid_t getzoneid(void);

 
typedef struct ksiddomain {
	uint_t	kd_ref;
	uint_t	kd_len;
	char	*kd_name;
} ksiddomain_t;

ksiddomain_t *ksid_lookupdomain(const char *);
void ksiddomain_rele(ksiddomain_t *);

#define	DDI_SLEEP	KM_SLEEP
#define	ddi_log_sysevent(_a, _b, _c, _d, _e, _f, _g) \
	sysevent_post_event(_c, _d, _b, "libzpool", _e, _f)

#define	zfs_sleep_until(wakeup)						\
	do {								\
		hrtime_t delta = wakeup - gethrtime();			\
		struct timespec ts;					\
		ts.tv_sec = delta / NANOSEC;				\
		ts.tv_nsec = delta % NANOSEC;				\
		(void) nanosleep(&ts, NULL);				\
	} while (0)

typedef int fstrans_cookie_t;

extern fstrans_cookie_t spl_fstrans_mark(void);
extern void spl_fstrans_unmark(fstrans_cookie_t);
extern int __spl_pf_fstrans_check(void);
extern int kmem_cache_reap_active(void);


 
#define	__init
#define	__exit

#endif   

#ifdef __cplusplus
};
#endif

#endif	 
