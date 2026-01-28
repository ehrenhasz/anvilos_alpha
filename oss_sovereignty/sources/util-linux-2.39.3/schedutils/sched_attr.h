
#ifndef UTIL_LINUX_SCHED_ATTR_H
#define UTIL_LINUX_SCHED_ATTR_H


#if defined (__linux__) && !defined(SCHED_BATCH)
# define SCHED_BATCH 3
#endif


#if defined (__linux__) && !defined(SCHED_IDLE)
# define SCHED_IDLE 5
#endif


#if defined(__linux__) && !defined(SCHED_RESET_ON_FORK)
# define SCHED_RESET_ON_FORK 0x40000000
#endif


#if defined(__linux__) && !defined(SCHED_FLAG_RESET_ON_FORK)
# define SCHED_FLAG_RESET_ON_FORK 0x01
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_RECLAIM)
# define SCHED_FLAG_RECLAIM 0x02
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_DL_OVERRUN)
# define SCHED_FLAG_DL_OVERRUN 0x04
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_KEEP_POLICY)
# define SCHED_FLAG_KEEP_POLICY 0x08
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_KEEP_PARAMS)
# define SCHED_FLAG_KEEP_PARAMS 0x10
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_UTIL_CLAMP_MIN)
# define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#endif

#if defined(__linux__) && !defined(SCHED_FLAG_UTIL_CLAMP_MAX)
# define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
#endif

#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif


#if defined (__linux__) && !defined(SYS_sched_setattr) && defined(__NR_sched_setattr)
# define SYS_sched_setattr __NR_sched_setattr
#endif

#if defined (__linux__) && !defined(SYS_sched_getattr) && defined(__NR_sched_getattr)
# define SYS_sched_getattr __NR_sched_getattr
#endif

#if defined (__linux__) && !defined(HAVE_SCHED_SETATTR) && defined(SYS_sched_setattr)
# define HAVE_SCHED_SETATTR

struct sched_attr {
	uint32_t size;
	uint32_t sched_policy;
	uint64_t sched_flags;

	
	int32_t sched_nice;

	
	uint32_t sched_priority;

	
	uint64_t sched_runtime;
	uint64_t sched_deadline;
	uint64_t sched_period;

	
	uint32_t sched_util_min;
	uint32_t sched_util_max;
};

static int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags)
{
	return syscall(SYS_sched_setattr, pid, attr, flags);
}

static int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags)
{
	return syscall(SYS_sched_getattr, pid, attr, size, flags);
}
#endif


#if defined (__linux__) && !defined(SCHED_DEADLINE) && defined(HAVE_SCHED_SETATTR)
# define SCHED_DEADLINE 6
#endif

#endif 
