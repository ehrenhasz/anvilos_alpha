
#ifndef UTIL_LINUX_LOGIN_LIBUSER_H
#define UTIL_LINUX_LOGIN_LIBUSER_H

#include <sys/types.h>

extern int set_value_libuser(const char *service_name, const char *username,
			uid_t uid, const char *attr, const char *val);

#endif 
