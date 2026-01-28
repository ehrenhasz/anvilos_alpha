



#ifndef _SYS_ZCP_H
#define	_SYS_ZCP_H

#include <sys/dmu_tx.h>
#include <sys/dsl_pool.h>

#include <sys/lua/lua.h>
#include <sys/lua/lualib.h>
#include <sys/lua/lauxlib.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	ZCP_RUN_INFO_KEY "runinfo"

extern uint64_t zfs_lua_max_instrlimit;
extern uint64_t zfs_lua_max_memlimit;

int zcp_argerror(lua_State *, int, const char *, ...);

int zcp_eval(const char *, const char *, boolean_t, uint64_t, uint64_t,
    nvpair_t *, nvlist_t *);

int zcp_load_list_lib(lua_State *);

int zcp_load_synctask_lib(lua_State *, boolean_t);

typedef void (zcp_cleanup_t)(void *);
typedef struct zcp_cleanup_handler {
	zcp_cleanup_t *zch_cleanup_func;
	void *zch_cleanup_arg;
	list_node_t zch_node;
} zcp_cleanup_handler_t;

typedef struct zcp_alloc_arg {
	boolean_t	aa_must_succeed;
	int64_t		aa_alloc_remaining;
	int64_t		aa_alloc_limit;
} zcp_alloc_arg_t;

typedef struct zcp_run_info {
	dsl_pool_t	*zri_pool;

	
	int		zri_space_used;

	
	cred_t		*zri_cred;
	proc_t		*zri_proc;

	
	dmu_tx_t	*zri_tx;

	
	uint64_t	zri_maxinstrs;

	
	uint64_t	zri_curinstrs;

	
	boolean_t	zri_timed_out;

	
	boolean_t	zri_canceled;

	
	boolean_t	zri_sync;

	
	list_t		zri_cleanup_handlers;

	
	lua_State	*zri_state;

	
	zcp_alloc_arg_t	*zri_allocargs;

	
	nvlist_t	*zri_outnvl;

	
	nvlist_t	*zri_new_zvols;

	
	int		zri_result;
} zcp_run_info_t;

zcp_run_info_t *zcp_run_info(lua_State *);
zcp_cleanup_handler_t *zcp_register_cleanup(lua_State *, zcp_cleanup_t, void *);
void zcp_deregister_cleanup(lua_State *, zcp_cleanup_handler_t *);
void zcp_cleanup(lua_State *);


typedef struct zcp_arg {
	
	const char *za_name;

	
	const int za_lua_type;
} zcp_arg_t;

void zcp_parse_args(lua_State *, const char *, const zcp_arg_t *,
    const zcp_arg_t *);
int zcp_nvlist_to_lua(lua_State *, nvlist_t *, char *, int);
int zcp_dataset_hold_error(lua_State *, dsl_pool_t *, const char *, int);
struct dsl_dataset *zcp_dataset_hold(lua_State *, dsl_pool_t *,
    const char *, const void *);

typedef int (zcp_lib_func_t)(lua_State *);
typedef struct zcp_lib_info {
	const char *name;
	zcp_lib_func_t *func;
	const zcp_arg_t pargs[4];
	const zcp_arg_t kwargs[2];
} zcp_lib_info_t;

#ifdef	__cplusplus
}
#endif

#endif	
