


#ifndef _SYS_VDEV_FILE_H
#define	_SYS_VDEV_FILE_H

#include <sys/vdev.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct vdev_file {
	zfs_file_t	*vf_file;
} vdev_file_t;

extern void vdev_file_init(void);
extern void vdev_file_fini(void);

#ifdef	__cplusplus
}
#endif

#endif	
