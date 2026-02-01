 
 

#ifndef _SYS_ZFS_DEBUG_H
#define	_SYS_ZFS_DEBUG_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef TRUE
#define	TRUE 1
#endif

#ifndef FALSE
#define	FALSE 0
#endif

extern int zfs_flags;
extern int zfs_recover;
extern int zfs_free_leak_on_eio;
extern int zfs_dbgmsg_enable;

#define	ZFS_DEBUG_DPRINTF		(1 << 0)
#define	ZFS_DEBUG_DBUF_VERIFY		(1 << 1)
#define	ZFS_DEBUG_DNODE_VERIFY		(1 << 2)
#define	ZFS_DEBUG_SNAPNAMES		(1 << 3)
#define	ZFS_DEBUG_MODIFY		(1 << 4)
 
#define	ZFS_DEBUG_ZIO_FREE		(1 << 6)
#define	ZFS_DEBUG_HISTOGRAM_VERIFY	(1 << 7)
#define	ZFS_DEBUG_METASLAB_VERIFY	(1 << 8)
#define	ZFS_DEBUG_SET_ERROR		(1 << 9)
#define	ZFS_DEBUG_INDIRECT_REMAP	(1 << 10)
#define	ZFS_DEBUG_TRIM			(1 << 11)
#define	ZFS_DEBUG_LOG_SPACEMAP		(1 << 12)
#define	ZFS_DEBUG_METASLAB_ALLOC	(1 << 13)
#define	ZFS_DEBUG_BRT			(1 << 14)

extern void __set_error(const char *file, const char *func, int line, int err);
extern void __zfs_dbgmsg(char *buf);
extern void __dprintf(boolean_t dprint, const char *file, const char *func,
    int line, const char *fmt, ...)  __attribute__((format(printf, 5, 6)));

 
#define	zfs_dbgmsg(...) \
	if (zfs_dbgmsg_enable) \
		__dprintf(B_FALSE, __FILE__, __func__, __LINE__, __VA_ARGS__)

#ifdef ZFS_DEBUG
 
#define	dprintf(...) \
	if (zfs_flags & ZFS_DEBUG_DPRINTF) \
		__dprintf(B_TRUE, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define	dprintf(...) ((void)0)
#endif  

extern void zfs_panic_recover(const char *fmt, ...);

extern void zfs_dbgmsg_init(void);
extern void zfs_dbgmsg_fini(void);

#ifndef _KERNEL
extern int dprintf_find_string(const char *string);
extern void zfs_dbgmsg_print(const char *tag);
#endif

#ifdef	__cplusplus
}
#endif

#endif	 
