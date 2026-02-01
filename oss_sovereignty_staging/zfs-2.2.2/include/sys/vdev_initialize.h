 

 

#ifndef _SYS_VDEV_INITIALIZE_H
#define	_SYS_VDEV_INITIALIZE_H

#include <sys/spa.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void vdev_initialize(vdev_t *vd);
extern void vdev_uninitialize(vdev_t *vd);
extern void vdev_initialize_stop(vdev_t *vd,
    vdev_initializing_state_t tgt_state, list_t *vd_list);
extern void vdev_initialize_stop_all(vdev_t *vd,
    vdev_initializing_state_t tgt_state);
extern void vdev_initialize_stop_wait(spa_t *spa, list_t *vd_list);
extern void vdev_initialize_restart(vdev_t *vd);

#ifdef	__cplusplus
}
#endif

#endif	 
