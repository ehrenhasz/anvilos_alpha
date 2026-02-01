
 
#ifndef PP_DEBUG_H
#define PP_DEBUG_H

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "amdgpu: [powerplay] " fmt

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#define PP_ASSERT_WITH_CODE(cond, msg, code)	\
	do {					\
		if (!(cond)) {			\
			pr_warn_ratelimited("%s\n", msg);	\
			code;			\
		}				\
	} while (0)

#define PP_ASSERT(cond, msg)	\
	do {					\
		if (!(cond)) {			\
			pr_warn_ratelimited("%s\n", msg);	\
		}				\
	} while (0)

#define PP_DBG_LOG(fmt, ...) \
	do { \
		pr_debug(fmt, ##__VA_ARGS__); \
	} while (0)


#define GET_FLEXIBLE_ARRAY_MEMBER_ADDR(type, member, ptr, n)	\
	(type *)((char *)&(ptr)->member + (sizeof(type) * (n)))

#endif  

