#include <errno.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <zone.h>
#include <sys/stat.h>
#include <sys/efi_partition.h>
#include <sys/systeminfo.h>
#include <sys/zfs_ioctl.h>
#include <sys/vdev_disk.h>
#include <dlfcn.h>
#include <libzutil.h>
#include "zfs_namecheck.h"
#include "zfs_prop.h"
#include "../../libzfs_impl.h"
#include "zfs_comutil.h"
#include "zfeature_common.h"
int
zpool_relabel_disk(libzfs_handle_t *hdl, const char *path, const char *msg)
{
	int fd, error;
	if ((fd = open(path, O_RDWR|O_DIRECT|O_CLOEXEC)) < 0) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "cannot "
		    "relabel '%s': unable to open device: %d"), path, errno);
		return (zfs_error(hdl, EZFS_OPENFAILED, msg));
	}
	error = efi_use_whole_disk(fd);
	(void) fsync(fd);
	(void) ioctl(fd, BLKFLSBUF);
	(void) close(fd);
	if (error && error != VT_ENOSPC) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "cannot "
		    "relabel '%s': unable to read disk capacity"), path);
		return (zfs_error(hdl, EZFS_NOCAP, msg));
	}
	return (0);
}
static int
read_efi_label(nvlist_t *config, diskaddr_t *sb)
{
	const char *path;
	int fd;
	char diskname[MAXPATHLEN];
	int err = -1;
	if (nvlist_lookup_string(config, ZPOOL_CONFIG_PATH, &path) != 0)
		return (err);
	(void) snprintf(diskname, sizeof (diskname), "%s%s", DISK_ROOT,
	    strrchr(path, '/'));
	if ((fd = open(diskname, O_RDONLY|O_DIRECT|O_CLOEXEC)) >= 0) {
		struct dk_gpt *vtoc;
		if ((err = efi_alloc_and_read(fd, &vtoc)) >= 0) {
			if (sb != NULL)
				*sb = vtoc->efi_parts[0].p_start;
			efi_free(vtoc);
		}
		(void) close(fd);
	}
	return (err);
}
static diskaddr_t
find_start_block(nvlist_t *config)
{
	nvlist_t **child;
	uint_t c, children;
	diskaddr_t sb = MAXOFFSET_T;
	uint64_t wholedisk;
	if (nvlist_lookup_nvlist_array(config,
	    ZPOOL_CONFIG_CHILDREN, &child, &children) != 0) {
		if (nvlist_lookup_uint64(config,
		    ZPOOL_CONFIG_WHOLE_DISK,
		    &wholedisk) != 0 || !wholedisk) {
			return (MAXOFFSET_T);
		}
		if (read_efi_label(config, &sb) < 0)
			sb = MAXOFFSET_T;
		return (sb);
	}
	for (c = 0; c < children; c++) {
		sb = find_start_block(child[c]);
		if (sb != MAXOFFSET_T) {
			return (sb);
		}
	}
	return (MAXOFFSET_T);
}
static int
zpool_label_disk_check(char *path)
{
	struct dk_gpt *vtoc;
	int fd, err;
	if ((fd = open(path, O_RDONLY|O_DIRECT|O_CLOEXEC)) < 0)
		return (errno);
	if ((err = efi_alloc_and_read(fd, &vtoc)) != 0) {
		(void) close(fd);
		return (err);
	}
	if (vtoc->efi_flags & EFI_GPT_PRIMARY_CORRUPT) {
		efi_free(vtoc);
		(void) close(fd);
		return (EIDRM);
	}
	efi_free(vtoc);
	(void) close(fd);
	return (0);
}
static void
zpool_label_name(char *label_name, int label_size)
{
	uint64_t id = 0;
	int fd;
	fd = open("/dev/urandom", O_RDONLY|O_CLOEXEC);
	if (fd >= 0) {
		if (read(fd, &id, sizeof (id)) != sizeof (id))
			id = 0;
		close(fd);
	}
	if (id == 0)
		id = (((uint64_t)rand()) << 32) | (uint64_t)rand();
	snprintf(label_name, label_size, "zfs-%016llx", (u_longlong_t)id);
}
int
zpool_label_disk(libzfs_handle_t *hdl, zpool_handle_t *zhp, const char *name)
{
	char path[MAXPATHLEN];
	struct dk_gpt *vtoc;
	int rval, fd;
	size_t resv = EFI_MIN_RESV_SIZE;
	uint64_t slice_size;
	diskaddr_t start_block;
	char errbuf[ERRBUFLEN];
	(void) snprintf(errbuf, sizeof (errbuf),
	    dgettext(TEXT_DOMAIN, "cannot label '%s'"), name);
	if (zhp) {
		nvlist_t *nvroot = fnvlist_lookup_nvlist(zhp->zpool_config,
		    ZPOOL_CONFIG_VDEV_TREE);
		if (zhp->zpool_start_block == 0)
			start_block = find_start_block(nvroot);
		else
			start_block = zhp->zpool_start_block;
		zhp->zpool_start_block = start_block;
	} else {
		start_block = NEW_START_BLOCK;
	}
	(void) snprintf(path, sizeof (path), "%s/%s", DISK_ROOT, name);
	if ((fd = open(path, O_RDWR|O_DIRECT|O_EXCL|O_CLOEXEC)) < 0) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "cannot "
		    "label '%s': unable to open device: %d"), path, errno);
		return (zfs_error(hdl, EZFS_OPENFAILED, errbuf));
	}
	if (efi_alloc_and_init(fd, EFI_NUMPAR, &vtoc) != 0) {
		if (errno == ENOMEM)
			(void) no_memory(hdl);
		(void) close(fd);
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "cannot "
		    "label '%s': unable to read disk capacity"), path);
		return (zfs_error(hdl, EZFS_NOCAP, errbuf));
	}
	slice_size = vtoc->efi_last_u_lba + 1;
	slice_size -= EFI_MIN_RESV_SIZE;
	if (start_block == MAXOFFSET_T)
		start_block = NEW_START_BLOCK;
	slice_size -= start_block;
	slice_size = P2ALIGN(slice_size, PARTITION_END_ALIGNMENT);
	vtoc->efi_parts[0].p_start = start_block;
	vtoc->efi_parts[0].p_size = slice_size;
	vtoc->efi_parts[0].p_tag = V_USR;
	zpool_label_name(vtoc->efi_parts[0].p_name, EFI_PART_NAME_LEN);
	vtoc->efi_parts[8].p_start = slice_size + start_block;
	vtoc->efi_parts[8].p_size = resv;
	vtoc->efi_parts[8].p_tag = V_RESERVED;
	rval = efi_write(fd, vtoc);
	(void) fsync(fd);
	(void) ioctl(fd, BLKFLSBUF);
	if (rval == 0)
		rval = efi_rescan(fd);
	if (rval != 0) {
		(void) close(fd);
		efi_free(vtoc);
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "try using "
		    "parted(8) and then provide a specific slice: %d"), rval);
		return (zfs_error(hdl, EZFS_LABELFAILED, errbuf));
	}
	(void) close(fd);
	efi_free(vtoc);
	(void) snprintf(path, sizeof (path), "%s/%s", DISK_ROOT, name);
	(void) zfs_append_partition(path, MAXPATHLEN);
	rval = zpool_label_disk_wait(path, DISK_LABEL_WAIT);
	if (rval) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "failed to "
		    "detect device partitions on '%s': %d"), path, rval);
		return (zfs_error(hdl, EZFS_LABELFAILED, errbuf));
	}
	(void) snprintf(path, sizeof (path), "%s/%s", DISK_ROOT, name);
	rval = zpool_label_disk_check(path);
	if (rval) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "freshly written "
		    "EFI label on '%s' is damaged.  Ensure\nthis device "
		    "is not in use, and is functioning properly: %d"),
		    path, rval);
		return (zfs_error(hdl, EZFS_LABELFAILED, errbuf));
	}
	return (0);
}
