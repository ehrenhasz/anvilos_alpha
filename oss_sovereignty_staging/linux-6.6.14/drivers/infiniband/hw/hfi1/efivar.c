
 

#include <linux/string.h>
#include <linux/string_helpers.h>

#include "efivar.h"

 
#define HFI1_EFIVAR_GUID EFI_GUID(0xc50a953e, 0xa8b2, 0x42a6, \
		0xbf, 0x89, 0xd3, 0x33, 0xa6, 0xe9, 0xe6, 0xd4)
 
#define EFI_DATA_SIZE 4096

 
static int read_efi_var(const char *name, unsigned long *size,
			void **return_data)
{
	efi_status_t status;
	efi_char16_t *uni_name;
	efi_guid_t guid;
	unsigned long temp_size;
	void *temp_buffer;
	void *data;
	int i;
	int ret;

	 
	*size = 0;
	*return_data = NULL;

	if (!efi_rt_services_supported(EFI_RT_SUPPORTED_GET_VARIABLE))
		return -EOPNOTSUPP;

	uni_name = kcalloc(strlen(name) + 1, sizeof(efi_char16_t), GFP_KERNEL);
	temp_buffer = kzalloc(EFI_DATA_SIZE, GFP_KERNEL);

	if (!uni_name || !temp_buffer) {
		ret = -ENOMEM;
		goto fail;
	}

	 
	temp_size = EFI_DATA_SIZE;

	 
	for (i = 0; name[i]; i++)
		uni_name[i] = name[i];

	 
	guid = HFI1_EFIVAR_GUID;

	 
	status = efi.get_variable(
			uni_name,
			&guid,
			NULL,
			&temp_size,
			temp_buffer);

	 
	ret = status == EFI_SUCCESS   ? 0 :
	      status == EFI_NOT_FOUND ? -ENOENT :
					-EINVAL;
	if (ret)
		goto fail;

	 
	data = kmemdup(temp_buffer, temp_size, GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	*size = temp_size;
	*return_data = data;

fail:
	kfree(uni_name);
	kfree(temp_buffer);

	return ret;
}

 
int read_hfi1_efi_var(struct hfi1_devdata *dd, const char *kind,
		      unsigned long *size, void **return_data)
{
	char prefix_name[64];
	char name[128];
	int result;

	 
	snprintf(prefix_name, sizeof(prefix_name), "%04x:%02x:%02x.%x",
		 pci_domain_nr(dd->pcidev->bus),
		 dd->pcidev->bus->number,
		 PCI_SLOT(dd->pcidev->devfn),
		 PCI_FUNC(dd->pcidev->devfn));
	snprintf(name, sizeof(name), "%s-%s", prefix_name, kind);
	result = read_efi_var(name, size, return_data);

	 
	if (result) {
		string_upper(prefix_name, prefix_name);
		snprintf(name, sizeof(name), "%s-%s", prefix_name, kind);
		result = read_efi_var(name, size, return_data);
	}

	return result;
}
