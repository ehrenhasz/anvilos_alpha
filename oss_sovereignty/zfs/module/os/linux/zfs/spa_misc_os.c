#include <sys/zfs_context.h>
#include <sys/spa_impl.h>
#include <sys/spa.h>
#include <sys/txg.h>
#include <sys/unique.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_prop.h>
#include <sys/fm/util.h>
#include <sys/dsl_scan.h>
#include <sys/fs/zfs.h>
#include <sys/kstat.h>
#include "zfs_prop.h"
int
param_set_deadman_failmode(const char *val, zfs_kernel_param_t *kp)
{
	int error;
	error = -param_set_deadman_failmode_common(val);
	if (error == 0)
		error = param_set_charp(val, kp);
	return (error);
}
int
param_set_deadman_ziotime(const char *val, zfs_kernel_param_t *kp)
{
	int error;
	error = spl_param_set_u64(val, kp);
	if (error < 0)
		return (SET_ERROR(error));
	spa_set_deadman_ziotime(MSEC2NSEC(zfs_deadman_ziotime_ms));
	return (0);
}
int
param_set_deadman_synctime(const char *val, zfs_kernel_param_t *kp)
{
	int error;
	error = spl_param_set_u64(val, kp);
	if (error < 0)
		return (SET_ERROR(error));
	spa_set_deadman_synctime(MSEC2NSEC(zfs_deadman_synctime_ms));
	return (0);
}
int
param_set_slop_shift(const char *buf, zfs_kernel_param_t *kp)
{
	unsigned long val;
	int error;
	error = kstrtoul(buf, 0, &val);
	if (error)
		return (SET_ERROR(error));
	if (val < 1 || val > 31)
		return (SET_ERROR(-EINVAL));
	error = param_set_int(buf, kp);
	if (error < 0)
		return (SET_ERROR(error));
	return (0);
}
const char *
spa_history_zone(void)
{
	return ("linux");
}
void
spa_import_os(spa_t *spa)
{
	(void) spa;
}
void
spa_export_os(spa_t *spa)
{
	(void) spa;
}
void
spa_activate_os(spa_t *spa)
{
	(void) spa;
}
void
spa_deactivate_os(spa_t *spa)
{
	(void) spa;
}
