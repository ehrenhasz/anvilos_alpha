 
 

#include <linux/cpufreq.h>
#include <linux/types.h>

int cbe_cpufreq_set_pmode(int cpu, unsigned int pmode);
int cbe_cpufreq_get_pmode(int cpu);

int cbe_cpufreq_set_pmode_pmi(int cpu, unsigned int pmode);

#if IS_ENABLED(CONFIG_CPU_FREQ_CBE_PMI)
extern bool cbe_cpufreq_has_pmi;
void cbe_cpufreq_pmi_policy_init(struct cpufreq_policy *policy);
void cbe_cpufreq_pmi_policy_exit(struct cpufreq_policy *policy);
void cbe_cpufreq_pmi_init(void);
void cbe_cpufreq_pmi_exit(void);
#else
#define cbe_cpufreq_has_pmi (0)
static inline void cbe_cpufreq_pmi_policy_init(struct cpufreq_policy *policy) {}
static inline void cbe_cpufreq_pmi_policy_exit(struct cpufreq_policy *policy) {}
static inline void cbe_cpufreq_pmi_init(void) {}
static inline void cbe_cpufreq_pmi_exit(void) {}
#endif
