











#include <linux/firmware.h>
#include "sof-priv.h"
#include "ops.h"

int snd_sof_load_firmware_raw(struct snd_sof_dev *sdev)
{
	struct snd_sof_pdata *plat_data = sdev->pdata;
	const char *fw_filename;
	ssize_t ext_man_size;
	int ret;

	 
	if (sdev->basefw.fw)
		return 0;

	fw_filename = kasprintf(GFP_KERNEL, "%s/%s",
				plat_data->fw_filename_prefix,
				plat_data->fw_filename);
	if (!fw_filename)
		return -ENOMEM;

	ret = request_firmware(&sdev->basefw.fw, fw_filename, sdev->dev);

	if (ret < 0) {
		dev_err(sdev->dev,
			"error: sof firmware file is missing, you might need to\n");
		dev_err(sdev->dev,
			"       download it from https://github.com/thesofproject/sof-bin/\n");
		goto err;
	} else {
		dev_dbg(sdev->dev, "request_firmware %s successful\n",
			fw_filename);
	}

	 
	ext_man_size = sdev->ipc->ops->fw_loader->parse_ext_manifest(sdev);
	if (ext_man_size > 0) {
		 
		sdev->basefw.payload_offset = ext_man_size;
	} else if (!ext_man_size) {
		 
		dev_dbg(sdev->dev, "firmware doesn't contain extended manifest\n");
	} else {
		ret = ext_man_size;
		dev_err(sdev->dev, "error: firmware %s contains unsupported or invalid extended manifest: %d\n",
			fw_filename, ret);
	}

err:
	kfree(fw_filename);

	return ret;
}
EXPORT_SYMBOL(snd_sof_load_firmware_raw);

int snd_sof_load_firmware_memcpy(struct snd_sof_dev *sdev)
{
	int ret;

	ret = snd_sof_load_firmware_raw(sdev);
	if (ret < 0)
		return ret;

	 
	ret = sdev->ipc->ops->fw_loader->validate(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: invalid FW header\n");
		goto error;
	}

	 
	ret = snd_sof_dsp_reset(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to reset DSP\n");
		goto error;
	}

	 
	if (sdev->ipc->ops->fw_loader->load_fw_to_dsp) {
		ret = sdev->ipc->ops->fw_loader->load_fw_to_dsp(sdev);
		if (ret < 0) {
			dev_err(sdev->dev, "Firmware loading failed\n");
			goto error;
		}
	}

	return 0;

error:
	release_firmware(sdev->basefw.fw);
	sdev->basefw.fw = NULL;
	return ret;

}
EXPORT_SYMBOL(snd_sof_load_firmware_memcpy);

int snd_sof_run_firmware(struct snd_sof_dev *sdev)
{
	int ret;

	init_waitqueue_head(&sdev->boot_wait);

	 
	sdev->dbg_dump_printed = false;
	sdev->ipc_dump_printed = false;

	 
	if (sdev->first_boot) {
		ret = snd_sof_debugfs_buf_item(sdev, &sdev->fw_version,
					       sizeof(sdev->fw_version),
					       "fw_version", 0444);
		 
		if (ret < 0) {
			dev_err(sdev->dev, "snd_sof_debugfs_buf_item failed\n");
			return ret;
		}
	}

	 
	ret = snd_sof_dsp_pre_fw_run(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "failed pre fw run op\n");
		return ret;
	}

	dev_dbg(sdev->dev, "booting DSP firmware\n");

	 
	ret = snd_sof_dsp_run(sdev);
	if (ret < 0) {
		snd_sof_dsp_dbg_dump(sdev, "Failed to start DSP",
				     SOF_DBG_DUMP_MBOX | SOF_DBG_DUMP_PCI);
		return ret;
	}

	 
	ret = wait_event_timeout(sdev->boot_wait,
				 sdev->fw_state > SOF_FW_BOOT_IN_PROGRESS,
				 msecs_to_jiffies(sdev->boot_timeout));
	if (ret == 0) {
		snd_sof_dsp_dbg_dump(sdev, "Firmware boot failure due to timeout",
				     SOF_DBG_DUMP_REGS | SOF_DBG_DUMP_MBOX |
				     SOF_DBG_DUMP_TEXT | SOF_DBG_DUMP_PCI);
		return -EIO;
	}

	if (sdev->fw_state == SOF_FW_BOOT_READY_FAILED)
		return -EIO;  

	dev_dbg(sdev->dev, "firmware boot complete\n");
	sof_set_fw_state(sdev, SOF_FW_BOOT_COMPLETE);

	 
	ret = snd_sof_dsp_post_fw_run(sdev);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed post fw run op\n");
		return ret;
	}

	if (sdev->ipc->ops->post_fw_boot)
		return sdev->ipc->ops->post_fw_boot(sdev);

	return 0;
}
EXPORT_SYMBOL(snd_sof_run_firmware);

void snd_sof_fw_unload(struct snd_sof_dev *sdev)
{
	 
	release_firmware(sdev->basefw.fw);
	sdev->basefw.fw = NULL;
}
EXPORT_SYMBOL(snd_sof_fw_unload);
