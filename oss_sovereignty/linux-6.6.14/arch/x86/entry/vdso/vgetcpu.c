
 

#include <linux/kernel.h>
#include <linux/getcpu.h>
#include <asm/segment.h>
#include <vdso/processor.h>

notrace long
__vdso_getcpu(unsigned *cpu, unsigned *node, struct getcpu_cache *unused)
{
	vdso_read_cpunode(cpu, node);

	return 0;
}

long getcpu(unsigned *cpu, unsigned *node, struct getcpu_cache *tcache)
	__attribute__((weak, alias("__vdso_getcpu")));
