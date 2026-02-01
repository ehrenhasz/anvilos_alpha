
 

#include <linux/preempt.h>
#include <linux/kdb.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/printk.h>
#include <linux/kprobes.h>

#include "internal.h"

static DEFINE_PER_CPU(int, printk_context);

 
void __printk_safe_enter(void)
{
	this_cpu_inc(printk_context);
}

 
void __printk_safe_exit(void)
{
	this_cpu_dec(printk_context);
}

asmlinkage int vprintk(const char *fmt, va_list args)
{
#ifdef CONFIG_KGDB_KDB
	 
	if (unlikely(kdb_trap_printk && kdb_printf_cpu < 0))
		return vkdb_printf(KDB_MSGSRC_PRINTK, fmt, args);
#endif

	 
	if (this_cpu_read(printk_context) || in_nmi())
		return vprintk_deferred(fmt, args);

	 
	return vprintk_default(fmt, args);
}
EXPORT_SYMBOL(vprintk);
