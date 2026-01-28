#ifndef ALPINE_CPU_RESUME_H_
#define ALPINE_CPU_RESUME_H_
struct al_cpu_resume_regs_per_cpu {
	uint32_t	flags;
	uint32_t	resume_addr;
};
struct al_cpu_resume_regs {
	uint32_t watermark;
	uint32_t flags;
	struct al_cpu_resume_regs_per_cpu per_cpu[];
};
#define AL_CPU_RESUME_MAGIC_NUM		0xf0e1d200
#define AL_CPU_RESUME_MAGIC_NUM_MASK	0xffffff00
#endif  
