
#undef _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <linux/string.h>

 
char *str_error_r(int errnum, char *buf, size_t buflen)
{
	int err = strerror_r(errnum, buf, buflen);
	if (err)
		snprintf(buf, buflen, "INTERNAL ERROR: strerror_r(%d, [buf], %zd)=%d", errnum, buflen, err);
	return buf;
}
