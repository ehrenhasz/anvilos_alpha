 
 

 

#include <ctype.h>
#include <fcntl.h>
#include <libnvpair.h>
#include <libzfs.h>
#include <libzutil.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/list.h>
#include <sys/sunddi.h>
#include <sys/sysevent/eventdefs.h>
#include <sys/sysevent/dev.h>
#include <thread_pool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "zfs_agents.h"
#include "../zed_log.h"

#define	DEV_BYID_PATH	"/dev/disk/by-id/"
#define	DEV_BYPATH_PATH	"/dev/disk/by-path/"
#define	DEV_BYVDEV_PATH	"/dev/disk/by-vdev/"

typedef void (*zfs_process_func_t)(zpool_handle_t *, nvlist_t *, boolean_t);

libzfs_handle_t *g_zfshdl;
list_t g_pool_list;	 
list_t g_device_list;	 
tpool_t *g_tpool;
boolean_t g_enumeration_done;
pthread_t g_zfs_tid;	 

typedef struct unavailpool {
	zpool_handle_t	*uap_zhp;
	list_node_t	uap_node;
} unavailpool_t;

typedef struct pendingdev {
	char		pd_physpath[128];
	list_node_t	pd_node;
} pendingdev_t;

static int
zfs_toplevel_state(zpool_handle_t *zhp)
{
	nvlist_t *nvroot;
	vdev_stat_t *vs;
	unsigned int c;

	verify(nvlist_lookup_nvlist(zpool_get_config(zhp, NULL),
	    ZPOOL_CONFIG_VDEV_TREE, &nvroot) == 0);
	verify(nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) == 0);
	return (vs->vs_state);
}

static int
zfs_unavail_pool(zpool_handle_t *zhp, void *data)
{
	zed_log_msg(LOG_INFO, "zfs_unavail_pool: examining '%s' (state %d)",
	    zpool_get_name(zhp), (int)zfs_toplevel_state(zhp));

	if (zfs_toplevel_state(zhp) < VDEV_STATE_DEGRADED) {
		unavailpool_t *uap;
		uap = malloc(sizeof (unavailpool_t));
		if (uap == NULL) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}

		uap->uap_zhp = zhp;
		list_insert_tail((list_t *)data, uap);
	} else {
		zpool_close(zhp);
	}
	return (0);
}

 
static void lines_to_zed_log_msg(char **lines, int lines_cnt)
{
	int i;
	for (i = 0; i < lines_cnt; i++) {
		zed_log_msg(LOG_INFO, "%s", lines[i]);
	}
}

 

 
static void
zfs_process_add(zpool_handle_t *zhp, nvlist_t *vdev, boolean_t labeled)
{
	const char *path;
	vdev_state_t newstate;
	nvlist_t *nvroot, *newvd;
	pendingdev_t *device;
	uint64_t wholedisk = 0ULL;
	uint64_t offline = 0ULL, faulted = 0ULL;
	uint64_t guid = 0ULL;
	uint64_t is_spare = 0;
	const char *physpath = NULL, *new_devid = NULL, *enc_sysfs_path = NULL;
	char rawpath[PATH_MAX], fullpath[PATH_MAX];
	char pathbuf[PATH_MAX];
	int ret;
	int online_flag = ZFS_ONLINE_CHECKREMOVE | ZFS_ONLINE_UNSPARE;
	boolean_t is_sd = B_FALSE;
	boolean_t is_mpath_wholedisk = B_FALSE;
	uint_t c;
	vdev_stat_t *vs;
	char **lines = NULL;
	int lines_cnt = 0;

	 
	if (nvlist_lookup_string(vdev, ZPOOL_CONFIG_PATH, &path) != 0)
		return;

	 
	verify(nvlist_lookup_uint64_array(vdev, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) == 0);
	if (vs->vs_state == VDEV_STATE_HEALTHY) {
		zed_log_msg(LOG_INFO, "%s: %s is already healthy, skip it.",
		    __func__, path);
		return;
	}

	(void) nvlist_lookup_string(vdev, ZPOOL_CONFIG_PHYS_PATH, &physpath);
	(void) nvlist_lookup_string(vdev, ZPOOL_CONFIG_VDEV_ENC_SYSFS_PATH,
	    &enc_sysfs_path);
	(void) nvlist_lookup_uint64(vdev, ZPOOL_CONFIG_WHOLE_DISK, &wholedisk);
	(void) nvlist_lookup_uint64(vdev, ZPOOL_CONFIG_OFFLINE, &offline);
	(void) nvlist_lookup_uint64(vdev, ZPOOL_CONFIG_FAULTED, &faulted);

	(void) nvlist_lookup_uint64(vdev, ZPOOL_CONFIG_GUID, &guid);
	(void) nvlist_lookup_uint64(vdev, ZPOOL_CONFIG_IS_SPARE, &is_spare);

	 
	if (physpath == NULL && path != NULL) {
		 
		if (strncmp(path, DEV_BYVDEV_PATH,
		    strlen(DEV_BYVDEV_PATH)) == 0) {
			 
			physpath = &path[strlen(DEV_BYVDEV_PATH)];
		}
	}

	 
	if (offline && !faulted) {
		zed_log_msg(LOG_INFO, "%s: %s is offline, skip autoreplace",
		    __func__, path);
		return;
	}

	is_mpath_wholedisk = is_mpath_whole_disk(path);
	zed_log_msg(LOG_INFO, "zfs_process_add: pool '%s' vdev '%s', phys '%s'"
	    " %s blank disk, %s mpath blank disk, %s labeled, enc sysfs '%s', "
	    "(guid %llu)",
	    zpool_get_name(zhp), path,
	    physpath ? physpath : "NULL",
	    wholedisk ? "is" : "not",
	    is_mpath_wholedisk? "is" : "not",
	    labeled ? "is" : "not",
	    enc_sysfs_path,
	    (long long unsigned int)guid);

	 
	if (guid != 0) {
		(void) snprintf(fullpath, sizeof (fullpath), "%llu",
		    (long long unsigned int)guid);
	} else {
		 
		(void) strlcpy(fullpath, path, sizeof (fullpath));
		if (wholedisk) {
			char *spath = zfs_strip_partition(fullpath);
			if (!spath) {
				zed_log_msg(LOG_INFO, "%s: Can't alloc",
				    __func__);
				return;
			}

			(void) strlcpy(fullpath, spath, sizeof (fullpath));
			free(spath);
		}
	}

	if (is_spare)
		online_flag |= ZFS_ONLINE_SPARE;

	 
	if (zpool_vdev_online(zhp, fullpath, online_flag, &newstate) == 0 &&
	    (newstate == VDEV_STATE_HEALTHY ||
	    newstate == VDEV_STATE_DEGRADED)) {
		zed_log_msg(LOG_INFO,
		    "  zpool_vdev_online: vdev '%s' ('%s') is "
		    "%s", fullpath, physpath, (newstate == VDEV_STATE_HEALTHY) ?
		    "HEALTHY" : "DEGRADED");
		return;
	}

	 
	if (physpath != NULL && strcmp("scsidebug", physpath) == 0)
		is_sd = B_TRUE;

	 
	if (!zpool_get_prop_int(zhp, ZPOOL_PROP_AUTOREPLACE, NULL) ||
	    !(wholedisk || is_mpath_wholedisk) || (physpath == NULL)) {
		(void) zpool_vdev_online(zhp, fullpath, ZFS_ONLINE_FORCEFAULT,
		    &newstate);
		zed_log_msg(LOG_INFO, "Pool's autoreplace is not enabled or "
		    "not a blank disk for '%s' ('%s')", fullpath,
		    physpath);
		return;
	}

	 
	(void) snprintf(rawpath, sizeof (rawpath), "%s%s",
	    is_sd ? DEV_BYVDEV_PATH : DEV_BYPATH_PATH, physpath);

	if (realpath(rawpath, pathbuf) == NULL && !is_mpath_wholedisk) {
		zed_log_msg(LOG_INFO, "  realpath: %s failed (%s)",
		    rawpath, strerror(errno));

		int err = zpool_vdev_online(zhp, fullpath,
		    ZFS_ONLINE_FORCEFAULT, &newstate);

		zed_log_msg(LOG_INFO, "  zpool_vdev_online: %s FORCEFAULT (%s) "
		    "err %d, new state %d",
		    fullpath, libzfs_error_description(g_zfshdl), err,
		    err ? (int)newstate : 0);
		return;
	}

	 
	if ((vs->vs_state != VDEV_STATE_DEGRADED) &&
	    (vs->vs_state != VDEV_STATE_FAULTED) &&
	    (vs->vs_state != VDEV_STATE_REMOVED) &&
	    (vs->vs_state != VDEV_STATE_CANT_OPEN)) {
		zed_log_msg(LOG_INFO, "  not autoreplacing since disk isn't in "
		    "a bad state (currently %llu)", vs->vs_state);
		return;
	}

	nvlist_lookup_string(vdev, "new_devid", &new_devid);

	if (is_mpath_wholedisk) {
		 
		zed_log_msg(LOG_INFO,
		    "  it's a multipath wholedisk, don't label");
		if (zpool_prepare_disk(zhp, vdev, "autoreplace", &lines,
		    &lines_cnt) != 0) {
			zed_log_msg(LOG_INFO,
			    "  zpool_prepare_disk: could not "
			    "prepare '%s' (%s)", fullpath,
			    libzfs_error_description(g_zfshdl));
			if (lines_cnt > 0) {
				zed_log_msg(LOG_INFO,
				    "  zfs_prepare_disk output:");
				lines_to_zed_log_msg(lines, lines_cnt);
			}
			libzfs_free_str_array(lines, lines_cnt);
			return;
		}
	} else if (!labeled) {
		 
		char *leafname;

		 
		leafname = strrchr(pathbuf, '/') + 1;

		 
		if (zpool_prepare_and_label_disk(g_zfshdl, zhp, leafname,
		    vdev, "autoreplace", &lines, &lines_cnt) != 0) {
			zed_log_msg(LOG_WARNING,
			    "  zpool_prepare_and_label_disk: could not "
			    "label '%s' (%s)", leafname,
			    libzfs_error_description(g_zfshdl));
			if (lines_cnt > 0) {
				zed_log_msg(LOG_INFO,
				"  zfs_prepare_disk output:");
				lines_to_zed_log_msg(lines, lines_cnt);
			}
			libzfs_free_str_array(lines, lines_cnt);

			(void) zpool_vdev_online(zhp, fullpath,
			    ZFS_ONLINE_FORCEFAULT, &newstate);
			return;
		}

		 
		device = malloc(sizeof (pendingdev_t));
		if (device == NULL) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}

		(void) strlcpy(device->pd_physpath, physpath,
		    sizeof (device->pd_physpath));
		list_insert_tail(&g_device_list, device);

		zed_log_msg(LOG_NOTICE, "  zpool_label_disk: async '%s' (%llu)",
		    leafname, (u_longlong_t)guid);

		return;	 

	} else   {
		boolean_t found = B_FALSE;
		 
		for (device = list_head(&g_device_list); device != NULL;
		    device = list_next(&g_device_list, device)) {
			if (strcmp(physpath, device->pd_physpath) == 0) {
				list_remove(&g_device_list, device);
				free(device);
				found = B_TRUE;
				break;
			}
			zed_log_msg(LOG_INFO, "zpool_label_disk: %s != %s",
			    physpath, device->pd_physpath);
		}
		if (!found) {
			 
			zed_log_msg(LOG_WARNING, "labeled disk %s was "
			    "unexpected here", fullpath);
			(void) zpool_vdev_online(zhp, fullpath,
			    ZFS_ONLINE_FORCEFAULT, &newstate);
			return;
		}

		zed_log_msg(LOG_INFO, "  zpool_label_disk: resume '%s' (%llu)",
		    physpath, (u_longlong_t)guid);

		 
		if (strncmp(path, DEV_BYID_PATH, strlen(DEV_BYID_PATH)) == 0) {
			(void) snprintf(pathbuf, sizeof (pathbuf), "%s%s",
			    DEV_BYID_PATH, new_devid);
			zed_log_msg(LOG_INFO, "  zpool_label_disk: path '%s' "
			    "replaced by '%s'", path, pathbuf);
			path = pathbuf;
		}
	}

	libzfs_free_str_array(lines, lines_cnt);

	 
	if (nvlist_alloc(&nvroot, NV_UNIQUE_NAME, 0) != 0) {
		zed_log_msg(LOG_WARNING, "zfs_mod: nvlist_alloc out of memory");
		return;
	}
	if (nvlist_alloc(&newvd, NV_UNIQUE_NAME, 0) != 0) {
		zed_log_msg(LOG_WARNING, "zfs_mod: nvlist_alloc out of memory");
		nvlist_free(nvroot);
		return;
	}

	if (nvlist_add_string(newvd, ZPOOL_CONFIG_TYPE, VDEV_TYPE_DISK) != 0 ||
	    nvlist_add_string(newvd, ZPOOL_CONFIG_PATH, path) != 0 ||
	    nvlist_add_string(newvd, ZPOOL_CONFIG_DEVID, new_devid) != 0 ||
	    (physpath != NULL && nvlist_add_string(newvd,
	    ZPOOL_CONFIG_PHYS_PATH, physpath) != 0) ||
	    (enc_sysfs_path != NULL && nvlist_add_string(newvd,
	    ZPOOL_CONFIG_VDEV_ENC_SYSFS_PATH, enc_sysfs_path) != 0) ||
	    nvlist_add_uint64(newvd, ZPOOL_CONFIG_WHOLE_DISK, wholedisk) != 0 ||
	    nvlist_add_string(nvroot, ZPOOL_CONFIG_TYPE, VDEV_TYPE_ROOT) != 0 ||
	    nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    (const nvlist_t **)&newvd, 1) != 0) {
		zed_log_msg(LOG_WARNING, "zfs_mod: unable to add nvlist pairs");
		nvlist_free(newvd);
		nvlist_free(nvroot);
		return;
	}

	nvlist_free(newvd);

	 
	if (zpool_label_disk_wait(path, DISK_LABEL_WAIT) != 0) {
		zed_log_msg(LOG_WARNING, "zfs_mod: pool '%s', after labeling "
		    "replacement disk, the expected disk partition link '%s' "
		    "is missing after waiting %u ms",
		    zpool_get_name(zhp), path, DISK_LABEL_WAIT);
		nvlist_free(nvroot);
		return;
	}

	 
	ret = zpool_vdev_attach(zhp, fullpath, path, nvroot, B_TRUE, B_TRUE);
	if (ret != 0) {
		ret = zpool_vdev_attach(zhp, fullpath, path, nvroot,
		    B_TRUE, B_FALSE);
	}

	zed_log_msg(LOG_WARNING, "  zpool_vdev_replace: %s with %s (%s)",
	    fullpath, path, (ret == 0) ? "no errors" :
	    libzfs_error_description(g_zfshdl));

	nvlist_free(nvroot);
}

 
typedef struct dev_data {
	const char		*dd_compare;
	const char		*dd_prop;
	zfs_process_func_t	dd_func;
	boolean_t		dd_found;
	boolean_t		dd_islabeled;
	uint64_t		dd_pool_guid;
	uint64_t		dd_vdev_guid;
	uint64_t		dd_new_vdev_guid;
	const char		*dd_new_devid;
	uint64_t		dd_num_spares;
} dev_data_t;

static void
zfs_iter_vdev(zpool_handle_t *zhp, nvlist_t *nvl, void *data)
{
	dev_data_t *dp = data;
	const char *path = NULL;
	uint_t c, children;
	nvlist_t **child;
	uint64_t guid = 0;
	uint64_t isspare = 0;

	 
	if (nvlist_lookup_nvlist_array(nvl, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++)
			zfs_iter_vdev(zhp, child[c], data);
	}

	 
	if (nvlist_lookup_nvlist_array(nvl, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++)
			zfs_iter_vdev(zhp, child[c], data);
	}
	if (nvlist_lookup_nvlist_array(nvl, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++)
			zfs_iter_vdev(zhp, child[c], data);
	}

	 
	if (dp->dd_found && dp->dd_num_spares == 0)
		return;
	(void) nvlist_lookup_uint64(nvl, ZPOOL_CONFIG_GUID, &guid);

	 
	if (dp->dd_vdev_guid != 0) {
		if (guid != dp->dd_vdev_guid)
			return;
		zed_log_msg(LOG_INFO, "  zfs_iter_vdev: matched on %llu", guid);
		dp->dd_found = B_TRUE;

	} else if (dp->dd_compare != NULL) {
		 
		if (nvlist_lookup_string(nvl, dp->dd_prop, &path) != 0 ||
		    strcmp(dp->dd_compare, path) != 0) {
			return;
		}
		if (dp->dd_new_vdev_guid != 0 && dp->dd_new_vdev_guid != guid) {
			zed_log_msg(LOG_INFO, "  %s: no match (GUID:%llu"
			    " != vdev GUID:%llu)", __func__,
			    dp->dd_new_vdev_guid, guid);
			return;
		}

		zed_log_msg(LOG_INFO, "  zfs_iter_vdev: matched %s on %s",
		    dp->dd_prop, path);
		dp->dd_found = B_TRUE;

		 
		if (dp->dd_new_devid != NULL) {
			(void) nvlist_add_string(nvl, "new_devid",
			    dp->dd_new_devid);
		}
	}

	if (dp->dd_found == B_TRUE && nvlist_lookup_uint64(nvl,
	    ZPOOL_CONFIG_IS_SPARE, &isspare) == 0 && isspare)
		dp->dd_num_spares++;

	(dp->dd_func)(zhp, nvl, dp->dd_islabeled);
}

static void
zfs_enable_ds(void *arg)
{
	unavailpool_t *pool = (unavailpool_t *)arg;

	(void) zpool_enable_datasets(pool->uap_zhp, NULL, 0);
	zpool_close(pool->uap_zhp);
	free(pool);
}

static int
zfs_iter_pool(zpool_handle_t *zhp, void *data)
{
	nvlist_t *config, *nvl;
	dev_data_t *dp = data;
	uint64_t pool_guid;
	unavailpool_t *pool;

	zed_log_msg(LOG_INFO, "zfs_iter_pool: evaluating vdevs on %s (by %s)",
	    zpool_get_name(zhp), dp->dd_vdev_guid ? "GUID" : dp->dd_prop);

	 
	if ((config = zpool_get_config(zhp, NULL)) != NULL) {
		if (dp->dd_pool_guid == 0 ||
		    (nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
		    &pool_guid) == 0 && pool_guid == dp->dd_pool_guid)) {
			(void) nvlist_lookup_nvlist(config,
			    ZPOOL_CONFIG_VDEV_TREE, &nvl);
			zfs_iter_vdev(zhp, nvl, data);
		}
	} else {
		zed_log_msg(LOG_INFO, "%s: no config\n", __func__);
	}

	 
	if (g_enumeration_done)  {
		for (pool = list_head(&g_pool_list); pool != NULL;
		    pool = list_next(&g_pool_list, pool)) {

			if (strcmp(zpool_get_name(zhp),
			    zpool_get_name(pool->uap_zhp)))
				continue;
			if (zfs_toplevel_state(zhp) >= VDEV_STATE_DEGRADED) {
				list_remove(&g_pool_list, pool);
				(void) tpool_dispatch(g_tpool, zfs_enable_ds,
				    pool);
				break;
			}
		}
	}

	zpool_close(zhp);

	 
	return (dp->dd_found && dp->dd_num_spares == 0);
}

 
static boolean_t
devphys_iter(const char *physical, const char *devid, zfs_process_func_t func,
    boolean_t is_slice, uint64_t new_vdev_guid)
{
	dev_data_t data = { 0 };

	data.dd_compare = physical;
	data.dd_func = func;
	data.dd_prop = ZPOOL_CONFIG_PHYS_PATH;
	data.dd_found = B_FALSE;
	data.dd_islabeled = is_slice;
	data.dd_new_devid = devid;	 
	data.dd_new_vdev_guid = new_vdev_guid;

	(void) zpool_iter(g_zfshdl, zfs_iter_pool, &data);

	return (data.dd_found);
}

 
static boolean_t
by_vdev_path_iter(const char *by_vdev_path, const char *devid,
    zfs_process_func_t func, boolean_t is_slice)
{
	dev_data_t data = { 0 };

	data.dd_compare = by_vdev_path;
	data.dd_func = func;
	data.dd_prop = ZPOOL_CONFIG_PATH;
	data.dd_found = B_FALSE;
	data.dd_islabeled = is_slice;
	data.dd_new_devid = devid;

	if (strncmp(by_vdev_path, DEV_BYVDEV_PATH,
	    strlen(DEV_BYVDEV_PATH)) != 0) {
		 
		return (B_FALSE);
	}

	(void) zpool_iter(g_zfshdl, zfs_iter_pool, &data);

	return (data.dd_found);
}

 
static boolean_t
devid_iter(const char *devid, zfs_process_func_t func, boolean_t is_slice)
{
	dev_data_t data = { 0 };

	data.dd_compare = devid;
	data.dd_func = func;
	data.dd_prop = ZPOOL_CONFIG_DEVID;
	data.dd_found = B_FALSE;
	data.dd_islabeled = is_slice;
	data.dd_new_devid = devid;

	(void) zpool_iter(g_zfshdl, zfs_iter_pool, &data);

	return (data.dd_found);
}

 
static boolean_t
guid_iter(uint64_t pool_guid, uint64_t vdev_guid, const char *devid,
    zfs_process_func_t func, boolean_t is_slice)
{
	dev_data_t data = { 0 };

	data.dd_func = func;
	data.dd_found = B_FALSE;
	data.dd_pool_guid = pool_guid;
	data.dd_vdev_guid = vdev_guid;
	data.dd_islabeled = is_slice;
	data.dd_new_devid = devid;

	(void) zpool_iter(g_zfshdl, zfs_iter_pool, &data);

	return (data.dd_found);
}

 
static int
zfs_deliver_add(nvlist_t *nvl)
{
	const char *devpath = NULL, *devid = NULL;
	uint64_t pool_guid = 0, vdev_guid = 0;
	boolean_t is_slice;

	 
	if (nvlist_lookup_string(nvl, DEV_IDENTIFIER, &devid) != 0) {
		zed_log_msg(LOG_INFO, "%s: no dev identifier\n", __func__);
		return (-1);
	}

	(void) nvlist_lookup_string(nvl, DEV_PHYS_PATH, &devpath);
	(void) nvlist_lookup_uint64(nvl, ZFS_EV_POOL_GUID, &pool_guid);
	(void) nvlist_lookup_uint64(nvl, ZFS_EV_VDEV_GUID, &vdev_guid);

	is_slice = (nvlist_lookup_boolean(nvl, DEV_IS_PART) == 0);

	zed_log_msg(LOG_INFO, "zfs_deliver_add: adding %s (%s) (is_slice %d)",
	    devid, devpath ? devpath : "NULL", is_slice);

	 
	if (devid_iter(devid, zfs_process_add, is_slice))
		return (0);
	if (devpath != NULL && devphys_iter(devpath, devid, zfs_process_add,
	    is_slice, vdev_guid))
		return (0);
	if (vdev_guid != 0)
		(void) guid_iter(pool_guid, vdev_guid, devid, zfs_process_add,
		    is_slice);

	if (devpath != NULL) {
		 
		char by_vdev_path[MAXPATHLEN];
		snprintf(by_vdev_path, sizeof (by_vdev_path),
		    "/dev/disk/by-vdev/%s", devpath);
		if (by_vdev_path_iter(by_vdev_path, devid, zfs_process_add,
		    is_slice))
			return (0);
	}

	return (0);
}

 
static int
zfs_deliver_check(nvlist_t *nvl)
{
	dev_data_t data = { 0 };

	if (nvlist_lookup_uint64(nvl, ZFS_EV_POOL_GUID,
	    &data.dd_pool_guid) != 0 ||
	    nvlist_lookup_uint64(nvl, ZFS_EV_VDEV_GUID,
	    &data.dd_vdev_guid) != 0 ||
	    data.dd_vdev_guid == 0)
		return (0);

	zed_log_msg(LOG_INFO, "zfs_deliver_check: pool '%llu', vdev %llu",
	    data.dd_pool_guid, data.dd_vdev_guid);

	data.dd_func = zfs_process_add;

	(void) zpool_iter(g_zfshdl, zfs_iter_pool, &data);

	return (0);
}

 
static uint64_t
vdev_size_from_config(zpool_handle_t *zhp, const char *vdev_path)
{
	nvlist_t *nvl = NULL;
	boolean_t avail_spare, l2cache, log;
	vdev_stat_t *vs = NULL;
	uint_t c;

	nvl = zpool_find_vdev(zhp, vdev_path, &avail_spare, &l2cache, &log);
	if (!nvl)
		return (0);

	verify(nvlist_lookup_uint64_array(nvl, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) == 0);
	if (!vs) {
		zed_log_msg(LOG_INFO, "%s: no nvlist for '%s'", __func__,
		    vdev_path);
		return (0);
	}

	return (vs->vs_pspace);
}

 
static uint64_t
vdev_whole_disk_from_config(zpool_handle_t *zhp, const char *vdev_path)
{
	nvlist_t *nvl = NULL;
	boolean_t avail_spare, l2cache, log;
	uint64_t wholedisk = 0;

	nvl = zpool_find_vdev(zhp, vdev_path, &avail_spare, &l2cache, &log);
	if (!nvl)
		return (0);

	(void) nvlist_lookup_uint64(nvl, ZPOOL_CONFIG_WHOLE_DISK, &wholedisk);

	return (wholedisk);
}

 
#define	DEVICE_GREW(oldsize, newsize) \
		    ((newsize > oldsize) && \
		    ((newsize / (newsize - oldsize)) <= 100))

static int
zfsdle_vdev_online(zpool_handle_t *zhp, void *data)
{
	boolean_t avail_spare, l2cache;
	nvlist_t *udev_nvl = data;
	nvlist_t *tgt;
	int error;

	const char *tmp_devname;
	char devname[MAXPATHLEN] = "";
	uint64_t guid;

	if (nvlist_lookup_uint64(udev_nvl, ZFS_EV_VDEV_GUID, &guid) == 0) {
		sprintf(devname, "%llu", (u_longlong_t)guid);
	} else if (nvlist_lookup_string(udev_nvl, DEV_PHYS_PATH,
	    &tmp_devname) == 0) {
		strlcpy(devname, tmp_devname, MAXPATHLEN);
		zfs_append_partition(devname, MAXPATHLEN);
	} else {
		zed_log_msg(LOG_INFO, "%s: no guid or physpath", __func__);
	}

	zed_log_msg(LOG_INFO, "zfsdle_vdev_online: searching for '%s' in '%s'",
	    devname, zpool_get_name(zhp));

	if ((tgt = zpool_find_vdev_by_physpath(zhp, devname,
	    &avail_spare, &l2cache, NULL)) != NULL) {
		const char *path;
		char fullpath[MAXPATHLEN];
		uint64_t wholedisk = 0;

		error = nvlist_lookup_string(tgt, ZPOOL_CONFIG_PATH, &path);
		if (error) {
			zpool_close(zhp);
			return (0);
		}

		(void) nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_WHOLE_DISK,
		    &wholedisk);

		if (wholedisk) {
			char *tmp;
			path = strrchr(path, '/');
			if (path != NULL) {
				tmp = zfs_strip_partition(path + 1);
				if (tmp == NULL) {
					zpool_close(zhp);
					return (0);
				}
			} else {
				zpool_close(zhp);
				return (0);
			}

			(void) strlcpy(fullpath, tmp, sizeof (fullpath));
			free(tmp);

			 
			boolean_t scrub_restart = B_FALSE;
			(void) zpool_reopen_one(zhp, &scrub_restart);
		} else {
			(void) strlcpy(fullpath, path, sizeof (fullpath));
		}

		if (zpool_get_prop_int(zhp, ZPOOL_PROP_AUTOEXPAND, NULL)) {
			vdev_state_t newstate;

			if (zpool_get_state(zhp) != POOL_STATE_UNAVAIL) {
				 
				uint64_t udev_size = 0, conf_size = 0,
				    wholedisk = 0, udev_parent_size = 0;

				 
				if (nvlist_lookup_uint64(udev_nvl, DEV_SIZE,
				    &udev_size) != 0) {
					udev_size = 0;
				}

				 
				if (nvlist_lookup_uint64(udev_nvl,
				    DEV_PARENT_SIZE, &udev_parent_size) != 0) {
					udev_parent_size = 0;
				}

				conf_size = vdev_size_from_config(zhp,
				    fullpath);

				wholedisk = vdev_whole_disk_from_config(zhp,
				    fullpath);

				 
				if (DEVICE_GREW(conf_size, udev_size) ||
				    (wholedisk && DEVICE_GREW(conf_size,
				    udev_parent_size))) {
					error = zpool_vdev_online(zhp, fullpath,
					    0, &newstate);

					zed_log_msg(LOG_INFO,
					    "%s: autoexpanding '%s' from %llu"
					    " to %llu bytes in pool '%s': %d",
					    __func__, fullpath, conf_size,
					    MAX(udev_size, udev_parent_size),
					    zpool_get_name(zhp), error);
				}
			}
		}
		zpool_close(zhp);
		return (1);
	}
	zpool_close(zhp);
	return (0);
}

 
static int
zfs_deliver_dle(nvlist_t *nvl)
{
	const char *devname;
	char name[MAXPATHLEN];
	uint64_t guid;

	if (nvlist_lookup_uint64(nvl, ZFS_EV_VDEV_GUID, &guid) == 0) {
		sprintf(name, "%llu", (u_longlong_t)guid);
	} else if (nvlist_lookup_string(nvl, DEV_PHYS_PATH, &devname) == 0) {
		strlcpy(name, devname, MAXPATHLEN);
		zfs_append_partition(name, MAXPATHLEN);
	} else {
		sprintf(name, "unknown");
		zed_log_msg(LOG_INFO, "zfs_deliver_dle: no guid or physpath");
	}

	if (zpool_iter(g_zfshdl, zfsdle_vdev_online, nvl) != 1) {
		zed_log_msg(LOG_INFO, "zfs_deliver_dle: device '%s' not "
		    "found", name);
		return (1);
	}

	return (0);
}

 
static int
zfs_slm_deliver_event(const char *class, const char *subclass, nvlist_t *nvl)
{
	int ret;
	boolean_t is_check = B_FALSE, is_dle = B_FALSE;

	if (strcmp(class, EC_DEV_ADD) == 0) {
		 
		if (strcmp(subclass, ESC_DISK) != 0 &&
		    strcmp(subclass, ESC_LOFI) != 0)
			return (0);

		is_check = B_FALSE;
	} else if (strcmp(class, EC_ZFS) == 0 &&
	    strcmp(subclass, ESC_ZFS_VDEV_CHECK) == 0) {
		 
		is_check = B_TRUE;
	} else if (strcmp(class, EC_DEV_STATUS) == 0 &&
	    strcmp(subclass, ESC_DEV_DLE) == 0) {
		is_dle = B_TRUE;
	} else {
		return (0);
	}

	if (is_dle)
		ret = zfs_deliver_dle(nvl);
	else if (is_check)
		ret = zfs_deliver_check(nvl);
	else
		ret = zfs_deliver_add(nvl);

	return (ret);
}

static void *
zfs_enum_pools(void *arg)
{
	(void) arg;

	(void) zpool_iter(g_zfshdl, zfs_unavail_pool, (void *)&g_pool_list);
	 
	g_enumeration_done = B_TRUE;
	return (NULL);
}

 
int
zfs_slm_init(void)
{
	if ((g_zfshdl = libzfs_init()) == NULL)
		return (-1);

	 
	list_create(&g_pool_list, sizeof (struct unavailpool),
	    offsetof(struct unavailpool, uap_node));

	if (pthread_create(&g_zfs_tid, NULL, zfs_enum_pools, NULL) != 0) {
		list_destroy(&g_pool_list);
		libzfs_fini(g_zfshdl);
		return (-1);
	}

	pthread_setname_np(g_zfs_tid, "enum-pools");
	list_create(&g_device_list, sizeof (struct pendingdev),
	    offsetof(struct pendingdev, pd_node));

	return (0);
}

void
zfs_slm_fini(void)
{
	unavailpool_t *pool;
	pendingdev_t *device;

	 
	(void) pthread_join(g_zfs_tid, NULL);
	 
	if (g_tpool != NULL) {
		tpool_wait(g_tpool);
		tpool_destroy(g_tpool);
	}

	while ((pool = list_remove_head(&g_pool_list)) != NULL) {
		zpool_close(pool->uap_zhp);
		free(pool);
	}
	list_destroy(&g_pool_list);

	while ((device = list_remove_head(&g_device_list)) != NULL)
		free(device);
	list_destroy(&g_device_list);

	libzfs_fini(g_zfshdl);
}

void
zfs_slm_event(const char *class, const char *subclass, nvlist_t *nvl)
{
	zed_log_msg(LOG_INFO, "zfs_slm_event: %s.%s", class, subclass);
	(void) zfs_slm_deliver_event(class, subclass, nvl);
}
