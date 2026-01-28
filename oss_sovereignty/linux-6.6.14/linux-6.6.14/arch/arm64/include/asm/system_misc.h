#ifndef __ASM_SYSTEM_MISC_H
#define __ASM_SYSTEM_MISC_H
#ifndef __ASSEMBLY__
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/irqflags.h>
#include <linux/signal.h>
#include <linux/ratelimit.h>
#include <linux/reboot.h>
struct pt_regs;
void die(const char *msg, struct pt_regs *regs, long err);
struct siginfo;
void arm64_notify_die(const char *str, struct pt_regs *regs,
		      int signo, int sicode, unsigned long far,
		      unsigned long err);
void hook_debug_fault_code(int nr, int (*fn)(unsigned long, unsigned long,
					     struct pt_regs *),
			   int sig, int code, const char *name);
struct mm_struct;
extern void __show_regs(struct pt_regs *);
#endif	 
#endif	 
