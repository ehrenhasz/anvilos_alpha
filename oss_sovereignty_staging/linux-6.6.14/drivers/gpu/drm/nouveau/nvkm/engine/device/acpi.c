 
#include "acpi.h"

#include <core/device.h>
#include <subdev/clk.h>

#ifdef CONFIG_ACPI
static int
nvkm_acpi_ntfy(struct notifier_block *nb, unsigned long val, void *data)
{
	struct nvkm_device *device = container_of(nb, typeof(*device), acpi.nb);
	struct acpi_bus_event *info = data;

	if (!strcmp(info->device_class, "ac_adapter"))
		nvkm_clk_pwrsrc(device);

	return NOTIFY_DONE;
}
#endif

void
nvkm_acpi_fini(struct nvkm_device *device)
{
#ifdef CONFIG_ACPI
	unregister_acpi_notifier(&device->acpi.nb);
#endif
}

void
nvkm_acpi_init(struct nvkm_device *device)
{
#ifdef CONFIG_ACPI
	device->acpi.nb.notifier_call = nvkm_acpi_ntfy;
	register_acpi_notifier(&device->acpi.nb);
#endif
}
