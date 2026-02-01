 

 

#ifndef _ZFS_KMAP_H
#define	_ZFS_KMAP_H

#include <linux/highmem.h>
#include <linux/uaccess.h>

 
#define	zfs_kmap_atomic(page)	kmap_atomic(page)
#define	zfs_kunmap_atomic(addr)	kunmap_atomic(addr)

 
#ifdef HAVE_ACCESS_OK_TYPE
#define	zfs_access_ok(type, addr, size)	access_ok(type, addr, size)
#else
#define	zfs_access_ok(type, addr, size)	access_ok(addr, size)
#endif

#endif	 
