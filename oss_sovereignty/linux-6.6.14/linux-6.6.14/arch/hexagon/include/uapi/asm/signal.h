#ifndef _ASM_SIGNAL_H
#define _ASM_SIGNAL_H
extern unsigned long __rt_sigtramp_template[2];
void do_signal(struct pt_regs *regs);
#include <asm-generic/signal.h>
#endif
