
 

#include <linux/capability.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/smp.h>
#include <linux/efi.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/ucs2_string.h>

#include "internal.h"

MODULE_IMPORT_NS(EFIVAR);

static bool
validate_device_path(efi_char16_t *var_name, int match, u8 *buffer,
		     unsigned long len)
{
	struct efi_generic_dev_path *node;
	int offset = 0;

	node = (struct efi_generic_dev_path *)buffer;

	if (len < sizeof(*node))
		return false;

	while (offset <= len - sizeof(*node) &&
	       node->length >= sizeof(*node) &&
		node->length <= len - offset) {
		offset += node->length;

		if ((node->type == EFI_DEV_END_PATH ||
		     node->type == EFI_DEV_END_PATH2) &&
		    node->sub_type == EFI_DEV_END_ENTIRE)
			return true;

		node = (struct efi_generic_dev_path *)(buffer + offset);
	}

	 
	return false;
}

static bool
validate_boot_order(efi_char16_t *var_name, int match, u8 *buffer,
		    unsigned long len)
{
	 
	if ((len % 2) != 0)
		return false;

	return true;
}

static bool
validate_load_option(efi_char16_t *var_name, int match, u8 *buffer,
		     unsigned long len)
{
	u16 filepathlength;
	int i, desclength = 0, namelen;

	namelen = ucs2_strnlen(var_name, EFI_VAR_NAME_LEN);

	 
	for (i = match; i < match+4; i++) {
		if (var_name[i] > 127 ||
		    hex_to_bin(var_name[i] & 0xff) < 0)
			return true;
	}

	 
	if (namelen > match + 4)
		return false;

	 
	if (len < 8)
		return false;

	filepathlength = buffer[4] | buffer[5] << 8;

	 
	desclength = ucs2_strsize((efi_char16_t *)(buffer + 6), len - 6) + 2;

	 
	if (!desclength)
		return false;

	 
	if ((desclength + filepathlength + 6) > len)
		return false;

	 
	return validate_device_path(var_name, match, buffer + desclength + 6,
				    filepathlength);
}

static bool
validate_uint16(efi_char16_t *var_name, int match, u8 *buffer,
		unsigned long len)
{
	 
	if (len != 2)
		return false;

	return true;
}

static bool
validate_ascii_string(efi_char16_t *var_name, int match, u8 *buffer,
		      unsigned long len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (buffer[i] > 127)
			return false;

		if (buffer[i] == 0)
			return true;
	}

	return false;
}

struct variable_validate {
	efi_guid_t vendor;
	char *name;
	bool (*validate)(efi_char16_t *var_name, int match, u8 *data,
			 unsigned long len);
};

 
static const struct variable_validate variable_validate[] = {
	{ EFI_GLOBAL_VARIABLE_GUID, "BootNext", validate_uint16 },
	{ EFI_GLOBAL_VARIABLE_GUID, "BootOrder", validate_boot_order },
	{ EFI_GLOBAL_VARIABLE_GUID, "Boot*", validate_load_option },
	{ EFI_GLOBAL_VARIABLE_GUID, "DriverOrder", validate_boot_order },
	{ EFI_GLOBAL_VARIABLE_GUID, "Driver*", validate_load_option },
	{ EFI_GLOBAL_VARIABLE_GUID, "ConIn", validate_device_path },
	{ EFI_GLOBAL_VARIABLE_GUID, "ConInDev", validate_device_path },
	{ EFI_GLOBAL_VARIABLE_GUID, "ConOut", validate_device_path },
	{ EFI_GLOBAL_VARIABLE_GUID, "ConOutDev", validate_device_path },
	{ EFI_GLOBAL_VARIABLE_GUID, "ErrOut", validate_device_path },
	{ EFI_GLOBAL_VARIABLE_GUID, "ErrOutDev", validate_device_path },
	{ EFI_GLOBAL_VARIABLE_GUID, "Lang", validate_ascii_string },
	{ EFI_GLOBAL_VARIABLE_GUID, "OsIndications", NULL },
	{ EFI_GLOBAL_VARIABLE_GUID, "PlatformLang", validate_ascii_string },
	{ EFI_GLOBAL_VARIABLE_GUID, "Timeout", validate_uint16 },
	{ LINUX_EFI_CRASH_GUID, "*", NULL },
	{ NULL_GUID, "", NULL },
};

 
static bool
variable_matches(const char *var_name, size_t len, const char *match_name,
		 int *match)
{
	for (*match = 0; ; (*match)++) {
		char c = match_name[*match];

		switch (c) {
		case '*':
			 
			return true;

		case '\0':
			 
			return (*match == len);

		default:
			 
			if (*match < len && c == var_name[*match])
				continue;
			return false;
		}
	}
}

bool
efivar_validate(efi_guid_t vendor, efi_char16_t *var_name, u8 *data,
		unsigned long data_size)
{
	int i;
	unsigned long utf8_size;
	u8 *utf8_name;

	utf8_size = ucs2_utf8size(var_name);
	utf8_name = kmalloc(utf8_size + 1, GFP_KERNEL);
	if (!utf8_name)
		return false;

	ucs2_as_utf8(utf8_name, var_name, utf8_size);
	utf8_name[utf8_size] = '\0';

	for (i = 0; variable_validate[i].name[0] != '\0'; i++) {
		const char *name = variable_validate[i].name;
		int match = 0;

		if (efi_guidcmp(vendor, variable_validate[i].vendor))
			continue;

		if (variable_matches(utf8_name, utf8_size+1, name, &match)) {
			if (variable_validate[i].validate == NULL)
				break;
			kfree(utf8_name);
			return variable_validate[i].validate(var_name, match,
							     data, data_size);
		}
	}
	kfree(utf8_name);
	return true;
}

bool
efivar_variable_is_removable(efi_guid_t vendor, const char *var_name,
			     size_t len)
{
	int i;
	bool found = false;
	int match = 0;

	 
	for (i = 0; variable_validate[i].name[0] != '\0'; i++) {
		if (efi_guidcmp(variable_validate[i].vendor, vendor))
			continue;

		if (variable_matches(var_name, len,
				     variable_validate[i].name, &match)) {
			found = true;
			break;
		}
	}

	 
	return found;
}

static bool variable_is_present(efi_char16_t *variable_name, efi_guid_t *vendor,
				struct list_head *head)
{
	struct efivar_entry *entry, *n;
	unsigned long strsize1, strsize2;
	bool found = false;

	strsize1 = ucs2_strsize(variable_name, 1024);
	list_for_each_entry_safe(entry, n, head, list) {
		strsize2 = ucs2_strsize(entry->var.VariableName, 1024);
		if (strsize1 == strsize2 &&
			!memcmp(variable_name, &(entry->var.VariableName),
				strsize2) &&
			!efi_guidcmp(entry->var.VendorGuid,
				*vendor)) {
			found = true;
			break;
		}
	}
	return found;
}

 
static unsigned long var_name_strnsize(efi_char16_t *variable_name,
				       unsigned long variable_name_size)
{
	unsigned long len;
	efi_char16_t c;

	 
	for (len = 2; len <= variable_name_size; len += sizeof(c)) {
		c = variable_name[(len / sizeof(c)) - 1];
		if (!c)
			break;
	}

	return min(len, variable_name_size);
}

 
static void dup_variable_bug(efi_char16_t *str16, efi_guid_t *vendor_guid,
			     unsigned long len16)
{
	size_t i, len8 = len16 / sizeof(efi_char16_t);
	char *str8;

	str8 = kzalloc(len8, GFP_KERNEL);
	if (!str8)
		return;

	for (i = 0; i < len8; i++)
		str8[i] = str16[i];

	printk(KERN_WARNING "efivars: duplicate variable: %s-%pUl\n",
	       str8, vendor_guid);
	kfree(str8);
}

 
int efivar_init(int (*func)(efi_char16_t *, efi_guid_t, unsigned long, void *),
		void *data, bool duplicates, struct list_head *head)
{
	unsigned long variable_name_size = 1024;
	efi_char16_t *variable_name;
	efi_status_t status;
	efi_guid_t vendor_guid;
	int err = 0;

	variable_name = kzalloc(variable_name_size, GFP_KERNEL);
	if (!variable_name) {
		printk(KERN_ERR "efivars: Memory allocation failed.\n");
		return -ENOMEM;
	}

	err = efivar_lock();
	if (err)
		goto free;

	 

	do {
		variable_name_size = 1024;

		status = efivar_get_next_variable(&variable_name_size,
						  variable_name,
						  &vendor_guid);
		switch (status) {
		case EFI_SUCCESS:
			variable_name_size = var_name_strnsize(variable_name,
							       variable_name_size);

			 
			if (duplicates &&
			    variable_is_present(variable_name, &vendor_guid,
						head)) {
				dup_variable_bug(variable_name, &vendor_guid,
						 variable_name_size);
				status = EFI_NOT_FOUND;
			} else {
				err = func(variable_name, vendor_guid,
					   variable_name_size, data);
				if (err)
					status = EFI_NOT_FOUND;
			}
			break;
		case EFI_UNSUPPORTED:
			err = -EOPNOTSUPP;
			status = EFI_NOT_FOUND;
			break;
		case EFI_NOT_FOUND:
			break;
		default:
			printk(KERN_WARNING "efivars: get_next_variable: status=%lx\n",
				status);
			status = EFI_NOT_FOUND;
			break;
		}

	} while (status != EFI_NOT_FOUND);

	efivar_unlock();
free:
	kfree(variable_name);

	return err;
}

 
int efivar_entry_add(struct efivar_entry *entry, struct list_head *head)
{
	int err;

	err = efivar_lock();
	if (err)
		return err;
	list_add(&entry->list, head);
	efivar_unlock();

	return 0;
}

 
void __efivar_entry_add(struct efivar_entry *entry, struct list_head *head)
{
	list_add(&entry->list, head);
}

 
void efivar_entry_remove(struct efivar_entry *entry)
{
	list_del(&entry->list);
}

 
static void efivar_entry_list_del_unlock(struct efivar_entry *entry)
{
	list_del(&entry->list);
	efivar_unlock();
}

 
int efivar_entry_delete(struct efivar_entry *entry)
{
	efi_status_t status;
	int err;

	err = efivar_lock();
	if (err)
		return err;

	status = efivar_set_variable_locked(entry->var.VariableName,
					    &entry->var.VendorGuid,
					    0, 0, NULL, false);
	if (!(status == EFI_SUCCESS || status == EFI_NOT_FOUND)) {
		efivar_unlock();
		return efi_status_to_err(status);
	}

	efivar_entry_list_del_unlock(entry);
	return 0;
}

 
int efivar_entry_size(struct efivar_entry *entry, unsigned long *size)
{
	efi_status_t status;
	int err;

	*size = 0;

	err = efivar_lock();
	if (err)
		return err;

	status = efivar_get_variable(entry->var.VariableName,
				     &entry->var.VendorGuid, NULL, size, NULL);
	efivar_unlock();

	if (status != EFI_BUFFER_TOO_SMALL)
		return efi_status_to_err(status);

	return 0;
}

 
int __efivar_entry_get(struct efivar_entry *entry, u32 *attributes,
		       unsigned long *size, void *data)
{
	efi_status_t status;

	status = efivar_get_variable(entry->var.VariableName,
				     &entry->var.VendorGuid,
				     attributes, size, data);

	return efi_status_to_err(status);
}

 
int efivar_entry_get(struct efivar_entry *entry, u32 *attributes,
		     unsigned long *size, void *data)
{
	int err;

	err = efivar_lock();
	if (err)
		return err;
	err = __efivar_entry_get(entry, attributes, size, data);
	efivar_unlock();

	return 0;
}

 
int efivar_entry_set_get_size(struct efivar_entry *entry, u32 attributes,
			      unsigned long *size, void *data, bool *set)
{
	efi_char16_t *name = entry->var.VariableName;
	efi_guid_t *vendor = &entry->var.VendorGuid;
	efi_status_t status;
	int err;

	*set = false;

	if (efivar_validate(*vendor, name, data, *size) == false)
		return -EINVAL;

	 
	err = efivar_lock();
	if (err)
		return err;

	status = efivar_set_variable_locked(name, vendor, attributes, *size,
					    data, false);
	if (status != EFI_SUCCESS) {
		err = efi_status_to_err(status);
		goto out;
	}

	*set = true;

	 
	*size = 0;
	status = efivar_get_variable(entry->var.VariableName,
				    &entry->var.VendorGuid,
				    NULL, size, NULL);

	if (status == EFI_NOT_FOUND)
		efivar_entry_list_del_unlock(entry);
	else
		efivar_unlock();

	if (status && status != EFI_BUFFER_TOO_SMALL)
		return efi_status_to_err(status);

	return 0;

out:
	efivar_unlock();
	return err;

}

 
int efivar_entry_iter(int (*func)(struct efivar_entry *, void *),
		      struct list_head *head, void *data)
{
	struct efivar_entry *entry, *n;
	int err = 0;

	err = efivar_lock();
	if (err)
		return err;

	list_for_each_entry_safe(entry, n, head, list) {
		err = func(entry, data);
		if (err)
			break;
	}
	efivar_unlock();

	return err;
}
