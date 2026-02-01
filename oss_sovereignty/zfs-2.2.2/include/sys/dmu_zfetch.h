 
 

 

#ifndef	_DMU_ZFETCH_H
#define	_DMU_ZFETCH_H

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct dnode;				 

typedef struct zfetch {
	kmutex_t	zf_lock;	 
	list_t		zf_stream;	 
	struct dnode	*zf_dnode;	 
	int		zf_numstreams;	 
} zfetch_t;

typedef struct zstream {
	uint64_t	zs_blkid;	 
	unsigned int	zs_pf_dist;	 
	unsigned int	zs_ipf_dist;	 
	uint64_t	zs_pf_start;	 
	uint64_t	zs_pf_end;	 
	uint64_t	zs_ipf_start;	 
	uint64_t	zs_ipf_end;	 

	list_node_t	zs_node;	 
	hrtime_t	zs_atime;	 
	zfetch_t	*zs_fetch;	 
	boolean_t	zs_missed;	 
	boolean_t	zs_more;	 
	zfs_refcount_t	zs_callers;	 
	 
	zfs_refcount_t	zs_refs;
} zstream_t;

void		zfetch_init(void);
void		zfetch_fini(void);

void		dmu_zfetch_init(zfetch_t *, struct dnode *);
void		dmu_zfetch_fini(zfetch_t *);
zstream_t	*dmu_zfetch_prepare(zfetch_t *, uint64_t, uint64_t, boolean_t,
    boolean_t);
void		dmu_zfetch_run(zstream_t *, boolean_t, boolean_t);
void		dmu_zfetch(zfetch_t *, uint64_t, uint64_t, boolean_t, boolean_t,
    boolean_t);


#ifdef	__cplusplus
}
#endif

#endif	 
