


#ifndef __CPUPOWERUTILS_HELPERS__
#define __CPUPOWERUTILS_HELPERS__

#include <libintl.h>
#include <locale.h>
#include <stdbool.h>

#include "helpers/bitmask.h"
#include <cpupower.h>


#ifdef NLS

#define _(String) gettext(String)
#ifndef gettext_noop
#define gettext_noop(String) String
#endif
#define N_(String) gettext_noop(String)

#else 

#define _(String) String
#define N_(String) String

#endif


extern int run_as_root;
extern int base_cpu;
extern struct bitmask *cpus_chosen;



#ifdef DEBUG
extern int be_verbose;

#define dprint(fmt, ...) {					\
		if (be_verbose) {				\
			fprintf(stderr, "%s: " fmt,		\
				__func__, ##__VA_ARGS__);	\
		}						\
	}
#else
static inline void dprint(const char *fmt, ...) { }
#endif
extern int be_verbose;



enum cpupower_cpu_vendor {X86_VENDOR_UNKNOWN = 0, X86_VENDOR_INTEL,
			  X86_VENDOR_AMD, X86_VENDOR_HYGON, X86_VENDOR_MAX};

#define CPUPOWER_CAP_INV_TSC		0x00000001
#define CPUPOWER_CAP_APERF		0x00000002
#define CPUPOWER_CAP_AMD_CPB		0x00000004
#define CPUPOWER_CAP_PERF_BIAS		0x00000008
#define CPUPOWER_CAP_HAS_TURBO_RATIO	0x00000010
#define CPUPOWER_CAP_IS_SNB		0x00000020
#define CPUPOWER_CAP_INTEL_IDA		0x00000040
#define CPUPOWER_CAP_AMD_RDPRU		0x00000080
#define CPUPOWER_CAP_AMD_HW_PSTATE	0x00000100
#define CPUPOWER_CAP_AMD_PSTATEDEF	0x00000200
#define CPUPOWER_CAP_AMD_CPB_MSR	0x00000400
#define CPUPOWER_CAP_AMD_PSTATE		0x00000800

#define CPUPOWER_AMD_CPBDIS		0x02000000

#define MAX_HW_PSTATES 10

struct cpupower_cpu_info {
	enum cpupower_cpu_vendor vendor;
	unsigned int family;
	unsigned int model;
	unsigned int stepping;
	
	unsigned long long caps;
};


extern int get_cpu_info(struct cpupower_cpu_info *cpu_info);
extern struct cpupower_cpu_info cpupower_cpu_info;





#if defined(__i386__) || defined(__x86_64__)

#include <pci/pci.h>


extern int read_msr(int cpu, unsigned int idx, unsigned long long *val);
extern int write_msr(int cpu, unsigned int idx, unsigned long long val);

extern int cpupower_intel_set_perf_bias(unsigned int cpu, unsigned int val);
extern int cpupower_intel_get_perf_bias(unsigned int cpu);
extern unsigned long long msr_intel_get_turbo_ratio(unsigned int cpu);

extern int cpupower_set_epp(unsigned int cpu, char *epp);
extern int cpupower_set_amd_pstate_mode(char *mode);
extern int cpupower_set_turbo_boost(int turbo_boost);




extern int amd_pci_get_num_boost_states(int *active, int *states);
extern struct pci_dev *pci_acc_init(struct pci_access **pacc, int domain,
				    int bus, int slot, int func, int vendor,
				    int dev);
extern struct pci_dev *pci_slot_func_init(struct pci_access **pacc,
					      int slot, int func);





extern int decode_pstates(unsigned int cpu, int boost_states,
			  unsigned long *pstates, int *no);



extern int cpufreq_has_boost_support(unsigned int cpu, int *support,
				     int *active, int * states);


bool cpupower_amd_pstate_enabled(void);
void amd_pstate_boost_init(unsigned int cpu,
			   int *support, int *active);
void amd_pstate_show_perf_and_freq(unsigned int cpu,
				   int no_rounding);




unsigned int cpuid_eax(unsigned int op);
unsigned int cpuid_ebx(unsigned int op);
unsigned int cpuid_ecx(unsigned int op);
unsigned int cpuid_edx(unsigned int op);



#else
static inline int decode_pstates(unsigned int cpu, int boost_states,
				 unsigned long *pstates, int *no)
{ return -1; };

static inline int read_msr(int cpu, unsigned int idx, unsigned long long *val)
{ return -1; };
static inline int write_msr(int cpu, unsigned int idx, unsigned long long val)
{ return -1; };
static inline int cpupower_intel_set_perf_bias(unsigned int cpu, unsigned int val)
{ return -1; };
static inline int cpupower_intel_get_perf_bias(unsigned int cpu)
{ return -1; };
static inline unsigned long long msr_intel_get_turbo_ratio(unsigned int cpu)
{ return 0; };

static inline int cpupower_set_epp(unsigned int cpu, char *epp)
{ return -1; };
static inline int cpupower_set_amd_pstate_mode(char *mode)
{ return -1; };
static inline int cpupower_set_turbo_boost(int turbo_boost)
{ return -1; };



static inline int cpufreq_has_boost_support(unsigned int cpu, int *support,
					    int *active, int * states)
{ return -1; }

static inline bool cpupower_amd_pstate_enabled(void)
{ return false; }
static inline void amd_pstate_boost_init(unsigned int cpu, int *support,
					 int *active)
{}
static inline void amd_pstate_show_perf_and_freq(unsigned int cpu,
						 int no_rounding)
{}



static inline unsigned int cpuid_eax(unsigned int op) { return 0; };
static inline unsigned int cpuid_ebx(unsigned int op) { return 0; };
static inline unsigned int cpuid_ecx(unsigned int op) { return 0; };
static inline unsigned int cpuid_edx(unsigned int op) { return 0; };
#endif 


extern struct bitmask *online_cpus;
extern struct bitmask *offline_cpus;

void get_cpustate(void);
void print_online_cpus(void);
void print_offline_cpus(void);
void print_speed(unsigned long speed, int no_rounding);

#endif 
