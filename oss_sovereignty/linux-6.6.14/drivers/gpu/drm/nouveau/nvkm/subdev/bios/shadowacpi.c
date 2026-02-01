 
#include "priv.h"

static int
acpi_read_bios(acpi_handle rom_handle, u8 *bios, u32 offset, u32 length)
{
#if defined(CONFIG_ACPI) && defined(CONFIG_X86)
	acpi_status status;
	union acpi_object rom_arg_elements[2], *obj;
	struct acpi_object_list rom_arg;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL};

	rom_arg.count = 2;
	rom_arg.pointer = &rom_arg_elements[0];

	rom_arg_elements[0].type = ACPI_TYPE_INTEGER;
	rom_arg_elements[0].integer.value = offset;

	rom_arg_elements[1].type = ACPI_TYPE_INTEGER;
	rom_arg_elements[1].integer.value = length;

	status = acpi_evaluate_object(rom_handle, NULL, &rom_arg, &buffer);
	if (ACPI_FAILURE(status)) {
		pr_info("failed to evaluate ROM got %s\n",
			acpi_format_exception(status));
		return -ENODEV;
	}
	obj = (union acpi_object *)buffer.pointer;
	length = min(length, obj->buffer.length);
	memcpy(bios+offset, obj->buffer.pointer, length);
	kfree(buffer.pointer);
	return length;
#else
	return -EINVAL;
#endif
}

 
static u32
acpi_read_fast(void *data, u32 offset, u32 length, struct nvkm_bios *bios)
{
	u32 limit = (offset + length + 0xfff) & ~0xfff;
	u32 start = offset & ~0x00000fff;
	u32 fetch = limit - start;

	if (nvbios_extend(bios, limit) >= 0) {
		int ret = acpi_read_bios(data, bios->data, start, fetch);
		if (ret == fetch)
			return fetch;
	}

	return 0;
}

 
static u32
acpi_read_slow(void *data, u32 offset, u32 length, struct nvkm_bios *bios)
{
	u32 limit = (offset + length + 0xfff) & ~0xfff;
	u32 start = offset & ~0xfff;
	u32 fetch = 0;

	if (nvbios_extend(bios, limit) >= 0) {
		while (start + fetch < limit) {
			int ret = acpi_read_bios(data, bios->data,
						 start + fetch, 0x1000);
			if (ret != 0x1000)
				break;
			fetch += 0x1000;
		}
	}

	return fetch;
}

static void *
acpi_init(struct nvkm_bios *bios, const char *name)
{
#if defined(CONFIG_ACPI) && defined(CONFIG_X86)
	acpi_status status;
	acpi_handle dhandle, rom_handle;

	dhandle = ACPI_HANDLE(bios->subdev.device->dev);
	if (!dhandle)
		return ERR_PTR(-ENODEV);

	status = acpi_get_handle(dhandle, "_ROM", &rom_handle);
	if (ACPI_FAILURE(status))
		return ERR_PTR(-ENODEV);

	return rom_handle;
#else
	return ERR_PTR(-ENODEV);
#endif
}

const struct nvbios_source
nvbios_acpi_fast = {
	.name = "ACPI",
	.init = acpi_init,
	.read = acpi_read_fast,
	.rw = false,
	.require_checksum = true,
};

const struct nvbios_source
nvbios_acpi_slow = {
	.name = "ACPI",
	.init = acpi_init,
	.read = acpi_read_slow,
	.rw = false,
};
