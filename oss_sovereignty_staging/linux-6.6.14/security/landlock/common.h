 
 

#ifndef _SECURITY_LANDLOCK_COMMON_H
#define _SECURITY_LANDLOCK_COMMON_H

#define LANDLOCK_NAME "landlock"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) LANDLOCK_NAME ": " fmt

#endif  
