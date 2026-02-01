 
 

 

#ifndef _SYS_PROCESSOR_H
#define	_SYS_PROCESSOR_H

#include <sys/types.h>
#ifdef	__cplusplus
extern "C" {
#endif

 

 
typedef uint16_t lgrpid_t;

 
typedef	int	processorid_t;
typedef int	chipid_t;

#define	getcpuid() curcpu

#ifdef __cplusplus
}
#endif

#endif	 
