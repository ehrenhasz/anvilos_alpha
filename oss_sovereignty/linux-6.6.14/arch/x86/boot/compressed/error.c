
 
#include "misc.h"
#include "error.h"

void warn(const char *m)
{
	error_putstr("\n\n");
	error_putstr(m);
	error_putstr("\n\n");
}

void error(char *m)
{
	warn(m);
	error_putstr(" -- System halted");

	while (1)
		asm("hlt");
}

 
#ifdef CONFIG_EFI_STUB
void panic(const char *fmt, ...)
{
	static char buf[1024];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	error(buf);
}
#endif
