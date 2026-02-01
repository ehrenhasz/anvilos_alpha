 
 

 
 

 

#ifndef _SYS_CRED_H
#define	_SYS_CRED_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

 

typedef struct ucred cred_t;

#define	CRED()		curthread->td_ucred

 
#define	kcred	(thread0.td_ucred)

#define	KUID_TO_SUID(x)		(x)
#define	KGID_TO_SGID(x)		(x)
#define	crgetuid(cr)		((cr)->cr_uid)
#define	crgetruid(cr)		((cr)->cr_ruid)
#define	crgetgid(cr)		((cr)->cr_gid)
#define	crgetgroups(cr)		((cr)->cr_groups)
#define	crgetngroups(cr)	((cr)->cr_ngroups)
#define	crgetzoneid(cr) 	((cr)->cr_prison->pr_id)

#ifdef	__cplusplus
}
#endif

#endif	 
