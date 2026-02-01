
 

#include "log.h"
#include "main.h"

#include <linux/stdarg.h>

#include "trace.h"

 
int batadv_debug_log(struct batadv_priv *bat_priv, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	trace_batadv_dbg(bat_priv, &vaf);

	va_end(args);

	return 0;
}
