#ifndef	_SYS_DSL_PROP_H
#define	_SYS_DSL_PROP_H
#include <sys/dmu.h>
#include <sys/dsl_pool.h>
#include <sys/zfs_context.h>
#include <sys/dsl_synctask.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct dsl_dataset;
struct dsl_dir;
typedef void (dsl_prop_changed_cb_t)(void *arg, uint64_t newval);
typedef struct dsl_prop_record {
	list_node_t pr_node;  
	const char *pr_propname;
	list_t pr_cbs;
} dsl_prop_record_t;
typedef struct dsl_prop_cb_record {
	list_node_t cbr_pr_node;  
	list_node_t cbr_ds_node;  
	dsl_prop_record_t *cbr_pr;
	struct dsl_dataset *cbr_ds;
	dsl_prop_changed_cb_t *cbr_func;
	void *cbr_arg;
} dsl_prop_cb_record_t;
typedef struct dsl_props_arg {
	nvlist_t *pa_props;
	zprop_source_t pa_source;
} dsl_props_arg_t;
typedef struct dsl_props_set_arg {
	const char *dpsa_dsname;
	zprop_source_t dpsa_source;
	nvlist_t *dpsa_props;
} dsl_props_set_arg_t;
void dsl_prop_init(dsl_dir_t *dd);
void dsl_prop_fini(dsl_dir_t *dd);
int dsl_prop_register(struct dsl_dataset *ds, const char *propname,
    dsl_prop_changed_cb_t *callback, void *cbarg);
int dsl_prop_unregister(struct dsl_dataset *ds, const char *propname,
    dsl_prop_changed_cb_t *callback, void *cbarg);
void dsl_prop_unregister_all(struct dsl_dataset *ds, void *cbarg);
void dsl_prop_notify_all(struct dsl_dir *dd);
boolean_t dsl_prop_hascb(struct dsl_dataset *ds);
int dsl_prop_get(const char *ddname, const char *propname,
    int intsz, int numints, void *buf, char *setpoint);
int dsl_prop_get_integer(const char *ddname, const char *propname,
    uint64_t *valuep, char *setpoint);
int dsl_prop_get_all(objset_t *os, nvlist_t **nvp);
int dsl_prop_get_received(const char *dsname, nvlist_t **nvp);
int dsl_prop_get_ds(struct dsl_dataset *ds, const char *propname,
    int intsz, int numints, void *buf, char *setpoint);
int dsl_prop_get_int_ds(struct dsl_dataset *ds, const char *propname,
    uint64_t *valuep);
int dsl_prop_get_dd(struct dsl_dir *dd, const char *propname,
    int intsz, int numints, void *buf, char *setpoint,
    boolean_t snapshot);
int dsl_props_set_check(void *arg, dmu_tx_t *tx);
void dsl_props_set_sync(void *arg, dmu_tx_t *tx);
void dsl_props_set_sync_impl(struct dsl_dataset *ds, zprop_source_t source,
    nvlist_t *props, dmu_tx_t *tx);
void dsl_prop_set_sync_impl(struct dsl_dataset *ds, const char *propname,
    zprop_source_t source, int intsz, int numints, const void *value,
    dmu_tx_t *tx);
int dsl_props_set(const char *dsname, zprop_source_t source, nvlist_t *nvl);
int dsl_prop_set_int(const char *dsname, const char *propname,
    zprop_source_t source, uint64_t value);
int dsl_prop_set_string(const char *dsname, const char *propname,
    zprop_source_t source, const char *value);
int dsl_prop_inherit(const char *dsname, const char *propname,
    zprop_source_t source);
int dsl_prop_predict(dsl_dir_t *dd, const char *propname,
    zprop_source_t source, uint64_t value, uint64_t *newvalp);
boolean_t dsl_prop_get_hasrecvd(const char *dsname);
int dsl_prop_set_hasrecvd(const char *dsname);
void dsl_prop_unset_hasrecvd(const char *dsname);
void dsl_prop_nvlist_add_uint64(nvlist_t *nv, zfs_prop_t prop, uint64_t value);
void dsl_prop_nvlist_add_string(nvlist_t *nv,
    zfs_prop_t prop, const char *value);
#ifdef	__cplusplus
}
#endif
#endif	 
