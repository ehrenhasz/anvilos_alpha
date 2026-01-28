#include <assert.h>
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
#include <paths.h>
#include <sys/stat.h>
#include <sys/disk.h>
#include <sys/mntent.h>
#include <libgeom.h>
#include "zpool_util.h"
#include <sys/zfs_context.h>
int
check_device(const char *name, boolean_t force, boolean_t isspare,
    boolean_t iswholedisk)
{
	(void) iswholedisk;
	char path[MAXPATHLEN];
	if (strncmp(name, _PATH_DEV, sizeof (_PATH_DEV) - 1) != 0)
		snprintf(path, sizeof (path), "%s%s", _PATH_DEV, name);
	else
		strlcpy(path, name, sizeof (path));
	return (check_file(path, force, isspare));
}
boolean_t
check_sector_size_database(char *path, int *sector_size)
{
	(void) path, (void) sector_size;
	return (0);
}
void
after_zpool_upgrade(zpool_handle_t *zhp)
{
	char bootfs[ZPOOL_MAXPROPLEN];
	if (zpool_get_prop(zhp, ZPOOL_PROP_BOOTFS, bootfs,
	    sizeof (bootfs), NULL, B_FALSE) == 0 &&
	    strcmp(bootfs, "-") != 0) {
		(void) printf(gettext("Pool '%s' has the bootfs "
		    "property set, you might need to update\nthe boot "
		    "code. See gptzfsboot(8) and loader.efi(8) for "
		    "details.\n"), zpool_get_name(zhp));
	}
}
int
check_file(const char *file, boolean_t force, boolean_t isspare)
{
	return (check_file_generic(file, force, isspare));
}
