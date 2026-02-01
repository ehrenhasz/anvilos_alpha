 

 

#ifndef _SYS_VDEV_OS_H
#define	_SYS_VDEV_OS_H

extern int vdev_label_write_pad2(vdev_t *vd, const char *buf, size_t size);
extern int vdev_geom_read_pool_label(const char *name, nvlist_t ***configs,
    uint64_t *count);

#endif
