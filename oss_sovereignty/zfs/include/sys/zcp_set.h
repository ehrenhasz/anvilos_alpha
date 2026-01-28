#ifndef _SYS_ZCP_SET_H
#define	_SYS_ZCP_SET_H
#include <sys/dmu_tx.h>
#include <sys/dsl_pool.h>
#ifdef  __cplusplus
extern "C" {
#endif
typedef struct zcp_set_prop_arg {
	lua_State	*state;
	const char	*dsname;
	const char	*prop;
	const char	*val;
} zcp_set_prop_arg_t;
int zcp_set_prop_check(void *arg, dmu_tx_t *tx);
void zcp_set_prop_sync(void *arg, dmu_tx_t *tx);
#ifdef  __cplusplus
}
#endif
#endif  
