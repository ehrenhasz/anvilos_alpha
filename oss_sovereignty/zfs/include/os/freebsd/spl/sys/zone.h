#ifndef _OPENSOLARIS_SYS_ZONE_H_
#define	_OPENSOLARIS_SYS_ZONE_H_
#include <sys/jail.h>
#define	GLOBAL_ZONEID	0
#define	INGLOBALZONE(proc)	(!jailed((proc)->p_ucred))
extern int zone_dataset_attach(struct ucred *, const char *, int);
extern int zone_dataset_detach(struct ucred *, const char *, int);
extern int zone_dataset_visible(const char *, int *);
extern uint32_t zone_get_hostid(void *);
#endif	 
