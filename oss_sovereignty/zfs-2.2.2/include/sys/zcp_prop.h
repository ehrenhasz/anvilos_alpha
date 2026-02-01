 

 

#ifndef _SYS_ZCP_PROP_H
#define	_SYS_ZCP_PROP_H

#ifdef	__cplusplus
extern "C" {
#endif

int zcp_load_get_lib(lua_State *state);
boolean_t prop_valid_for_ds(dsl_dataset_t *ds, zfs_prop_t zfs_prop);

#ifdef	__cplusplus
}
#endif

#endif  
