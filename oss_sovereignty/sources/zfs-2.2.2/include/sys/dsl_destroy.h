


#ifndef	_SYS_DSL_DESTROY_H
#define	_SYS_DSL_DESTROY_H

#ifdef	__cplusplus
extern "C" {
#endif

struct nvlist;
struct dsl_dataset;
struct dsl_pool;
struct dmu_tx;

int dsl_destroy_snapshots_nvl(struct nvlist *, boolean_t,
    struct nvlist *);
int dsl_destroy_snapshot(const char *, boolean_t);
int dsl_destroy_head(const char *);
int dsl_destroy_head_check_impl(struct dsl_dataset *, int);
void dsl_destroy_head_sync_impl(struct dsl_dataset *, struct dmu_tx *);
int dsl_destroy_inconsistent(const char *, void *);
int dsl_destroy_snapshot_check_impl(struct dsl_dataset *, boolean_t);
void dsl_destroy_snapshot_sync_impl(struct dsl_dataset *,
    boolean_t, struct dmu_tx *);
void dsl_dir_remove_clones_key(dsl_dir_t *, uint64_t, dmu_tx_t *);

typedef struct dsl_destroy_snapshot_arg {
	const char *ddsa_name;
	boolean_t ddsa_defer;
} dsl_destroy_snapshot_arg_t;

int dsl_destroy_snapshot_check(void *, dmu_tx_t *);
void dsl_destroy_snapshot_sync(void *, dmu_tx_t *);

typedef struct dsl_destroy_head_arg {
	const char *ddha_name;
} dsl_destroy_head_arg_t;

int dsl_destroy_head_check(void *, dmu_tx_t *);
void dsl_destroy_head_sync(void *, dmu_tx_t *);

#ifdef	__cplusplus
}
#endif

#endif 
