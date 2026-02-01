 

#ifndef _SPL_SYSMACROS_H
#define	_SPL_SYSMACROS_H

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/cpumask.h>
#include <sys/debug.h>
#include <sys/zone.h>
#include <sys/signal.h>
#include <asm/page.h>


#ifndef _KERNEL
#define	_KERNEL				__KERNEL__
#endif

#define	FALSE				0
#define	TRUE				1

#define	INT8_MAX			(127)
#define	INT8_MIN			(-128)
#define	UINT8_MAX			(255)
#define	UINT8_MIN			(0)

#define	INT16_MAX			(32767)
#define	INT16_MIN			(-32768)
#define	UINT16_MAX			(65535)
#define	UINT16_MIN			(0)

#define	INT32_MAX			INT_MAX
#define	INT32_MIN			INT_MIN
#define	UINT32_MAX			UINT_MAX
#define	UINT32_MIN			UINT_MIN

#define	INT64_MAX			LLONG_MAX
#define	INT64_MIN			LLONG_MIN
#define	UINT64_MAX			ULLONG_MAX
#define	UINT64_MIN			ULLONG_MIN

#define	NBBY				8

#define	MAXMSGLEN			256
#define	MAXNAMELEN			256
#define	MAXPATHLEN			4096
#define	MAXOFFSET_T			LLONG_MAX
#define	MAXBSIZE			8192
#define	DEV_BSIZE			512
#define	DEV_BSHIFT			9  

#define	proc_pageout			NULL
#define	curproc				current
#define	max_ncpus			num_possible_cpus()
#define	boot_ncpus			num_online_cpus()
#define	CPU_SEQID			smp_processor_id()
#define	CPU_SEQID_UNSTABLE		raw_smp_processor_id()
#define	is_system_labeled()		0

#ifndef RLIM64_INFINITY
#define	RLIM64_INFINITY			(~0ULL)
#endif

 
#define	minclsyspri			(MAX_PRIO-1)
#define	maxclsyspri			(MAX_RT_PRIO)
#define	defclsyspri			(DEFAULT_PRIO)

#ifndef NICE_TO_PRIO
#define	NICE_TO_PRIO(nice)		(MAX_RT_PRIO + (nice) + 20)
#endif
#ifndef PRIO_TO_NICE
#define	PRIO_TO_NICE(prio)		((prio) - MAX_RT_PRIO - 20)
#endif

 
#ifndef PAGESIZE
#define	PAGESIZE			PAGE_SIZE
#endif

#ifndef PAGESHIFT
#define	PAGESHIFT			PAGE_SHIFT
#endif

 
extern unsigned long spl_hostid;

 
extern uint32_t zone_get_hostid(void *zone);
extern void spl_setup(void);
extern void spl_cleanup(void);

 
static inline dev_t
makedev(unsigned int major, unsigned int minor)
{
	return ((major & 0xFFF) << 8) | (minor & 0xFF);
}

#define	highbit(x)		__fls(x)
#define	lowbit(x)		__ffs(x)

#define	highbit64(x)		fls64(x)
#define	makedevice(maj, min)	makedev(maj, min)

 
#ifndef MIN
#define	MIN(a, b)		((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define	MAX(a, b)		((a) < (b) ? (b) : (a))
#endif
#ifndef ABS
#define	ABS(a)			((a) < 0 ? -(a) : (a))
#endif
#ifndef DIV_ROUND_UP
#define	DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#endif
#ifndef roundup
#define	roundup(x, y)		((((x) + ((y) - 1)) / (y)) * (y))
#endif
#ifndef howmany
#define	howmany(x, y)		(((x) + ((y) - 1)) / (y))
#endif

 
#define	P2ALIGN(x, align)	((x) & -(align))
#define	P2CROSS(x, y, align)	(((x) ^ (y)) > (align) - 1)
#define	P2ROUNDUP(x, align)	((((x) - 1) | ((align) - 1)) + 1)
#define	P2PHASE(x, align)	((x) & ((align) - 1))
#define	P2NPHASE(x, align)	(-(x) & ((align) - 1))
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
#define	P2CROSS_TYPED(x, y, align, type)	\
	(((type)(x) ^ (type)(y)) > (type)(align) - 1)
#define	P2SAMEHIGHBIT_TYPED(x, y, type) \
	(((type)(x) ^ (type)(y)) < ((type)(x) & (type)(y)))

#define	SET_ERROR(err) \
	(__set_error(__FILE__, __func__, __LINE__, err), err)

#include <linux/sort.h>
#define	qsort(base, num, size, cmp)		\
	sort(base, num, size, cmp, NULL)

#if !defined(_KMEMUSER) && !defined(offsetof)

 

#define	offsetof(s, m)  ((size_t)(&(((s *)0)->m)))
#endif

#endif   
