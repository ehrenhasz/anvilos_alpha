
 

#include <linux/kernel.h>
#include <linux/buildid.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/smp.h>
#include <linux/atomic.h>
#include <linux/kexec.h>
#include <linux/utsname.h>
#include <linux/stop_machine.h>

static char dump_stack_arch_desc_str[128];

 
void __init dump_stack_set_arch_desc(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(dump_stack_arch_desc_str, sizeof(dump_stack_arch_desc_str),
		  fmt, args);
	va_end(args);
}

#if IS_ENABLED(CONFIG_STACKTRACE_BUILD_ID)
#define BUILD_ID_FMT " %20phN"
#define BUILD_ID_VAL vmlinux_build_id
#else
#define BUILD_ID_FMT "%s"
#define BUILD_ID_VAL ""
#endif

 
void dump_stack_print_info(const char *log_lvl)
{
	printk("%sCPU: %d PID: %d Comm: %.20s %s%s %s %.*s" BUILD_ID_FMT "\n",
	       log_lvl, raw_smp_processor_id(), current->pid, current->comm,
	       kexec_crash_loaded() ? "Kdump: loaded " : "",
	       print_tainted(),
	       init_utsname()->release,
	       (int)strcspn(init_utsname()->version, " "),
	       init_utsname()->version, BUILD_ID_VAL);

	if (dump_stack_arch_desc_str[0] != '\0')
		printk("%sHardware name: %s\n",
		       log_lvl, dump_stack_arch_desc_str);

	print_worker_info(log_lvl, current);
	print_stop_info(log_lvl, current);
}

 
void show_regs_print_info(const char *log_lvl)
{
	dump_stack_print_info(log_lvl);
}

static void __dump_stack(const char *log_lvl)
{
	dump_stack_print_info(log_lvl);
	show_stack(NULL, NULL, log_lvl);
}

 
asmlinkage __visible void dump_stack_lvl(const char *log_lvl)
{
	unsigned long flags;

	 
	printk_cpu_sync_get_irqsave(flags);
	__dump_stack(log_lvl);
	printk_cpu_sync_put_irqrestore(flags);
}
EXPORT_SYMBOL(dump_stack_lvl);

asmlinkage __visible void dump_stack(void)
{
	dump_stack_lvl(KERN_DEFAULT);
}
EXPORT_SYMBOL(dump_stack);
