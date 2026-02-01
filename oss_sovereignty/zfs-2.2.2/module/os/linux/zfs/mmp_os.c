 
 

#include <sys/zfs_context.h>
#include <sys/mmp.h>

int
param_set_multihost_interval(const char *val, zfs_kernel_param_t *kp)
{
	int ret;

	ret = spl_param_set_u64(val, kp);
	if (ret < 0)
		return (ret);

	if (spa_mode_global != SPA_MODE_UNINIT)
		mmp_signal_all_threads();

	return (ret);
}
