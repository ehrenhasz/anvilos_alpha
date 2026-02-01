 
#ifndef PERF_CPUID_H
#define PERF_CPUID_H 1


static inline void
cpuid(unsigned int op, unsigned int op2, unsigned int *a, unsigned int *b,
	unsigned int *c, unsigned int *d)
{
	 
	asm(
#if defined(__x86_64__)
		"mov %%rbx, %%rdi\n"
		"cpuid\n"
		"xchg %%rdi, %%rbx\n"
#else
		"pushl %%ebx\n"
		"cpuid\n"
		"movl %%ebx, %%edi\n"
		"popl %%ebx\n"
#endif
		: "=a"(*a), "=D"(*b), "=c"(*c), "=d"(*d)
		: "a"(op), "2"(op2));
}

void get_cpuid_0(char *vendor, unsigned int *lvl);

#endif
