#include <linux/compiler.h>
#ifndef _NATFEAT_H
#define _NATFEAT_H
long nf_get_id(const char *feature_name);
long nf_call(long id, ...);
void nf_init(void);
void nf_shutdown(void);
void nfprint(const char *fmt, ...)
	__printf(1, 2);
# endif  
