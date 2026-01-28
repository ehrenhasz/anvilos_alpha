#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/misc.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/zfs_context.h>
static struct opensolaris_utsname hw_utsname = {
	.machine = MACHINE
};
utsname_t *
utsname(void)
{
	return (&hw_utsname);
}
static void
opensolaris_utsname_init(void *arg)
{
	hw_utsname.sysname = ostype;
	hw_utsname.nodename = prison0.pr_hostname;
	hw_utsname.release = osrelease;
	snprintf(hw_utsname.version, sizeof (hw_utsname.version),
	    "%d", osreldate);
}
char *
kmem_strdup(const char *s)
{
	char *buf;
	buf = kmem_alloc(strlen(s) + 1, KM_SLEEP);
	strcpy(buf, s);
	return (buf);
}
int
ddi_copyin(const void *from, void *to, size_t len, int flags)
{
	if (flags & FKIOCTL) {
		memcpy(to, from, len);
		return (0);
	}
	return (copyin(from, to, len));
}
int
ddi_copyout(const void *from, void *to, size_t len, int flags)
{
	if (flags & FKIOCTL) {
		memcpy(to, from, len);
		return (0);
	}
	return (copyout(from, to, len));
}
void
spl_panic(const char *file, const char *func, int line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vpanic(fmt, ap);
	va_end(ap);
}
SYSINIT(opensolaris_utsname_init, SI_SUB_TUNABLES, SI_ORDER_ANY,
    opensolaris_utsname_init, NULL);
