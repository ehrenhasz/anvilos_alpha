
 

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/serdev.h>

#include "bno055_ser_trace.h"
#include "bno055.h"

 

 
#define BNO055_SER_XFER_BURST_BREAK_THRESHOLD 22

struct bno055_ser_priv {
	enum {
		CMD_NONE,
		CMD_READ,
		CMD_WRITE,
	} expect_response;
	int expected_data_len;
	u8 *response_buf;

	 
	enum {
		STATUS_CRIT = -1,
		STATUS_OK = 0,
		STATUS_FAIL = 1,
	} cmd_status;

	 
	struct mutex lock;

	 
	struct {
		enum {
			RX_IDLE,
			RX_START,
			RX_DATA,
		} state;
		int databuf_count;
		int expected_len;
		int type;
	} rx;

	 
	bool cmd_stale;

	struct completion cmd_complete;
	struct serdev_device *serdev;
};

static int bno055_ser_send_chunk(struct bno055_ser_priv *priv, const u8 *data, int len)
{
	int ret;

	trace_send_chunk(len, data);
	ret = serdev_device_write(priv->serdev, data, len, msecs_to_jiffies(25));
	if (ret < 0)
		return ret;

	if (ret < len)
		return -EIO;

	return 0;
}

 
static int bno055_ser_do_send_cmd(struct bno055_ser_priv *priv,
				  bool read, int addr, int len, const u8 *data)
{
	u8 hdr[] = {0xAA, read, addr, len};
	int chunk_len;
	int ret;

	ret = bno055_ser_send_chunk(priv, hdr, 2);
	if (ret)
		goto fail;
	usleep_range(2000, 3000);
	ret = bno055_ser_send_chunk(priv, hdr + 2, 2);
	if (ret)
		goto fail;

	if (read)
		return 0;

	while (len) {
		chunk_len = min(len, 2);
		usleep_range(2000, 3000);
		ret = bno055_ser_send_chunk(priv, data, chunk_len);
		if (ret)
			goto fail;
		data += chunk_len;
		len -= chunk_len;
	}

	return 0;
fail:
	 
	usleep_range(40000, 50000);
	return ret;
}

static int bno055_ser_send_cmd(struct bno055_ser_priv *priv,
			       bool read, int addr, int len, const u8 *data)
{
	const int retry_max = 5;
	int retry = retry_max;
	int ret = 0;

	 
	if (priv->cmd_stale) {
		ret = wait_for_completion_interruptible_timeout(&priv->cmd_complete,
								msecs_to_jiffies(100));
		if (ret == -ERESTARTSYS)
			return -ERESTARTSYS;

		priv->cmd_stale = false;
		 
		if (priv->cmd_status == STATUS_CRIT)
			return -EIO;
	}

	 
	do {
		mutex_lock(&priv->lock);
		priv->expect_response = read ? CMD_READ : CMD_WRITE;
		reinit_completion(&priv->cmd_complete);
		mutex_unlock(&priv->lock);

		if (retry != retry_max)
			trace_cmd_retry(read, addr, retry_max - retry);
		ret = bno055_ser_do_send_cmd(priv, read, addr, len, data);
		if (ret)
			continue;

		ret = wait_for_completion_interruptible_timeout(&priv->cmd_complete,
								msecs_to_jiffies(100));
		if (ret == -ERESTARTSYS) {
			priv->cmd_stale = true;
			return -ERESTARTSYS;
		}

		if (!ret)
			return -ETIMEDOUT;

		if (priv->cmd_status == STATUS_OK)
			return 0;
		if (priv->cmd_status == STATUS_CRIT)
			return -EIO;

		 
	} while (--retry);

	if (ret < 0)
		return ret;
	if (priv->cmd_status == STATUS_FAIL)
		return -EINVAL;
	return 0;
}

static int bno055_ser_write_reg(void *context, const void *_data, size_t count)
{
	const u8 *data = _data;
	struct bno055_ser_priv *priv = context;

	if (count < 2) {
		dev_err(&priv->serdev->dev, "Invalid write count %zu", count);
		return -EINVAL;
	}

	trace_write_reg(data[0], data[1]);
	return bno055_ser_send_cmd(priv, 0, data[0], count - 1, data + 1);
}

static int bno055_ser_read_reg(void *context,
			       const void *_reg, size_t reg_size,
			       void *val, size_t val_size)
{
	int ret;
	int reg_addr;
	const u8 *reg = _reg;
	struct bno055_ser_priv *priv = context;

	if (val_size > 128) {
		dev_err(&priv->serdev->dev, "Invalid read valsize %zu", val_size);
		return -EINVAL;
	}

	reg_addr = *reg;
	trace_read_reg(reg_addr, val_size);
	mutex_lock(&priv->lock);
	priv->expected_data_len = val_size;
	priv->response_buf = val;
	mutex_unlock(&priv->lock);

	ret = bno055_ser_send_cmd(priv, 1, reg_addr, val_size, NULL);

	mutex_lock(&priv->lock);
	priv->response_buf = NULL;
	mutex_unlock(&priv->lock);

	return ret;
}

 
static void bno055_ser_handle_rx(struct bno055_ser_priv *priv, int status)
{
	mutex_lock(&priv->lock);
	switch (priv->expect_response) {
	case CMD_NONE:
		dev_warn(&priv->serdev->dev, "received unexpected, yet valid, data from sensor");
		mutex_unlock(&priv->lock);
		return;

	case CMD_READ:
		priv->cmd_status = status;
		if (status == STATUS_OK &&
		    priv->rx.databuf_count != priv->expected_data_len) {
			 
			priv->cmd_status = STATUS_FAIL;
			dev_warn(&priv->serdev->dev,
				 "received an unexpected amount of, yet valid, data from sensor");
		}
		break;

	case CMD_WRITE:
		priv->cmd_status = status;
		break;
	}

	priv->expect_response = CMD_NONE;
	mutex_unlock(&priv->lock);
	complete(&priv->cmd_complete);
}

 
static int bno055_ser_receive_buf(struct serdev_device *serdev,
				  const unsigned char *buf, size_t size)
{
	int status;
	struct bno055_ser_priv *priv = serdev_device_get_drvdata(serdev);
	int remaining = size;

	if (size == 0)
		return 0;

	trace_recv(size, buf);
	switch (priv->rx.state) {
	case RX_IDLE:
		 
		if (buf[0] != 0xEE && buf[0] != 0xBB) {
			dev_err(&priv->serdev->dev,
				"Invalid packet start %x", buf[0]);
			bno055_ser_handle_rx(priv, STATUS_CRIT);
			break;
		}
		priv->rx.type = buf[0];
		priv->rx.state = RX_START;
		remaining--;
		buf++;
		priv->rx.databuf_count = 0;
		fallthrough;

	case RX_START:
		 
		if (remaining == 0)
			break;

		if (priv->rx.type == 0xEE) {
			if (remaining > 1) {
				dev_err(&priv->serdev->dev, "EE pkt. Extra data received");
				status = STATUS_CRIT;
			} else {
				status = (buf[0] == 1) ? STATUS_OK : STATUS_FAIL;
			}
			bno055_ser_handle_rx(priv, status);
			priv->rx.state = RX_IDLE;
			break;

		} else {
			 
			priv->rx.state = RX_DATA;
			priv->rx.expected_len = buf[0];
			remaining--;
			buf++;
		}
		fallthrough;

	case RX_DATA:
		 
		if (remaining == 0)
			break;

		if (priv->rx.databuf_count + remaining > priv->rx.expected_len) {
			 
			dev_err(&priv->serdev->dev, "BB pkt. Extra data received");
			bno055_ser_handle_rx(priv, STATUS_CRIT);
			priv->rx.state = RX_IDLE;
			break;
		}

		mutex_lock(&priv->lock);
		 
		if (priv->response_buf &&
		     
		    (priv->rx.databuf_count + remaining <= priv->expected_data_len))
			memcpy(priv->response_buf + priv->rx.databuf_count,
			       buf, remaining);
		mutex_unlock(&priv->lock);

		priv->rx.databuf_count += remaining;

		 
		if (priv->rx.databuf_count == priv->rx.expected_len) {
			bno055_ser_handle_rx(priv, STATUS_OK);
			priv->rx.state = RX_IDLE;
		}
		break;
	}

	return size;
}

static const struct serdev_device_ops bno055_ser_serdev_ops = {
	.receive_buf = bno055_ser_receive_buf,
	.write_wakeup = serdev_device_write_wakeup,
};

static struct regmap_bus bno055_ser_regmap_bus = {
	.write = bno055_ser_write_reg,
	.read = bno055_ser_read_reg,
};

static int bno055_ser_probe(struct serdev_device *serdev)
{
	struct bno055_ser_priv *priv;
	struct regmap *regmap;
	int ret;

	priv = devm_kzalloc(&serdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	serdev_device_set_drvdata(serdev, priv);
	priv->serdev = serdev;
	mutex_init(&priv->lock);
	init_completion(&priv->cmd_complete);

	serdev_device_set_client_ops(serdev, &bno055_ser_serdev_ops);
	ret = devm_serdev_device_open(&serdev->dev, serdev);
	if (ret)
		return ret;

	if (serdev_device_set_baudrate(serdev, 115200) != 115200) {
		dev_err(&serdev->dev, "Cannot set required baud rate");
		return -EIO;
	}

	ret = serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
	if (ret) {
		dev_err(&serdev->dev, "Cannot set required parity setting");
		return ret;
	}
	serdev_device_set_flow_control(serdev, false);

	regmap = devm_regmap_init(&serdev->dev, &bno055_ser_regmap_bus,
				  priv, &bno055_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(&serdev->dev, PTR_ERR(regmap),
				     "Unable to init register map");

	return bno055_probe(&serdev->dev, regmap,
			    BNO055_SER_XFER_BURST_BREAK_THRESHOLD, false);
}

static const struct of_device_id bno055_ser_of_match[] = {
	{ .compatible = "bosch,bno055" },
	{ }
};
MODULE_DEVICE_TABLE(of, bno055_ser_of_match);

static struct serdev_device_driver bno055_ser_driver = {
	.driver = {
		.name = "bno055-ser",
		.of_match_table = bno055_ser_of_match,
	},
	.probe = bno055_ser_probe,
};
module_serdev_device_driver(bno055_ser_driver);

MODULE_AUTHOR("Andrea Merello <andrea.merello@iit.it>");
MODULE_DESCRIPTION("Bosch BNO055 serdev interface");
MODULE_IMPORT_NS(IIO_BNO055);
MODULE_LICENSE("GPL");
