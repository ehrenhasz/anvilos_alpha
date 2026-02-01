 
 

#ifndef	_SYS_DSL_DELEG_H
#define	_SYS_DSL_DELEG_H

#include <sys/dmu.h>
#include <sys/dsl_pool.h>
#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	ZFS_DELEG_PERM_NONE		""
#define	ZFS_DELEG_PERM_CREATE		"create"
#define	ZFS_DELEG_PERM_DESTROY		"destroy"
#define	ZFS_DELEG_PERM_SNAPSHOT		"snapshot"
#define	ZFS_DELEG_PERM_ROLLBACK		"rollback"
#define	ZFS_DELEG_PERM_CLONE		"clone"
#define	ZFS_DELEG_PERM_PROMOTE		"promote"
#define	ZFS_DELEG_PERM_RENAME		"rename"
#define	ZFS_DELEG_PERM_MOUNT		"mount"
#define	ZFS_DELEG_PERM_SHARE		"share"
#define	ZFS_DELEG_PERM_SEND		"send"
#define	ZFS_DELEG_PERM_RECEIVE		"receive"
#define	ZFS_DELEG_PERM_ALLOW		"allow"
#define	ZFS_DELEG_PERM_USERPROP		"userprop"
#define	ZFS_DELEG_PERM_VSCAN		"vscan"
#define	ZFS_DELEG_PERM_USERQUOTA	"userquota"
#define	ZFS_DELEG_PERM_GROUPQUOTA	"groupquota"
#define	ZFS_DELEG_PERM_USEROBJQUOTA	"userobjquota"
#define	ZFS_DELEG_PERM_GROUPOBJQUOTA	"groupobjquota"
#define	ZFS_DELEG_PERM_USERUSED		"userused"
#define	ZFS_DELEG_PERM_GROUPUSED	"groupused"
#define	ZFS_DELEG_PERM_USEROBJUSED	"userobjused"
#define	ZFS_DELEG_PERM_GROUPOBJUSED	"groupobjused"
#define	ZFS_DELEG_PERM_HOLD		"hold"
#define	ZFS_DELEG_PERM_RELEASE		"release"
#define	ZFS_DELEG_PERM_DIFF		"diff"
#define	ZFS_DELEG_PERM_BOOKMARK		"bookmark"
#define	ZFS_DELEG_PERM_LOAD_KEY		"load-key"
#define	ZFS_DELEG_PERM_CHANGE_KEY	"change-key"
#define	ZFS_DELEG_PERM_PROJECTUSED	"projectused"
#define	ZFS_DELEG_PERM_PROJECTQUOTA	"projectquota"
#define	ZFS_DELEG_PERM_PROJECTOBJUSED	"projectobjused"
#define	ZFS_DELEG_PERM_PROJECTOBJQUOTA	"projectobjquota"

 

int dsl_deleg_get(const char *ddname, nvlist_t **nvp);
int dsl_deleg_set(const char *ddname, nvlist_t *nvp, boolean_t unset);
int dsl_deleg_access(const char *ddname, const char *perm, cred_t *cr);
int dsl_deleg_access_impl(struct dsl_dataset *ds, const char *perm, cred_t *cr);
void dsl_deleg_set_create_perms(dsl_dir_t *dd, dmu_tx_t *tx, cred_t *cr);
int dsl_deleg_can_allow(char *ddname, nvlist_t *nvp, cred_t *cr);
int dsl_deleg_can_unallow(char *ddname, nvlist_t *nvp, cred_t *cr);
int dsl_deleg_destroy(objset_t *os, uint64_t zapobj, dmu_tx_t *tx);
boolean_t dsl_delegation_on(objset_t *os);

#ifdef	__cplusplus
}
#endif

#endif	 
