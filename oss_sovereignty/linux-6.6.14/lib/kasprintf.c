
 

#include <linux/stdarg.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/string.h>

 
char *kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	unsigned int first, second;
	char *p;
	va_list aq;

	va_copy(aq, ap);
	first = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	p = kmalloc_track_caller(first+1, gfp);
	if (!p)
		return NULL;

	second = vsnprintf(p, first+1, fmt, ap);
	WARN(first != second, "different return values (%u and %u) from vsnprintf(\"%s\", ...)",
	     first, second, fmt);

	return p;
}
EXPORT_SYMBOL(kvasprintf);

 
const char *kvasprintf_const(gfp_t gfp, const char *fmt, va_list ap)
{
	if (!strchr(fmt, '%'))
		return kstrdup_const(fmt, gfp);
	if (!strcmp(fmt, "%s"))
		return kstrdup_const(va_arg(ap, const char*), gfp);
	return kvasprintf(gfp, fmt, ap);
}
EXPORT_SYMBOL(kvasprintf_const);

char *kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return p;
}
EXPORT_SYMBOL(kasprintf);
