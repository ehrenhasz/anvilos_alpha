 
#include <nvif/driver.h>
#include <nvif/client.h>

static const struct nvif_driver *
nvif_driver[] = {
#ifdef __KERNEL__
	&nvif_driver_nvkm,
#else
	&nvif_driver_drm,
	&nvif_driver_lib,
	&nvif_driver_null,
#endif
	NULL
};

int
nvif_driver_init(const char *drv, const char *cfg, const char *dbg,
		 const char *name, u64 device, struct nvif_client *client)
{
	int ret = -EINVAL, i;

	for (i = 0; (client->driver = nvif_driver[i]); i++) {
		if (!drv || !strcmp(client->driver->name, drv)) {
			ret = client->driver->init(name, device, cfg, dbg,
						   &client->object.priv);
			if (ret == 0)
				break;
			client->driver->fini(client->object.priv);
		}
	}

	if (ret == 0)
		ret = nvif_client_ctor(client, name, device, client);
	return ret;
}
