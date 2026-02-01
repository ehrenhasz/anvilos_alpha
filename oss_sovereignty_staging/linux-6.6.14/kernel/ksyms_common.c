
 
#include <linux/kallsyms.h>
#include <linux/security.h>

static inline int kallsyms_for_perf(void)
{
#ifdef CONFIG_PERF_EVENTS
	extern int sysctl_perf_event_paranoid;

	if (sysctl_perf_event_paranoid <= 1)
		return 1;
#endif
	return 0;
}

 
bool kallsyms_show_value(const struct cred *cred)
{
	switch (kptr_restrict) {
	case 0:
		if (kallsyms_for_perf())
			return true;
		fallthrough;
	case 1:
		if (security_capable(cred, &init_user_ns, CAP_SYSLOG,
				     CAP_OPT_NOAUDIT) == 0)
			return true;
		fallthrough;
	default:
		return false;
	}
}
