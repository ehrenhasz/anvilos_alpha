#include <sys/lua/lua.h>
#include <sys/lua/lualib.h>
#include <sys/lua/lauxlib.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_synctask.h>
#include <sys/dsl_dataset.h>
#include <sys/zcp.h>
#include <sys/zcp_set.h>
#include <sys/zcp_iter.h>
#include <sys/zcp_global.h>
#include <sys/zvol.h>
#include <zfs_prop.h>
static void
zcp_set_user_prop(lua_State *state, dsl_pool_t *dp, const char *dsname,
    const char *prop_name, const char *prop_val, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = zcp_dataset_hold(state, dp, dsname, FTAG);
	if (ds == NULL)
		return;  
	nvlist_t *nvl = fnvlist_alloc();
	fnvlist_add_string(nvl, prop_name, prop_val);
	dsl_props_set_sync_impl(ds, ZPROP_SRC_LOCAL, nvl, tx);
	fnvlist_free(nvl);
	dsl_dataset_rele(ds, FTAG);
}
int
zcp_set_prop_check(void *arg, dmu_tx_t *tx)
{
	zcp_set_prop_arg_t *args = arg;
	const char *prop_name = args->prop;
	dsl_props_set_arg_t dpsa = {
		.dpsa_dsname = args->dsname,
		.dpsa_source = ZPROP_SRC_LOCAL,
	};
	nvlist_t *nvl = NULL;
	int ret = 0;
	if (!zfs_prop_user(prop_name)) {
		return (EINVAL);
	}
	nvl = fnvlist_alloc();
	fnvlist_add_string(nvl, args->prop, args->val);
	dpsa.dpsa_props = nvl;
	ret = dsl_props_set_check(&dpsa, tx);
	nvlist_free(nvl);
	return (ret);
}
void
zcp_set_prop_sync(void *arg, dmu_tx_t *tx)
{
	zcp_set_prop_arg_t *args = arg;
	zcp_run_info_t *ri = zcp_run_info(args->state);
	dsl_pool_t *dp = ri->zri_pool;
	const char *dsname = args->dsname;
	const char *prop_name = args->prop;
	const char *prop_val = args->val;
	if (zfs_prop_user(prop_name)) {
		zcp_set_user_prop(args->state, dp, dsname, prop_name,
		    prop_val, tx);
	}
}
