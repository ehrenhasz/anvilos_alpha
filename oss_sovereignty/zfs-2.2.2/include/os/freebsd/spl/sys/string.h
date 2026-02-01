 

#ifndef _OPENSOLARIS_SYS_STRING_H_
#define	_OPENSOLARIS_SYS_STRING_H_

#include <sys/libkern.h>

char	*strpbrk(const char *, const char *);
void	 strident_canon(char *, size_t);
void	 kmem_strfree(char *);
char	*kmem_strdup(const char *s);

#endif	 
