

#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/types.h>

char *skip_spaces(const char *str)
{
	while (isspace(*str))
		++str;
	return (char *)str;
}
