
#ifndef UTIL_LINUX_LOGIN_AUTH_H
#define UTIL_LINUX_LOGIN_AUTH_H

#include <sys/types.h>

extern int auth_pam(const char *service_name, uid_t uid, const char *username);

#endif 
