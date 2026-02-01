 

#include <linux/debugfs.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>

#include "amdgpu.h"
#include "amdgpu_fw_attestation.h"
#include "amdgpu_psp.h"
#include "amdgpu_ucode.h"
#include "soc15_common.h"

#define FW_ATTESTATION_DB_COOKIE        0x143b6a37
#define FW_ATTESTATION_RECORD_VALID	1
#define FW_ATTESTATION_MAX_SIZE		4096

struct FW_ATT_DB_HEADER {
	uint32_t AttDbVersion;            
	uint32_t AttDbCookie;             
};

struct FW_ATT_RECORD {
	uint16_t AttFwIdV1;               
	uint16_t AttFwIdV2;               
	uint32_t AttFWVersion;            
	uint16_t AttFWActiveFunctionID;   
	uint8_t  AttSource;               
	uint8_t  RecordValid;             
	uint32_t AttFwTaId;               
};

static ssize_t amdgpu_fw_attestation_debugfs_read(struct file *f,
						  char __user *buf,
						  size_t size,
						  loff_t *pos)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)file_inode(f)->i_private;
	uint64_t records_addr = 0;
	uint64_t vram_pos = 0;
	struct FW_ATT_DB_HEADER fw_att_hdr = {0};
	struct FW_ATT_RECORD fw_att_record = {0};

	if (size < sizeof(struct FW_ATT_RECORD)) {
		DRM_WARN("FW attestation input buffer not enough memory");
		return -EINVAL;
	}

	if ((*pos + sizeof(struct FW_ATT_DB_HEADER)) >= FW_ATTESTATION_MAX_SIZE) {
		DRM_WARN("FW attestation out of bounds");
		return 0;
	}

	if (psp_get_fw_attestation_records_addr(&adev->psp, &records_addr)) {
		DRM_WARN("Failed to get FW attestation record address");
		return -EINVAL;
	}

	vram_pos =  records_addr - adev->gmc.vram_start;

	if (*pos == 0) {
		amdgpu_device_vram_access(adev,
					  vram_pos,
					  (uint32_t *)&fw_att_hdr,
					  sizeof(struct FW_ATT_DB_HEADER),
					  false);

		if (fw_att_hdr.AttDbCookie != FW_ATTESTATION_DB_COOKIE) {
			DRM_WARN("Invalid FW attestation cookie");
			return -EINVAL;
		}

		DRM_INFO("FW attestation version = 0x%X", fw_att_hdr.AttDbVersion);
	}

	amdgpu_device_vram_access(adev,
				  vram_pos + sizeof(struct FW_ATT_DB_HEADER) + *pos,
				  (uint32_t *)&fw_att_record,
				  sizeof(struct FW_ATT_RECORD),
				  false);

	if (fw_att_record.RecordValid != FW_ATTESTATION_RECORD_VALID)
		return 0;

	if (copy_to_user(buf, (void *)&fw_att_record, sizeof(struct FW_ATT_RECORD)))
		return -EINVAL;

	*pos += sizeof(struct FW_ATT_RECORD);

	return sizeof(struct FW_ATT_RECORD);
}

static const struct file_operations amdgpu_fw_attestation_debugfs_ops = {
	.owner = THIS_MODULE,
	.read = amdgpu_fw_attestation_debugfs_read,
	.write = NULL,
	.llseek = default_llseek
};

static int amdgpu_is_fw_attestation_supported(struct amdgpu_device *adev)
{
	if (adev->flags & AMD_IS_APU)
		return 0;

	if (adev->asic_type >= CHIP_SIENNA_CICHLID)
		return 1;

	return 0;
}

void amdgpu_fw_attestation_debugfs_init(struct amdgpu_device *adev)
{
	if (!amdgpu_is_fw_attestation_supported(adev))
		return;

	debugfs_create_file("amdgpu_fw_attestation",
			    0400,
			    adev_to_drm(adev)->primary->debugfs_root,
			    adev,
			    &amdgpu_fw_attestation_debugfs_ops);
}
