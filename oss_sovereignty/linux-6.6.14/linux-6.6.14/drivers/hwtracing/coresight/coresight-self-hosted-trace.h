#ifndef __CORESIGHT_SELF_HOSTED_TRACE_H
#define __CORESIGHT_SELF_HOSTED_TRACE_H
#include <asm/sysreg.h>
static inline u64 read_trfcr(void)
{
	return read_sysreg_s(SYS_TRFCR_EL1);
}
static inline void write_trfcr(u64 val)
{
	write_sysreg_s(val, SYS_TRFCR_EL1);
	isb();
}
static inline u64 cpu_prohibit_trace(void)
{
	u64 trfcr = read_trfcr();
	write_trfcr(trfcr & ~(TRFCR_ELx_ExTRE | TRFCR_ELx_E0TRE));
	return trfcr;
}
#endif  
