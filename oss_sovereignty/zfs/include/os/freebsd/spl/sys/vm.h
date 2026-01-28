#ifndef _OPENSOLARIS_SYS_VM_H_
#define	_OPENSOLARIS_SYS_VM_H_
#include <sys/sf_buf.h>
extern const int zfs_vm_pagerret_bad;
extern const int zfs_vm_pagerret_error;
extern const int zfs_vm_pagerret_ok;
extern const int zfs_vm_pagerput_sync;
extern const int zfs_vm_pagerput_inval;
void	zfs_vmobject_assert_wlocked(vm_object_t object);
void	zfs_vmobject_wlock(vm_object_t object);
void	zfs_vmobject_wunlock(vm_object_t object);
#if __FreeBSD_version >= 1300081
#define	zfs_vmobject_assert_wlocked_12(x)
#define	zfs_vmobject_wlock_12(x)
#define	zfs_vmobject_wunlock_12(x)
#else
#define	zfs_vmobject_assert_wlocked_12(x)		\
	zfs_vmobject_assert_wlocked((x))
#define	zfs_vmobject_wlock_12(x)				\
	zfs_vmobject_wlock(x)
#define	zfs_vmobject_wunlock_12(x)				\
	zfs_vmobject_wunlock(x)
#define	vm_page_grab_unlocked(obj, idx, flags)	\
	vm_page_grab((obj), (idx), (flags))
#define	vm_page_grab_valid_unlocked(m, obj, idx, flags)	\
	vm_page_grab_valid((m), (obj), (idx), (flags))
#endif
static inline caddr_t
zfs_map_page(vm_page_t pp, struct sf_buf **sfp)
{
	*sfp = sf_buf_alloc(pp, 0);
	return ((caddr_t)sf_buf_kva(*sfp));
}
static inline void
zfs_unmap_page(struct sf_buf *sf)
{
	sf_buf_free(sf);
}
#endif	 
