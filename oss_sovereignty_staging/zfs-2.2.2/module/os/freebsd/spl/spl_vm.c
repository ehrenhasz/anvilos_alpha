 

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/counter.h>

#include <sys/byteorder.h>
#include <sys/lock.h>
#include <sys/freebsd_rwlock.h>
#include <sys/vm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

const int zfs_vm_pagerret_bad = VM_PAGER_BAD;
const int zfs_vm_pagerret_error = VM_PAGER_ERROR;
const int zfs_vm_pagerret_ok = VM_PAGER_OK;
const int zfs_vm_pagerput_sync = VM_PAGER_PUT_SYNC;
const int zfs_vm_pagerput_inval = VM_PAGER_PUT_INVAL;

void
zfs_vmobject_assert_wlocked(vm_object_t object)
{

	 
	VM_OBJECT_ASSERT_WLOCKED(object);
}

void
zfs_vmobject_wlock(vm_object_t object)
{

	VM_OBJECT_WLOCK(object);
}

void
zfs_vmobject_wunlock(vm_object_t object)
{

	VM_OBJECT_WUNLOCK(object);
}
