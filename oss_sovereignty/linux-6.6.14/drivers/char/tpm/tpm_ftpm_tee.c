
 

#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/tee_drv.h>
#include <linux/tpm.h>
#include <linux/uuid.h>

#include "tpm.h"
#include "tpm_ftpm_tee.h"

 
static const uuid_t ftpm_ta_uuid =
	UUID_INIT(0xBC50D971, 0xD4C9, 0x42C4,
		  0x82, 0xCB, 0x34, 0x3F, 0xB7, 0xF3, 0x78, 0x96);

 
static int ftpm_tee_tpm_op_recv(struct tpm_chip *chip, u8 *buf, size_t count)
{
	struct ftpm_tee_private *pvt_data = dev_get_drvdata(chip->dev.parent);
	size_t len;

	len = pvt_data->resp_len;
	if (count < len) {
		dev_err(&chip->dev,
			"%s: Invalid size in recv: count=%zd, resp_len=%zd\n",
			__func__, count, len);
		return -EIO;
	}

	memcpy(buf, pvt_data->resp_buf, len);
	pvt_data->resp_len = 0;

	return len;
}

 
static int ftpm_tee_tpm_op_send(struct tpm_chip *chip, u8 *buf, size_t len)
{
	struct ftpm_tee_private *pvt_data = dev_get_drvdata(chip->dev.parent);
	size_t resp_len;
	int rc;
	u8 *temp_buf;
	struct tpm_header *resp_header;
	struct tee_ioctl_invoke_arg transceive_args;
	struct tee_param command_params[4];
	struct tee_shm *shm = pvt_data->shm;

	if (len > MAX_COMMAND_SIZE) {
		dev_err(&chip->dev,
			"%s: len=%zd exceeds MAX_COMMAND_SIZE supported by fTPM TA\n",
			__func__, len);
		return -EIO;
	}

	memset(&transceive_args, 0, sizeof(transceive_args));
	memset(command_params, 0, sizeof(command_params));
	pvt_data->resp_len = 0;

	 
	transceive_args = (struct tee_ioctl_invoke_arg) {
		.func = FTPM_OPTEE_TA_SUBMIT_COMMAND,
		.session = pvt_data->session,
		.num_params = 4,
	};

	 
	command_params[0] = (struct tee_param) {
		.attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT,
		.u.memref = {
			.shm = shm,
			.size = len,
			.shm_offs = 0,
		},
	};

	temp_buf = tee_shm_get_va(shm, 0);
	if (IS_ERR(temp_buf)) {
		dev_err(&chip->dev, "%s: tee_shm_get_va failed for transmit\n",
			__func__);
		return PTR_ERR(temp_buf);
	}
	memset(temp_buf, 0, (MAX_COMMAND_SIZE + MAX_RESPONSE_SIZE));
	memcpy(temp_buf, buf, len);

	command_params[1] = (struct tee_param) {
		.attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT,
		.u.memref = {
			.shm = shm,
			.size = MAX_RESPONSE_SIZE,
			.shm_offs = MAX_COMMAND_SIZE,
		},
	};

	rc = tee_client_invoke_func(pvt_data->ctx, &transceive_args,
				    command_params);
	if ((rc < 0) || (transceive_args.ret != 0)) {
		dev_err(&chip->dev, "%s: SUBMIT_COMMAND invoke error: 0x%x\n",
			__func__, transceive_args.ret);
		return (rc < 0) ? rc : transceive_args.ret;
	}

	temp_buf = tee_shm_get_va(shm, command_params[1].u.memref.shm_offs);
	if (IS_ERR(temp_buf)) {
		dev_err(&chip->dev, "%s: tee_shm_get_va failed for receive\n",
			__func__);
		return PTR_ERR(temp_buf);
	}

	resp_header = (struct tpm_header *)temp_buf;
	resp_len = be32_to_cpu(resp_header->length);

	 
	if (resp_len < TPM_HEADER_SIZE) {
		dev_err(&chip->dev, "%s: tpm response header too small\n",
			__func__);
		return -EIO;
	}
	if (resp_len > MAX_RESPONSE_SIZE) {
		dev_err(&chip->dev,
			"%s: resp_len=%zd exceeds MAX_RESPONSE_SIZE\n",
			__func__, resp_len);
		return -EIO;
	}

	 
	memcpy(pvt_data->resp_buf, temp_buf, resp_len);
	pvt_data->resp_len = resp_len;

	return 0;
}

static void ftpm_tee_tpm_op_cancel(struct tpm_chip *chip)
{
	 
}

static u8 ftpm_tee_tpm_op_status(struct tpm_chip *chip)
{
	return 0;
}

static bool ftpm_tee_tpm_req_canceled(struct tpm_chip *chip, u8 status)
{
	return false;
}

static const struct tpm_class_ops ftpm_tee_tpm_ops = {
	.flags = TPM_OPS_AUTO_STARTUP,
	.recv = ftpm_tee_tpm_op_recv,
	.send = ftpm_tee_tpm_op_send,
	.cancel = ftpm_tee_tpm_op_cancel,
	.status = ftpm_tee_tpm_op_status,
	.req_complete_mask = 0,
	.req_complete_val = 0,
	.req_canceled = ftpm_tee_tpm_req_canceled,
};

 
static int ftpm_tee_match(struct tee_ioctl_version_data *ver, const void *data)
{
	 
	if ((ver->impl_id == TEE_IMPL_ID_OPTEE) &&
		(ver->gen_caps & TEE_GEN_CAP_GP))
		return 1;
	else
		return 0;
}

 
static int ftpm_tee_probe(struct device *dev)
{
	int rc;
	struct tpm_chip *chip;
	struct ftpm_tee_private *pvt_data = NULL;
	struct tee_ioctl_open_session_arg sess_arg;

	pvt_data = devm_kzalloc(dev, sizeof(struct ftpm_tee_private),
				GFP_KERNEL);
	if (!pvt_data)
		return -ENOMEM;

	dev_set_drvdata(dev, pvt_data);

	 
	pvt_data->ctx = tee_client_open_context(NULL, ftpm_tee_match, NULL,
						NULL);
	if (IS_ERR(pvt_data->ctx)) {
		if (PTR_ERR(pvt_data->ctx) == -ENOENT)
			return -EPROBE_DEFER;
		dev_err(dev, "%s: tee_client_open_context failed\n", __func__);
		return PTR_ERR(pvt_data->ctx);
	}

	 
	memset(&sess_arg, 0, sizeof(sess_arg));
	export_uuid(sess_arg.uuid, &ftpm_ta_uuid);
	sess_arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	sess_arg.num_params = 0;

	rc = tee_client_open_session(pvt_data->ctx, &sess_arg, NULL);
	if ((rc < 0) || (sess_arg.ret != 0)) {
		dev_err(dev, "%s: tee_client_open_session failed, err=%x\n",
			__func__, sess_arg.ret);
		rc = -EINVAL;
		goto out_tee_session;
	}
	pvt_data->session = sess_arg.session;

	 
	pvt_data->shm = tee_shm_alloc_kernel_buf(pvt_data->ctx,
						 MAX_COMMAND_SIZE +
						 MAX_RESPONSE_SIZE);
	if (IS_ERR(pvt_data->shm)) {
		dev_err(dev, "%s: tee_shm_alloc_kernel_buf failed\n", __func__);
		rc = -ENOMEM;
		goto out_shm_alloc;
	}

	 
	chip = tpm_chip_alloc(dev, &ftpm_tee_tpm_ops);
	if (IS_ERR(chip)) {
		dev_err(dev, "%s: tpm_chip_alloc failed\n", __func__);
		rc = PTR_ERR(chip);
		goto out_chip_alloc;
	}

	pvt_data->chip = chip;
	pvt_data->chip->flags |= TPM_CHIP_FLAG_TPM2;

	 
	rc = tpm_chip_register(pvt_data->chip);
	if (rc) {
		dev_err(dev, "%s: tpm_chip_register failed with rc=%d\n",
			__func__, rc);
		goto out_chip;
	}

	return 0;

out_chip:
	put_device(&pvt_data->chip->dev);
out_chip_alloc:
	tee_shm_free(pvt_data->shm);
out_shm_alloc:
	tee_client_close_session(pvt_data->ctx, pvt_data->session);
out_tee_session:
	tee_client_close_context(pvt_data->ctx);

	return rc;
}

static int ftpm_plat_tee_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	return ftpm_tee_probe(dev);
}

 
static int ftpm_tee_remove(struct device *dev)
{
	struct ftpm_tee_private *pvt_data = dev_get_drvdata(dev);

	 
	tpm_chip_unregister(pvt_data->chip);

	 
	put_device(&pvt_data->chip->dev);

	 
	tee_shm_free(pvt_data->shm);

	 
	tee_client_close_session(pvt_data->ctx, pvt_data->session);

	 
	tee_client_close_context(pvt_data->ctx);

	 

	return 0;
}

static void ftpm_plat_tee_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	ftpm_tee_remove(dev);
}

 
static void ftpm_plat_tee_shutdown(struct platform_device *pdev)
{
	struct ftpm_tee_private *pvt_data = dev_get_drvdata(&pdev->dev);

	tee_shm_free(pvt_data->shm);
	tee_client_close_session(pvt_data->ctx, pvt_data->session);
	tee_client_close_context(pvt_data->ctx);
}

static const struct of_device_id of_ftpm_tee_ids[] = {
	{ .compatible = "microsoft,ftpm" },
	{ }
};
MODULE_DEVICE_TABLE(of, of_ftpm_tee_ids);

static struct platform_driver ftpm_tee_plat_driver = {
	.driver = {
		.name = "ftpm-tee",
		.of_match_table = of_match_ptr(of_ftpm_tee_ids),
	},
	.shutdown = ftpm_plat_tee_shutdown,
	.probe = ftpm_plat_tee_probe,
	.remove_new = ftpm_plat_tee_remove,
};

 
static const struct tee_client_device_id optee_ftpm_id_table[] = {
	{UUID_INIT(0xbc50d971, 0xd4c9, 0x42c4,
		   0x82, 0xcb, 0x34, 0x3f, 0xb7, 0xf3, 0x78, 0x96)},
	{}
};

MODULE_DEVICE_TABLE(tee, optee_ftpm_id_table);

static struct tee_client_driver ftpm_tee_driver = {
	.id_table	= optee_ftpm_id_table,
	.driver		= {
		.name		= "optee-ftpm",
		.bus		= &tee_bus_type,
		.probe		= ftpm_tee_probe,
		.remove		= ftpm_tee_remove,
	},
};

static int __init ftpm_mod_init(void)
{
	int rc;

	rc = platform_driver_register(&ftpm_tee_plat_driver);
	if (rc)
		return rc;

	rc = driver_register(&ftpm_tee_driver.driver);
	if (rc) {
		platform_driver_unregister(&ftpm_tee_plat_driver);
		return rc;
	}

	return 0;
}

static void __exit ftpm_mod_exit(void)
{
	platform_driver_unregister(&ftpm_tee_plat_driver);
	driver_unregister(&ftpm_tee_driver.driver);
}

module_init(ftpm_mod_init);
module_exit(ftpm_mod_exit);

MODULE_AUTHOR("Thirupathaiah Annapureddy <thiruan@microsoft.com>");
MODULE_DESCRIPTION("TPM Driver for fTPM TA in TEE");
MODULE_LICENSE("GPL v2");
