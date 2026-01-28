#ifndef __TOOLS_LINUX_ERR_H
#define __TOOLS_LINUX_ERR_H
#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/errno.h>
#define MAX_ERRNO	4095
#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)
static inline void * __must_check ERR_PTR(long error_)
{
	return (void *) error_;
}
static inline long __must_check PTR_ERR(__force const void *ptr)
{
	return (long) ptr;
}
static inline bool __must_check IS_ERR(__force const void *ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}
static inline bool __must_check IS_ERR_OR_NULL(__force const void *ptr)
{
	return unlikely(!ptr) || IS_ERR_VALUE((unsigned long)ptr);
}
static inline int __must_check PTR_ERR_OR_ZERO(__force const void *ptr)
{
	if (IS_ERR(ptr))
		return PTR_ERR(ptr);
	else
		return 0;
}
static inline void * __must_check ERR_CAST(__force const void *ptr)
{
	return (void *) ptr;
}
#endif  
