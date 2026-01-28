#ifndef _SYS_FRAME_H
#define	_SYS_FRAME_H
#ifdef	__cplusplus
extern "C" {
#endif
#if defined(__KERNEL__) && defined(HAVE_KERNEL_OBJTOOL) && \
    defined(HAVE_STACK_FRAME_NON_STANDARD)
#if defined(HAVE_KERNEL_OBJTOOL_HEADER)
#include <linux/objtool.h>
#else
#include <linux/frame.h>
#endif
#else
#define	STACK_FRAME_NON_STANDARD(func)
#endif
#ifdef	__cplusplus
}
#endif
#endif	 
