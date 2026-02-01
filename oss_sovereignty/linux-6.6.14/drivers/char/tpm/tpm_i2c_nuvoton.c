
  

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/of_device.h>
#include "tpm.h"

 
#define TPM_STS			0x00
#define TPM_BURST_COUNT		0x01
#define TPM_DATA_FIFO_W		0x20
#define TPM_DATA_FIFO_R		0x40
#define TPM_VID_DID_RID		0x60
#define TPM_I2C_RETRIES		5
 
#define TPM_I2C_MAX_BUF_SIZE           32
#define TPM_I2C_RETRY_COUNT            32
#define TPM_I2C_BUS_DELAY              1000      	 
#define TPM_I2C_RETRY_DELAY_SHORT      (2 * 1000)	 
#define TPM_I2C_RETRY_DELAY_LONG       (10 * 1000) 	 
#define TPM_I2C_DELAY_RANGE            300		 

#define OF_IS_TPM2 ((void *)1)
#define I2C_IS_TPM2 1

struct priv_data {
	int irq;
	unsigned int intrs;
	wait_queue_head_t read_queue;
};

static s32 i2c_nuvoton_read_buf(struct i2c_client *client, u8 offset, u8 size,
				u8 *data)
{
	s32 status;

	status = i2c_smbus_read_i2c_block_data(client, offset, size, data);
	dev_dbg(&client->dev,
		"%s(offset=%u size=%u data=%*ph) -> sts=%d\n", __func__,
		offset, size, (int)size, data, status);
	return status;
}

static s32 i2c_nuvoton_write_buf(struct i2c_client *client, u8 offset, u8 size,
				 u8 *data)
{
	s32 status;

	status = i2c_smbus_write_i2c_block_data(client, offset, size, data);
	dev_dbg(&client->dev,
		"%s(offset=%u size=%u data=%*ph) -> sts=%d\n", __func__,
		offset, size, (int)size, data, status);
	return status;
}

#define TPM_STS_VALID          0x80
#define TPM_STS_COMMAND_READY  0x40
#define TPM_STS_GO             0x20
#define TPM_STS_DATA_AVAIL     0x10
#define TPM_STS_EXPECT         0x08
#define TPM_STS_RESPONSE_RETRY 0x02
#define TPM_STS_ERR_VAL        0x07     

#define TPM_I2C_SHORT_TIMEOUT  750      
#define TPM_I2C_LONG_TIMEOUT   2000     

 
static u8 i2c_nuvoton_read_status(struct tpm_chip *chip)
{
	struct i2c_client *client = to_i2c_client(chip->dev.parent);
	s32 status;
	u8 data;

	status = i2c_nuvoton_read_buf(client, TPM_STS, 1, &data);
	if (status <= 0) {
		dev_err(&chip->dev, "%s() error return %d\n", __func__,
			status);
		data = TPM_STS_ERR_VAL;
	}

	return data;
}

 
static s32 i2c_nuvoton_write_status(struct i2c_client *client, u8 data)
{
	s32 status;
	int i;

	 
	for (i = 0, status = -1; i < TPM_I2C_RETRY_COUNT && status < 0; i++) {
		status = i2c_nuvoton_write_buf(client, TPM_STS, 1, &data);
		if (status < 0)
			usleep_range(TPM_I2C_BUS_DELAY, TPM_I2C_BUS_DELAY
				     + TPM_I2C_DELAY_RANGE);
	}
	return status;
}

 
static void i2c_nuvoton_ready(struct tpm_chip *chip)
{
	struct i2c_client *client = to_i2c_client(chip->dev.parent);
	s32 status;

	 
	status = i2c_nuvoton_write_status(client, TPM_STS_COMMAND_READY);
	if (status < 0)
		dev_err(&chip->dev,
			"%s() fail to write TPM_STS.commandReady\n", __func__);
}

 
static int i2c_nuvoton_get_burstcount(struct i2c_client *client,
				      struct tpm_chip *chip)
{
	unsigned long stop = jiffies + chip->timeout_d;
	s32 status;
	int burst_count = -1;
	u8 data;

	 
	do {
		 
		status = i2c_nuvoton_read_buf(client, TPM_BURST_COUNT, 1,
					      &data);
		if (status > 0 && data > 0) {
			burst_count = min_t(u8, TPM_I2C_MAX_BUF_SIZE, data);
			break;
		}
		usleep_range(TPM_I2C_BUS_DELAY, TPM_I2C_BUS_DELAY
			     + TPM_I2C_DELAY_RANGE);
	} while (time_before(jiffies, stop));

	return burst_count;
}

 
static bool i2c_nuvoton_check_status(struct tpm_chip *chip, u8 mask, u8 value)
{
	u8 status = i2c_nuvoton_read_status(chip);
	return (status != TPM_STS_ERR_VAL) && ((status & mask) == value);
}

static int i2c_nuvoton_wait_for_stat(struct tpm_chip *chip, u8 mask, u8 value,
				     u32 timeout, wait_queue_head_t *queue)
{
	if ((chip->flags & TPM_CHIP_FLAG_IRQ) && queue) {
		s32 rc;
		struct priv_data *priv = dev_get_drvdata(&chip->dev);
		unsigned int cur_intrs = priv->intrs;

		enable_irq(priv->irq);
		rc = wait_event_interruptible_timeout(*queue,
						      cur_intrs != priv->intrs,
						      timeout);
		if (rc > 0)
			return 0;
		 
	} else {
		unsigned long ten_msec, stop;
		bool status_valid;

		 
		status_valid = i2c_nuvoton_check_status(chip, mask, value);
		if (status_valid)
			return 0;

		 
		ten_msec = jiffies + usecs_to_jiffies(TPM_I2C_RETRY_DELAY_LONG);
		stop = jiffies + timeout;
		do {
			if (time_before(jiffies, ten_msec))
				usleep_range(TPM_I2C_RETRY_DELAY_SHORT,
					     TPM_I2C_RETRY_DELAY_SHORT
					     + TPM_I2C_DELAY_RANGE);
			else
				usleep_range(TPM_I2C_RETRY_DELAY_LONG,
					     TPM_I2C_RETRY_DELAY_LONG
					     + TPM_I2C_DELAY_RANGE);
			status_valid = i2c_nuvoton_check_status(chip, mask,
								value);
			if (status_valid)
				return 0;
		} while (time_before(jiffies, stop));
	}
	dev_err(&chip->dev, "%s(%02x, %02x) -> timeout\n", __func__, mask,
		value);
	return -ETIMEDOUT;
}

 
static int i2c_nuvoton_wait_for_data_avail(struct tpm_chip *chip, u32 timeout,
					   wait_queue_head_t *queue)
{
	return i2c_nuvoton_wait_for_stat(chip,
					 TPM_STS_DATA_AVAIL | TPM_STS_VALID,
					 TPM_STS_DATA_AVAIL | TPM_STS_VALID,
					 timeout, queue);
}

 
static int i2c_nuvoton_recv_data(struct i2c_client *client,
				 struct tpm_chip *chip, u8 *buf, size_t count)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	s32 rc;
	int burst_count, bytes2read, size = 0;

	while (size < count &&
	       i2c_nuvoton_wait_for_data_avail(chip,
					       chip->timeout_c,
					       &priv->read_queue) == 0) {
		burst_count = i2c_nuvoton_get_burstcount(client, chip);
		if (burst_count < 0) {
			dev_err(&chip->dev,
				"%s() fail to read burstCount=%d\n", __func__,
				burst_count);
			return -EIO;
		}
		bytes2read = min_t(size_t, burst_count, count - size);
		rc = i2c_nuvoton_read_buf(client, TPM_DATA_FIFO_R,
					  bytes2read, &buf[size]);
		if (rc < 0) {
			dev_err(&chip->dev,
				"%s() fail on i2c_nuvoton_read_buf()=%d\n",
				__func__, rc);
			return -EIO;
		}
		dev_dbg(&chip->dev, "%s(%d):", __func__, bytes2read);
		size += bytes2read;
	}

	return size;
}

 
static int i2c_nuvoton_recv(struct tpm_chip *chip, u8 *buf, size_t count)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	struct device *dev = chip->dev.parent;
	struct i2c_client *client = to_i2c_client(dev);
	s32 rc;
	int status;
	int burst_count;
	int retries;
	int size = 0;
	u32 expected;

	if (count < TPM_HEADER_SIZE) {
		i2c_nuvoton_ready(chip);     
		dev_err(dev, "%s() count < header size\n", __func__);
		return -EIO;
	}
	for (retries = 0; retries < TPM_I2C_RETRIES; retries++) {
		if (retries > 0) {
			 
			i2c_nuvoton_write_status(client,
						 TPM_STS_RESPONSE_RETRY);
		}
		 
		status = i2c_nuvoton_wait_for_data_avail(
			chip, chip->timeout_c, &priv->read_queue);
		if (status != 0) {
			dev_err(dev, "%s() timeout on dataAvail\n", __func__);
			size = -ETIMEDOUT;
			continue;
		}
		burst_count = i2c_nuvoton_get_burstcount(client, chip);
		if (burst_count < 0) {
			dev_err(dev, "%s() fail to get burstCount\n", __func__);
			size = -EIO;
			continue;
		}
		size = i2c_nuvoton_recv_data(client, chip, buf,
					     burst_count);
		if (size < TPM_HEADER_SIZE) {
			dev_err(dev, "%s() fail to read header\n", __func__);
			size = -EIO;
			continue;
		}
		 
		expected = be32_to_cpu(*(__be32 *) (buf + 2));
		if (expected > count || expected < size) {
			dev_err(dev, "%s() expected > count\n", __func__);
			size = -EIO;
			continue;
		}
		rc = i2c_nuvoton_recv_data(client, chip, &buf[size],
					   expected - size);
		size += rc;
		if (rc < 0 || size < expected) {
			dev_err(dev, "%s() fail to read remainder of result\n",
				__func__);
			size = -EIO;
			continue;
		}
		if (i2c_nuvoton_wait_for_stat(
			    chip, TPM_STS_VALID | TPM_STS_DATA_AVAIL,
			    TPM_STS_VALID, chip->timeout_c,
			    NULL)) {
			dev_err(dev, "%s() error left over data\n", __func__);
			size = -ETIMEDOUT;
			continue;
		}
		break;
	}
	i2c_nuvoton_ready(chip);
	dev_dbg(&chip->dev, "%s() -> %d\n", __func__, size);
	return size;
}

 
static int i2c_nuvoton_send(struct tpm_chip *chip, u8 *buf, size_t len)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	struct device *dev = chip->dev.parent;
	struct i2c_client *client = to_i2c_client(dev);
	u32 ordinal;
	unsigned long duration;
	size_t count = 0;
	int burst_count, bytes2write, retries, rc = -EIO;

	for (retries = 0; retries < TPM_RETRY; retries++) {
		i2c_nuvoton_ready(chip);
		if (i2c_nuvoton_wait_for_stat(chip, TPM_STS_COMMAND_READY,
					      TPM_STS_COMMAND_READY,
					      chip->timeout_b, NULL)) {
			dev_err(dev, "%s() timeout on commandReady\n",
				__func__);
			rc = -EIO;
			continue;
		}
		rc = 0;
		while (count < len - 1) {
			burst_count = i2c_nuvoton_get_burstcount(client,
								 chip);
			if (burst_count < 0) {
				dev_err(dev, "%s() fail get burstCount\n",
					__func__);
				rc = -EIO;
				break;
			}
			bytes2write = min_t(size_t, burst_count,
					    len - 1 - count);
			rc = i2c_nuvoton_write_buf(client, TPM_DATA_FIFO_W,
						   bytes2write, &buf[count]);
			if (rc < 0) {
				dev_err(dev, "%s() fail i2cWriteBuf\n",
					__func__);
				break;
			}
			dev_dbg(dev, "%s(%d):", __func__, bytes2write);
			count += bytes2write;
			rc = i2c_nuvoton_wait_for_stat(chip,
						       TPM_STS_VALID |
						       TPM_STS_EXPECT,
						       TPM_STS_VALID |
						       TPM_STS_EXPECT,
						       chip->timeout_c,
						       NULL);
			if (rc < 0) {
				dev_err(dev, "%s() timeout on Expect\n",
					__func__);
				rc = -ETIMEDOUT;
				break;
			}
		}
		if (rc < 0)
			continue;

		 
		rc = i2c_nuvoton_write_buf(client, TPM_DATA_FIFO_W, 1,
					   &buf[count]);
		if (rc < 0) {
			dev_err(dev, "%s() fail to write last byte\n",
				__func__);
			rc = -EIO;
			continue;
		}
		dev_dbg(dev, "%s(last): %02x", __func__, buf[count]);
		rc = i2c_nuvoton_wait_for_stat(chip,
					       TPM_STS_VALID | TPM_STS_EXPECT,
					       TPM_STS_VALID,
					       chip->timeout_c, NULL);
		if (rc) {
			dev_err(dev, "%s() timeout on Expect to clear\n",
				__func__);
			rc = -ETIMEDOUT;
			continue;
		}
		break;
	}
	if (rc < 0) {
		 
		i2c_nuvoton_ready(chip);
		return rc;
	}
	 
	rc = i2c_nuvoton_write_status(client, TPM_STS_GO);
	if (rc < 0) {
		dev_err(dev, "%s() fail to write Go\n", __func__);
		i2c_nuvoton_ready(chip);
		return rc;
	}
	ordinal = be32_to_cpu(*((__be32 *) (buf + 6)));
	duration = tpm_calc_ordinal_duration(chip, ordinal);

	rc = i2c_nuvoton_wait_for_data_avail(chip, duration, &priv->read_queue);
	if (rc) {
		dev_err(dev, "%s() timeout command duration %ld\n",
			__func__, duration);
		i2c_nuvoton_ready(chip);
		return rc;
	}

	dev_dbg(dev, "%s() -> %zd\n", __func__, len);
	return 0;
}

static bool i2c_nuvoton_req_canceled(struct tpm_chip *chip, u8 status)
{
	return (status == TPM_STS_COMMAND_READY);
}

static const struct tpm_class_ops tpm_i2c = {
	.flags = TPM_OPS_AUTO_STARTUP,
	.status = i2c_nuvoton_read_status,
	.recv = i2c_nuvoton_recv,
	.send = i2c_nuvoton_send,
	.cancel = i2c_nuvoton_ready,
	.req_complete_mask = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_complete_val = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_canceled = i2c_nuvoton_req_canceled,
};

 
static irqreturn_t i2c_nuvoton_int_handler(int dummy, void *dev_id)
{
	struct tpm_chip *chip = dev_id;
	struct priv_data *priv = dev_get_drvdata(&chip->dev);

	priv->intrs++;
	wake_up(&priv->read_queue);
	disable_irq_nosync(priv->irq);
	return IRQ_HANDLED;
}

static int get_vid(struct i2c_client *client, u32 *res)
{
	static const u8 vid_did_rid_value[] = { 0x50, 0x10, 0xfe };
	u32 temp;
	s32 rc;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;
	rc = i2c_nuvoton_read_buf(client, TPM_VID_DID_RID, 4, (u8 *)&temp);
	if (rc < 0)
		return rc;

	 
	if (memcmp(&temp, vid_did_rid_value, sizeof(vid_did_rid_value))) {
		 
		rc = i2c_nuvoton_read_buf(client, TPM_DATA_FIFO_W, 4,
					  (u8 *) (&temp));
		if (rc < 0)
			return rc;

		 
		if (memcmp(&temp, vid_did_rid_value,
			   sizeof(vid_did_rid_value)))
			return -ENODEV;
	}

	*res = temp;
	return 0;
}

static int i2c_nuvoton_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	int rc;
	struct tpm_chip *chip;
	struct device *dev = &client->dev;
	struct priv_data *priv;
	u32 vid = 0;

	rc = get_vid(client, &vid);
	if (rc)
		return rc;

	dev_info(dev, "VID: %04X DID: %02X RID: %02X\n", (u16) vid,
		 (u8) (vid >> 16), (u8) (vid >> 24));

	chip = tpmm_chip_alloc(dev, &tpm_i2c);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	priv = devm_kzalloc(dev, sizeof(struct priv_data), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (dev->of_node) {
		const struct of_device_id *of_id;

		of_id = of_match_device(dev->driver->of_match_table, dev);
		if (of_id && of_id->data == OF_IS_TPM2)
			chip->flags |= TPM_CHIP_FLAG_TPM2;
	} else
		if (id->driver_data == I2C_IS_TPM2)
			chip->flags |= TPM_CHIP_FLAG_TPM2;

	init_waitqueue_head(&priv->read_queue);

	 
	chip->timeout_a = msecs_to_jiffies(TPM_I2C_SHORT_TIMEOUT);
	chip->timeout_b = msecs_to_jiffies(TPM_I2C_LONG_TIMEOUT);
	chip->timeout_c = msecs_to_jiffies(TPM_I2C_SHORT_TIMEOUT);
	chip->timeout_d = msecs_to_jiffies(TPM_I2C_SHORT_TIMEOUT);

	dev_set_drvdata(&chip->dev, priv);

	 
	priv->irq = client->irq;
	if (client->irq) {
		dev_dbg(dev, "%s() priv->irq\n", __func__);
		rc = devm_request_irq(dev, client->irq,
				      i2c_nuvoton_int_handler,
				      IRQF_TRIGGER_LOW,
				      dev_name(&chip->dev),
				      chip);
		if (rc) {
			dev_err(dev, "%s() Unable to request irq: %d for use\n",
				__func__, priv->irq);
			priv->irq = 0;
		} else {
			chip->flags |= TPM_CHIP_FLAG_IRQ;
			 
			i2c_nuvoton_ready(chip);
			 
			rc = i2c_nuvoton_wait_for_stat(chip,
						       TPM_STS_COMMAND_READY,
						       TPM_STS_COMMAND_READY,
						       chip->timeout_b,
						       NULL);
			if (rc == 0) {
				 
				rc = i2c_nuvoton_write_buf(client,
							   TPM_DATA_FIFO_W,
							   1, (u8 *) (&rc));
				if (rc < 0)
					return rc;
				 
				i2c_nuvoton_ready(chip);
			} else {
				 
				if (i2c_nuvoton_read_status(chip) !=
				    TPM_STS_VALID)
					return -EIO;
			}
		}
	}

	return tpm_chip_register(chip);
}

static void i2c_nuvoton_remove(struct i2c_client *client)
{
	struct tpm_chip *chip = i2c_get_clientdata(client);

	tpm_chip_unregister(chip);
}

static const struct i2c_device_id i2c_nuvoton_id[] = {
	{"tpm_i2c_nuvoton"},
	{"tpm2_i2c_nuvoton", .driver_data = I2C_IS_TPM2},
	{}
};
MODULE_DEVICE_TABLE(i2c, i2c_nuvoton_id);

#ifdef CONFIG_OF
static const struct of_device_id i2c_nuvoton_of_match[] = {
	{.compatible = "nuvoton,npct501"},
	{.compatible = "winbond,wpct301"},
	{.compatible = "nuvoton,npct601", .data = OF_IS_TPM2},
	{},
};
MODULE_DEVICE_TABLE(of, i2c_nuvoton_of_match);
#endif

static SIMPLE_DEV_PM_OPS(i2c_nuvoton_pm_ops, tpm_pm_suspend, tpm_pm_resume);

static struct i2c_driver i2c_nuvoton_driver = {
	.id_table = i2c_nuvoton_id,
	.probe = i2c_nuvoton_probe,
	.remove = i2c_nuvoton_remove,
	.driver = {
		.name = "tpm_i2c_nuvoton",
		.pm = &i2c_nuvoton_pm_ops,
		.of_match_table = of_match_ptr(i2c_nuvoton_of_match),
	},
};

module_i2c_driver(i2c_nuvoton_driver);

MODULE_AUTHOR("Dan Morav (dan.morav@nuvoton.com)");
MODULE_DESCRIPTION("Nuvoton TPM I2C Driver");
MODULE_LICENSE("GPL");
