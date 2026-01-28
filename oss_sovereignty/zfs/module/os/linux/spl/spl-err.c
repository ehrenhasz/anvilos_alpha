#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
static unsigned int spl_panic_halt;
module_param(spl_panic_halt, uint, 0644);
MODULE_PARM_DESC(spl_panic_halt, "Cause kernel panic on assertion failures");
void
spl_dumpstack(void)
{
	printk("Showing stack for process %d\n", current->pid);
	dump_stack();
}
EXPORT_SYMBOL(spl_dumpstack);
void
spl_panic(const char *file, const char *func, int line, const char *fmt, ...)
{
	const char *newfile;
	char msg[MAXMSGLEN];
	va_list ap;
	newfile = strrchr(file, '/');
	if (newfile != NULL)
		newfile = newfile + 1;
	else
		newfile = file;
	va_start(ap, fmt);
	(void) vsnprintf(msg, sizeof (msg), fmt, ap);
	va_end(ap);
	printk(KERN_EMERG "%s", msg);
	printk(KERN_EMERG "PANIC at %s:%d:%s()\n", newfile, line, func);
	if (spl_panic_halt)
		panic("%s", msg);
	spl_dumpstack();
	set_current_state(TASK_UNINTERRUPTIBLE);
	while (1)
		schedule();
}
EXPORT_SYMBOL(spl_panic);
void
vcmn_err(int ce, const char *fmt, va_list ap)
{
	char msg[MAXMSGLEN];
	vsnprintf(msg, MAXMSGLEN, fmt, ap);
	switch (ce) {
	case CE_IGNORE:
		break;
	case CE_CONT:
		printk("%s", msg);
		break;
	case CE_NOTE:
		printk(KERN_NOTICE "NOTICE: %s\n", msg);
		break;
	case CE_WARN:
		printk(KERN_WARNING "WARNING: %s\n", msg);
		break;
	case CE_PANIC:
		printk(KERN_EMERG "PANIC: %s\n", msg);
		if (spl_panic_halt)
			panic("%s", msg);
		spl_dumpstack();
		set_current_state(TASK_UNINTERRUPTIBLE);
		while (1)
			schedule();
	}
}  
EXPORT_SYMBOL(vcmn_err);
void
cmn_err(int ce, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcmn_err(ce, fmt, ap);
	va_end(ap);
}  
EXPORT_SYMBOL(cmn_err);
