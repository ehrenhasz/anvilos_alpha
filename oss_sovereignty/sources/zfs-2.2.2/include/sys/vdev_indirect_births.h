



#ifndef	_SYS_VDEV_INDIRECT_BIRTHS_H
#define	_SYS_VDEV_INDIRECT_BIRTHS_H

#include <sys/dmu.h>
#include <sys/spa.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct vdev_indirect_birth_entry_phys {
	uint64_t vibe_offset;
	uint64_t vibe_phys_birth_txg;
} vdev_indirect_birth_entry_phys_t;

typedef struct vdev_indirect_birth_phys {
	uint64_t	vib_count; 
} vdev_indirect_birth_phys_t;

typedef struct vdev_indirect_births {
	uint64_t	vib_object;

	
	vdev_indirect_birth_entry_phys_t *vib_entries;

	objset_t	*vib_objset;

	dmu_buf_t	*vib_dbuf;
	vdev_indirect_birth_phys_t	*vib_phys;
} vdev_indirect_births_t;

extern vdev_indirect_births_t *vdev_indirect_births_open(objset_t *os,
    uint64_t object);
extern void vdev_indirect_births_close(vdev_indirect_births_t *vib);
extern boolean_t vdev_indirect_births_is_open(vdev_indirect_births_t *vib);
extern uint64_t vdev_indirect_births_alloc(objset_t *os, dmu_tx_t *tx);
extern void vdev_indirect_births_free(objset_t *os, uint64_t object,
    dmu_tx_t *tx);

extern uint64_t vdev_indirect_births_count(vdev_indirect_births_t *vib);
extern uint64_t vdev_indirect_births_object(vdev_indirect_births_t *vib);

extern void vdev_indirect_births_add_entry(vdev_indirect_births_t *vib,
    uint64_t offset, uint64_t txg, dmu_tx_t *tx);

extern uint64_t vdev_indirect_births_physbirth(vdev_indirect_births_t *vib,
    uint64_t offset, uint64_t asize);

extern uint64_t vdev_indirect_births_last_entry_txg(
    vdev_indirect_births_t *vib);

#ifdef	__cplusplus
}
#endif

#endif	
