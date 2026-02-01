
 

#include "i915_drv.h"

#include "gem/i915_gem_region.h"
#include "gt/intel_gt.h"

#include "intel_pxp.h"
#include "intel_pxp_huc.h"
#include "intel_pxp_tee.h"
#include "intel_pxp_types.h"
#include "intel_pxp_cmd_interface_43.h"

int intel_pxp_huc_load_and_auth(struct intel_pxp *pxp)
{
	struct intel_gt *gt;
	struct intel_huc *huc;
	struct pxp43_start_huc_auth_in huc_in = {0};
	struct pxp43_huc_auth_out huc_out = {0};
	dma_addr_t huc_phys_addr;
	u8 client_id = 0;
	u8 fence_id = 0;
	int err;

	if (!pxp || !pxp->pxp_component)
		return -ENODEV;

	gt = pxp->ctrl_gt;
	huc = &gt->uc.huc;

	huc_phys_addr = i915_gem_object_get_dma_address(huc->fw.obj, 0);

	 
	huc_in.header.api_version = PXP_APIVER(4, 3);
	huc_in.header.command_id  = PXP43_CMDID_START_HUC_AUTH;
	huc_in.header.status      = 0;
	huc_in.header.buffer_len  = sizeof(huc_in.huc_base_address);
	huc_in.huc_base_address   = cpu_to_le64(huc_phys_addr);

	err = intel_pxp_tee_stream_message(pxp, client_id, fence_id,
					   &huc_in, sizeof(huc_in),
					   &huc_out, sizeof(huc_out));
	if (err < 0) {
		drm_err(&gt->i915->drm,
			"Failed to send HuC load and auth command to GSC [%d]!\n",
			err);
		return err;
	}

	 
	if (huc_out.header.status != PXP_STATUS_SUCCESS &&
	    huc_out.header.status != PXP_STATUS_OP_NOT_PERMITTED) {
		drm_err(&gt->i915->drm,
			"HuC load failed with GSC error = 0x%x\n",
			huc_out.header.status);
		return -EPROTO;
	}

	return 0;
}
