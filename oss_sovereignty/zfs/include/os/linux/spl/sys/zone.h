#ifndef _SPL_ZONE_H
#define	_SPL_ZONE_H
#include <sys/byteorder.h>
#include <sys/cred.h>
#include <linux/cred.h>
#include <linux/user_namespace.h>
extern int zone_dataset_attach(cred_t *, const char *, int);
extern int zone_dataset_detach(cred_t *, const char *, int);
extern int zone_dataset_visible(const char *dataset, int *write);
int spl_zone_init(void);
void spl_zone_fini(void);
extern unsigned int crgetzoneid(const cred_t *);
extern unsigned int global_zoneid(void);
extern boolean_t inglobalzone(proc_t *);
#define	INGLOBALZONE(x) inglobalzone(x)
#define	GLOBAL_ZONEID	global_zoneid()
#endif  
