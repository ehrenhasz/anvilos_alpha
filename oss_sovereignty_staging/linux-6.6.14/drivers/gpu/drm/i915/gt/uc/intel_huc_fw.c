
 

#include "gt/intel_gsc.h"
#include "gt/intel_gt.h"
#include "intel_gsc_binary_headers.h"
#include "intel_gsc_uc_heci_cmd_submit.h"
#include "intel_huc.h"
#include "intel_huc_fw.h"
#include "intel_huc_print.h"
#include "i915_drv.h"
#include "pxp/intel_pxp_huc.h"
#include "pxp/intel_pxp_cmd_interface_43.h"

struct mtl_huc_auth_msg_in {
	struct intel_gsc_mtl_header header;
	struct pxp43_new_huc_auth_in huc_in;
} __packed;

struct mtl_huc_auth_msg_out {
	struct intel_gsc_mtl_header header;
	struct pxp43_huc_auth_out huc_out;
} __packed;

int intel_huc_fw_auth_via_gsccs(struct intel_huc *huc)
{
	struct intel_gt *gt = huc_to_gt(huc);
	struct drm_i915_gem_object *obj;
	struct mtl_huc_auth_msg_in *msg_in;
	struct mtl_huc_auth_msg_out *msg_out;
	void *pkt_vaddr;
	u64 pkt_offset;
	int retry = 5;
	int err = 0;

	if (!huc->heci_pkt)
		return -ENODEV;

	obj = huc->heci_pkt->obj;
	pkt_offset = i915_ggtt_offset(huc->heci_pkt);

	pkt_vaddr = i915_gem_object_pin_map_unlocked(obj,
						     intel_gt_coherent_map_type(gt, obj, true));
	if (IS_ERR(pkt_vaddr))
		return PTR_ERR(pkt_vaddr);

	msg_in = pkt_vaddr;
	msg_out = pkt_vaddr + PXP43_HUC_AUTH_INOUT_SIZE;

	intel_gsc_uc_heci_cmd_emit_mtl_header(&msg_in->header,
					      HECI_MEADDRESS_PXP,
					      sizeof(*msg_in), 0);

	msg_in->huc_in.header.api_version = PXP_APIVER(4, 3);
	msg_in->huc_in.header.command_id = PXP43_CMDID_NEW_HUC_AUTH;
	msg_in->huc_in.header.status = 0;
	msg_in->huc_in.header.buffer_len = sizeof(msg_in->huc_in) -
					   sizeof(msg_in->huc_in.header);
	msg_in->huc_in.huc_base_address = huc->fw.vma_res.start;
	msg_in->huc_in.huc_size = huc->fw.obj->base.size;

	do {
		err = intel_gsc_uc_heci_cmd_submit_packet(&gt->uc.gsc,
							  pkt_offset, sizeof(*msg_in),
							  pkt_offset + PXP43_HUC_AUTH_INOUT_SIZE,
							  PXP43_HUC_AUTH_INOUT_SIZE);
		if (err) {
			huc_err(huc, "failed to submit GSC request to auth: %d\n", err);
			goto out_unpin;
		}

		if (msg_out->header.flags & GSC_OUTFLAG_MSG_PENDING) {
			msg_in->header.gsc_message_handle = msg_out->header.gsc_message_handle;
			err = -EBUSY;
			msleep(50);
		}
	} while (--retry && err == -EBUSY);

	if (err)
		goto out_unpin;

	if (msg_out->header.message_size != sizeof(*msg_out)) {
		huc_err(huc, "invalid GSC reply length %u [expected %zu]\n",
			msg_out->header.message_size, sizeof(*msg_out));
		err = -EPROTO;
		goto out_unpin;
	}

	 
	if (msg_out->huc_out.header.status != PXP_STATUS_SUCCESS &&
	    msg_out->huc_out.header.status != PXP_STATUS_OP_NOT_PERMITTED) {
		huc_err(huc, "auth failed with GSC error = 0x%x\n",
			msg_out->huc_out.header.status);
		err = -EIO;
		goto out_unpin;
	}

out_unpin:
	i915_gem_object_unpin_map(obj);
	return err;
}

static bool css_valid(const void *data, size_t size)
{
	const struct uc_css_header *css = data;

	if (unlikely(size < sizeof(struct uc_css_header)))
		return false;

	if (css->module_type != 0x6)
		return false;

	if (css->module_vendor != PCI_VENDOR_ID_INTEL)
		return false;

	return true;
}

static inline u32 entry_offset(const struct intel_gsc_cpd_entry *entry)
{
	return entry->offset & INTEL_GSC_CPD_ENTRY_OFFSET_MASK;
}

int intel_huc_fw_get_binary_info(struct intel_uc_fw *huc_fw, const void *data, size_t size)
{
	struct intel_huc *huc = container_of(huc_fw, struct intel_huc, fw);
	const struct intel_gsc_cpd_header_v2 *header = data;
	const struct intel_gsc_cpd_entry *entry;
	size_t min_size = sizeof(*header);
	int i;

	if (!huc_fw->has_gsc_headers) {
		huc_err(huc, "Invalid FW type for GSC header parsing!\n");
		return -EINVAL;
	}

	if (size < sizeof(*header)) {
		huc_err(huc, "FW too small! %zu < %zu\n", size, min_size);
		return -ENODATA;
	}

	 

	if (header->header_marker != INTEL_GSC_CPD_HEADER_MARKER) {
		huc_err(huc, "invalid marker for CPD header: 0x%08x!\n",
			header->header_marker);
		return -EINVAL;
	}

	 
	if (header->header_version != 2 || header->entry_version != 1) {
		huc_err(huc, "invalid CPD header/entry version %u:%u!\n",
			header->header_version, header->entry_version);
		return -EINVAL;
	}

	if (header->header_length < sizeof(struct intel_gsc_cpd_header_v2)) {
		huc_err(huc, "invalid CPD header length %u!\n",
			header->header_length);
		return -EINVAL;
	}

	min_size = header->header_length + sizeof(*entry) * header->num_of_entries;
	if (size < min_size) {
		huc_err(huc, "FW too small! %zu < %zu\n", size, min_size);
		return -ENODATA;
	}

	entry = data + header->header_length;

	for (i = 0; i < header->num_of_entries; i++, entry++) {
		if (strcmp(entry->name, "HUCP.man") == 0)
			intel_uc_fw_version_from_gsc_manifest(&huc_fw->file_selected.ver,
							      data + entry_offset(entry));

		if (strcmp(entry->name, "huc_fw") == 0) {
			u32 offset = entry_offset(entry);

			if (offset < size && css_valid(data + offset, size - offset))
				huc_fw->dma_start_offset = offset;
		}
	}

	return 0;
}

int intel_huc_fw_load_and_auth_via_gsc(struct intel_huc *huc)
{
	int ret;

	if (!intel_huc_is_loaded_by_gsc(huc))
		return -ENODEV;

	if (!intel_uc_fw_is_loadable(&huc->fw))
		return -ENOEXEC;

	 
	if (intel_huc_is_authenticated(huc, INTEL_HUC_AUTH_BY_GSC)) {
		intel_uc_fw_change_status(&huc->fw, INTEL_UC_FIRMWARE_RUNNING);
		return 0;
	}

	GEM_WARN_ON(intel_uc_fw_is_loaded(&huc->fw));

	ret = intel_pxp_huc_load_and_auth(huc_to_gt(huc)->i915->pxp);
	if (ret)
		return ret;

	intel_uc_fw_change_status(&huc->fw, INTEL_UC_FIRMWARE_TRANSFERRED);

	return intel_huc_wait_for_auth_complete(huc, INTEL_HUC_AUTH_BY_GSC);
}

 
int intel_huc_fw_upload(struct intel_huc *huc)
{
	if (intel_huc_is_loaded_by_gsc(huc))
		return -ENODEV;

	 
	return intel_uc_fw_upload(&huc->fw, 0, HUC_UKERNEL);
}
