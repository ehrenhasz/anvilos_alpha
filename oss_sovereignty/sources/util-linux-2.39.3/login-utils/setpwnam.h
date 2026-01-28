
#ifndef UTIL_LINUX_SETPWNAM_H
#define UTIL_LINUX_SETPWNAM_H

#include "pathnames.h"

#ifndef DEBUG
# define PASSWD_FILE	_PATH_PASSWD
# define GROUP_FILE	_PATH_GROUP
# define SHADOW_FILE	_PATH_SHADOW_PASSWD
# define SGROUP_FILE	_PATH_GSHADOW
#else
# define PASSWD_FILE	"/tmp/passwd"
# define GROUP_FILE	"/tmp/group"
# define SHADOW_FILE	"/tmp/shadow"
# define SGROUP_FILE	"/tmp/gshadow"
#endif

extern int setpwnam (struct passwd *pwd, const char *prefix);

#endif 
