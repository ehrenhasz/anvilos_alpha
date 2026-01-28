


#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#define	ASMABI

#if defined(__i386) || defined(__amd64)

#include <sys/ia32/asm_linkage.h>	

#endif

#if defined(_KERNEL) && defined(HAVE_KERNEL_OBJTOOL)

#include <asm/frame.h>

#else 
#define	FRAME_BEGIN
#define	FRAME_END
#endif


#endif	
