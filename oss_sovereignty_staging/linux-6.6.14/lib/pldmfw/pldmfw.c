
 

#include <asm/unaligned.h>
#include <linux/crc32.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pldmfw.h>
#include <linux/slab.h>
#include <linux/uuid.h>

#include "pldmfw_private.h"

 
struct pldmfw_priv {
	struct pldmfw *context;
	const struct firmware *fw;

	 
	size_t offset;

	struct list_head records;
	struct list_head components;

	 
	const struct __pldm_header *header;
	u16 total_header_size;

	 
	u16 component_bitmap_len;
	u16 bitmap_size;

	 
	u16 component_count;
	const u8 *component_start;

	 
	const u8 *record_start;
	u8 record_count;

	 
	u32 header_crc;

	struct pldmfw_record *matching_record;
};

 
static int
pldm_check_fw_space(struct pldmfw_priv *data, size_t offset, size_t length)
{
	size_t expected_size = offset + length;
	struct device *dev = data->context->dev;

	if (data->fw->size < expected_size) {
		dev_dbg(dev, "Firmware file size smaller than expected. Got %zu bytes, needed %zu bytes\n",
			data->fw->size, expected_size);
		return -EFAULT;
	}

	return 0;
}

 
static int
pldm_move_fw_offset(struct pldmfw_priv *data, size_t bytes_to_move)
{
	int err;

	err = pldm_check_fw_space(data, data->offset, bytes_to_move);
	if (err)
		return err;

	data->offset += bytes_to_move;

	return 0;
}

 
static int pldm_parse_header(struct pldmfw_priv *data)
{
	const struct __pldmfw_record_area *record_area;
	struct device *dev = data->context->dev;
	const struct __pldm_header *header;
	size_t header_size;
	int err;

	err = pldm_move_fw_offset(data, sizeof(*header));
	if (err)
		return err;

	header = (const struct __pldm_header *)data->fw->data;
	data->header = header;

	if (!uuid_equal(&header->id, &pldm_firmware_header_id)) {
		dev_dbg(dev, "Invalid package header identifier. Expected UUID %pUB, but got %pUB\n",
			&pldm_firmware_header_id, &header->id);
		return -EINVAL;
	}

	if (header->revision != PACKAGE_HEADER_FORMAT_REVISION) {
		dev_dbg(dev, "Invalid package header revision. Expected revision %u but got %u\n",
			PACKAGE_HEADER_FORMAT_REVISION, header->revision);
		return -EOPNOTSUPP;
	}

	data->total_header_size = get_unaligned_le16(&header->size);
	header_size = data->total_header_size - sizeof(*header);

	err = pldm_check_fw_space(data, data->offset, header_size);
	if (err)
		return err;

	data->component_bitmap_len =
		get_unaligned_le16(&header->component_bitmap_len);

	if (data->component_bitmap_len % 8 != 0) {
		dev_dbg(dev, "Invalid component bitmap length. The length is %u, which is not a multiple of 8\n",
			data->component_bitmap_len);
		return -EINVAL;
	}

	data->bitmap_size = data->component_bitmap_len / 8;

	err = pldm_move_fw_offset(data, header->version_len);
	if (err)
		return err;

	 
	record_area = (const struct __pldmfw_record_area *)(data->fw->data +
							 data->offset);

	err = pldm_move_fw_offset(data, sizeof(*record_area));
	if (err)
		return err;

	data->record_count = record_area->record_count;
	data->record_start = record_area->records;

	return 0;
}

 
static int
pldm_check_desc_tlv_len(struct pldmfw_priv *data, u16 type, u16 size)
{
	struct device *dev = data->context->dev;
	u16 expected_size;

	switch (type) {
	case PLDM_DESC_ID_PCI_VENDOR_ID:
	case PLDM_DESC_ID_PCI_DEVICE_ID:
	case PLDM_DESC_ID_PCI_SUBVENDOR_ID:
	case PLDM_DESC_ID_PCI_SUBDEV_ID:
		expected_size = 2;
		break;
	case PLDM_DESC_ID_PCI_REVISION_ID:
		expected_size = 1;
		break;
	case PLDM_DESC_ID_PNP_VENDOR_ID:
		expected_size = 3;
		break;
	case PLDM_DESC_ID_IANA_ENTERPRISE_ID:
	case PLDM_DESC_ID_ACPI_VENDOR_ID:
	case PLDM_DESC_ID_PNP_PRODUCT_ID:
	case PLDM_DESC_ID_ACPI_PRODUCT_ID:
		expected_size = 4;
		break;
	case PLDM_DESC_ID_UUID:
		expected_size = 16;
		break;
	case PLDM_DESC_ID_VENDOR_DEFINED:
		return 0;
	default:
		 
		dev_dbg(dev, "Found unrecognized TLV type 0x%04x\n", type);
		return 0;
	}

	if (size != expected_size) {
		dev_dbg(dev, "Found TLV type 0x%04x with unexpected length. Got %u bytes, but expected %u bytes\n",
			type, size, expected_size);
		return -EINVAL;
	}

	return 0;
}

 
static int
pldm_parse_desc_tlvs(struct pldmfw_priv *data, struct pldmfw_record *record, u8 desc_count)
{
	const struct __pldmfw_desc_tlv *__desc;
	const u8 *desc_start;
	u8 i;

	desc_start = data->fw->data + data->offset;

	pldm_for_each_desc_tlv(i, __desc, desc_start, desc_count) {
		struct pldmfw_desc_tlv *desc;
		int err;
		u16 type, size;

		err = pldm_move_fw_offset(data, sizeof(*__desc));
		if (err)
			return err;

		type = get_unaligned_le16(&__desc->type);

		 
		size = get_unaligned_le16(&__desc->size);

		err = pldm_check_desc_tlv_len(data, type, size);
		if (err)
			return err;

		 
		err = pldm_move_fw_offset(data, size);
		if (err)
			return err;

		desc = kzalloc(sizeof(*desc), GFP_KERNEL);
		if (!desc)
			return -ENOMEM;

		desc->type = type;
		desc->size = size;
		desc->data = __desc->data;

		list_add_tail(&desc->entry, &record->descs);
	}

	return 0;
}

 
static int
pldm_parse_one_record(struct pldmfw_priv *data,
		      const struct __pldmfw_record_info *__record)
{
	struct pldmfw_record *record;
	size_t measured_length;
	int err;
	const u8 *bitmap_ptr;
	u16 record_len;
	int i;

	 
	record = kzalloc(sizeof(*record), GFP_KERNEL);
	if (!record)
		return -ENOMEM;

	INIT_LIST_HEAD(&record->descs);
	list_add_tail(&record->entry, &data->records);

	 
	err = pldm_move_fw_offset(data, sizeof(*__record));
	if (err)
		return err;

	record_len = get_unaligned_le16(&__record->record_len);
	record->package_data_len = get_unaligned_le16(&__record->package_data_len);
	record->version_len = __record->version_len;
	record->version_type = __record->version_type;

	bitmap_ptr = data->fw->data + data->offset;

	 
	err = pldm_move_fw_offset(data, data->bitmap_size);
	if (err)
		return err;

	record->component_bitmap_len = data->component_bitmap_len;
	record->component_bitmap = bitmap_zalloc(record->component_bitmap_len,
						 GFP_KERNEL);
	if (!record->component_bitmap)
		return -ENOMEM;

	for (i = 0; i < data->bitmap_size; i++)
		bitmap_set_value8(record->component_bitmap, bitmap_ptr[i], i * 8);

	record->version_string = data->fw->data + data->offset;

	err = pldm_move_fw_offset(data, record->version_len);
	if (err)
		return err;

	 
	err = pldm_parse_desc_tlvs(data, record, __record->descriptor_count);
	if (err)
		return err;

	record->package_data = data->fw->data + data->offset;

	err = pldm_move_fw_offset(data, record->package_data_len);
	if (err)
		return err;

	measured_length = data->offset - ((const u8 *)__record - data->fw->data);
	if (measured_length != record_len) {
		dev_dbg(data->context->dev, "Unexpected record length. Measured record length is %zu bytes, expected length is %u bytes\n",
			measured_length, record_len);
		return -EFAULT;
	}

	return 0;
}

 
static int pldm_parse_records(struct pldmfw_priv *data)
{
	const struct __pldmfw_component_area *component_area;
	const struct __pldmfw_record_info *record;
	int err;
	u8 i;

	pldm_for_each_record(i, record, data->record_start, data->record_count) {
		err = pldm_parse_one_record(data, record);
		if (err)
			return err;
	}

	 
	component_area = (const struct __pldmfw_component_area *)(data->fw->data + data->offset);

	err = pldm_move_fw_offset(data, sizeof(*component_area));
	if (err)
		return err;

	data->component_count =
		get_unaligned_le16(&component_area->component_image_count);
	data->component_start = component_area->components;

	return 0;
}

 
static int pldm_parse_components(struct pldmfw_priv *data)
{
	const struct __pldmfw_component_info *__component;
	struct device *dev = data->context->dev;
	const u8 *header_crc_ptr;
	int err;
	u8 i;

	pldm_for_each_component(i, __component, data->component_start, data->component_count) {
		struct pldmfw_component *component;
		u32 offset, size;

		err = pldm_move_fw_offset(data, sizeof(*__component));
		if (err)
			return err;

		err = pldm_move_fw_offset(data, __component->version_len);
		if (err)
			return err;

		offset = get_unaligned_le32(&__component->location_offset);
		size = get_unaligned_le32(&__component->size);

		err = pldm_check_fw_space(data, offset, size);
		if (err)
			return err;

		component = kzalloc(sizeof(*component), GFP_KERNEL);
		if (!component)
			return -ENOMEM;

		component->index = i;
		component->classification = get_unaligned_le16(&__component->classification);
		component->identifier = get_unaligned_le16(&__component->identifier);
		component->comparison_stamp = get_unaligned_le32(&__component->comparison_stamp);
		component->options = get_unaligned_le16(&__component->options);
		component->activation_method = get_unaligned_le16(&__component->activation_method);
		component->version_type = __component->version_type;
		component->version_len = __component->version_len;
		component->version_string = __component->version_string;
		component->component_data = data->fw->data + offset;
		component->component_size = size;

		list_add_tail(&component->entry, &data->components);
	}

	header_crc_ptr = data->fw->data + data->offset;

	err = pldm_move_fw_offset(data, sizeof(data->header_crc));
	if (err)
		return err;

	 
	if (data->offset != data->total_header_size) {
		dev_dbg(dev, "Invalid firmware header size. Expected %u but got %zu\n",
			data->total_header_size, data->offset);
		return -EFAULT;
	}

	data->header_crc = get_unaligned_le32(header_crc_ptr);

	return 0;
}

 
static int pldm_verify_header_crc(struct pldmfw_priv *data)
{
	struct device *dev = data->context->dev;
	u32 calculated_crc;
	size_t length;

	 
	length = data->offset - sizeof(data->header_crc);
	calculated_crc = crc32_le(~0, data->fw->data, length) ^ ~0;

	if (calculated_crc != data->header_crc) {
		dev_dbg(dev, "Invalid CRC in firmware header. Got 0x%08x but expected 0x%08x\n",
			calculated_crc, data->header_crc);
		return -EBADMSG;
	}

	return 0;
}

 
static void pldmfw_free_priv(struct pldmfw_priv *data)
{
	struct pldmfw_component *component, *c_safe;
	struct pldmfw_record *record, *r_safe;
	struct pldmfw_desc_tlv *desc, *d_safe;

	list_for_each_entry_safe(component, c_safe, &data->components, entry) {
		list_del(&component->entry);
		kfree(component);
	}

	list_for_each_entry_safe(record, r_safe, &data->records, entry) {
		list_for_each_entry_safe(desc, d_safe, &record->descs, entry) {
			list_del(&desc->entry);
			kfree(desc);
		}

		if (record->component_bitmap) {
			bitmap_free(record->component_bitmap);
			record->component_bitmap = NULL;
		}

		list_del(&record->entry);
		kfree(record);
	}
}

 
static int pldm_parse_image(struct pldmfw_priv *data)
{
	int err;

	if (WARN_ON(!(data->context->dev && data->fw->data && data->fw->size)))
		return -EINVAL;

	err = pldm_parse_header(data);
	if (err)
		return err;

	err = pldm_parse_records(data);
	if (err)
		return err;

	err = pldm_parse_components(data);
	if (err)
		return err;

	return pldm_verify_header_crc(data);
}

 
struct pldm_pci_record_id {
	int vendor;
	int device;
	int subsystem_vendor;
	int subsystem_device;
};

 
bool pldmfw_op_pci_match_record(struct pldmfw *context, struct pldmfw_record *record)
{
	struct pci_dev *pdev = to_pci_dev(context->dev);
	struct pldm_pci_record_id id = {
		.vendor = PCI_ANY_ID,
		.device = PCI_ANY_ID,
		.subsystem_vendor = PCI_ANY_ID,
		.subsystem_device = PCI_ANY_ID,
	};
	struct pldmfw_desc_tlv *desc;

	list_for_each_entry(desc, &record->descs, entry) {
		u16 value;
		int *ptr;

		switch (desc->type) {
		case PLDM_DESC_ID_PCI_VENDOR_ID:
			ptr = &id.vendor;
			break;
		case PLDM_DESC_ID_PCI_DEVICE_ID:
			ptr = &id.device;
			break;
		case PLDM_DESC_ID_PCI_SUBVENDOR_ID:
			ptr = &id.subsystem_vendor;
			break;
		case PLDM_DESC_ID_PCI_SUBDEV_ID:
			ptr = &id.subsystem_device;
			break;
		default:
			 
			continue;
		}

		value = get_unaligned_le16(desc->data);
		 
		if (value)
			*ptr = (int)value;
		else
			*ptr = PCI_ANY_ID;
	}

	if ((id.vendor == PCI_ANY_ID || id.vendor == pdev->vendor) &&
	    (id.device == PCI_ANY_ID || id.device == pdev->device) &&
	    (id.subsystem_vendor == PCI_ANY_ID || id.subsystem_vendor == pdev->subsystem_vendor) &&
	    (id.subsystem_device == PCI_ANY_ID || id.subsystem_device == pdev->subsystem_device))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(pldmfw_op_pci_match_record);

 
static int pldm_find_matching_record(struct pldmfw_priv *data)
{
	struct pldmfw_record *record;

	list_for_each_entry(record, &data->records, entry) {
		if (data->context->ops->match_record(data->context, record)) {
			data->matching_record = record;
			return 0;
		}
	}

	return -ENOENT;
}

 
static int
pldm_send_package_data(struct pldmfw_priv *data)
{
	struct pldmfw_record *record = data->matching_record;
	const struct pldmfw_ops *ops = data->context->ops;

	return ops->send_package_data(data->context, record->package_data,
				      record->package_data_len);
}

 
static int
pldm_send_component_tables(struct pldmfw_priv *data)
{
	unsigned long *bitmap = data->matching_record->component_bitmap;
	struct pldmfw_component *component;
	int err;

	list_for_each_entry(component, &data->components, entry) {
		u8 index = component->index, transfer_flag = 0;

		 
		if (!test_bit(index, bitmap))
			continue;

		 
		if (index == find_first_bit(bitmap, data->component_bitmap_len))
			transfer_flag |= PLDM_TRANSFER_FLAG_START;
		if (index == find_last_bit(bitmap, data->component_bitmap_len))
			transfer_flag |= PLDM_TRANSFER_FLAG_END;
		if (!transfer_flag)
			transfer_flag = PLDM_TRANSFER_FLAG_MIDDLE;

		err = data->context->ops->send_component_table(data->context,
							       component,
							       transfer_flag);
		if (err)
			return err;
	}

	return 0;
}

 
static int pldm_flash_components(struct pldmfw_priv *data)
{
	unsigned long *bitmap = data->matching_record->component_bitmap;
	struct pldmfw_component *component;
	int err;

	list_for_each_entry(component, &data->components, entry) {
		u8 index = component->index;

		 
		if (!test_bit(index, bitmap))
			continue;

		err = data->context->ops->flash_component(data->context, component);
		if (err)
			return err;
	}

	return 0;
}

 
static int pldm_finalize_update(struct pldmfw_priv *data)
{
	if (data->context->ops->finalize_update)
		return data->context->ops->finalize_update(data->context);

	return 0;
}

 
int pldmfw_flash_image(struct pldmfw *context, const struct firmware *fw)
{
	struct pldmfw_priv *data;
	int err;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	INIT_LIST_HEAD(&data->records);
	INIT_LIST_HEAD(&data->components);

	data->fw = fw;
	data->context = context;

	err = pldm_parse_image(data);
	if (err)
		goto out_release_data;

	err = pldm_find_matching_record(data);
	if (err)
		goto out_release_data;

	err = pldm_send_package_data(data);
	if (err)
		goto out_release_data;

	err = pldm_send_component_tables(data);
	if (err)
		goto out_release_data;

	err = pldm_flash_components(data);
	if (err)
		goto out_release_data;

	err = pldm_finalize_update(data);

out_release_data:
	pldmfw_free_priv(data);
	kfree(data);

	return err;
}
EXPORT_SYMBOL(pldmfw_flash_image);

MODULE_AUTHOR("Jacob Keller <jacob.e.keller@intel.com>");
MODULE_DESCRIPTION("PLDM firmware flash update library");
