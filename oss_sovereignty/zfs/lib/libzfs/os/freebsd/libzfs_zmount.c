#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/mntent.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mnttab.h>
#include <sys/errno.h>
#include <libzfs.h>
#include "../../libzfs_impl.h"
static void
build_iovec(struct iovec **iov, int *iovlen, const char *name, void *val,
    size_t len)
{
	int i;
	if (*iovlen < 0)
		return;
	i = *iovlen;
	*iov = realloc(*iov, sizeof (**iov) * (i + 2));
	if (*iov == NULL) {
		*iovlen = -1;
		return;
	}
	(*iov)[i].iov_base = strdup(name);
	(*iov)[i].iov_len = strlen(name) + 1;
	i++;
	(*iov)[i].iov_base = val;
	if (len == (size_t)-1) {
		if (val != NULL)
			len = strlen(val) + 1;
		else
			len = 0;
	}
	(*iov)[i].iov_len = (int)len;
	*iovlen = ++i;
}
int
do_mount(zfs_handle_t *zhp, const char *mntpt, const char *opts, int flags)
{
	struct iovec *iov;
	char *optstr, *p, *tofree;
	int iovlen, rv;
	const char *spec = zfs_get_name(zhp);
	assert(spec != NULL);
	assert(mntpt != NULL);
	assert(opts != NULL);
	tofree = optstr = strdup(opts);
	assert(optstr != NULL);
	iov = NULL;
	iovlen = 0;
	if (strstr(optstr, MNTOPT_REMOUNT) != NULL)
		build_iovec(&iov, &iovlen, "update", NULL, 0);
	if (flags & MS_RDONLY)
		build_iovec(&iov, &iovlen, "ro", NULL, 0);
	build_iovec(&iov, &iovlen, "fstype", __DECONST(char *, MNTTYPE_ZFS),
	    (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", __DECONST(char *, mntpt),
	    (size_t)-1);
	build_iovec(&iov, &iovlen, "from", __DECONST(char *, spec), (size_t)-1);
	while ((p = strsep(&optstr, ",/")) != NULL)
		build_iovec(&iov, &iovlen, p, NULL, (size_t)-1);
	rv = nmount(iov, iovlen, 0);
	free(tofree);
	if (rv < 0)
		return (errno);
	return (rv);
}
int
do_unmount(zfs_handle_t *zhp, const char *mntpt, int flags)
{
	(void) zhp;
	if (unmount(mntpt, flags) < 0)
		return (errno);
	return (0);
}
int
zfs_mount_delegation_check(void)
{
	return (0);
}
void
zpool_disable_datasets_os(zpool_handle_t *zhp, boolean_t force)
{
	(void) zhp, (void) force;
}
void
zpool_disable_volume_os(const char *name)
{
	(void) name;
}
