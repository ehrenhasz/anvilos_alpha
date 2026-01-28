
#ifndef _TOOLS_LINUX_STRING_H_
#define _TOOLS_LINUX_STRING_H_

#include <linux/types.h>	
#include <string.h>

void *memdup(const void *src, size_t len);

char **argv_split(const char *str, int *argcp);
void argv_free(char **argv);

int strtobool(const char *s, bool *res);


#if defined(__GLIBC__) && !defined(__UCLIBC__)

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#endif
extern size_t strlcpy(char *dest, const char *src, size_t size);
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#endif
#endif

char *str_error_r(int errnum, char *buf, size_t buflen);

char *strreplace(char *s, char old, char new);


static inline bool strstarts(const char *str, const char *prefix)
{
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

extern char * __must_check skip_spaces(const char *);

extern char *strim(char *);

extern void *memchr_inv(const void *start, int c, size_t bytes);
#endif 
