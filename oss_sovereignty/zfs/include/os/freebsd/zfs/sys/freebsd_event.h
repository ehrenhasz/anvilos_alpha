#ifndef	_ZFS_FREEBSD_EVENT_H
#define	_ZFS_FREEBSD_EVENT_H
#ifdef _KERNEL
void   knlist_init_sx(struct knlist *knl, struct sx *lock);
#endif  
#endif  
