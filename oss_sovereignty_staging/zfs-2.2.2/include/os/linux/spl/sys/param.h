 

#ifndef _SPL_PARAM_H
#define	_SPL_PARAM_H

#include <asm/page.h>

 
#define	ptob(pages)			((pages) << PAGE_SHIFT)
#define	btop(bytes)			((bytes) >> PAGE_SHIFT)

#define	MAXUID				UINT32_MAX

#endif  
