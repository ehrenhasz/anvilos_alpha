


#ifndef _LIBSPL_STRING_H
#define	_LIBSPL_STRING_H

#include_next <string.h>

#ifndef HAVE_STRLCAT
extern size_t strlcat(char *dst, const char *src, size_t dstsize);
#endif

#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *dst, const char *src, size_t len);
#endif

#endif
