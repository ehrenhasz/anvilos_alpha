 

 

#ifndef	_SYS_FM_UTIL_H
#define	_SYS_FM_UTIL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/nvpair.h>
#include <sys/zfs_file.h>

 
#define	FM_MAX_CLASS 100
#define	FM_ERROR_CHAN	"com.sun:fm:error"
#define	FM_PUB		"fm"

 
#define	ERPT_MAGIC	0xf00d4eddU
#define	ERPT_MAX_ERRS	16
#define	ERPT_DATA_SZ	(6 * 1024)
#define	ERPT_EVCH_MAX	256
#define	ERPT_HIWAT	64

typedef struct erpt_dump {
	uint32_t ed_magic;	 
	uint32_t ed_chksum;	 
	uint32_t ed_size;	 
	uint32_t ed_pad;	 
	hrtime_t ed_hrt_nsec;	 
	hrtime_t ed_hrt_base;	 
	struct {
		uint64_t sec;	 
		uint64_t nsec;	 
	} ed_tod_base;
} erpt_dump_t;

#ifdef _KERNEL

#define	ZEVENT_SHUTDOWN		0x1

typedef void zevent_cb_t(nvlist_t *, nvlist_t *);

typedef struct zevent_s {
	nvlist_t	*ev_nvl;	 
	nvlist_t	*ev_detector;	 
	list_t		ev_ze_list;	 
	list_node_t	ev_node;	 
	zevent_cb_t	*ev_cb;		 
	uint64_t	ev_eid;
} zevent_t;

typedef struct zfs_zevent {
	zevent_t	*ze_zevent;	 
	list_node_t	ze_node;	 
	uint64_t	ze_dropped;	 
} zfs_zevent_t;

extern void fm_init(void);
extern void fm_fini(void);
extern void zfs_zevent_post_cb(nvlist_t *nvl, nvlist_t *detector);
extern int zfs_zevent_post(nvlist_t *, nvlist_t *, zevent_cb_t *);
extern void zfs_zevent_drain_all(uint_t *);
extern zfs_file_t *zfs_zevent_fd_hold(int, minor_t *, zfs_zevent_t **);
extern void zfs_zevent_fd_rele(zfs_file_t *);
extern int zfs_zevent_next(zfs_zevent_t *, nvlist_t **, uint64_t *, uint64_t *);
extern int zfs_zevent_wait(zfs_zevent_t *);
extern int zfs_zevent_seek(zfs_zevent_t *, uint64_t);
extern void zfs_zevent_init(zfs_zevent_t **);
extern void zfs_zevent_destroy(zfs_zevent_t *);

extern void zfs_zevent_track_duplicate(void);
extern void zfs_ereport_init(void);
extern void zfs_ereport_fini(void);
#else

static inline void fm_init(void) { }
static inline void fm_fini(void) { }

#endif   

#ifdef	__cplusplus
}
#endif

#endif  
