#ifndef _SECURITY_LANDLOCK_SETUP_H
#define _SECURITY_LANDLOCK_SETUP_H
#include <linux/lsm_hooks.h>
extern bool landlock_initialized;
extern struct lsm_blob_sizes landlock_blob_sizes;
#endif  
