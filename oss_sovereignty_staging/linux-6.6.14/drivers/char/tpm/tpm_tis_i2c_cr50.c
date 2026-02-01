
 

#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include "tpm_tis_core.h"

#define TPM_CR50_MAX_BUFSIZE		64
#define TPM_CR50_TIMEOUT_SHORT_MS	2		 
#define TPM_CR50_TIMEOUT_NOIRQ_MS	20		 
#define TPM_CR50_I2C_DID_VID		0x00281ae0L	 
#define TPM_TI50_I2C_DID_VID		0x504a6666L	 
#define TPM_CR50_I2C_MAX_RETRIES	3		 
#define TPM_CR50_I2C_RETRY_DELAY_LO	55		 
#define TPM_CR50_I2C_RETRY_DELAY_HI	65		 

#define TPM_I2C_ACCESS(l)	(0x0000 | ((l) << 4))
#define TPM_I2C_STS(l)		(0x0001 | ((l) << 4))
#define TPM_I2C_DATA_FIFO(l)	(0x0005 | ((l) << 4))
#define TPM_I2C_DID_VID(l)	(0x0006 | ((l) << 4))

 
struct tpm_i2c_cr50_priv_data {
	int irq;
	struct completion tpm_ready;
	u8 buf[TPM_CR50_MAX_BUFSIZE];
};

 
static irqreturn_t tpm_cr50_i2c_int_handler(int dummy, void *tpm_info)
{
	struct tpm_chip *chip = tpm_info;
	struct tpm_i2c_cr50_priv_data *priv = dev_get_drvdata(&chip->dev);

	complete(&priv->tpm_ready);

	return IRQ_HANDLED;
}

 
static int tpm_cr50_i2c_wait_tpm_ready(struct tpm_chip *chip)
{
	struct tpm_i2c_cr50_priv_data *priv = dev_get_drvdata(&chip->dev);

	 
	if (priv->irq <= 0) {
		msleep(TPM_CR50_TIMEOUT_NOIRQ_MS);
		return 0;
	}

	 
	if (!wait_for_completion_timeout(&priv->tpm_ready, chip->timeout_a)) {
		dev_warn(&chip->dev, "Timeout waiting for TPM ready\n");
		return -ETIMEDOUT;
	}

	return 0;
}

 
static void tpm_cr50_i2c_enable_tpm_irq(struct tpm_chip *chip)
{
	struct tpm_i2c_cr50_priv_data *priv = dev_get_drvdata(&chip->dev);

	if (priv->irq > 0) {
		reinit_completion(&priv->tpm_ready);
		enable_irq(priv->irq);
	}
}

 
static void tpm_cr50_i2c_disable_tpm_irq(struct tpm_chip *chip)
{
	struct tpm_i2c_cr50_priv_data *priv = dev_get_drvdata(&chip->dev);

	if (priv->irq > 0)
		disable_irq(priv->irq);
}

 
static int tpm_cr50_i2c_transfer_message(struct device *dev,
					 struct i2c_adapter *adapter,
					 struct i2c_msg *msg)
{
	unsigned int try;
	int rc;

	for (try = 0; try < TPM_CR50_I2C_MAX_RETRIES; try++) {
		rc = __i2c_transfer(adapter, msg, 1);
		if (rc == 1)
			return 0;  
		if (try)
			dev_warn(dev, "i2c transfer failed (attempt %d/%d): %d\n",
				 try + 1, TPM_CR50_I2C_MAX_RETRIES, rc);
		usleep_range(TPM_CR50_I2C_RETRY_DELAY_LO, TPM_CR50_I2C_RETRY_DELAY_HI);
	}

	 
	return -EIO;
}

 
static int tpm_cr50_i2c_read(struct tpm_chip *chip, u8 addr, u8 *buffer, size_t len)
{
	struct i2c_client *client = to_i2c_client(chip->dev.parent);
	struct i2c_msg msg_reg_addr = {
		.addr = client->addr,
		.len = 1,
		.buf = &addr
	};
	struct i2c_msg msg_response = {
		.addr = client->addr,
		.flags = I2C_M_RD,
		.len = len,
		.buf = buffer
	};
	int rc;

	i2c_lock_bus(client->adapter, I2C_LOCK_SEGMENT);

	 
	tpm_cr50_i2c_enable_tpm_irq(chip);

	 
	rc = tpm_cr50_i2c_transfer_message(&chip->dev, client->adapter, &msg_reg_addr);
	if (rc < 0)
		goto out;

	 
	rc = tpm_cr50_i2c_wait_tpm_ready(chip);
	if (rc < 0)
		goto out;

	 
	rc = tpm_cr50_i2c_transfer_message(&chip->dev, client->adapter, &msg_response);

out:
	tpm_cr50_i2c_disable_tpm_irq(chip);
	i2c_unlock_bus(client->adapter, I2C_LOCK_SEGMENT);

	if (rc < 0)
		return rc;

	return 0;
}

 
static int tpm_cr50_i2c_write(struct tpm_chip *chip, u8 addr, u8 *buffer,
			      size_t len)
{
	struct tpm_i2c_cr50_priv_data *priv = dev_get_drvdata(&chip->dev);
	struct i2c_client *client = to_i2c_client(chip->dev.parent);
	struct i2c_msg msg = {
		.addr = client->addr,
		.len = len + 1,
		.buf = priv->buf
	};
	int rc;

	if (len > TPM_CR50_MAX_BUFSIZE - 1)
		return -EINVAL;

	 
	priv->buf[0] = addr;
	memcpy(priv->buf + 1, buffer, len);

	i2c_lock_bus(client->adapter, I2C_LOCK_SEGMENT);

	 
	tpm_cr50_i2c_enable_tpm_irq(chip);

	 
	rc = tpm_cr50_i2c_transfer_message(&chip->dev, client->adapter, &msg);
	if (rc < 0)
		goto out;

	 
	tpm_cr50_i2c_wait_tpm_ready(chip);

out:
	tpm_cr50_i2c_disable_tpm_irq(chip);
	i2c_unlock_bus(client->adapter, I2C_LOCK_SEGMENT);

	if (rc < 0)
		return rc;

	return 0;
}

 
static int tpm_cr50_check_locality(struct tpm_chip *chip)
{
	u8 mask = TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY;
	u8 buf;
	int rc;

	rc = tpm_cr50_i2c_read(chip, TPM_I2C_ACCESS(0), &buf, sizeof(buf));
	if (rc < 0)
		return rc;

	if ((buf & mask) == mask)
		return 0;

	return -EIO;
}

 
static void tpm_cr50_release_locality(struct tpm_chip *chip, bool force)
{
	u8 mask = TPM_ACCESS_VALID | TPM_ACCESS_REQUEST_PENDING;
	u8 addr = TPM_I2C_ACCESS(0);
	u8 buf;

	if (tpm_cr50_i2c_read(chip, addr, &buf, sizeof(buf)) < 0)
		return;

	if (force || (buf & mask) == mask) {
		buf = TPM_ACCESS_ACTIVE_LOCALITY;
		tpm_cr50_i2c_write(chip, addr, &buf, sizeof(buf));
	}
}

 
static int tpm_cr50_request_locality(struct tpm_chip *chip)
{
	u8 buf = TPM_ACCESS_REQUEST_USE;
	unsigned long stop;
	int rc;

	if (!tpm_cr50_check_locality(chip))
		return 0;

	rc = tpm_cr50_i2c_write(chip, TPM_I2C_ACCESS(0), &buf, sizeof(buf));
	if (rc < 0)
		return rc;

	stop = jiffies + chip->timeout_a;
	do {
		if (!tpm_cr50_check_locality(chip))
			return 0;

		msleep(TPM_CR50_TIMEOUT_SHORT_MS);
	} while (time_before(jiffies, stop));

	return -ETIMEDOUT;
}

 
static u8 tpm_cr50_i2c_tis_status(struct tpm_chip *chip)
{
	u8 buf[4];

	if (tpm_cr50_i2c_read(chip, TPM_I2C_STS(0), buf, sizeof(buf)) < 0)
		return 0;

	return buf[0];
}

 
static void tpm_cr50_i2c_tis_set_ready(struct tpm_chip *chip)
{
	u8 buf[4] = { TPM_STS_COMMAND_READY };

	tpm_cr50_i2c_write(chip, TPM_I2C_STS(0), buf, sizeof(buf));
	msleep(TPM_CR50_TIMEOUT_SHORT_MS);
}

 
static int tpm_cr50_i2c_get_burst_and_status(struct tpm_chip *chip, u8 mask,
					     size_t *burst, u32 *status)
{
	unsigned long stop;
	u8 buf[4];

	*status = 0;

	 
	stop = jiffies + chip->timeout_b;

	do {
		if (tpm_cr50_i2c_read(chip, TPM_I2C_STS(0), buf, sizeof(buf)) < 0) {
			msleep(TPM_CR50_TIMEOUT_SHORT_MS);
			continue;
		}

		*status = *buf;
		*burst = le16_to_cpup((__le16 *)(buf + 1));

		if ((*status & mask) == mask &&
		    *burst > 0 && *burst <= TPM_CR50_MAX_BUFSIZE - 1)
			return 0;

		msleep(TPM_CR50_TIMEOUT_SHORT_MS);
	} while (time_before(jiffies, stop));

	dev_err(&chip->dev, "Timeout reading burst and status\n");
	return -ETIMEDOUT;
}

 
static int tpm_cr50_i2c_tis_recv(struct tpm_chip *chip, u8 *buf, size_t buf_len)
{

	u8 mask = TPM_STS_VALID | TPM_STS_DATA_AVAIL;
	size_t burstcnt, cur, len, expected;
	u8 addr = TPM_I2C_DATA_FIFO(0);
	u32 status;
	int rc;

	if (buf_len < TPM_HEADER_SIZE)
		return -EINVAL;

	rc = tpm_cr50_i2c_get_burst_and_status(chip, mask, &burstcnt, &status);
	if (rc < 0)
		goto out_err;

	if (burstcnt > buf_len || burstcnt < TPM_HEADER_SIZE) {
		dev_err(&chip->dev,
			"Unexpected burstcnt: %zu (max=%zu, min=%d)\n",
			burstcnt, buf_len, TPM_HEADER_SIZE);
		rc = -EIO;
		goto out_err;
	}

	 
	rc = tpm_cr50_i2c_read(chip, addr, buf, burstcnt);
	if (rc < 0) {
		dev_err(&chip->dev, "Read of first chunk failed\n");
		goto out_err;
	}

	 
	expected = be32_to_cpup((__be32 *)(buf + 2));
	if (expected > buf_len) {
		dev_err(&chip->dev, "Buffer too small to receive i2c data\n");
		rc = -E2BIG;
		goto out_err;
	}

	 
	cur = burstcnt;
	while (cur < expected) {
		 
		rc = tpm_cr50_i2c_get_burst_and_status(chip, mask, &burstcnt, &status);
		if (rc < 0)
			goto out_err;

		len = min_t(size_t, burstcnt, expected - cur);
		rc = tpm_cr50_i2c_read(chip, addr, buf + cur, len);
		if (rc < 0) {
			dev_err(&chip->dev, "Read failed\n");
			goto out_err;
		}

		cur += len;
	}

	 
	rc = tpm_cr50_i2c_get_burst_and_status(chip, TPM_STS_VALID, &burstcnt, &status);
	if (rc < 0)
		goto out_err;
	if (status & TPM_STS_DATA_AVAIL) {
		dev_err(&chip->dev, "Data still available\n");
		rc = -EIO;
		goto out_err;
	}

	tpm_cr50_release_locality(chip, false);
	return cur;

out_err:
	 
	if (tpm_cr50_i2c_tis_status(chip) & TPM_STS_COMMAND_READY)
		tpm_cr50_i2c_tis_set_ready(chip);

	tpm_cr50_release_locality(chip, false);
	return rc;
}

 
static int tpm_cr50_i2c_tis_send(struct tpm_chip *chip, u8 *buf, size_t len)
{
	size_t burstcnt, limit, sent = 0;
	u8 tpm_go[4] = { TPM_STS_GO };
	unsigned long stop;
	u32 status;
	int rc;

	rc = tpm_cr50_request_locality(chip);
	if (rc < 0)
		return rc;

	 
	stop = jiffies + chip->timeout_b;
	while (!(tpm_cr50_i2c_tis_status(chip) & TPM_STS_COMMAND_READY)) {
		if (time_after(jiffies, stop)) {
			rc = -ETIMEDOUT;
			goto out_err;
		}

		tpm_cr50_i2c_tis_set_ready(chip);
	}

	while (len > 0) {
		u8 mask = TPM_STS_VALID;

		 
		if (sent > 0)
			mask |= TPM_STS_DATA_EXPECT;

		 
		rc = tpm_cr50_i2c_get_burst_and_status(chip, mask, &burstcnt, &status);
		if (rc < 0)
			goto out_err;

		 
		limit = min_t(size_t, burstcnt - 1, len);
		rc = tpm_cr50_i2c_write(chip, TPM_I2C_DATA_FIFO(0), &buf[sent], limit);
		if (rc < 0) {
			dev_err(&chip->dev, "Write failed\n");
			goto out_err;
		}

		sent += limit;
		len -= limit;
	}

	 
	rc = tpm_cr50_i2c_get_burst_and_status(chip, TPM_STS_VALID, &burstcnt, &status);
	if (rc < 0)
		goto out_err;
	if (status & TPM_STS_DATA_EXPECT) {
		dev_err(&chip->dev, "Data still expected\n");
		rc = -EIO;
		goto out_err;
	}

	 
	rc = tpm_cr50_i2c_write(chip, TPM_I2C_STS(0), tpm_go,
				sizeof(tpm_go));
	if (rc < 0) {
		dev_err(&chip->dev, "Start command failed\n");
		goto out_err;
	}
	return 0;

out_err:
	 
	if (tpm_cr50_i2c_tis_status(chip) & TPM_STS_COMMAND_READY)
		tpm_cr50_i2c_tis_set_ready(chip);

	tpm_cr50_release_locality(chip, false);
	return rc;
}

 
static bool tpm_cr50_i2c_req_canceled(struct tpm_chip *chip, u8 status)
{
	return status == TPM_STS_COMMAND_READY;
}

static bool tpm_cr50_i2c_is_firmware_power_managed(struct device *dev)
{
	u8 val;
	int ret;

	 
	ret = device_property_read_u8(dev, "firmware-power-managed", &val);
	if (ret)
		return true;

	return val;
}

static const struct tpm_class_ops cr50_i2c = {
	.flags = TPM_OPS_AUTO_STARTUP,
	.status = &tpm_cr50_i2c_tis_status,
	.recv = &tpm_cr50_i2c_tis_recv,
	.send = &tpm_cr50_i2c_tis_send,
	.cancel = &tpm_cr50_i2c_tis_set_ready,
	.req_complete_mask = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_complete_val = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_canceled = &tpm_cr50_i2c_req_canceled,
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id cr50_i2c_acpi_id[] = {
	{ "GOOG0005", 0 },
	{}
};
MODULE_DEVICE_TABLE(acpi, cr50_i2c_acpi_id);
#endif

#ifdef CONFIG_OF
static const struct of_device_id of_cr50_i2c_match[] = {
	{ .compatible = "google,cr50", },
	{}
};
MODULE_DEVICE_TABLE(of, of_cr50_i2c_match);
#endif

 
static int tpm_cr50_i2c_probe(struct i2c_client *client)
{
	struct tpm_i2c_cr50_priv_data *priv;
	struct device *dev = &client->dev;
	struct tpm_chip *chip;
	u32 vendor;
	u8 buf[4];
	int rc;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	chip = tpmm_chip_alloc(dev, &cr50_i2c);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	 
	chip->flags |= TPM_CHIP_FLAG_TPM2;
	if (tpm_cr50_i2c_is_firmware_power_managed(dev))
		chip->flags |= TPM_CHIP_FLAG_FIRMWARE_POWER_MANAGED;

	 
	chip->timeout_a = msecs_to_jiffies(TIS_SHORT_TIMEOUT);
	chip->timeout_b = msecs_to_jiffies(TIS_LONG_TIMEOUT);
	chip->timeout_c = msecs_to_jiffies(TIS_SHORT_TIMEOUT);
	chip->timeout_d = msecs_to_jiffies(TIS_SHORT_TIMEOUT);

	dev_set_drvdata(&chip->dev, priv);
	init_completion(&priv->tpm_ready);

	if (client->irq > 0) {
		rc = devm_request_irq(dev, client->irq, tpm_cr50_i2c_int_handler,
				      IRQF_TRIGGER_FALLING | IRQF_ONESHOT |
				      IRQF_NO_AUTOEN,
				      dev->driver->name, chip);
		if (rc < 0) {
			dev_err(dev, "Failed to probe IRQ %d\n", client->irq);
			return rc;
		}

		priv->irq = client->irq;
	} else {
		dev_warn(dev, "No IRQ, will use %ums delay for TPM ready\n",
			 TPM_CR50_TIMEOUT_NOIRQ_MS);
	}

	rc = tpm_cr50_request_locality(chip);
	if (rc < 0) {
		dev_err(dev, "Could not request locality\n");
		return rc;
	}

	 
	rc = tpm_cr50_i2c_read(chip, TPM_I2C_DID_VID(0), buf, sizeof(buf));
	if (rc < 0) {
		dev_err(dev, "Could not read vendor id\n");
		tpm_cr50_release_locality(chip, true);
		return rc;
	}

	vendor = le32_to_cpup((__le32 *)buf);
	if (vendor != TPM_CR50_I2C_DID_VID && vendor != TPM_TI50_I2C_DID_VID) {
		dev_err(dev, "Vendor ID did not match! ID was %08x\n", vendor);
		tpm_cr50_release_locality(chip, true);
		return -ENODEV;
	}

	dev_info(dev, "%s TPM 2.0 (i2c 0x%02x irq %d id 0x%x)\n",
		 vendor == TPM_TI50_I2C_DID_VID ? "ti50" : "cr50",
		 client->addr, client->irq, vendor >> 16);
	return tpm_chip_register(chip);
}

 
static void tpm_cr50_i2c_remove(struct i2c_client *client)
{
	struct tpm_chip *chip = i2c_get_clientdata(client);
	struct device *dev = &client->dev;

	if (!chip) {
		dev_crit(dev, "Could not get client data at remove, memory corruption ahead\n");
		return;
	}

	tpm_chip_unregister(chip);
	tpm_cr50_release_locality(chip, true);
}

static SIMPLE_DEV_PM_OPS(cr50_i2c_pm, tpm_pm_suspend, tpm_pm_resume);

static struct i2c_driver cr50_i2c_driver = {
	.probe = tpm_cr50_i2c_probe,
	.remove = tpm_cr50_i2c_remove,
	.driver = {
		.name = "cr50_i2c",
		.pm = &cr50_i2c_pm,
		.acpi_match_table = ACPI_PTR(cr50_i2c_acpi_id),
		.of_match_table = of_match_ptr(of_cr50_i2c_match),
	},
};

module_i2c_driver(cr50_i2c_driver);

MODULE_DESCRIPTION("cr50 TPM I2C Driver");
MODULE_LICENSE("GPL");
