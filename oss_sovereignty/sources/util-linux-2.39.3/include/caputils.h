

#ifndef CAPUTILS_H
#define CAPUTILS_H

#include <linux/capability.h>

#ifndef PR_CAP_AMBIENT
# define PR_CAP_AMBIENT		47
#  define PR_CAP_AMBIENT_IS_SET	1
#  define PR_CAP_AMBIENT_RAISE	2
#  define PR_CAP_AMBIENT_LOWER	3
#endif

extern int capset(cap_user_header_t header, cap_user_data_t data);
extern int capget(cap_user_header_t header, const cap_user_data_t data);

extern int cap_last_cap(void);

#endif 
