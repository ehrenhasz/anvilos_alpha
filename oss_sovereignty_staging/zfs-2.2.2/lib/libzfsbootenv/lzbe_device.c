 
 

#include <sys/types.h>
#include <string.h>
#include <libzfs.h>
#include <libzfsbootenv.h>
#include <sys/zfs_bootenv.h>
#include <sys/vdev_impl.h>

 
int
lzbe_set_boot_device(const char *pool, lzbe_flags_t flag, const char *device)
{
	libzfs_handle_t *hdl;
	zpool_handle_t *zphdl;
	nvlist_t *nv;
	char *descriptor;
	uint64_t version;
	int rv = -1;

	if (pool == NULL || *pool == '\0')
		return (rv);

	if ((hdl = libzfs_init()) == NULL)
		return (rv);

	zphdl = zpool_open(hdl, pool);
	if (zphdl == NULL) {
		libzfs_fini(hdl);
		return (rv);
	}

	switch (flag) {
	case lzbe_add:
		rv = zpool_get_bootenv(zphdl, &nv);
		if (rv == 0) {
			 
			rv = nvlist_lookup_uint64(nv, BOOTENV_VERSION,
			    &version);
			if (rv == 0 && version == VB_NVLIST)
				break;

			 
			fnvlist_free(nv);
		}
		zfs_fallthrough;
	case lzbe_replace:
		nv = fnvlist_alloc();
		break;
	default:
		return (rv);
	}

	 
	fnvlist_add_uint64(nv, BOOTENV_VERSION, VB_NVLIST);

	rv = 0;
	 
	if ((device == NULL || *device == '\0')) {
		if (nvlist_exists(nv, OS_BOOTONCE))
			fnvlist_remove(nv, OS_BOOTONCE);
	} else {
		 
		if (strncmp(device, "zfs:", 4) == 0) {
			fnvlist_add_string(nv, OS_BOOTONCE, device);
		} else {
			if (asprintf(&descriptor, "zfs:%s:", device) > 0) {
				fnvlist_add_string(nv, OS_BOOTONCE, descriptor);
				free(descriptor);
			} else
				rv = ENOMEM;
		}
	}
	if (rv == 0)
		rv = zpool_set_bootenv(zphdl, nv);
	if (rv != 0)
		fprintf(stderr, "%s\n", libzfs_error_description(hdl));

	fnvlist_free(nv);
	zpool_close(zphdl);
	libzfs_fini(hdl);
	return (rv);
}

 
int
lzbe_get_boot_device(const char *pool, char **device)
{
	libzfs_handle_t *hdl;
	zpool_handle_t *zphdl;
	nvlist_t *nv;
	const char *val;
	int rv = -1;

	if (pool == NULL || *pool == '\0' || device == NULL)
		return (rv);

	if ((hdl = libzfs_init()) == NULL)
		return (rv);

	zphdl = zpool_open(hdl, pool);
	if (zphdl == NULL) {
		libzfs_fini(hdl);
		return (rv);
	}

	rv = zpool_get_bootenv(zphdl, &nv);
	if (rv == 0) {
		rv = nvlist_lookup_string(nv, OS_BOOTONCE, &val);
		if (rv == 0) {
			 
			if (strncmp(val, "zfs:", 4) == 0) {
				char *tmp = strdup(val + 4);
				if (tmp != NULL) {
					size_t len = strlen(tmp);

					if (tmp[len - 1] == ':')
						tmp[len - 1] = '\0';
					*device = tmp;
				} else {
					rv = ENOMEM;
				}
			} else {
				rv = EINVAL;
			}
		}
		nvlist_free(nv);
	}

	zpool_close(zphdl);
	libzfs_fini(hdl);
	return (rv);
}
