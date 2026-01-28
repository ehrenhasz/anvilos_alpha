#ifndef _SYS_VDEV_RAIDZ_H
#define	_SYS_VDEV_RAIDZ_H
#include <sys/types.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct zio;
struct raidz_col;
struct raidz_row;
struct raidz_map;
#if !defined(_KERNEL)
struct kernel_param {};
#endif
struct raidz_map *vdev_raidz_map_alloc(struct zio *, uint64_t, uint64_t,
    uint64_t);
void vdev_raidz_map_free(struct raidz_map *);
void vdev_raidz_generate_parity_row(struct raidz_map *, struct raidz_row *);
void vdev_raidz_generate_parity(struct raidz_map *);
void vdev_raidz_reconstruct(struct raidz_map *, const int *, int);
void vdev_raidz_child_done(zio_t *);
void vdev_raidz_io_done(zio_t *);
void vdev_raidz_checksum_error(zio_t *, struct raidz_col *, abd_t *);
extern const zio_vsd_ops_t vdev_raidz_vsd_ops;
void vdev_raidz_math_init(void);
void vdev_raidz_math_fini(void);
const struct raidz_impl_ops *vdev_raidz_math_get_ops(void);
int vdev_raidz_math_generate(struct raidz_map *, struct raidz_row *);
int vdev_raidz_math_reconstruct(struct raidz_map *, struct raidz_row *,
    const int *, const int *, const int);
int vdev_raidz_impl_set(const char *);
typedef struct vdev_raidz {
	int vd_logical_width;
	int vd_nparity;
} vdev_raidz_t;
#ifdef	__cplusplus
}
#endif
#endif  
