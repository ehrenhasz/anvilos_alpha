#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/misc.h>
#include <sys/sunddi.h>
#include <sys/sysctl.h>
int
ddi_strtol(const char *str, char **nptr, int base, long *result)
{
	*result = strtol(str, nptr, base);
	return (0);
}
int
ddi_strtoull(const char *str, char **nptr, int base, unsigned long long *result)
{
	*result = (unsigned long long)strtouq(str, nptr, base);
	return (0);
}
int
ddi_strtoll(const char *str, char **nptr, int base, long long *result)
{
	*result = (long long)strtoq(str, nptr, base);
	return (0);
}
