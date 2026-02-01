 
 

#ifndef __PRINT_SUPPORT_H_INCLUDED__
#define __PRINT_SUPPORT_H_INCLUDED__

#include <linux/stdarg.h>

extern int (*sh_css_printf)(const char *fmt, va_list args);
 
static inline  __printf(1, 2) void ia_css_print(const char *fmt, ...)
{
	va_list ap;

	if (sh_css_printf) {
		va_start(ap, fmt);
		sh_css_printf(fmt, ap);
		va_end(ap);
	}
}

 
 
#define PWARN(format, ...) ia_css_print("warning: ", ##__VA_ARGS__)
#define PRINT(format, ...) ia_css_print(format, ##__VA_ARGS__)
#define PERROR(format, ...) ia_css_print("error: " format, ##__VA_ARGS__)
#define PDEBUG(format, ...) ia_css_print("debug: " format, ##__VA_ARGS__)

#endif  
