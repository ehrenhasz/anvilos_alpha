#ifndef _SYS_DMU_IMPL_H
#define	_SYS_DMU_IMPL_H
#include <sys/txg_impl.h>
#include <sys/zio.h>
#include <sys/dnode.h>
#include <sys/zfs_context.h>
#include <sys/zfs_ioctl.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct objset;
struct dmu_pool;
typedef struct dmu_sendstatus {
	list_node_t dss_link;
	int dss_outfd;
	proc_t *dss_proc;
	offset_t *dss_off;
	uint64_t dss_blocks;  
} dmu_sendstatus_t;
void dmu_object_zapify(objset_t *, uint64_t, dmu_object_type_t, dmu_tx_t *);
void dmu_object_free_zapified(objset_t *, uint64_t, dmu_tx_t *);
#ifdef	__cplusplus
}
#endif
#endif	 
