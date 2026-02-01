 
 

#include <libzfs.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <sys/dmu.h>
#include <sys/dmu_objset.h>
#include <sys/dnode.h>
#include <sys/vdev_impl.h>

#include <sys/mkdev.h>

#include "zinject.h"

static int debug;

static void
ziprintf(const char *fmt, ...)
{
	va_list ap;

	if (!debug)
		return;

	va_start(ap, fmt);
	(void) vprintf(fmt, ap);
	va_end(ap);
}

static void
compress_slashes(const char *src, char *dest)
{
	while (*src != '\0') {
		*dest = *src++;
		while (*dest == '/' && *src == '/')
			++src;
		++dest;
	}
	*dest = '\0';
}

 
static int
parse_pathname(const char *inpath, char *dataset, char *relpath,
    struct stat64 *statbuf)
{
	struct extmnttab mp;
	const char *rel;
	char fullpath[MAXPATHLEN];

	compress_slashes(inpath, fullpath);

	if (fullpath[0] != '/') {
		(void) fprintf(stderr, "invalid object '%s': must be full "
		    "path\n", fullpath);
		usage();
		return (-1);
	}

	if (getextmntent(fullpath, &mp, statbuf) != 0) {
		(void) fprintf(stderr, "cannot find mountpoint for '%s'\n",
		    fullpath);
		return (-1);
	}

	if (strcmp(mp.mnt_fstype, MNTTYPE_ZFS) != 0) {
		(void) fprintf(stderr, "invalid path '%s': not a ZFS "
		    "filesystem\n", fullpath);
		return (-1);
	}

	if (strncmp(fullpath, mp.mnt_mountp, strlen(mp.mnt_mountp)) != 0) {
		(void) fprintf(stderr, "invalid path '%s': mountpoint "
		    "doesn't match path\n", fullpath);
		return (-1);
	}

	(void) strlcpy(dataset, mp.mnt_special, MAXNAMELEN);

	rel = fullpath + strlen(mp.mnt_mountp);
	if (rel[0] == '/')
		rel++;
	(void) strlcpy(relpath, rel, MAXPATHLEN);

	return (0);
}

 
static int
object_from_path(const char *dataset, uint64_t object, zinject_record_t *record)
{
	zfs_handle_t *zhp;

	if ((zhp = zfs_open(g_zfs, dataset, ZFS_TYPE_DATASET)) == NULL)
		return (-1);

	record->zi_objset = zfs_prop_get_int(zhp, ZFS_PROP_OBJSETID);
	record->zi_object = object;

	zfs_close(zhp);

	return (0);
}

 
static int
initialize_range(err_type_t type, int level, char *range,
    zinject_record_t *record)
{
	 
	if (range == NULL) {
		 
		record->zi_start = 0;
		record->zi_end = -1ULL;
	} else {
		char *end;

		 
		record->zi_start = strtoull(range, &end, 10);


		if (*end == '\0')
			record->zi_end = record->zi_start + 1;
		else if (*end == ',')
			record->zi_end = strtoull(end + 1, &end, 10);

		if (*end != '\0') {
			(void) fprintf(stderr, "invalid range '%s': must be "
			    "a numeric range of the form 'start[,end]'\n",
			    range);
			return (-1);
		}
	}

	switch (type) {
	default:
		break;

	case TYPE_DATA:
		break;

	case TYPE_DNODE:
		 
		if (range != NULL) {
			(void) fprintf(stderr, "range cannot be specified when "
			    "type is 'dnode'\n");
			return (-1);
		}

		record->zi_start = record->zi_object * sizeof (dnode_phys_t);
		record->zi_end = record->zi_start + sizeof (dnode_phys_t);
		record->zi_object = 0;
		break;
	}

	record->zi_level = level;

	return (0);
}

int
translate_record(err_type_t type, const char *object, const char *range,
    int level, zinject_record_t *record, char *poolname, char *dataset)
{
	char path[MAXPATHLEN];
	char *slash;
	struct stat64 statbuf;
	int ret = -1;

	debug = (getenv("ZINJECT_DEBUG") != NULL);

	ziprintf("translating: %s\n", object);

	if (MOS_TYPE(type)) {
		 
		switch (type) {
		default:
			break;
		case TYPE_MOS:
			record->zi_type = 0;
			break;
		case TYPE_MOSDIR:
			record->zi_type = DMU_OT_OBJECT_DIRECTORY;
			break;
		case TYPE_METASLAB:
			record->zi_type = DMU_OT_OBJECT_ARRAY;
			break;
		case TYPE_CONFIG:
			record->zi_type = DMU_OT_PACKED_NVLIST;
			break;
		case TYPE_BPOBJ:
			record->zi_type = DMU_OT_BPOBJ;
			break;
		case TYPE_SPACEMAP:
			record->zi_type = DMU_OT_SPACE_MAP;
			break;
		case TYPE_ERRLOG:
			record->zi_type = DMU_OT_ERROR_LOG;
			break;
		}

		dataset[0] = '\0';
		(void) strlcpy(poolname, object, MAXNAMELEN);
		return (0);
	}

	 
	if (parse_pathname(object, dataset, path, &statbuf) != 0)
		goto err;

	ziprintf("   dataset: %s\n", dataset);
	ziprintf("      path: %s\n", path);

	 
	if (object_from_path(dataset, statbuf.st_ino, record) != 0)
		goto err;

	ziprintf("raw objset: %llu\n", record->zi_objset);
	ziprintf("raw object: %llu\n", record->zi_object);

	 
	if (initialize_range(type, level, (char *)range, record) != 0)
		goto err;

	ziprintf("    objset: %llu\n", record->zi_objset);
	ziprintf("    object: %llu\n", record->zi_object);
	if (record->zi_start == 0 &&
	    record->zi_end == -1ULL)
		ziprintf("     range: all\n");
	else
		ziprintf("     range: [%llu, %llu]\n", record->zi_start,
		    record->zi_end);

	 
	(void) strlcpy(poolname, dataset, MAXNAMELEN);
	if ((slash = strchr(poolname, '/')) != NULL)
		*slash = '\0';

	ret = 0;

err:
	return (ret);
}

int
translate_raw(const char *str, zinject_record_t *record)
{
	 
	if (sscanf(str, "%llx:%llx:%x:%llx", (u_longlong_t *)&record->zi_objset,
	    (u_longlong_t *)&record->zi_object, &record->zi_level,
	    (u_longlong_t *)&record->zi_start) != 4) {
		(void) fprintf(stderr, "bad raw spec '%s': must be of the form "
		    "'objset:object:level:blkid'\n", str);
		return (-1);
	}

	record->zi_end = record->zi_start;

	return (0);
}

int
translate_device(const char *pool, const char *device, err_type_t label_type,
    zinject_record_t *record)
{
	char *end;
	zpool_handle_t *zhp;
	nvlist_t *tgt;
	boolean_t isspare, iscache;

	 
	if ((zhp = zpool_open(g_zfs, pool)) == NULL)
		return (-1);

	record->zi_guid = strtoull(device, &end, 0);
	if (record->zi_guid == 0 || *end != '\0') {
		tgt = zpool_find_vdev(zhp, device, &isspare, &iscache, NULL);

		if (tgt == NULL) {
			(void) fprintf(stderr, "cannot find device '%s' in "
			    "pool '%s'\n", device, pool);
			zpool_close(zhp);
			return (-1);
		}

		verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID,
		    &record->zi_guid) == 0);
	}

	 
	if (record->zi_timer != 0) {
		record->zi_cmd = ZINJECT_DELAY_IO;
	} else if (label_type != TYPE_INVAL) {
		record->zi_cmd = ZINJECT_LABEL_FAULT;
	} else {
		record->zi_cmd = ZINJECT_DEVICE_FAULT;
	}

	switch (label_type) {
	default:
		break;
	case TYPE_LABEL_UBERBLOCK:
		record->zi_start = offsetof(vdev_label_t, vl_uberblock[0]);
		record->zi_end = record->zi_start + VDEV_UBERBLOCK_RING - 1;
		break;
	case TYPE_LABEL_NVLIST:
		record->zi_start = offsetof(vdev_label_t, vl_vdev_phys);
		record->zi_end = record->zi_start + VDEV_PHYS_SIZE - 1;
		break;
	case TYPE_LABEL_PAD1:
		record->zi_start = offsetof(vdev_label_t, vl_pad1);
		record->zi_end = record->zi_start + VDEV_PAD_SIZE - 1;
		break;
	case TYPE_LABEL_PAD2:
		record->zi_start = offsetof(vdev_label_t, vl_be);
		record->zi_end = record->zi_start + VDEV_PAD_SIZE - 1;
		break;
	}
	zpool_close(zhp);
	return (0);
}
