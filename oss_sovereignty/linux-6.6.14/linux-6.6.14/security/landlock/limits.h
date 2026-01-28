#ifndef _SECURITY_LANDLOCK_LIMITS_H
#define _SECURITY_LANDLOCK_LIMITS_H
#include <linux/bitops.h>
#include <linux/limits.h>
#include <uapi/linux/landlock.h>
#define LANDLOCK_MAX_NUM_LAYERS		16
#define LANDLOCK_MAX_NUM_RULES		U32_MAX
#define LANDLOCK_LAST_ACCESS_FS		LANDLOCK_ACCESS_FS_TRUNCATE
#define LANDLOCK_MASK_ACCESS_FS		((LANDLOCK_LAST_ACCESS_FS << 1) - 1)
#define LANDLOCK_NUM_ACCESS_FS		__const_hweight64(LANDLOCK_MASK_ACCESS_FS)
#endif  
