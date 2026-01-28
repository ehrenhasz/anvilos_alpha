#ifndef _PERF_CPUM_CF_DIAG_H
#define _PERF_CPUM_CF_DIAG_H
#include <linux/ioctl.h>
#include <linux/types.h>
#define S390_HWCTR_DEVICE		"hwctr"
#define S390_HWCTR_START_VERSION	1
struct s390_ctrset_start {		 
	__u64 version;			 
	__u64 data_bytes;		 
	__u64 cpumask_len;		 
	__u64 *cpumask;			 
	__u64 counter_sets;		 
};
struct s390_ctrset_setdata {		 
	__u32 set;			 
	__u32 no_cnts;			 
	__u64 cv[];			 
};
struct s390_ctrset_cpudata {		 
	__u32 cpu_nr;			 
	__u32 no_sets;			 
	struct s390_ctrset_setdata data[];
};
struct s390_ctrset_read {		 
	__u64 no_cpus;			 
	struct s390_ctrset_cpudata data[];
};
#define S390_HWCTR_MAGIC	'C'	 
#define	S390_HWCTR_START	_IOWR(S390_HWCTR_MAGIC, 1, struct s390_ctrset_start)
#define	S390_HWCTR_STOP		_IO(S390_HWCTR_MAGIC, 2)
#define	S390_HWCTR_READ		_IOWR(S390_HWCTR_MAGIC, 3, struct s390_ctrset_read)
#endif
