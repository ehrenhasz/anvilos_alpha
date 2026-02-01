
 

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/firmware.h>

#include <drm/display/drm_hdcp_helper.h>
#include <drm/drm_sysfs.h>
#include <drm/drm_print.h>
#include <drm/drm_device.h>
#include <drm/drm_property.h>
#include <drm/drm_mode_object.h>
#include <drm/drm_connector.h>

static inline void drm_hdcp_print_ksv(const u8 *ksv)
{
	DRM_DEBUG("\t%#02x, %#02x, %#02x, %#02x, %#02x\n",
		  ksv[0], ksv[1], ksv[2], ksv[3], ksv[4]);
}

static u32 drm_hdcp_get_revoked_ksv_count(const u8 *buf, u32 vrls_length)
{
	u32 parsed_bytes = 0, ksv_count = 0, vrl_ksv_cnt, vrl_sz;

	while (parsed_bytes < vrls_length) {
		vrl_ksv_cnt = *buf;
		ksv_count += vrl_ksv_cnt;

		vrl_sz = (vrl_ksv_cnt * DRM_HDCP_KSV_LEN) + 1;
		buf += vrl_sz;
		parsed_bytes += vrl_sz;
	}

	 
	if (parsed_bytes != vrls_length)
		ksv_count = 0;

	return ksv_count;
}

static u32 drm_hdcp_get_revoked_ksvs(const u8 *buf, u8 **revoked_ksv_list,
				     u32 vrls_length)
{
	u32 vrl_ksv_cnt, vrl_ksv_sz, vrl_idx = 0;
	u32 parsed_bytes = 0, ksv_count = 0;

	do {
		vrl_ksv_cnt = *buf;
		vrl_ksv_sz = vrl_ksv_cnt * DRM_HDCP_KSV_LEN;

		buf++;

		DRM_DEBUG("vrl: %d, Revoked KSVs: %d\n", vrl_idx++,
			  vrl_ksv_cnt);
		memcpy((*revoked_ksv_list) + (ksv_count * DRM_HDCP_KSV_LEN),
		       buf, vrl_ksv_sz);

		ksv_count += vrl_ksv_cnt;
		buf += vrl_ksv_sz;

		parsed_bytes += (vrl_ksv_sz + 1);
	} while (parsed_bytes < vrls_length);

	return ksv_count;
}

static inline u32 get_vrl_length(const u8 *buf)
{
	return drm_hdcp_be24_to_cpu(buf);
}

static int drm_hdcp_parse_hdcp1_srm(const u8 *buf, size_t count,
				    u8 **revoked_ksv_list, u32 *revoked_ksv_cnt)
{
	struct hdcp_srm_header *header;
	u32 vrl_length, ksv_count;

	if (count < (sizeof(struct hdcp_srm_header) +
	    DRM_HDCP_1_4_VRL_LENGTH_SIZE + DRM_HDCP_1_4_DCP_SIG_SIZE)) {
		DRM_ERROR("Invalid blob length\n");
		return -EINVAL;
	}

	header = (struct hdcp_srm_header *)buf;
	DRM_DEBUG("SRM ID: 0x%x, SRM Ver: 0x%x, SRM Gen No: 0x%x\n",
		  header->srm_id,
		  be16_to_cpu(header->srm_version), header->srm_gen_no);

	WARN_ON(header->reserved);

	buf = buf + sizeof(*header);
	vrl_length = get_vrl_length(buf);
	if (count < (sizeof(struct hdcp_srm_header) + vrl_length) ||
	    vrl_length < (DRM_HDCP_1_4_VRL_LENGTH_SIZE +
			  DRM_HDCP_1_4_DCP_SIG_SIZE)) {
		DRM_ERROR("Invalid blob length or vrl length\n");
		return -EINVAL;
	}

	 
	vrl_length -= (DRM_HDCP_1_4_VRL_LENGTH_SIZE +
		       DRM_HDCP_1_4_DCP_SIG_SIZE);

	if (!vrl_length) {
		DRM_ERROR("No vrl found\n");
		return -EINVAL;
	}

	buf += DRM_HDCP_1_4_VRL_LENGTH_SIZE;
	ksv_count = drm_hdcp_get_revoked_ksv_count(buf, vrl_length);
	if (!ksv_count) {
		DRM_DEBUG("Revoked KSV count is 0\n");
		return 0;
	}

	*revoked_ksv_list = kcalloc(ksv_count, DRM_HDCP_KSV_LEN, GFP_KERNEL);
	if (!*revoked_ksv_list) {
		DRM_ERROR("Out of Memory\n");
		return -ENOMEM;
	}

	if (drm_hdcp_get_revoked_ksvs(buf, revoked_ksv_list,
				      vrl_length) != ksv_count) {
		*revoked_ksv_cnt = 0;
		kfree(*revoked_ksv_list);
		return -EINVAL;
	}

	*revoked_ksv_cnt = ksv_count;
	return 0;
}

static int drm_hdcp_parse_hdcp2_srm(const u8 *buf, size_t count,
				    u8 **revoked_ksv_list, u32 *revoked_ksv_cnt)
{
	struct hdcp_srm_header *header;
	u32 vrl_length, ksv_count, ksv_sz;

	if (count < (sizeof(struct hdcp_srm_header) +
	    DRM_HDCP_2_VRL_LENGTH_SIZE + DRM_HDCP_2_DCP_SIG_SIZE)) {
		DRM_ERROR("Invalid blob length\n");
		return -EINVAL;
	}

	header = (struct hdcp_srm_header *)buf;
	DRM_DEBUG("SRM ID: 0x%x, SRM Ver: 0x%x, SRM Gen No: 0x%x\n",
		  header->srm_id & DRM_HDCP_SRM_ID_MASK,
		  be16_to_cpu(header->srm_version), header->srm_gen_no);

	if (header->reserved)
		return -EINVAL;

	buf = buf + sizeof(*header);
	vrl_length = get_vrl_length(buf);

	if (count < (sizeof(struct hdcp_srm_header) + vrl_length) ||
	    vrl_length < (DRM_HDCP_2_VRL_LENGTH_SIZE +
	    DRM_HDCP_2_DCP_SIG_SIZE)) {
		DRM_ERROR("Invalid blob length or vrl length\n");
		return -EINVAL;
	}

	 
	vrl_length -= (DRM_HDCP_2_VRL_LENGTH_SIZE +
		       DRM_HDCP_2_DCP_SIG_SIZE);

	if (!vrl_length) {
		DRM_ERROR("No vrl found\n");
		return -EINVAL;
	}

	buf += DRM_HDCP_2_VRL_LENGTH_SIZE;
	ksv_count = (*buf << 2) | DRM_HDCP_2_KSV_COUNT_2_LSBITS(*(buf + 1));
	if (!ksv_count) {
		DRM_DEBUG("Revoked KSV count is 0\n");
		return 0;
	}

	*revoked_ksv_list = kcalloc(ksv_count, DRM_HDCP_KSV_LEN, GFP_KERNEL);
	if (!*revoked_ksv_list) {
		DRM_ERROR("Out of Memory\n");
		return -ENOMEM;
	}

	ksv_sz = ksv_count * DRM_HDCP_KSV_LEN;
	buf += DRM_HDCP_2_NO_OF_DEV_PLUS_RESERVED_SZ;

	DRM_DEBUG("Revoked KSVs: %d\n", ksv_count);
	memcpy(*revoked_ksv_list, buf, ksv_sz);

	*revoked_ksv_cnt = ksv_count;
	return 0;
}

static inline bool is_srm_version_hdcp1(const u8 *buf)
{
	return *buf == (u8)(DRM_HDCP_1_4_SRM_ID << 4);
}

static inline bool is_srm_version_hdcp2(const u8 *buf)
{
	return *buf == (u8)(DRM_HDCP_2_SRM_ID << 4 | DRM_HDCP_2_INDICATOR);
}

static int drm_hdcp_srm_update(const u8 *buf, size_t count,
			       u8 **revoked_ksv_list, u32 *revoked_ksv_cnt)
{
	if (count < sizeof(struct hdcp_srm_header))
		return -EINVAL;

	if (is_srm_version_hdcp1(buf))
		return drm_hdcp_parse_hdcp1_srm(buf, count, revoked_ksv_list,
						revoked_ksv_cnt);
	else if (is_srm_version_hdcp2(buf))
		return drm_hdcp_parse_hdcp2_srm(buf, count, revoked_ksv_list,
						revoked_ksv_cnt);
	else
		return -EINVAL;
}

static int drm_hdcp_request_srm(struct drm_device *drm_dev,
				u8 **revoked_ksv_list, u32 *revoked_ksv_cnt)
{
	char fw_name[36] = "display_hdcp_srm.bin";
	const struct firmware *fw;
	int ret;

	ret = request_firmware_direct(&fw, (const char *)fw_name,
				      drm_dev->dev);
	if (ret < 0) {
		*revoked_ksv_cnt = 0;
		*revoked_ksv_list = NULL;
		ret = 0;
		goto exit;
	}

	if (fw->size && fw->data)
		ret = drm_hdcp_srm_update(fw->data, fw->size, revoked_ksv_list,
					  revoked_ksv_cnt);

exit:
	release_firmware(fw);
	return ret;
}

 
int drm_hdcp_check_ksvs_revoked(struct drm_device *drm_dev, u8 *ksvs,
				u32 ksv_count)
{
	u32 revoked_ksv_cnt = 0, i, j;
	u8 *revoked_ksv_list = NULL;
	int ret = 0;

	ret = drm_hdcp_request_srm(drm_dev, &revoked_ksv_list,
				   &revoked_ksv_cnt);
	if (ret)
		return ret;

	 
	for (i = 0; i < revoked_ksv_cnt; i++)
		for  (j = 0; j < ksv_count; j++)
			if (!memcmp(&ksvs[j * DRM_HDCP_KSV_LEN],
				    &revoked_ksv_list[i * DRM_HDCP_KSV_LEN],
				    DRM_HDCP_KSV_LEN)) {
				DRM_DEBUG("Revoked KSV is ");
				drm_hdcp_print_ksv(&ksvs[j * DRM_HDCP_KSV_LEN]);
				ret++;
			}

	kfree(revoked_ksv_list);
	return ret;
}
EXPORT_SYMBOL_GPL(drm_hdcp_check_ksvs_revoked);

static struct drm_prop_enum_list drm_cp_enum_list[] = {
	{ DRM_MODE_CONTENT_PROTECTION_UNDESIRED, "Undesired" },
	{ DRM_MODE_CONTENT_PROTECTION_DESIRED, "Desired" },
	{ DRM_MODE_CONTENT_PROTECTION_ENABLED, "Enabled" },
};
DRM_ENUM_NAME_FN(drm_get_content_protection_name, drm_cp_enum_list)

static struct drm_prop_enum_list drm_hdcp_content_type_enum_list[] = {
	{ DRM_MODE_HDCP_CONTENT_TYPE0, "HDCP Type0" },
	{ DRM_MODE_HDCP_CONTENT_TYPE1, "HDCP Type1" },
};
DRM_ENUM_NAME_FN(drm_get_hdcp_content_type_name,
		 drm_hdcp_content_type_enum_list)

 
int drm_connector_attach_content_protection_property(
		struct drm_connector *connector, bool hdcp_content_type)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop =
			dev->mode_config.content_protection_property;

	if (!prop)
		prop = drm_property_create_enum(dev, 0, "Content Protection",
						drm_cp_enum_list,
						ARRAY_SIZE(drm_cp_enum_list));
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&connector->base, prop,
				   DRM_MODE_CONTENT_PROTECTION_UNDESIRED);
	dev->mode_config.content_protection_property = prop;

	if (!hdcp_content_type)
		return 0;

	prop = dev->mode_config.hdcp_content_type_property;
	if (!prop)
		prop = drm_property_create_enum(dev, 0, "HDCP Content Type",
					drm_hdcp_content_type_enum_list,
					ARRAY_SIZE(
					drm_hdcp_content_type_enum_list));
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&connector->base, prop,
				   DRM_MODE_HDCP_CONTENT_TYPE0);
	dev->mode_config.hdcp_content_type_property = prop;

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_content_protection_property);

 
void drm_hdcp_update_content_protection(struct drm_connector *connector,
					u64 val)
{
	struct drm_device *dev = connector->dev;
	struct drm_connector_state *state = connector->state;

	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));
	if (state->content_protection == val)
		return;

	state->content_protection = val;
	drm_sysfs_connector_property_event(connector,
					   dev->mode_config.content_protection_property);
}
EXPORT_SYMBOL(drm_hdcp_update_content_protection);
