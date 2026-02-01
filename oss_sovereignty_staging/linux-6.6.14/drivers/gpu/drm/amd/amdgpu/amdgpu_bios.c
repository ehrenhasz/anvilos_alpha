 

#include "amdgpu.h"
#include "atom.h"

#include <linux/device.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/acpi.h>
 

#define AMD_VBIOS_SIGNATURE " 761295520"
#define AMD_VBIOS_SIGNATURE_OFFSET 0x30
#define AMD_VBIOS_SIGNATURE_SIZE sizeof(AMD_VBIOS_SIGNATURE)
#define AMD_VBIOS_SIGNATURE_END (AMD_VBIOS_SIGNATURE_OFFSET + AMD_VBIOS_SIGNATURE_SIZE)
#define AMD_IS_VALID_VBIOS(p) ((p)[0] == 0x55 && (p)[1] == 0xAA)
#define AMD_VBIOS_LENGTH(p) ((p)[2] << 9)

 
static bool check_atom_bios(uint8_t *bios, size_t size)
{
	uint16_t tmp, bios_header_start;

	if (!bios || size < 0x49) {
		DRM_INFO("vbios mem is null or mem size is wrong\n");
		return false;
	}

	if (!AMD_IS_VALID_VBIOS(bios)) {
		DRM_INFO("BIOS signature incorrect %x %x\n", bios[0], bios[1]);
		return false;
	}

	bios_header_start = bios[0x48] | (bios[0x49] << 8);
	if (!bios_header_start) {
		DRM_INFO("Can't locate bios header\n");
		return false;
	}

	tmp = bios_header_start + 4;
	if (size < tmp) {
		DRM_INFO("BIOS header is broken\n");
		return false;
	}

	if (!memcmp(bios + tmp, "ATOM", 4) ||
	    !memcmp(bios + tmp, "MOTA", 4)) {
		DRM_DEBUG("ATOMBIOS detected\n");
		return true;
	}

	return false;
}

 
static bool igp_read_bios_from_vram(struct amdgpu_device *adev)
{
	uint8_t __iomem *bios;
	resource_size_t vram_base;
	resource_size_t size = 256 * 1024;  

	if (!(adev->flags & AMD_IS_APU))
		if (amdgpu_device_need_post(adev))
			return false;

	 
	if (pci_resource_len(adev->pdev, 0) == 0)
		return false;

	adev->bios = NULL;
	vram_base = pci_resource_start(adev->pdev, 0);
	bios = ioremap_wc(vram_base, size);
	if (!bios)
		return false;

	adev->bios = kmalloc(size, GFP_KERNEL);
	if (!adev->bios) {
		iounmap(bios);
		return false;
	}
	adev->bios_size = size;
	memcpy_fromio(adev->bios, bios, size);
	iounmap(bios);

	if (!check_atom_bios(adev->bios, size)) {
		kfree(adev->bios);
		return false;
	}

	return true;
}

bool amdgpu_read_bios(struct amdgpu_device *adev)
{
	uint8_t __iomem *bios;
	size_t size;

	adev->bios = NULL;
	 
	bios = pci_map_rom(adev->pdev, &size);
	if (!bios)
		return false;

	adev->bios = kzalloc(size, GFP_KERNEL);
	if (adev->bios == NULL) {
		pci_unmap_rom(adev->pdev, bios);
		return false;
	}
	adev->bios_size = size;
	memcpy_fromio(adev->bios, bios, size);
	pci_unmap_rom(adev->pdev, bios);

	if (!check_atom_bios(adev->bios, size)) {
		kfree(adev->bios);
		return false;
	}

	return true;
}

static bool amdgpu_read_bios_from_rom(struct amdgpu_device *adev)
{
	u8 header[AMD_VBIOS_SIGNATURE_END+1] = {0};
	int len;

	if (!adev->asic_funcs || !adev->asic_funcs->read_bios_from_rom)
		return false;

	 
	if (amdgpu_asic_read_bios_from_rom(adev, &header[0], sizeof(header)) == false)
		return false;
	header[AMD_VBIOS_SIGNATURE_END] = 0;

	if ((!AMD_IS_VALID_VBIOS(header)) ||
		memcmp((char *)&header[AMD_VBIOS_SIGNATURE_OFFSET],
		       AMD_VBIOS_SIGNATURE,
		       strlen(AMD_VBIOS_SIGNATURE)) != 0)
		return false;

	 
	len = AMD_VBIOS_LENGTH(header);
	len = ALIGN(len, 4);
	adev->bios = kmalloc(len, GFP_KERNEL);
	if (!adev->bios) {
		DRM_ERROR("no memory to allocate for BIOS\n");
		return false;
	}
	adev->bios_size = len;

	 
	amdgpu_asic_read_bios_from_rom(adev, adev->bios, len);

	if (!check_atom_bios(adev->bios, len)) {
		kfree(adev->bios);
		return false;
	}

	return true;
}

static bool amdgpu_read_platform_bios(struct amdgpu_device *adev)
{
	phys_addr_t rom = adev->pdev->rom;
	size_t romlen = adev->pdev->romlen;
	void __iomem *bios;

	adev->bios = NULL;

	if (!rom || romlen == 0)
		return false;

	adev->bios = kzalloc(romlen, GFP_KERNEL);
	if (!adev->bios)
		return false;

	bios = ioremap(rom, romlen);
	if (!bios)
		goto free_bios;

	memcpy_fromio(adev->bios, bios, romlen);
	iounmap(bios);

	if (!check_atom_bios(adev->bios, romlen))
		goto free_bios;

	adev->bios_size = romlen;

	return true;
free_bios:
	kfree(adev->bios);
	return false;
}

#ifdef CONFIG_ACPI
 
 
#define ATRM_BIOS_PAGE 4096
 
static int amdgpu_atrm_call(acpi_handle atrm_handle, uint8_t *bios,
			    int offset, int len)
{
	acpi_status status;
	union acpi_object atrm_arg_elements[2], *obj;
	struct acpi_object_list atrm_arg;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL};

	atrm_arg.count = 2;
	atrm_arg.pointer = &atrm_arg_elements[0];

	atrm_arg_elements[0].type = ACPI_TYPE_INTEGER;
	atrm_arg_elements[0].integer.value = offset;

	atrm_arg_elements[1].type = ACPI_TYPE_INTEGER;
	atrm_arg_elements[1].integer.value = len;

	status = acpi_evaluate_object(atrm_handle, NULL, &atrm_arg, &buffer);
	if (ACPI_FAILURE(status)) {
		DRM_ERROR("failed to evaluate ATRM got %s\n", acpi_format_exception(status));
		return -ENODEV;
	}

	obj = (union acpi_object *)buffer.pointer;
	memcpy(bios+offset, obj->buffer.pointer, obj->buffer.length);
	len = obj->buffer.length;
	kfree(buffer.pointer);
	return len;
}

static bool amdgpu_atrm_get_bios(struct amdgpu_device *adev)
{
	int ret;
	int size = 256 * 1024;
	int i;
	struct pci_dev *pdev = NULL;
	acpi_handle dhandle, atrm_handle;
	acpi_status status;
	bool found = false;

	 
	if (adev->flags & AMD_IS_APU)
		return false;

	 
	if (dev_is_removable(&adev->pdev->dev))
		return false;

	while ((pdev = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, pdev)) != NULL) {
		dhandle = ACPI_HANDLE(&pdev->dev);
		if (!dhandle)
			continue;

		status = acpi_get_handle(dhandle, "ATRM", &atrm_handle);
		if (ACPI_SUCCESS(status)) {
			found = true;
			break;
		}
	}

	if (!found) {
		while ((pdev = pci_get_class(PCI_CLASS_DISPLAY_OTHER << 8, pdev)) != NULL) {
			dhandle = ACPI_HANDLE(&pdev->dev);
			if (!dhandle)
				continue;

			status = acpi_get_handle(dhandle, "ATRM", &atrm_handle);
			if (ACPI_SUCCESS(status)) {
				found = true;
				break;
			}
		}
	}

	if (!found)
		return false;
	pci_dev_put(pdev);

	adev->bios = kmalloc(size, GFP_KERNEL);
	if (!adev->bios) {
		dev_err(adev->dev, "Unable to allocate bios\n");
		return false;
	}

	for (i = 0; i < size / ATRM_BIOS_PAGE; i++) {
		ret = amdgpu_atrm_call(atrm_handle,
				       adev->bios,
				       (i * ATRM_BIOS_PAGE),
				       ATRM_BIOS_PAGE);
		if (ret < ATRM_BIOS_PAGE)
			break;
	}

	if (!check_atom_bios(adev->bios, size)) {
		kfree(adev->bios);
		return false;
	}
	adev->bios_size = size;
	return true;
}
#else
static inline bool amdgpu_atrm_get_bios(struct amdgpu_device *adev)
{
	return false;
}
#endif

static bool amdgpu_read_disabled_bios(struct amdgpu_device *adev)
{
	if (adev->flags & AMD_IS_APU)
		return igp_read_bios_from_vram(adev);
	else
		return (!adev->asic_funcs || !adev->asic_funcs->read_disabled_bios) ?
			false : amdgpu_asic_read_disabled_bios(adev);
}

#ifdef CONFIG_ACPI
static bool amdgpu_acpi_vfct_bios(struct amdgpu_device *adev)
{
	struct acpi_table_header *hdr;
	acpi_size tbl_size;
	UEFI_ACPI_VFCT *vfct;
	unsigned int offset;

	if (!ACPI_SUCCESS(acpi_get_table("VFCT", 1, &hdr)))
		return false;
	tbl_size = hdr->length;
	if (tbl_size < sizeof(UEFI_ACPI_VFCT)) {
		dev_info(adev->dev, "ACPI VFCT table present but broken (too short #1),skipping\n");
		return false;
	}

	vfct = (UEFI_ACPI_VFCT *)hdr;
	offset = vfct->VBIOSImageOffset;

	while (offset < tbl_size) {
		GOP_VBIOS_CONTENT *vbios = (GOP_VBIOS_CONTENT *)((char *)hdr + offset);
		VFCT_IMAGE_HEADER *vhdr = &vbios->VbiosHeader;

		offset += sizeof(VFCT_IMAGE_HEADER);
		if (offset > tbl_size) {
			dev_info(adev->dev, "ACPI VFCT image header truncated,skipping\n");
			return false;
		}

		offset += vhdr->ImageLength;
		if (offset > tbl_size) {
			dev_info(adev->dev, "ACPI VFCT image truncated,skipping\n");
			return false;
		}

		if (vhdr->ImageLength &&
		    vhdr->PCIBus == adev->pdev->bus->number &&
		    vhdr->PCIDevice == PCI_SLOT(adev->pdev->devfn) &&
		    vhdr->PCIFunction == PCI_FUNC(adev->pdev->devfn) &&
		    vhdr->VendorID == adev->pdev->vendor &&
		    vhdr->DeviceID == adev->pdev->device) {
			adev->bios = kmemdup(&vbios->VbiosContent,
					     vhdr->ImageLength,
					     GFP_KERNEL);

			if (!check_atom_bios(adev->bios, vhdr->ImageLength)) {
				kfree(adev->bios);
				return false;
			}
			adev->bios_size = vhdr->ImageLength;
			return true;
		}
	}

	dev_info(adev->dev, "ACPI VFCT table present but broken (too short #2),skipping\n");
	return false;
}
#else
static inline bool amdgpu_acpi_vfct_bios(struct amdgpu_device *adev)
{
	return false;
}
#endif

bool amdgpu_get_bios(struct amdgpu_device *adev)
{
	if (amdgpu_atrm_get_bios(adev)) {
		dev_info(adev->dev, "Fetched VBIOS from ATRM\n");
		goto success;
	}

	if (amdgpu_acpi_vfct_bios(adev)) {
		dev_info(adev->dev, "Fetched VBIOS from VFCT\n");
		goto success;
	}

	if (igp_read_bios_from_vram(adev)) {
		dev_info(adev->dev, "Fetched VBIOS from VRAM BAR\n");
		goto success;
	}

	if (amdgpu_read_bios(adev)) {
		dev_info(adev->dev, "Fetched VBIOS from ROM BAR\n");
		goto success;
	}

	if (amdgpu_read_bios_from_rom(adev)) {
		dev_info(adev->dev, "Fetched VBIOS from ROM\n");
		goto success;
	}

	if (amdgpu_read_disabled_bios(adev)) {
		dev_info(adev->dev, "Fetched VBIOS from disabled ROM BAR\n");
		goto success;
	}

	if (amdgpu_read_platform_bios(adev)) {
		dev_info(adev->dev, "Fetched VBIOS from platform\n");
		goto success;
	}

	dev_err(adev->dev, "Unable to locate a BIOS ROM\n");
	return false;

success:
	adev->is_atom_fw = adev->asic_type >= CHIP_VEGA10;
	return true;
}

 
bool amdgpu_soc15_read_bios_from_rom(struct amdgpu_device *adev,
				     u8 *bios, u32 length_bytes)
{
	u32 *dw_ptr;
	u32 i, length_dw;
	u32 rom_offset;
	u32 rom_index_offset;
	u32 rom_data_offset;

	if (bios == NULL)
		return false;
	if (length_bytes == 0)
		return false;
	 
	if (adev->flags & AMD_IS_APU)
		return false;
	if (!adev->smuio.funcs ||
	    !adev->smuio.funcs->get_rom_index_offset ||
	    !adev->smuio.funcs->get_rom_data_offset)
		return false;

	dw_ptr = (u32 *)bios;
	length_dw = ALIGN(length_bytes, 4) / 4;

	rom_index_offset =
		adev->smuio.funcs->get_rom_index_offset(adev);
	rom_data_offset =
		adev->smuio.funcs->get_rom_data_offset(adev);

	if (adev->nbio.funcs &&
	    adev->nbio.funcs->get_rom_offset) {
		rom_offset = adev->nbio.funcs->get_rom_offset(adev);
		rom_offset = rom_offset << 17;
	} else {
		rom_offset = 0;
	}

	 
	WREG32(rom_index_offset, rom_offset);
	 
	for (i = 0; i < length_dw; i++)
		dw_ptr[i] = RREG32(rom_data_offset);

	return true;
}
