 
 

#include "parse.h"

long long get_time();

int set_cpufreq_governor(char *governor, unsigned int cpu);
int set_cpu_affinity(unsigned int cpu);
int set_process_priority(int priority);

void prepare_user(const struct config *config);
void prepare_system(const struct config *config);
