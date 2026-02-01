 
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>

#include "amdgpu.h"
#include "amdgpu_rap.h"

 
static ssize_t amdgpu_rap_debugfs_write(struct file *f, const char __user *buf,
		size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)file_inode(f)->i_private;
	struct ta_rap_shared_memory *rap_shared_mem;
	struct ta_rap_cmd_output_data *rap_cmd_output;
	struct drm_device *dev = adev_to_drm(adev);
	uint32_t op;
	enum ta_rap_status status;
	int ret;

	if (*pos || size != 2)
		return -EINVAL;

	ret = kstrtouint_from_user(buf, size, *pos, &op);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(dev->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(dev->dev);
		return ret;
	}

	 
	amdgpu_gfx_off_ctrl(adev, false);

	switch (op) {
	case 2:
		ret = psp_rap_invoke(&adev->psp, op, &status);
		if (!ret && status == TA_RAP_STATUS__SUCCESS) {
			dev_info(adev->dev, "RAP L0 validate test success.\n");
		} else {
			rap_shared_mem = (struct ta_rap_shared_memory *)
					 adev->psp.rap_context.context.mem_context.shared_buf;
			rap_cmd_output = &(rap_shared_mem->rap_out_message.output);

			dev_info(adev->dev, "RAP test failed, the output is:\n");
			dev_info(adev->dev, "\tlast_subsection: 0x%08x.\n",
				 rap_cmd_output->last_subsection);
			dev_info(adev->dev, "\tnum_total_validate: 0x%08x.\n",
				 rap_cmd_output->num_total_validate);
			dev_info(adev->dev, "\tnum_valid: 0x%08x.\n",
				 rap_cmd_output->num_valid);
			dev_info(adev->dev, "\tlast_validate_addr: 0x%08x.\n",
				 rap_cmd_output->last_validate_addr);
			dev_info(adev->dev, "\tlast_validate_val: 0x%08x.\n",
				 rap_cmd_output->last_validate_val);
			dev_info(adev->dev, "\tlast_validate_val_exptd: 0x%08x.\n",
				 rap_cmd_output->last_validate_val_exptd);
		}
		break;
	default:
		dev_info(adev->dev, "Unsupported op id: %d, ", op);
		dev_info(adev->dev, "Only support op 2(L0 validate test).\n");
		break;
	}

	amdgpu_gfx_off_ctrl(adev, true);
	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);

	return size;
}

static const struct file_operations amdgpu_rap_debugfs_ops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = amdgpu_rap_debugfs_write,
	.llseek = default_llseek
};

void amdgpu_rap_debugfs_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev_to_drm(adev)->primary;

	if (!adev->psp.rap_context.context.initialized)
		return;

	debugfs_create_file("rap_test", S_IWUSR, minor->debugfs_root,
				adev, &amdgpu_rap_debugfs_ops);
}
