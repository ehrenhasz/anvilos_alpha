#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
void
vcmn_err(int ce, const char *fmt, va_list adx)
{
	char buf[256];
	const char *prefix;
	prefix = NULL;  
	switch (ce) {
	case CE_CONT:
		prefix = "Solaris(cont): ";
		break;
	case CE_NOTE:
		prefix = "Solaris: NOTICE: ";
		break;
	case CE_WARN:
		prefix = "Solaris: WARNING: ";
		break;
	case CE_PANIC:
		prefix = "Solaris(panic): ";
		break;
	case CE_IGNORE:
		break;
	default:
		panic("Solaris: unknown severity level");
	}
	if (ce == CE_PANIC) {
		vsnprintf(buf, sizeof (buf), fmt, adx);
		panic("%s%s", prefix, buf);
	}
	if (ce != CE_IGNORE) {
		printf("%s", prefix);
		vprintf(fmt, adx);
		printf("\n");
	}
}
void
cmn_err(int type, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vcmn_err(type, fmt, ap);
	va_end(ap);
}
