#ifndef __ASSERT_SUPPORT_H_INCLUDED__
#define __ASSERT_SUPPORT_H_INCLUDED__
#define COMPILATION_ERROR_IF(condition) ((void)sizeof(char[1 - 2 * !!(condition)]))
#ifndef CT_ASSERT
#define CT_ASSERT(cnd) ((void)sizeof(char[(cnd) ? 1 :  -1]))
#endif  
#include <linux/bug.h>
#define assert(cnd) \
	do { \
		if (!(cnd)) \
			BUG(); \
	} while (0)
#ifndef PIPE_GENERATION
#define OP___assert(cnd) assert(cnd)
static inline void compile_time_assert(unsigned int cond)
{
	void _compile_time_assert(void);
	if (!cond) _compile_time_assert();
}
#endif  
#endif  
