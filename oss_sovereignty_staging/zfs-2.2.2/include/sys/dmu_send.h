 

 

#ifndef _DMU_SEND_H
#define	_DMU_SEND_H

#include <sys/inttypes.h>
#include <sys/dsl_crypt.h>
#include <sys/dsl_bookmark.h>
#include <sys/spa.h>
#include <sys/objlist.h>
#include <sys/dmu_redact.h>

#define	BEGINNV_REDACT_SNAPS		"redact_snaps"
#define	BEGINNV_REDACT_FROM_SNAPS	"redact_from_snaps"
#define	BEGINNV_RESUME_OBJECT		"resume_object"
#define	BEGINNV_RESUME_OFFSET		"resume_offset"

struct vnode;
struct dsl_dataset;
struct drr_begin;
struct avl_tree;
struct dmu_replay_record;
struct dmu_send_outparams;
int
dmu_send(const char *tosnap, const char *fromsnap, boolean_t embedok,
    boolean_t large_block_ok, boolean_t compressok, boolean_t rawok,
    boolean_t savedok, uint64_t resumeobj, uint64_t resumeoff,
    const char *redactbook, int outfd, offset_t *off,
    struct dmu_send_outparams *dsop);
int dmu_send_estimate_fast(struct dsl_dataset *ds, struct dsl_dataset *fromds,
    zfs_bookmark_phys_t *frombook, boolean_t stream_compressed,
    boolean_t saved, uint64_t *sizep);
int dmu_send_obj(const char *pool, uint64_t tosnap, uint64_t fromsnap,
    boolean_t embedok, boolean_t large_block_ok, boolean_t compressok,
    boolean_t rawok, boolean_t savedok, int outfd, offset_t *off,
    struct dmu_send_outparams *dso);

typedef int (*dmu_send_outfunc_t)(objset_t *os, void *buf, int len, void *arg);
typedef struct dmu_send_outparams {
	dmu_send_outfunc_t	dso_outfunc;
	void			*dso_arg;
	boolean_t		dso_dryrun;
} dmu_send_outparams_t;

#endif  
