 
 

#ifndef _LIBSPL_ZONE_H
#define	_LIBSPL_ZONE_H

#include <sys/types.h>
#include <sys/zone.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __FreeBSD__
#define	GLOBAL_ZONEID	0
#else
 
#define	GLOBAL_ZONEID	4026531837U
#endif

extern zoneid_t		getzoneid(void);

#ifdef	__cplusplus
}
#endif

#endif  
