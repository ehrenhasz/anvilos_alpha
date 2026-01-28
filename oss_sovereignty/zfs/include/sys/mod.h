#ifndef _SYS_MOD_H
#define	_SYS_MOD_H
#ifdef _KERNEL
#include <sys/mod_os.h>
#else
#define	EXPORT_SYMBOL(x)
#endif
#endif  
