 
 
#ifndef S390_CPUMCF_KERNEL_H
#define S390_CPUMCF_KERNEL_H

#define	S390_CPUMCF_DIAG_DEF	0xfeef	 
#define	PERF_EVENT_CPUM_CF_DIAG	0xBC000	 
#define PERF_EVENT_CPUM_SF_DIAG	0xBD000  

struct cf_ctrset_entry {	 
	unsigned int def:16;	 
	unsigned int set:16;	 
	unsigned int ctr:16;	 
	unsigned int res1:16;	 
};

struct cf_trailer_entry {	 
	 
	union {
		struct {
			unsigned int clock_base:1;	 
			unsigned int speed:1;		 
			 
			unsigned int mtda:1;	 
			unsigned int caca:1;	 
			unsigned int lcda:1;	 
		};
		unsigned long flags;		 
	};
	 
	unsigned int cfvn:16;			 
	unsigned int csvn:16;			 
	unsigned int cpu_speed:32;		 
	 
	unsigned long timestamp;		 
	 
	union {
		struct {
			unsigned long progusage1;
			unsigned long progusage2;
			unsigned long progusage3;
			unsigned long tod_base;
		};
		unsigned long progusage[4];
	};
	 
	unsigned int mach_type:16;		 
	unsigned int res1:16;			 
	unsigned int res2:32;			 
};

#define	CPUMF_CTR_SET_BASIC	0	 
#define	CPUMF_CTR_SET_USER	1	 
#define	CPUMF_CTR_SET_CRYPTO	2	 
#define	CPUMF_CTR_SET_EXT	3	 
#define	CPUMF_CTR_SET_MT_DIAG	4	 
#endif
