#ifndef _ASM_POWERPC_VPHN_H
#define _ASM_POWERPC_VPHN_H
#define VPHN_REGISTER_COUNT 6
#define VPHN_ASSOC_BUFSIZE (VPHN_REGISTER_COUNT*sizeof(u64)/sizeof(u16) + 1)
#define VPHN_FLAG_VCPU	1
#define VPHN_FLAG_PCPU	2
long hcall_vphn(unsigned long cpu, u64 flags, __be32 *associativity);
#endif  
