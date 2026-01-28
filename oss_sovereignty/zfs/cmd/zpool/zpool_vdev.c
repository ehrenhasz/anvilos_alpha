#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <libnvpair.h>
#include <libzutil.h>
#include <limits.h>
#include <sys/spa.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "zpool_util.h"
#include <sys/zfs_context.h>
#include <sys/stat.h>
boolean_t error_seen;
boolean_t is_force;
void
vdev_error(const char *fmt, ...)
{
	va_list ap;
	if (!error_seen) {
		(void) fprintf(stderr, gettext("invalid vdev specification\n"));
		if (!is_force)
			(void) fprintf(stderr, gettext("use '-f' to override "
			    "the following errors:\n"));
		else
			(void) fprintf(stderr, gettext("the following errors "
			    "must be manually repaired:\n"));
		error_seen = B_TRUE;
	}
	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}
int
check_file_generic(const char *file, boolean_t force, boolean_t isspare)
{
	char  *name;
	int fd;
	int ret = 0;
	pool_state_t state;
	boolean_t inuse;
	if ((fd = open(file, O_RDONLY)) < 0)
		return (0);
	if (zpool_in_use(g_zfs, fd, &state, &name, &inuse) == 0 && inuse) {
		const char *desc;
		switch (state) {
		case POOL_STATE_ACTIVE:
			desc = gettext("active");
			break;
		case POOL_STATE_EXPORTED:
			desc = gettext("exported");
			break;
		case POOL_STATE_POTENTIALLY_ACTIVE:
			desc = gettext("potentially active");
			break;
		default:
			desc = gettext("unknown");
			break;
		}
		if (state == POOL_STATE_SPARE && isspare) {
			free(name);
			(void) close(fd);
			return (0);
		}
		if (state == POOL_STATE_ACTIVE ||
		    state == POOL_STATE_SPARE || !force) {
			switch (state) {
			case POOL_STATE_SPARE:
				vdev_error(gettext("%s is reserved as a hot "
				    "spare for pool %s\n"), file, name);
				break;
			default:
				vdev_error(gettext("%s is part of %s pool "
				    "'%s'\n"), file, desc, name);
				break;
			}
			ret = -1;
		}
		free(name);
	}
	(void) close(fd);
	return (ret);
}
static int
is_shorthand_path(const char *arg, char *path, size_t path_size,
    struct stat64 *statbuf, boolean_t *wholedisk)
{
	int error;
	error = zfs_resolve_shortname(arg, path, path_size);
	if (error == 0) {
		*wholedisk = zfs_dev_is_whole_disk(path);
		if (*wholedisk || (stat64(path, statbuf) == 0))
			return (0);
	}
	strlcpy(path, arg, path_size);
	memset(statbuf, 0, sizeof (*statbuf));
	*wholedisk = B_FALSE;
	return (error);
}
static boolean_t
is_spare(nvlist_t *config, const char *path)
{
	int fd;
	pool_state_t state;
	char *name = NULL;
	nvlist_t *label;
	uint64_t guid, spareguid;
	nvlist_t *nvroot;
	nvlist_t **spares;
	uint_t i, nspares;
	boolean_t inuse;
	if (zpool_is_draid_spare(path))
		return (B_TRUE);
	if ((fd = open(path, O_RDONLY|O_DIRECT)) < 0)
		return (B_FALSE);
	if (zpool_in_use(g_zfs, fd, &state, &name, &inuse) != 0 ||
	    !inuse ||
	    state != POOL_STATE_SPARE ||
	    zpool_read_label(fd, &label, NULL) != 0) {
		free(name);
		(void) close(fd);
		return (B_FALSE);
	}
	free(name);
	(void) close(fd);
	if (config == NULL) {
		nvlist_free(label);
		return (B_TRUE);
	}
	verify(nvlist_lookup_uint64(label, ZPOOL_CONFIG_GUID, &guid) == 0);
	nvlist_free(label);
	verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);
	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		for (i = 0; i < nspares; i++) {
			verify(nvlist_lookup_uint64(spares[i],
			    ZPOOL_CONFIG_GUID, &spareguid) == 0);
			if (spareguid == guid)
				return (B_TRUE);
		}
	}
	return (B_FALSE);
}
static nvlist_t *
make_leaf_vdev(nvlist_t *props, const char *arg, boolean_t is_primary)
{
	char path[MAXPATHLEN];
	struct stat64 statbuf;
	nvlist_t *vdev = NULL;
	const char *type = NULL;
	boolean_t wholedisk = B_FALSE;
	uint64_t ashift = 0;
	int err;
	if (arg[0] == '/') {
		if (realpath(arg, path) == NULL) {
			(void) fprintf(stderr,
			    gettext("cannot resolve path '%s'\n"), arg);
			return (NULL);
		}
		wholedisk = zfs_dev_is_whole_disk(path);
		if (!wholedisk && (stat64(path, &statbuf) != 0)) {
			(void) fprintf(stderr,
			    gettext("cannot open '%s': %s\n"),
			    path, strerror(errno));
			return (NULL);
		}
		strlcpy(path, arg, sizeof (path));
	} else if (zpool_is_draid_spare(arg)) {
		if (!is_primary) {
			(void) fprintf(stderr,
			    gettext("cannot open '%s': dRAID spares can only "
			    "be used to replace primary vdevs\n"), arg);
			return (NULL);
		}
		wholedisk = B_TRUE;
		strlcpy(path, arg, sizeof (path));
		type = VDEV_TYPE_DRAID_SPARE;
	} else {
		err = is_shorthand_path(arg, path, sizeof (path),
		    &statbuf, &wholedisk);
		if (err != 0) {
			if (err == ENOENT) {
				(void) fprintf(stderr,
				    gettext("cannot open '%s': no such "
				    "device in %s\n"), arg, DISK_ROOT);
				(void) fprintf(stderr,
				    gettext("must be a full path or "
				    "shorthand device name\n"));
				return (NULL);
			} else {
				(void) fprintf(stderr,
				    gettext("cannot open '%s': %s\n"),
				    path, strerror(errno));
				return (NULL);
			}
		}
	}
	if (type == NULL) {
		if (wholedisk || S_ISBLK(statbuf.st_mode)) {
			type = VDEV_TYPE_DISK;
		} else if (S_ISREG(statbuf.st_mode)) {
			type = VDEV_TYPE_FILE;
		} else {
			fprintf(stderr, gettext("cannot use '%s': must "
			    "be a block device or regular file\n"), path);
			return (NULL);
		}
	}
	verify(nvlist_alloc(&vdev, NV_UNIQUE_NAME, 0) == 0);
	verify(nvlist_add_string(vdev, ZPOOL_CONFIG_PATH, path) == 0);
	verify(nvlist_add_string(vdev, ZPOOL_CONFIG_TYPE, type) == 0);
	if (strcmp(type, VDEV_TYPE_DISK) == 0)
		verify(nvlist_add_uint64(vdev, ZPOOL_CONFIG_WHOLE_DISK,
		    (uint64_t)wholedisk) == 0);
	if (props != NULL) {
		const char *value = NULL;
		if (nvlist_lookup_string(props,
		    zpool_prop_to_name(ZPOOL_PROP_ASHIFT), &value) == 0) {
			if (zfs_nicestrtonum(NULL, value, &ashift) != 0) {
				(void) fprintf(stderr,
				    gettext("ashift must be a number.\n"));
				return (NULL);
			}
			if (ashift != 0 &&
			    (ashift < ASHIFT_MIN || ashift > ASHIFT_MAX)) {
				(void) fprintf(stderr,
				    gettext("invalid 'ashift=%" PRIu64 "' "
				    "property: only values between %" PRId32 " "
				    "and %" PRId32 " are allowed.\n"),
				    ashift, ASHIFT_MIN, ASHIFT_MAX);
				return (NULL);
			}
		}
	}
	if (ashift == 0) {
		int sector_size;
		if (check_sector_size_database(path, &sector_size) == B_TRUE)
			ashift = highbit64(sector_size) - 1;
	}
	if (ashift > 0)
		(void) nvlist_add_uint64(vdev, ZPOOL_CONFIG_ASHIFT, ashift);
	return (vdev);
}
typedef struct replication_level {
	const char *zprl_type;
	uint64_t zprl_children;
	uint64_t zprl_parity;
} replication_level_t;
#define	ZPOOL_FUZZ	(16 * 1024 * 1024)
static boolean_t
is_raidz_mirror(replication_level_t *a, replication_level_t *b,
    replication_level_t **raidz, replication_level_t **mirror)
{
	if ((strcmp(a->zprl_type, "raidz") == 0 ||
	    strcmp(a->zprl_type, "draid") == 0) &&
	    strcmp(b->zprl_type, "mirror") == 0) {
		*raidz = a;
		*mirror = b;
		return (B_TRUE);
	}
	return (B_FALSE);
}
static boolean_t
is_raidz_draid(replication_level_t *a, replication_level_t *b)
{
	if ((strcmp(a->zprl_type, "raidz") == 0 ||
	    strcmp(a->zprl_type, "draid") == 0) &&
	    (strcmp(b->zprl_type, "raidz") == 0 ||
	    strcmp(b->zprl_type, "draid") == 0)) {
		return (B_TRUE);
	}
	return (B_FALSE);
}
static replication_level_t *
get_replication(nvlist_t *nvroot, boolean_t fatal)
{
	nvlist_t **top;
	uint_t t, toplevels;
	nvlist_t **child;
	uint_t c, children;
	nvlist_t *nv;
	const char *type;
	replication_level_t lastrep = {0};
	replication_level_t rep;
	replication_level_t *ret;
	replication_level_t *raidz, *mirror;
	boolean_t dontreport;
	ret = safe_malloc(sizeof (replication_level_t));
	verify(nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &top, &toplevels) == 0);
	for (t = 0; t < toplevels; t++) {
		uint64_t is_log = B_FALSE;
		nv = top[t];
		(void) nvlist_lookup_uint64(nv, ZPOOL_CONFIG_IS_LOG, &is_log);
		if (is_log)
			continue;
		verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type) == 0);
		if (strcmp(type, VDEV_TYPE_HOLE) == 0 ||
		    strcmp(type, VDEV_TYPE_INDIRECT) == 0)
			continue;
		if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
		    &child, &children) != 0) {
			rep.zprl_type = type;
			rep.zprl_children = 1;
			rep.zprl_parity = 0;
		} else {
			int64_t vdev_size;
			rep.zprl_type = type;
			rep.zprl_children = 0;
			if (strcmp(type, VDEV_TYPE_RAIDZ) == 0 ||
			    strcmp(type, VDEV_TYPE_DRAID) == 0) {
				verify(nvlist_lookup_uint64(nv,
				    ZPOOL_CONFIG_NPARITY,
				    &rep.zprl_parity) == 0);
				assert(rep.zprl_parity != 0);
			} else {
				rep.zprl_parity = 0;
			}
			type = NULL;
			dontreport = 0;
			vdev_size = -1LL;
			for (c = 0; c < children; c++) {
				nvlist_t *cnv = child[c];
				const char *path;
				struct stat64 statbuf;
				int64_t size = -1LL;
				const char *childtype;
				int fd, err;
				rep.zprl_children++;
				verify(nvlist_lookup_string(cnv,
				    ZPOOL_CONFIG_TYPE, &childtype) == 0);
				while (strcmp(childtype,
				    VDEV_TYPE_REPLACING) == 0 ||
				    strcmp(childtype, VDEV_TYPE_SPARE) == 0) {
					nvlist_t **rchild;
					uint_t rchildren;
					verify(nvlist_lookup_nvlist_array(cnv,
					    ZPOOL_CONFIG_CHILDREN, &rchild,
					    &rchildren) == 0);
					assert(rchildren == 2);
					cnv = rchild[0];
					verify(nvlist_lookup_string(cnv,
					    ZPOOL_CONFIG_TYPE,
					    &childtype) == 0);
				}
				verify(nvlist_lookup_string(cnv,
				    ZPOOL_CONFIG_PATH, &path) == 0);
				if (!dontreport && type != NULL &&
				    strcmp(type, childtype) != 0) {
					if (ret != NULL)
						free(ret);
					ret = NULL;
					if (fatal)
						vdev_error(gettext(
						    "mismatched replication "
						    "level: %s contains both "
						    "files and devices\n"),
						    rep.zprl_type);
					else
						return (NULL);
					dontreport = B_TRUE;
				}
				if ((fd = open(path, O_RDONLY)) >= 0) {
					err = fstat64_blk(fd, &statbuf);
					(void) close(fd);
				} else {
					err = stat64(path, &statbuf);
				}
				if (err != 0 ||
				    statbuf.st_size == 0 ||
				    statbuf.st_size == MAXOFFSET_T)
					continue;
				size = statbuf.st_size;
				if (!dontreport &&
				    (vdev_size != -1LL &&
				    (llabs(size - vdev_size) >
				    ZPOOL_FUZZ))) {
					if (ret != NULL)
						free(ret);
					ret = NULL;
					if (fatal)
						vdev_error(gettext(
						    "%s contains devices of "
						    "different sizes\n"),
						    rep.zprl_type);
					else
						return (NULL);
					dontreport = B_TRUE;
				}
				type = childtype;
				vdev_size = size;
			}
		}
		if (lastrep.zprl_type != NULL) {
			if (is_raidz_mirror(&lastrep, &rep, &raidz, &mirror) ||
			    is_raidz_mirror(&rep, &lastrep, &raidz, &mirror)) {
				if (raidz->zprl_parity !=
				    mirror->zprl_children - 1) {
					if (ret != NULL)
						free(ret);
					ret = NULL;
					if (fatal)
						vdev_error(gettext(
						    "mismatched replication "
						    "level: "
						    "%s and %s vdevs with "
						    "different redundancy, "
						    "%llu vs. %llu (%llu-way) "
						    "are present\n"),
						    raidz->zprl_type,
						    mirror->zprl_type,
						    (u_longlong_t)
						    raidz->zprl_parity,
						    (u_longlong_t)
						    mirror->zprl_children - 1,
						    (u_longlong_t)
						    mirror->zprl_children);
					else
						return (NULL);
				}
			} else if (is_raidz_draid(&lastrep, &rep)) {
				if (lastrep.zprl_parity != rep.zprl_parity) {
					if (ret != NULL)
						free(ret);
					ret = NULL;
					if (fatal)
						vdev_error(gettext(
						    "mismatched replication "
						    "level: %s and %s vdevs "
						    "with different "
						    "redundancy, %llu vs. "
						    "%llu are present\n"),
						    lastrep.zprl_type,
						    rep.zprl_type,
						    (u_longlong_t)
						    lastrep.zprl_parity,
						    (u_longlong_t)
						    rep.zprl_parity);
					else
						return (NULL);
				}
			} else if (strcmp(lastrep.zprl_type, rep.zprl_type) !=
			    0) {
				if (ret != NULL)
					free(ret);
				ret = NULL;
				if (fatal)
					vdev_error(gettext(
					    "mismatched replication level: "
					    "both %s and %s vdevs are "
					    "present\n"),
					    lastrep.zprl_type, rep.zprl_type);
				else
					return (NULL);
			} else if (lastrep.zprl_parity != rep.zprl_parity) {
				if (ret)
					free(ret);
				ret = NULL;
				if (fatal)
					vdev_error(gettext(
					    "mismatched replication level: "
					    "both %llu and %llu device parity "
					    "%s vdevs are present\n"),
					    (u_longlong_t)
					    lastrep.zprl_parity,
					    (u_longlong_t)rep.zprl_parity,
					    rep.zprl_type);
				else
					return (NULL);
			} else if (lastrep.zprl_children != rep.zprl_children) {
				if (ret)
					free(ret);
				ret = NULL;
				if (fatal)
					vdev_error(gettext(
					    "mismatched replication level: "
					    "both %llu-way and %llu-way %s "
					    "vdevs are present\n"),
					    (u_longlong_t)
					    lastrep.zprl_children,
					    (u_longlong_t)
					    rep.zprl_children,
					    rep.zprl_type);
				else
					return (NULL);
			}
		}
		lastrep = rep;
	}
	if (ret != NULL)
		*ret = rep;
	return (ret);
}
static int
check_replication(nvlist_t *config, nvlist_t *newroot)
{
	nvlist_t **child;
	uint_t	children;
	replication_level_t *current = NULL, *new;
	replication_level_t *raidz, *mirror;
	int ret;
	if (config != NULL) {
		nvlist_t *nvroot;
		verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &nvroot) == 0);
		if ((current = get_replication(nvroot, B_FALSE)) == NULL)
			return (0);
	}
	if ((nvlist_lookup_nvlist_array(newroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) || (children == 0)) {
		free(current);
		return (0);
	}
	if (num_logs(newroot) == children) {
		free(current);
		return (0);
	}
	if ((new = get_replication(newroot, B_TRUE)) == NULL) {
		free(current);
		return (-1);
	}
	ret = 0;
	if (current != NULL) {
		if (is_raidz_mirror(current, new, &raidz, &mirror) ||
		    is_raidz_mirror(new, current, &raidz, &mirror)) {
			if (raidz->zprl_parity != mirror->zprl_children - 1) {
				vdev_error(gettext(
				    "mismatched replication level: pool and "
				    "new vdev with different redundancy, %s "
				    "and %s vdevs, %llu vs. %llu (%llu-way)\n"),
				    raidz->zprl_type,
				    mirror->zprl_type,
				    (u_longlong_t)raidz->zprl_parity,
				    (u_longlong_t)mirror->zprl_children - 1,
				    (u_longlong_t)mirror->zprl_children);
				ret = -1;
			}
		} else if (strcmp(current->zprl_type, new->zprl_type) != 0) {
			vdev_error(gettext(
			    "mismatched replication level: pool uses %s "
			    "and new vdev is %s\n"),
			    current->zprl_type, new->zprl_type);
			ret = -1;
		} else if (current->zprl_parity != new->zprl_parity) {
			vdev_error(gettext(
			    "mismatched replication level: pool uses %llu "
			    "device parity and new vdev uses %llu\n"),
			    (u_longlong_t)current->zprl_parity,
			    (u_longlong_t)new->zprl_parity);
			ret = -1;
		} else if (current->zprl_children != new->zprl_children) {
			vdev_error(gettext(
			    "mismatched replication level: pool uses %llu-way "
			    "%s and new vdev uses %llu-way %s\n"),
			    (u_longlong_t)current->zprl_children,
			    current->zprl_type,
			    (u_longlong_t)new->zprl_children,
			    new->zprl_type);
			ret = -1;
		}
	}
	free(new);
	if (current != NULL)
		free(current);
	return (ret);
}
static int
zero_label(const char *path)
{
	const int size = 4096;
	char buf[size];
	int err, fd;
	if ((fd = open(path, O_WRONLY|O_EXCL)) < 0) {
		(void) fprintf(stderr, gettext("cannot open '%s': %s\n"),
		    path, strerror(errno));
		return (-1);
	}
	memset(buf, 0, size);
	err = write(fd, buf, size);
	(void) fdatasync(fd);
	(void) close(fd);
	if (err == -1) {
		(void) fprintf(stderr, gettext("cannot zero first %d bytes "
		    "of '%s': %s\n"), size, path, strerror(errno));
		return (-1);
	}
	if (err != size) {
		(void) fprintf(stderr, gettext("could only zero %d/%d bytes "
		    "of '%s'\n"), err, size, path);
		return (-1);
	}
	return (0);
}
static void
lines_to_stderr(char *lines[], int lines_cnt)
{
	int i;
	for (i = 0; i < lines_cnt; i++) {
		fprintf(stderr, "%s\n", lines[i]);
	}
}
static int
make_disks(zpool_handle_t *zhp, nvlist_t *nv, boolean_t replacing)
{
	nvlist_t **child;
	uint_t c, children;
	const char *type, *path;
	char devpath[MAXPATHLEN];
	char udevpath[MAXPATHLEN];
	uint64_t wholedisk;
	struct stat64 statbuf;
	int is_exclusive = 0;
	int fd;
	int ret;
	verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type) == 0);
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) {
		if (strcmp(type, VDEV_TYPE_DISK) != 0)
			return (0);
		verify(!nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path));
		verify(!nvlist_lookup_uint64(nv, ZPOOL_CONFIG_WHOLE_DISK,
		    &wholedisk));
		if (!wholedisk) {
			if (is_mpath_whole_disk(path))
				update_vdev_config_dev_strs(nv);
			if (!is_spare(NULL, path))
				(void) zero_label(path);
			return (0);
		}
		if (realpath(path, devpath) == NULL) {
			ret = errno;
			(void) fprintf(stderr,
			    gettext("cannot resolve path '%s'\n"), path);
			return (ret);
		}
		strlcpy(udevpath, path, MAXPATHLEN);
		(void) zfs_append_partition(udevpath, MAXPATHLEN);
		fd = open(devpath, O_RDWR|O_EXCL);
		if (fd == -1) {
			if (errno == EBUSY)
				is_exclusive = 1;
#ifdef __FreeBSD__
			if (errno == EPERM)
				is_exclusive = 1;
#endif
		} else {
			(void) close(fd);
		}
		if (!is_exclusive && !is_spare(NULL, udevpath)) {
			char *devnode = strrchr(devpath, '/') + 1;
			char **lines = NULL;
			int lines_cnt = 0;
			ret = strncmp(udevpath, UDISK_ROOT, strlen(UDISK_ROOT));
			if (ret == 0) {
				ret = lstat64(udevpath, &statbuf);
				if (ret == 0 && S_ISLNK(statbuf.st_mode))
					(void) unlink(udevpath);
			}
			if (zpool_prepare_and_label_disk(g_zfs, zhp, devnode,
			    nv, zhp == NULL ? "create" :
			    replacing ? "replace" : "add", &lines,
			    &lines_cnt) != 0) {
				(void) fprintf(stderr,
				    gettext(
				    "Error preparing/labeling disk.\n"));
				if (lines_cnt > 0) {
					(void) fprintf(stderr,
					gettext("zfs_prepare_disk output:\n"));
					lines_to_stderr(lines, lines_cnt);
				}
				libzfs_free_str_array(lines, lines_cnt);
				return (-1);
			}
			libzfs_free_str_array(lines, lines_cnt);
			ret = zpool_label_disk_wait(udevpath, DISK_LABEL_WAIT);
			if (ret) {
				(void) fprintf(stderr,
				    gettext("missing link: %s was "
				    "partitioned but %s is missing\n"),
				    devnode, udevpath);
				return (ret);
			}
			ret = zero_label(udevpath);
			if (ret)
				return (ret);
		}
		verify(nvlist_add_string(nv, ZPOOL_CONFIG_PATH, udevpath) == 0);
		update_vdev_config_dev_strs(nv);
		return (0);
	}
	for (c = 0; c < children; c++)
		if ((ret = make_disks(zhp, child[c], replacing)) != 0)
			return (ret);
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0)
		for (c = 0; c < children; c++)
			if ((ret = make_disks(zhp, child[c], replacing)) != 0)
				return (ret);
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0)
		for (c = 0; c < children; c++)
			if ((ret = make_disks(zhp, child[c], replacing)) != 0)
				return (ret);
	return (0);
}
static boolean_t
is_device_in_use(nvlist_t *config, nvlist_t *nv, boolean_t force,
    boolean_t replacing, boolean_t isspare)
{
	nvlist_t **child;
	uint_t c, children;
	const char *type, *path;
	int ret = 0;
	char buf[MAXPATHLEN];
	uint64_t wholedisk = B_FALSE;
	boolean_t anyinuse = B_FALSE;
	verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type) == 0);
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) {
		verify(!nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path));
		if (strcmp(type, VDEV_TYPE_DISK) == 0)
			verify(!nvlist_lookup_uint64(nv,
			    ZPOOL_CONFIG_WHOLE_DISK, &wholedisk));
		if (replacing) {
			(void) strlcpy(buf, path, sizeof (buf));
			if (wholedisk) {
				ret = zfs_append_partition(buf,  sizeof (buf));
				if (ret == -1)
					return (-1);
			}
			if (is_spare(config, buf))
				return (B_FALSE);
		}
		if (strcmp(type, VDEV_TYPE_DISK) == 0)
			ret = check_device(path, force, isspare, wholedisk);
		else if (strcmp(type, VDEV_TYPE_FILE) == 0)
			ret = check_file(path, force, isspare);
		return (ret != 0);
	}
	for (c = 0; c < children; c++)
		if (is_device_in_use(config, child[c], force, replacing,
		    B_FALSE))
			anyinuse = B_TRUE;
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0)
		for (c = 0; c < children; c++)
			if (is_device_in_use(config, child[c], force, replacing,
			    B_TRUE))
				anyinuse = B_TRUE;
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0)
		for (c = 0; c < children; c++)
			if (is_device_in_use(config, child[c], force, replacing,
			    B_FALSE))
				anyinuse = B_TRUE;
	return (anyinuse);
}
static int
get_parity(const char *type)
{
	long parity = 0;
	const char *p;
	if (strncmp(type, VDEV_TYPE_RAIDZ, strlen(VDEV_TYPE_RAIDZ)) == 0) {
		p = type + strlen(VDEV_TYPE_RAIDZ);
		if (*p == '\0') {
			return (1);
		} else if (*p == '0') {
			return (0);
		} else {
			char *end;
			errno = 0;
			parity = strtol(p, &end, 10);
			if (errno != 0 || *end != '\0' ||
			    parity < 1 || parity > VDEV_RAIDZ_MAXPARITY) {
				return (0);
			}
		}
	} else if (strncmp(type, VDEV_TYPE_DRAID,
	    strlen(VDEV_TYPE_DRAID)) == 0) {
		p = type + strlen(VDEV_TYPE_DRAID);
		if (*p == '\0' || *p == ':') {
			return (1);
		} else if (*p == '0') {
			return (0);
		} else {
			char *end;
			errno = 0;
			parity = strtol(p, &end, 10);
			if (errno != 0 ||
			    parity < 1 || parity > VDEV_DRAID_MAXPARITY ||
			    (*end != '\0' && *end != ':')) {
				return (0);
			}
		}
	}
	return ((int)parity);
}
static const char *
is_grouping(const char *type, int *mindev, int *maxdev)
{
	int nparity;
	if (strncmp(type, VDEV_TYPE_RAIDZ, strlen(VDEV_TYPE_RAIDZ)) == 0 ||
	    strncmp(type, VDEV_TYPE_DRAID, strlen(VDEV_TYPE_DRAID)) == 0) {
		nparity = get_parity(type);
		if (nparity == 0)
			return (NULL);
		if (mindev != NULL)
			*mindev = nparity + 1;
		if (maxdev != NULL)
			*maxdev = 255;
		if (strncmp(type, VDEV_TYPE_RAIDZ,
		    strlen(VDEV_TYPE_RAIDZ)) == 0) {
			return (VDEV_TYPE_RAIDZ);
		} else {
			return (VDEV_TYPE_DRAID);
		}
	}
	if (maxdev != NULL)
		*maxdev = INT_MAX;
	if (strcmp(type, "mirror") == 0) {
		if (mindev != NULL)
			*mindev = 2;
		return (VDEV_TYPE_MIRROR);
	}
	if (strcmp(type, "spare") == 0) {
		if (mindev != NULL)
			*mindev = 1;
		return (VDEV_TYPE_SPARE);
	}
	if (strcmp(type, "log") == 0) {
		if (mindev != NULL)
			*mindev = 1;
		return (VDEV_TYPE_LOG);
	}
	if (strcmp(type, VDEV_ALLOC_BIAS_SPECIAL) == 0 ||
	    strcmp(type, VDEV_ALLOC_BIAS_DEDUP) == 0) {
		if (mindev != NULL)
			*mindev = 1;
		return (type);
	}
	if (strcmp(type, "cache") == 0) {
		if (mindev != NULL)
			*mindev = 1;
		return (VDEV_TYPE_L2CACHE);
	}
	return (NULL);
}
static int
draid_config_by_type(nvlist_t *nv, const char *type, uint64_t children)
{
	uint64_t nparity = 1;
	uint64_t nspares = 0;
	uint64_t ndata = UINT64_MAX;
	uint64_t ngroups = 1;
	long value;
	if (strncmp(type, VDEV_TYPE_DRAID, strlen(VDEV_TYPE_DRAID)) != 0)
		return (EINVAL);
	nparity = (uint64_t)get_parity(type);
	if (nparity == 0 || nparity > VDEV_DRAID_MAXPARITY) {
		fprintf(stderr,
		    gettext("invalid dRAID parity level %llu; must be "
		    "between 1 and %d\n"), (u_longlong_t)nparity,
		    VDEV_DRAID_MAXPARITY);
		return (EINVAL);
	}
	char *p = (char *)type;
	while ((p = strchr(p, ':')) != NULL) {
		char *end;
		p = p + 1;
		errno = 0;
		if (!isdigit(p[0])) {
			(void) fprintf(stderr, gettext("invalid dRAID "
			    "syntax; expected [:<number><c|d|s>] not '%s'\n"),
			    type);
			return (EINVAL);
		}
		value = strtol(p, &end, 10);
		char suffix = tolower(*end);
		if (errno != 0 ||
		    (suffix != 'c' && suffix != 'd' && suffix != 's')) {
			(void) fprintf(stderr, gettext("invalid dRAID "
			    "syntax; expected [:<number><c|d|s>] not '%s'\n"),
			    type);
			return (EINVAL);
		}
		if (suffix == 'c') {
			if ((uint64_t)value != children) {
				fprintf(stderr,
				    gettext("invalid number of dRAID children; "
				    "%llu required but %llu provided\n"),
				    (u_longlong_t)value,
				    (u_longlong_t)children);
				return (EINVAL);
			}
		} else if (suffix == 'd') {
			ndata = (uint64_t)value;
		} else if (suffix == 's') {
			nspares = (uint64_t)value;
		} else {
			verify(0);  
		}
	}
	if (ndata == UINT64_MAX) {
		if (children > nspares + nparity) {
			ndata = MIN(children - nspares - nparity, 8);
		} else {
			fprintf(stderr, gettext("request number of "
			    "distributed spares %llu and parity level %llu\n"
			    "leaves no disks available for data\n"),
			    (u_longlong_t)nspares, (u_longlong_t)nparity);
			return (EINVAL);
		}
	}
	if (ndata == 0 || (ndata + nparity > children - nspares)) {
		fprintf(stderr, gettext("requested number of dRAID data "
		    "disks per group %llu is too high,\nat most %llu disks "
		    "are available for data\n"), (u_longlong_t)ndata,
		    (u_longlong_t)(children - nspares - nparity));
		return (EINVAL);
	}
	if (nspares > 100 || nspares > (children - (ndata + nparity))) {
		fprintf(stderr,
		    gettext("invalid number of dRAID spares %llu; additional "
		    "disks would be required\n"), (u_longlong_t)nspares);
		return (EINVAL);
	}
	if (children < (ndata + nparity + nspares)) {
		fprintf(stderr, gettext("%llu disks were provided, but at "
		    "least %llu disks are required for this config\n"),
		    (u_longlong_t)children,
		    (u_longlong_t)(ndata + nparity + nspares));
	}
	if (children > VDEV_DRAID_MAX_CHILDREN) {
		fprintf(stderr, gettext("%llu disks were provided, but "
		    "dRAID only supports up to %u disks"),
		    (u_longlong_t)children, VDEV_DRAID_MAX_CHILDREN);
	}
	while (ngroups * (ndata + nparity) % (children - nspares) != 0)
		ngroups++;
	fnvlist_add_uint64(nv, ZPOOL_CONFIG_NPARITY, nparity);
	fnvlist_add_uint64(nv, ZPOOL_CONFIG_DRAID_NDATA, ndata);
	fnvlist_add_uint64(nv, ZPOOL_CONFIG_DRAID_NSPARES, nspares);
	fnvlist_add_uint64(nv, ZPOOL_CONFIG_DRAID_NGROUPS, ngroups);
	return (0);
}
static nvlist_t *
construct_spec(nvlist_t *props, int argc, char **argv)
{
	nvlist_t *nvroot, *nv, **top, **spares, **l2cache;
	int t, toplevels, mindev, maxdev, nspares, nlogs, nl2cache;
	const char *type, *fulltype;
	boolean_t is_log, is_special, is_dedup, is_spare;
	boolean_t seen_logs;
	top = NULL;
	toplevels = 0;
	spares = NULL;
	l2cache = NULL;
	nspares = 0;
	nlogs = 0;
	nl2cache = 0;
	is_log = is_special = is_dedup = is_spare = B_FALSE;
	seen_logs = B_FALSE;
	nvroot = NULL;
	while (argc > 0) {
		fulltype = argv[0];
		nv = NULL;
		if ((type = is_grouping(fulltype, &mindev, &maxdev)) != NULL) {
			nvlist_t **child = NULL;
			int c, children = 0;
			if (strcmp(type, VDEV_TYPE_SPARE) == 0) {
				if (spares != NULL) {
					(void) fprintf(stderr,
					    gettext("invalid vdev "
					    "specification: 'spare' can be "
					    "specified only once\n"));
					goto spec_out;
				}
				is_spare = B_TRUE;
				is_log = is_special = is_dedup = B_FALSE;
			}
			if (strcmp(type, VDEV_TYPE_LOG) == 0) {
				if (seen_logs) {
					(void) fprintf(stderr,
					    gettext("invalid vdev "
					    "specification: 'log' can be "
					    "specified only once\n"));
					goto spec_out;
				}
				seen_logs = B_TRUE;
				is_log = B_TRUE;
				is_special = is_dedup = is_spare = B_FALSE;
				argc--;
				argv++;
				continue;
			}
			if (strcmp(type, VDEV_ALLOC_BIAS_SPECIAL) == 0) {
				is_special = B_TRUE;
				is_log = is_dedup = is_spare = B_FALSE;
				argc--;
				argv++;
				continue;
			}
			if (strcmp(type, VDEV_ALLOC_BIAS_DEDUP) == 0) {
				is_dedup = B_TRUE;
				is_log = is_special = is_spare = B_FALSE;
				argc--;
				argv++;
				continue;
			}
			if (strcmp(type, VDEV_TYPE_L2CACHE) == 0) {
				if (l2cache != NULL) {
					(void) fprintf(stderr,
					    gettext("invalid vdev "
					    "specification: 'cache' can be "
					    "specified only once\n"));
					goto spec_out;
				}
				is_log = is_special = B_FALSE;
				is_dedup = is_spare = B_FALSE;
			}
			if (is_log || is_special || is_dedup) {
				if (strcmp(type, VDEV_TYPE_MIRROR) != 0) {
					(void) fprintf(stderr,
					    gettext("invalid vdev "
					    "specification: unsupported '%s' "
					    "device: %s\n"), is_log ? "log" :
					    "special", type);
					goto spec_out;
				}
				nlogs++;
			}
			for (c = 1; c < argc; c++) {
				if (is_grouping(argv[c], NULL, NULL) != NULL)
					break;
				children++;
				child = realloc(child,
				    children * sizeof (nvlist_t *));
				if (child == NULL)
					zpool_no_memory();
				if ((nv = make_leaf_vdev(props, argv[c],
				    !(is_log || is_special || is_dedup ||
				    is_spare))) == NULL) {
					for (c = 0; c < children - 1; c++)
						nvlist_free(child[c]);
					free(child);
					goto spec_out;
				}
				child[children - 1] = nv;
			}
			if (children < mindev) {
				(void) fprintf(stderr, gettext("invalid vdev "
				    "specification: %s requires at least %d "
				    "devices\n"), argv[0], mindev);
				for (c = 0; c < children; c++)
					nvlist_free(child[c]);
				free(child);
				goto spec_out;
			}
			if (children > maxdev) {
				(void) fprintf(stderr, gettext("invalid vdev "
				    "specification: %s supports no more than "
				    "%d devices\n"), argv[0], maxdev);
				for (c = 0; c < children; c++)
					nvlist_free(child[c]);
				free(child);
				goto spec_out;
			}
			argc -= c;
			argv += c;
			if (strcmp(type, VDEV_TYPE_SPARE) == 0) {
				spares = child;
				nspares = children;
				continue;
			} else if (strcmp(type, VDEV_TYPE_L2CACHE) == 0) {
				l2cache = child;
				nl2cache = children;
				continue;
			} else {
				verify(nvlist_alloc(&nv, NV_UNIQUE_NAME,
				    0) == 0);
				verify(nvlist_add_string(nv, ZPOOL_CONFIG_TYPE,
				    type) == 0);
				verify(nvlist_add_uint64(nv,
				    ZPOOL_CONFIG_IS_LOG, is_log) == 0);
				if (is_log) {
					verify(nvlist_add_string(nv,
					    ZPOOL_CONFIG_ALLOCATION_BIAS,
					    VDEV_ALLOC_BIAS_LOG) == 0);
				}
				if (is_special) {
					verify(nvlist_add_string(nv,
					    ZPOOL_CONFIG_ALLOCATION_BIAS,
					    VDEV_ALLOC_BIAS_SPECIAL) == 0);
				}
				if (is_dedup) {
					verify(nvlist_add_string(nv,
					    ZPOOL_CONFIG_ALLOCATION_BIAS,
					    VDEV_ALLOC_BIAS_DEDUP) == 0);
				}
				if (strcmp(type, VDEV_TYPE_RAIDZ) == 0) {
					verify(nvlist_add_uint64(nv,
					    ZPOOL_CONFIG_NPARITY,
					    mindev - 1) == 0);
				}
				if (strcmp(type, VDEV_TYPE_DRAID) == 0) {
					if (draid_config_by_type(nv,
					    fulltype, children) != 0) {
						for (c = 0; c < children; c++)
							nvlist_free(child[c]);
						free(child);
						goto spec_out;
					}
				}
				verify(nvlist_add_nvlist_array(nv,
				    ZPOOL_CONFIG_CHILDREN,
				    (const nvlist_t **)child, children) == 0);
				for (c = 0; c < children; c++)
					nvlist_free(child[c]);
				free(child);
			}
		} else {
			if ((nv = make_leaf_vdev(props, argv[0], !(is_log ||
			    is_special || is_dedup || is_spare))) == NULL)
				goto spec_out;
			verify(nvlist_add_uint64(nv,
			    ZPOOL_CONFIG_IS_LOG, is_log) == 0);
			if (is_log) {
				verify(nvlist_add_string(nv,
				    ZPOOL_CONFIG_ALLOCATION_BIAS,
				    VDEV_ALLOC_BIAS_LOG) == 0);
				nlogs++;
			}
			if (is_special) {
				verify(nvlist_add_string(nv,
				    ZPOOL_CONFIG_ALLOCATION_BIAS,
				    VDEV_ALLOC_BIAS_SPECIAL) == 0);
			}
			if (is_dedup) {
				verify(nvlist_add_string(nv,
				    ZPOOL_CONFIG_ALLOCATION_BIAS,
				    VDEV_ALLOC_BIAS_DEDUP) == 0);
			}
			argc--;
			argv++;
		}
		toplevels++;
		top = realloc(top, toplevels * sizeof (nvlist_t *));
		if (top == NULL)
			zpool_no_memory();
		top[toplevels - 1] = nv;
	}
	if (toplevels == 0 && nspares == 0 && nl2cache == 0) {
		(void) fprintf(stderr, gettext("invalid vdev "
		    "specification: at least one toplevel vdev must be "
		    "specified\n"));
		goto spec_out;
	}
	if (seen_logs && nlogs == 0) {
		(void) fprintf(stderr, gettext("invalid vdev specification: "
		    "log requires at least 1 device\n"));
		goto spec_out;
	}
	verify(nvlist_alloc(&nvroot, NV_UNIQUE_NAME, 0) == 0);
	verify(nvlist_add_string(nvroot, ZPOOL_CONFIG_TYPE,
	    VDEV_TYPE_ROOT) == 0);
	verify(nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    (const nvlist_t **)top, toplevels) == 0);
	if (nspares != 0)
		verify(nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    (const nvlist_t **)spares, nspares) == 0);
	if (nl2cache != 0)
		verify(nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_L2CACHE,
		    (const nvlist_t **)l2cache, nl2cache) == 0);
spec_out:
	for (t = 0; t < toplevels; t++)
		nvlist_free(top[t]);
	for (t = 0; t < nspares; t++)
		nvlist_free(spares[t]);
	for (t = 0; t < nl2cache; t++)
		nvlist_free(l2cache[t]);
	free(spares);
	free(l2cache);
	free(top);
	return (nvroot);
}
nvlist_t *
split_mirror_vdev(zpool_handle_t *zhp, char *newname, nvlist_t *props,
    splitflags_t flags, int argc, char **argv)
{
	nvlist_t *newroot = NULL, **child;
	uint_t c, children;
	if (argc > 0) {
		if ((newroot = construct_spec(props, argc, argv)) == NULL) {
			(void) fprintf(stderr, gettext("Unable to build a "
			    "pool from the specified devices\n"));
			return (NULL);
		}
		if (!flags.dryrun && make_disks(zhp, newroot, B_FALSE) != 0) {
			nvlist_free(newroot);
			return (NULL);
		}
		verify(nvlist_lookup_nvlist_array(newroot,
		    ZPOOL_CONFIG_CHILDREN, &child, &children) == 0);
		for (c = 0; c < children; c++) {
			const char *path;
			const char *type;
			int min, max;
			verify(nvlist_lookup_string(child[c],
			    ZPOOL_CONFIG_PATH, &path) == 0);
			if ((type = is_grouping(path, &min, &max)) != NULL) {
				(void) fprintf(stderr, gettext("Cannot use "
				    "'%s' as a device for splitting\n"), type);
				nvlist_free(newroot);
				return (NULL);
			}
		}
	}
	if (zpool_vdev_split(zhp, newname, &newroot, props, flags) != 0) {
		nvlist_free(newroot);
		return (NULL);
	}
	return (newroot);
}
static int
num_normal_vdevs(nvlist_t *nvroot)
{
	nvlist_t **top;
	uint_t t, toplevels, normal = 0;
	verify(nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &top, &toplevels) == 0);
	for (t = 0; t < toplevels; t++) {
		uint64_t log = B_FALSE;
		(void) nvlist_lookup_uint64(top[t], ZPOOL_CONFIG_IS_LOG, &log);
		if (log)
			continue;
		if (nvlist_exists(top[t], ZPOOL_CONFIG_ALLOCATION_BIAS))
			continue;
		normal++;
	}
	return (normal);
}
nvlist_t *
make_root_vdev(zpool_handle_t *zhp, nvlist_t *props, int force, int check_rep,
    boolean_t replacing, boolean_t dryrun, int argc, char **argv)
{
	nvlist_t *newroot;
	nvlist_t *poolconfig = NULL;
	is_force = force;
	if ((newroot = construct_spec(props, argc, argv)) == NULL)
		return (NULL);
	if (zhp && ((poolconfig = zpool_get_config(zhp, NULL)) == NULL)) {
		nvlist_free(newroot);
		return (NULL);
	}
	if (is_device_in_use(poolconfig, newroot, force, replacing, B_FALSE)) {
		nvlist_free(newroot);
		return (NULL);
	}
	if (check_rep && check_replication(poolconfig, newroot) != 0) {
		nvlist_free(newroot);
		return (NULL);
	}
	if (poolconfig == NULL && num_normal_vdevs(newroot) == 0) {
		vdev_error(gettext("at least one general top-level vdev must "
		    "be specified\n"));
		nvlist_free(newroot);
		return (NULL);
	}
	if (!dryrun && make_disks(zhp, newroot, replacing) != 0) {
		nvlist_free(newroot);
		return (NULL);
	}
	return (newroot);
}
