#ifndef __CPUPOWER_CPUPOWER_H__
#define __CPUPOWER_CPUPOWER_H__
struct cpupower_topology {
	unsigned int cores;
	unsigned int pkgs;
	unsigned int threads;  
	struct cpuid_core_info *core_info;
};
struct cpuid_core_info {
	int pkg;
	int core;
	int cpu;
	unsigned int is_online:1;
};
#ifdef __cplusplus
extern "C" {
#endif
int get_cpu_topology(struct cpupower_topology *cpu_top);
void cpu_topology_release(struct cpupower_topology cpu_top);
int cpupower_is_cpu_online(unsigned int cpu);
#ifdef __cplusplus
}
#endif
#endif
