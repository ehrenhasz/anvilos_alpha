#ifdef HAVE_LIBUDEV
#include <errno.h>
#include <fcntl.h>
#include <libnvpair.h>
#include <libudev.h>
#include <libzfs.h>
#include <libzutil.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysevent/eventdefs.h>
#include <sys/sysevent/dev.h>
#include "zed_log.h"
#include "zed_disk_event.h"
#include "agents/zfs_agents.h"
pthread_t g_mon_tid;
struct udev *g_udev;
struct udev_monitor *g_mon;
#define	DEV_BYID_PATH	"/dev/disk/by-id/"
#define	MINIMUM_SECTORS		131072ULL
static void
zed_udev_event(const char *class, const char *subclass, nvlist_t *nvl)
{
	const char *strval;
	uint64_t numval;
	zed_log_msg(LOG_INFO, "zed_disk_event:");
	zed_log_msg(LOG_INFO, "\tclass: %s", class);
	zed_log_msg(LOG_INFO, "\tsubclass: %s", subclass);
	if (nvlist_lookup_string(nvl, DEV_NAME, &strval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %s", DEV_NAME, strval);
	if (nvlist_lookup_string(nvl, DEV_PATH, &strval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %s", DEV_PATH, strval);
	if (nvlist_lookup_string(nvl, DEV_IDENTIFIER, &strval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %s", DEV_IDENTIFIER, strval);
	if (nvlist_lookup_boolean(nvl, DEV_IS_PART) == B_TRUE)
		zed_log_msg(LOG_INFO, "\t%s: B_TRUE", DEV_IS_PART);
	if (nvlist_lookup_string(nvl, DEV_PHYS_PATH, &strval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %s", DEV_PHYS_PATH, strval);
	if (nvlist_lookup_uint64(nvl, DEV_SIZE, &numval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %llu", DEV_SIZE, numval);
	if (nvlist_lookup_uint64(nvl, DEV_PARENT_SIZE, &numval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %llu", DEV_PARENT_SIZE, numval);
	if (nvlist_lookup_uint64(nvl, ZFS_EV_POOL_GUID, &numval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %llu", ZFS_EV_POOL_GUID, numval);
	if (nvlist_lookup_uint64(nvl, ZFS_EV_VDEV_GUID, &numval) == 0)
		zed_log_msg(LOG_INFO, "\t%s: %llu", ZFS_EV_VDEV_GUID, numval);
	(void) zfs_agent_post_event(class, subclass, nvl);
}
static nvlist_t *
dev_event_nvlist(struct udev_device *dev)
{
	nvlist_t *nvl;
	char strval[128];
	const char *value, *path;
	uint64_t guid;
	if (nvlist_alloc(&nvl, NV_UNIQUE_NAME, 0) != 0)
		return (NULL);
	if (zfs_device_get_devid(dev, strval, sizeof (strval)) == 0)
		(void) nvlist_add_string(nvl, DEV_IDENTIFIER, strval);
	if (zfs_device_get_physical(dev, strval, sizeof (strval)) == 0)
		(void) nvlist_add_string(nvl, DEV_PHYS_PATH, strval);
	if ((path = udev_device_get_devnode(dev)) != NULL)
		(void) nvlist_add_string(nvl, DEV_NAME, path);
	if ((value = udev_device_get_devpath(dev)) != NULL)
		(void) nvlist_add_string(nvl, DEV_PATH, value);
	value = udev_device_get_devtype(dev);
	if ((value != NULL && strcmp("partition", value) == 0) ||
	    (udev_device_get_property_value(dev, "ID_PART_ENTRY_NUMBER")
	    != NULL)) {
		(void) nvlist_add_boolean(nvl, DEV_IS_PART);
	}
	if ((value = udev_device_get_sysattr_value(dev, "size")) != NULL) {
		uint64_t numval = DEV_BSIZE;
		numval *= strtoull(value, NULL, 10);
		(void) nvlist_add_uint64(nvl, DEV_SIZE, numval);
		struct udev_device *parent_dev = udev_device_get_parent(dev);
		if ((value = udev_device_get_sysattr_value(parent_dev, "size"))
		    != NULL) {
			uint64_t numval = DEV_BSIZE;
			numval *= strtoull(value, NULL, 10);
			(void) nvlist_add_uint64(nvl, DEV_PARENT_SIZE, numval);
		}
	}
	value = udev_device_get_property_value(dev, "ID_FS_UUID");
	if (value != NULL && (guid = strtoull(value, NULL, 10)) != 0)
		(void) nvlist_add_uint64(nvl, ZFS_EV_POOL_GUID, guid);
	value = udev_device_get_property_value(dev, "ID_FS_UUID_SUB");
	if (value != NULL && (guid = strtoull(value, NULL, 10)) != 0)
		(void) nvlist_add_uint64(nvl, ZFS_EV_VDEV_GUID, guid);
	if (!nvlist_exists(nvl, DEV_IDENTIFIER) &&
	    !nvlist_exists(nvl, ZFS_EV_VDEV_GUID)) {
		nvlist_free(nvl);
		return (NULL);
	}
	return (nvl);
}
static void *
zed_udev_monitor(void *arg)
{
	struct udev_monitor *mon = arg;
	const char *tmp;
	char *tmp2;
	zed_log_msg(LOG_INFO, "Waiting for new udev disk events...");
	while (1) {
		struct udev_device *dev;
		const char *action, *type, *part, *sectors;
		const char *bus, *uuid, *devpath;
		const char *class, *subclass;
		nvlist_t *nvl;
		boolean_t is_zfs = B_FALSE;
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		if ((dev = udev_monitor_receive_device(mon)) == NULL) {
			zed_log_msg(LOG_WARNING, "zed_udev_monitor: receive "
			    "device error %d", errno);
			continue;
		}
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		type = udev_device_get_property_value(dev, "ID_FS_TYPE");
		if (type != NULL && type[0] != '\0') {
			if (strcmp(type, "zfs_member") == 0) {
				is_zfs = B_TRUE;
			} else {
				zed_log_msg(LOG_INFO, "zed_udev_monitor: skip "
				    "%s (in use by %s)",
				    udev_device_get_devnode(dev), type);
				udev_device_unref(dev);
				continue;
			}
		}
		type = udev_device_get_property_value(dev, "DEVTYPE");
		part = udev_device_get_property_value(dev,
		    "ID_PART_TABLE_TYPE");
		if (type != NULL && type[0] != '\0' &&
		    strcmp(type, "disk") == 0 &&
		    part != NULL && part[0] != '\0') {
			const char *devname =
			    udev_device_get_property_value(dev, "DEVNAME");
			if (strcmp(part, "atari") == 0) {
				zed_log_msg(LOG_INFO,
				    "%s: %s is reporting an atari partition, "
				    "but we're going to assume it's a false "
				    "positive and still use it (issue #13497)",
				    __func__, devname);
			} else {
				zed_log_msg(LOG_INFO,
				    "%s: skip %s since it has a %s partition "
				    "already", __func__, devname, part);
				udev_device_unref(dev);
				continue;
			}
		}
		sectors = udev_device_get_property_value(dev,
		    "ID_PART_ENTRY_SIZE");
		if (sectors == NULL)
			sectors = udev_device_get_sysattr_value(dev, "size");
		if (sectors != NULL &&
		    strtoull(sectors, NULL, 10) < MINIMUM_SECTORS) {
			zed_log_msg(LOG_INFO,
			    "%s: %s sectors %s < %llu (minimum)",
			    __func__,
			    udev_device_get_property_value(dev, "DEVNAME"),
			    sectors, MINIMUM_SECTORS);
			udev_device_unref(dev);
			continue;
		}
		bus = udev_device_get_property_value(dev, "ID_BUS");
		uuid = udev_device_get_property_value(dev, "DM_UUID");
		devpath = udev_device_get_devpath(dev);
		if (!is_zfs && (bus == NULL && uuid == NULL &&
		    strstr(devpath, "/nvme/") == NULL)) {
			zed_log_msg(LOG_INFO, "zed_udev_monitor: %s no devid "
			    "source", udev_device_get_devnode(dev));
			udev_device_unref(dev);
			continue;
		}
		action = udev_device_get_action(dev);
		if (strcmp(action, "add") == 0) {
			class = EC_DEV_ADD;
			subclass = ESC_DISK;
		} else if (strcmp(action, "remove") == 0) {
			class = EC_DEV_REMOVE;
			subclass = ESC_DISK;
		} else if (strcmp(action, "change") == 0) {
			class = EC_DEV_STATUS;
			subclass = ESC_DEV_DLE;
		} else {
			zed_log_msg(LOG_WARNING, "zed_udev_monitor: %s unknown",
			    action);
			udev_device_unref(dev);
			continue;
		}
		if (strcmp(class, EC_DEV_STATUS) == 0 &&
		    udev_device_get_property_value(dev, "DM_UUID") &&
		    udev_device_get_property_value(dev, "MPATH_SBIN_PATH")) {
			tmp = udev_device_get_devnode(dev);
			tmp2 = zfs_get_underlying_path(tmp);
			if (tmp && tmp2 && (strcmp(tmp, tmp2) != 0)) {
				class = EC_DEV_ADD;
				subclass = ESC_DISK;
			} else {
				tmp = udev_device_get_property_value(dev,
				    "DM_NR_VALID_PATHS");
				if (tmp != NULL && strcmp(tmp, "0") == 0) {
					class = EC_DEV_REMOVE;
					subclass = ESC_DISK;
				}
			}
			free(tmp2);
		}
		if (strcmp(class, EC_DEV_STATUS) == 0 &&
		    udev_device_get_property_value(dev, "ID_VDEV") &&
		    udev_device_get_property_value(dev, "ID_MODEL")) {
			const char *id_model, *id_model_sd = "scsi_debug";
			id_model = udev_device_get_property_value(dev,
			    "ID_MODEL");
			if (strcmp(id_model, id_model_sd) == 0) {
				class = EC_DEV_ADD;
				subclass = ESC_DISK;
			}
		}
		if ((nvl = dev_event_nvlist(dev)) != NULL) {
			zed_udev_event(class, subclass, nvl);
			nvlist_free(nvl);
		}
		udev_device_unref(dev);
	}
	return (NULL);
}
int
zed_disk_event_init(void)
{
	int fd, fflags;
	if ((g_udev = udev_new()) == NULL) {
		zed_log_msg(LOG_WARNING, "udev_new failed (%d)", errno);
		return (-1);
	}
	g_mon = udev_monitor_new_from_netlink(g_udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(g_mon, "block", "disk");
	udev_monitor_filter_add_match_subsystem_devtype(g_mon, "block",
	    "partition");
	udev_monitor_enable_receiving(g_mon);
	fd = udev_monitor_get_fd(g_mon);
	if ((fflags = fcntl(fd, F_GETFL)) & O_NONBLOCK)
		(void) fcntl(fd, F_SETFL, fflags & ~O_NONBLOCK);
	if (pthread_create(&g_mon_tid, NULL, zed_udev_monitor, g_mon) != 0) {
		udev_monitor_unref(g_mon);
		udev_unref(g_udev);
		zed_log_msg(LOG_WARNING, "pthread_create failed");
		return (-1);
	}
	pthread_setname_np(g_mon_tid, "udev monitor");
	zed_log_msg(LOG_INFO, "zed_disk_event_init");
	return (0);
}
void
zed_disk_event_fini(void)
{
	(void) pthread_cancel(g_mon_tid);
	(void) pthread_join(g_mon_tid, NULL);
	udev_monitor_unref(g_mon);
	udev_unref(g_udev);
	zed_log_msg(LOG_INFO, "zed_disk_event_fini");
}
#else
#include "zed_disk_event.h"
int
zed_disk_event_init(void)
{
	return (0);
}
void
zed_disk_event_fini(void)
{
}
#endif  
