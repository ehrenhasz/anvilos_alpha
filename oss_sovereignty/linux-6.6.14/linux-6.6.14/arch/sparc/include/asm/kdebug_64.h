#ifndef _SPARC64_KDEBUG_H
#define _SPARC64_KDEBUG_H
struct pt_regs;
void bad_trap(struct pt_regs *, long);
enum die_val {
	DIE_OOPS = 1,
	DIE_DEBUG,	 
	DIE_DEBUG_2,	 
	DIE_BPT,	 
	DIE_SSTEP,	 
	DIE_DIE,
	DIE_TRAP,
	DIE_TRAP_TL1,
	DIE_CALL,
	DIE_NMI,
	DIE_NMIWATCHDOG,
};
#endif
