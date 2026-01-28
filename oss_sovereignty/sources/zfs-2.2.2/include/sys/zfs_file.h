

#ifndef	_SYS_ZFS_FILE_H
#define	_SYS_ZFS_FILE_H

#include <sys/zfs_context.h>

#ifndef _KERNEL
typedef struct zfs_file {
	int f_fd;
	int f_dump_fd;
} zfs_file_t;
#elif defined(__linux__) || defined(__FreeBSD__)
typedef struct file zfs_file_t;
#else
#error "unknown OS"
#endif

typedef struct zfs_file_attr {
	uint64_t	zfa_size;	
	mode_t		zfa_mode;	
} zfs_file_attr_t;

int zfs_file_open(const char *path, int flags, int mode, zfs_file_t **fp);
void zfs_file_close(zfs_file_t *fp);

int zfs_file_write(zfs_file_t *fp, const void *buf, size_t len, ssize_t *resid);
int zfs_file_pwrite(zfs_file_t *fp, const void *buf, size_t len, loff_t off,
    ssize_t *resid);
int zfs_file_read(zfs_file_t *fp, void *buf, size_t len, ssize_t *resid);
int zfs_file_pread(zfs_file_t *fp, void *buf, size_t len, loff_t off,
    ssize_t *resid);

int zfs_file_seek(zfs_file_t *fp, loff_t *offp, int whence);
int zfs_file_getattr(zfs_file_t *fp, zfs_file_attr_t *zfattr);
int zfs_file_fsync(zfs_file_t *fp, int flags);
int zfs_file_fallocate(zfs_file_t *fp, int mode, loff_t offset, loff_t len);
loff_t zfs_file_off(zfs_file_t *fp);
int zfs_file_unlink(const char *);

zfs_file_t *zfs_file_get(int fd);
void zfs_file_put(zfs_file_t *fp);
void *zfs_file_private(zfs_file_t *fp);

#endif 
