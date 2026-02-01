
 

#include <linux/efi.h>
#include <asm/efi.h>

#include "efistub.h"

 
#define EFI_RT_VIRTUAL_BASE	SZ_512M

 
#ifndef EFI_RT_VIRTUAL_OFFSET
#define EFI_RT_VIRTUAL_OFFSET	0
#endif

static u64 virtmap_base = EFI_RT_VIRTUAL_BASE;
static bool flat_va_mapping = (EFI_RT_VIRTUAL_OFFSET != 0);

void __weak free_screen_info(struct screen_info *si)
{
}

static struct screen_info *setup_graphics(void)
{
	efi_guid_t gop_proto = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	efi_status_t status;
	unsigned long size;
	void **gop_handle = NULL;
	struct screen_info *si = NULL;

	size = 0;
	status = efi_bs_call(locate_handle, EFI_LOCATE_BY_PROTOCOL,
			     &gop_proto, NULL, &size, gop_handle);
	if (status == EFI_BUFFER_TOO_SMALL) {
		si = alloc_screen_info();
		if (!si)
			return NULL;
		status = efi_setup_gop(si, &gop_proto, size);
		if (status != EFI_SUCCESS) {
			free_screen_info(si);
			return NULL;
		}
	}
	return si;
}

static void install_memreserve_table(void)
{
	struct linux_efi_memreserve *rsv;
	efi_guid_t memreserve_table_guid = LINUX_EFI_MEMRESERVE_TABLE_GUID;
	efi_status_t status;

	status = efi_bs_call(allocate_pool, EFI_LOADER_DATA, sizeof(*rsv),
			     (void **)&rsv);
	if (status != EFI_SUCCESS) {
		efi_err("Failed to allocate memreserve entry!\n");
		return;
	}

	rsv->next = 0;
	rsv->size = 0;
	atomic_set(&rsv->count, 0);

	status = efi_bs_call(install_configuration_table,
			     &memreserve_table_guid, rsv);
	if (status != EFI_SUCCESS)
		efi_err("Failed to install memreserve config table!\n");
}

static u32 get_supported_rt_services(void)
{
	const efi_rt_properties_table_t *rt_prop_table;
	u32 supported = EFI_RT_SUPPORTED_ALL;

	rt_prop_table = get_efi_config_table(EFI_RT_PROPERTIES_TABLE_GUID);
	if (rt_prop_table)
		supported &= rt_prop_table->runtime_services_supported;

	return supported;
}

efi_status_t efi_handle_cmdline(efi_loaded_image_t *image, char **cmdline_ptr)
{
	int cmdline_size = 0;
	efi_status_t status;
	char *cmdline;

	 
	cmdline = efi_convert_cmdline(image, &cmdline_size);
	if (!cmdline) {
		efi_err("getting command line via LOADED_IMAGE_PROTOCOL\n");
		return EFI_OUT_OF_RESOURCES;
	}

	if (IS_ENABLED(CONFIG_CMDLINE_EXTEND) ||
	    IS_ENABLED(CONFIG_CMDLINE_FORCE) ||
	    cmdline_size == 0) {
		status = efi_parse_options(CONFIG_CMDLINE);
		if (status != EFI_SUCCESS) {
			efi_err("Failed to parse options\n");
			goto fail_free_cmdline;
		}
	}

	if (!IS_ENABLED(CONFIG_CMDLINE_FORCE) && cmdline_size > 0) {
		status = efi_parse_options(cmdline);
		if (status != EFI_SUCCESS) {
			efi_err("Failed to parse options\n");
			goto fail_free_cmdline;
		}
	}

	*cmdline_ptr = cmdline;
	return EFI_SUCCESS;

fail_free_cmdline:
	efi_bs_call(free_pool, cmdline_ptr);
	return status;
}

efi_status_t efi_stub_common(efi_handle_t handle,
			     efi_loaded_image_t *image,
			     unsigned long image_addr,
			     char *cmdline_ptr)
{
	struct screen_info *si;
	efi_status_t status;

	status = check_platform_features();
	if (status != EFI_SUCCESS)
		return status;

	si = setup_graphics();

	efi_retrieve_tpm2_eventlog();

	 
	efi_enable_reset_attack_mitigation();

	efi_load_initrd(image, ULONG_MAX, efi_get_max_initrd_addr(image_addr),
			NULL);

	efi_random_get_seed();

	 
	efi_novamap |= !(get_supported_rt_services() &
			 EFI_RT_SUPPORTED_SET_VIRTUAL_ADDRESS_MAP);

	install_memreserve_table();

	status = efi_boot_kernel(handle, image, image_addr, cmdline_ptr);

	free_screen_info(si);
	return status;
}

 
efi_status_t efi_alloc_virtmap(efi_memory_desc_t **virtmap,
			       unsigned long *desc_size, u32 *desc_ver)
{
	unsigned long size, mmap_key;
	efi_status_t status;

	 
	size = 0;
	status = efi_bs_call(get_memory_map, &size, NULL, &mmap_key, desc_size,
			     desc_ver);
	if (status != EFI_BUFFER_TOO_SMALL)
		return EFI_LOAD_ERROR;

	return efi_bs_call(allocate_pool, EFI_LOADER_DATA, size,
			   (void **)virtmap);
}

 
void efi_get_virtmap(efi_memory_desc_t *memory_map, unsigned long map_size,
		     unsigned long desc_size, efi_memory_desc_t *runtime_map,
		     int *count)
{
	u64 efi_virt_base = virtmap_base;
	efi_memory_desc_t *in, *out = runtime_map;
	int l;

	*count = 0;

	for (l = 0; l < map_size; l += desc_size) {
		u64 paddr, size;

		in = (void *)memory_map + l;
		if (!(in->attribute & EFI_MEMORY_RUNTIME))
			continue;

		paddr = in->phys_addr;
		size = in->num_pages * EFI_PAGE_SIZE;

		in->virt_addr = in->phys_addr + EFI_RT_VIRTUAL_OFFSET;
		if (efi_novamap) {
			continue;
		}

		 
		if (!flat_va_mapping) {

			paddr = round_down(in->phys_addr, SZ_64K);
			size += in->phys_addr - paddr;

			 
			if (IS_ALIGNED(in->phys_addr, SZ_2M) && size >= SZ_2M)
				efi_virt_base = round_up(efi_virt_base, SZ_2M);
			else
				efi_virt_base = round_up(efi_virt_base, SZ_64K);

			in->virt_addr += efi_virt_base - paddr;
			efi_virt_base += size;
		}

		memcpy(out, in, desc_size);
		out = (void *)out + desc_size;
		++*count;
	}
}
