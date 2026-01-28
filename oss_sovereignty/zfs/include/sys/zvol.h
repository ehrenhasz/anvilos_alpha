#ifndef	_SYS_ZVOL_H
#define	_SYS_ZVOL_H
#include <sys/zfs_context.h>
#define	ZVOL_OBJ		1ULL
#define	ZVOL_ZAP_OBJ		2ULL
#define	SPEC_MAXOFFSET_T	((1LL << ((NBBY * sizeof (daddr32_t)) + \
				DEV_BSHIFT - 1)) - 1)
extern void zvol_create_minor(const char *);
extern void zvol_create_minors_recursive(const char *);
extern void zvol_remove_minors(spa_t *, const char *, boolean_t);
extern void zvol_rename_minors(spa_t *, const char *, const char *, boolean_t);
#ifdef _KERNEL
struct zvol_state;
typedef struct zvol_state zvol_state_handle_t;
extern int zvol_check_volsize(uint64_t, uint64_t);
extern int zvol_check_volblocksize(const char *, uint64_t);
extern int zvol_get_stats(objset_t *, nvlist_t *);
extern boolean_t zvol_is_zvol(const char *);
extern void zvol_create_cb(objset_t *, void *, cred_t *, dmu_tx_t *);
extern int zvol_set_volsize(const char *, uint64_t);
extern int zvol_set_snapdev(const char *, zprop_source_t, uint64_t);
extern int zvol_set_volmode(const char *, zprop_source_t, uint64_t);
extern zvol_state_handle_t *zvol_suspend(const char *);
extern int zvol_resume(zvol_state_handle_t *);
extern void *zvol_tag(zvol_state_handle_t *);
extern int zvol_init(void);
extern void zvol_fini(void);
extern int zvol_busy(void);
#endif  
#endif  
