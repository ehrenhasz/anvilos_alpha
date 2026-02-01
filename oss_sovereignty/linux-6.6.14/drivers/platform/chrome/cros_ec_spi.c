




#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <uapi/linux/sched/types.h>

#include "cros_ec.h"

 
#define EC_MSG_HEADER			0xec

 
#define EC_MSG_PREAMBLE_COUNT		32

 
#define EC_MSG_DEADLINE_MS		200

 
#define EC_SPI_RECOVERY_TIME_NS	(200 * 1000)

 
struct cros_ec_spi {
	struct spi_device *spi;
	s64 last_transfer_ns;
	unsigned int start_of_msg_delay;
	unsigned int end_of_msg_delay;
	struct kthread_worker *high_pri_worker;
};

typedef int (*cros_ec_xfer_fn_t) (struct cros_ec_device *ec_dev,
				  struct cros_ec_command *ec_msg);

 

struct cros_ec_xfer_work_params {
	struct kthread_work work;
	cros_ec_xfer_fn_t fn;
	struct cros_ec_device *ec_dev;
	struct cros_ec_command *ec_msg;
	int ret;
};

static void debug_packet(struct device *dev, const char *name, u8 *ptr,
			 int len)
{
#ifdef DEBUG
	dev_dbg(dev, "%s: %*ph\n", name, len, ptr);
#endif
}

static int terminate_request(struct cros_ec_device *ec_dev)
{
	struct cros_ec_spi *ec_spi = ec_dev->priv;
	struct spi_message msg;
	struct spi_transfer trans;
	int ret;

	 
	spi_message_init(&msg);
	memset(&trans, 0, sizeof(trans));
	trans.delay.value = ec_spi->end_of_msg_delay;
	trans.delay.unit = SPI_DELAY_UNIT_USECS;
	spi_message_add_tail(&trans, &msg);

	ret = spi_sync_locked(ec_spi->spi, &msg);

	 
	ec_spi->last_transfer_ns = ktime_get_ns();
	if (ret < 0) {
		dev_err(ec_dev->dev,
			"cs-deassert spi transfer failed: %d\n",
			ret);
	}

	return ret;
}

 
static int receive_n_bytes(struct cros_ec_device *ec_dev, u8 *buf, int n)
{
	struct cros_ec_spi *ec_spi = ec_dev->priv;
	struct spi_transfer trans;
	struct spi_message msg;
	int ret;

	if (buf - ec_dev->din + n > ec_dev->din_size)
		return -EINVAL;

	memset(&trans, 0, sizeof(trans));
	trans.cs_change = 1;
	trans.rx_buf = buf;
	trans.len = n;

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);
	ret = spi_sync_locked(ec_spi->spi, &msg);
	if (ret < 0)
		dev_err(ec_dev->dev, "spi transfer failed: %d\n", ret);

	return ret;
}

 
static int cros_ec_spi_receive_packet(struct cros_ec_device *ec_dev,
				      int need_len)
{
	struct ec_host_response *response;
	u8 *ptr, *end;
	int ret;
	unsigned long deadline;
	int todo;

	if (ec_dev->din_size < EC_MSG_PREAMBLE_COUNT)
		return -EINVAL;

	 
	deadline = jiffies + msecs_to_jiffies(EC_MSG_DEADLINE_MS);
	while (true) {
		unsigned long start_jiffies = jiffies;

		ret = receive_n_bytes(ec_dev,
				      ec_dev->din,
				      EC_MSG_PREAMBLE_COUNT);
		if (ret < 0)
			return ret;

		ptr = ec_dev->din;
		for (end = ptr + EC_MSG_PREAMBLE_COUNT; ptr != end; ptr++) {
			if (*ptr == EC_SPI_FRAME_START) {
				dev_dbg(ec_dev->dev, "msg found at %zd\n",
					ptr - ec_dev->din);
				break;
			}
		}
		if (ptr != end)
			break;

		 
		if (time_after(start_jiffies, deadline)) {
			dev_warn(ec_dev->dev, "EC failed to respond in time\n");
			return -ETIMEDOUT;
		}
	}

	 
	todo = end - ++ptr;
	todo = min(todo, need_len);
	memmove(ec_dev->din, ptr, todo);
	ptr = ec_dev->din + todo;
	dev_dbg(ec_dev->dev, "need %d, got %d bytes from preamble\n",
		need_len, todo);
	need_len -= todo;

	 
	if (todo < sizeof(*response)) {
		ret = receive_n_bytes(ec_dev, ptr, sizeof(*response) - todo);
		if (ret < 0)
			return -EBADMSG;
		ptr += (sizeof(*response) - todo);
		todo = sizeof(*response);
	}

	response = (struct ec_host_response *)ec_dev->din;

	 
	if (response->data_len > ec_dev->din_size)
		return -EMSGSIZE;

	 
	while (need_len > 0) {
		 
		todo = min(need_len, 256);
		dev_dbg(ec_dev->dev, "loop, todo=%d, need_len=%d, ptr=%zd\n",
			todo, need_len, ptr - ec_dev->din);

		ret = receive_n_bytes(ec_dev, ptr, todo);
		if (ret < 0)
			return ret;

		ptr += todo;
		need_len -= todo;
	}

	dev_dbg(ec_dev->dev, "loop done, ptr=%zd\n", ptr - ec_dev->din);

	return 0;
}

 
static int cros_ec_spi_receive_response(struct cros_ec_device *ec_dev,
					int need_len)
{
	u8 *ptr, *end;
	int ret;
	unsigned long deadline;
	int todo;

	if (ec_dev->din_size < EC_MSG_PREAMBLE_COUNT)
		return -EINVAL;

	 
	deadline = jiffies + msecs_to_jiffies(EC_MSG_DEADLINE_MS);
	while (true) {
		unsigned long start_jiffies = jiffies;

		ret = receive_n_bytes(ec_dev,
				      ec_dev->din,
				      EC_MSG_PREAMBLE_COUNT);
		if (ret < 0)
			return ret;

		ptr = ec_dev->din;
		for (end = ptr + EC_MSG_PREAMBLE_COUNT; ptr != end; ptr++) {
			if (*ptr == EC_SPI_FRAME_START) {
				dev_dbg(ec_dev->dev, "msg found at %zd\n",
					ptr - ec_dev->din);
				break;
			}
		}
		if (ptr != end)
			break;

		 
		if (time_after(start_jiffies, deadline)) {
			dev_warn(ec_dev->dev, "EC failed to respond in time\n");
			return -ETIMEDOUT;
		}
	}

	 
	todo = end - ++ptr;
	todo = min(todo, need_len);
	memmove(ec_dev->din, ptr, todo);
	ptr = ec_dev->din + todo;
	dev_dbg(ec_dev->dev, "need %d, got %d bytes from preamble\n",
		 need_len, todo);
	need_len -= todo;

	 
	while (need_len > 0) {
		 
		todo = min(need_len, 256);
		dev_dbg(ec_dev->dev, "loop, todo=%d, need_len=%d, ptr=%zd\n",
			todo, need_len, ptr - ec_dev->din);

		ret = receive_n_bytes(ec_dev, ptr, todo);
		if (ret < 0)
			return ret;

		debug_packet(ec_dev->dev, "interim", ptr, todo);
		ptr += todo;
		need_len -= todo;
	}

	dev_dbg(ec_dev->dev, "loop done, ptr=%zd\n", ptr - ec_dev->din);

	return 0;
}

 
static int do_cros_ec_pkt_xfer_spi(struct cros_ec_device *ec_dev,
				   struct cros_ec_command *ec_msg)
{
	struct ec_host_response *response;
	struct cros_ec_spi *ec_spi = ec_dev->priv;
	struct spi_transfer trans, trans_delay;
	struct spi_message msg;
	int i, len;
	u8 *ptr;
	u8 *rx_buf;
	u8 sum;
	u8 rx_byte;
	int ret = 0, final_ret;
	unsigned long delay;

	len = cros_ec_prepare_tx(ec_dev, ec_msg);
	if (len < 0)
		return len;
	dev_dbg(ec_dev->dev, "prepared, len=%d\n", len);

	 
	delay = ktime_get_ns() - ec_spi->last_transfer_ns;
	if (delay < EC_SPI_RECOVERY_TIME_NS)
		ndelay(EC_SPI_RECOVERY_TIME_NS - delay);

	rx_buf = kzalloc(len, GFP_KERNEL);
	if (!rx_buf)
		return -ENOMEM;

	spi_bus_lock(ec_spi->spi->master);

	 
	spi_message_init(&msg);
	if (ec_spi->start_of_msg_delay) {
		memset(&trans_delay, 0, sizeof(trans_delay));
		trans_delay.delay.value = ec_spi->start_of_msg_delay;
		trans_delay.delay.unit = SPI_DELAY_UNIT_USECS;
		spi_message_add_tail(&trans_delay, &msg);
	}

	 
	memset(&trans, 0, sizeof(trans));
	trans.tx_buf = ec_dev->dout;
	trans.rx_buf = rx_buf;
	trans.len = len;
	trans.cs_change = 1;
	spi_message_add_tail(&trans, &msg);
	ret = spi_sync_locked(ec_spi->spi, &msg);

	 
	if (!ret) {
		 
		for (i = 0; i < len; i++) {
			rx_byte = rx_buf[i];
			 
			if (rx_byte == EC_SPI_PAST_END  ||
			    rx_byte == EC_SPI_RX_BAD_DATA ||
			    rx_byte == EC_SPI_NOT_READY) {
				ret = -EAGAIN;
				break;
			}
		}
	}

	if (!ret)
		ret = cros_ec_spi_receive_packet(ec_dev,
				ec_msg->insize + sizeof(*response));
	else if (ret != -EAGAIN)
		dev_err(ec_dev->dev, "spi transfer failed: %d\n", ret);

	final_ret = terminate_request(ec_dev);

	spi_bus_unlock(ec_spi->spi->master);

	if (!ret)
		ret = final_ret;
	if (ret < 0)
		goto exit;

	ptr = ec_dev->din;

	 
	response = (struct ec_host_response *)ptr;
	ec_msg->result = response->result;

	ret = cros_ec_check_result(ec_dev, ec_msg);
	if (ret)
		goto exit;

	len = response->data_len;
	sum = 0;
	if (len > ec_msg->insize) {
		dev_err(ec_dev->dev, "packet too long (%d bytes, expected %d)",
			len, ec_msg->insize);
		ret = -EMSGSIZE;
		goto exit;
	}

	for (i = 0; i < sizeof(*response); i++)
		sum += ptr[i];

	 
	memcpy(ec_msg->data, ptr + sizeof(*response), len);
	for (i = 0; i < len; i++)
		sum += ec_msg->data[i];

	if (sum) {
		dev_err(ec_dev->dev,
			"bad packet checksum, calculated %x\n",
			sum);
		ret = -EBADMSG;
		goto exit;
	}

	ret = len;
exit:
	kfree(rx_buf);
	if (ec_msg->command == EC_CMD_REBOOT_EC)
		msleep(EC_REBOOT_DELAY_MS);

	return ret;
}

 
static int do_cros_ec_cmd_xfer_spi(struct cros_ec_device *ec_dev,
				   struct cros_ec_command *ec_msg)
{
	struct cros_ec_spi *ec_spi = ec_dev->priv;
	struct spi_transfer trans;
	struct spi_message msg;
	int i, len;
	u8 *ptr;
	u8 *rx_buf;
	u8 rx_byte;
	int sum;
	int ret = 0, final_ret;
	unsigned long delay;

	len = cros_ec_prepare_tx(ec_dev, ec_msg);
	if (len < 0)
		return len;
	dev_dbg(ec_dev->dev, "prepared, len=%d\n", len);

	 
	delay = ktime_get_ns() - ec_spi->last_transfer_ns;
	if (delay < EC_SPI_RECOVERY_TIME_NS)
		ndelay(EC_SPI_RECOVERY_TIME_NS - delay);

	rx_buf = kzalloc(len, GFP_KERNEL);
	if (!rx_buf)
		return -ENOMEM;

	spi_bus_lock(ec_spi->spi->master);

	 
	debug_packet(ec_dev->dev, "out", ec_dev->dout, len);
	memset(&trans, 0, sizeof(trans));
	trans.tx_buf = ec_dev->dout;
	trans.rx_buf = rx_buf;
	trans.len = len;
	trans.cs_change = 1;
	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);
	ret = spi_sync_locked(ec_spi->spi, &msg);

	 
	if (!ret) {
		 
		for (i = 0; i < len; i++) {
			rx_byte = rx_buf[i];
			 
			if (rx_byte == EC_SPI_PAST_END  ||
			    rx_byte == EC_SPI_RX_BAD_DATA ||
			    rx_byte == EC_SPI_NOT_READY) {
				ret = -EAGAIN;
				break;
			}
		}
	}

	if (!ret)
		ret = cros_ec_spi_receive_response(ec_dev,
				ec_msg->insize + EC_MSG_TX_PROTO_BYTES);
	else if (ret != -EAGAIN)
		dev_err(ec_dev->dev, "spi transfer failed: %d\n", ret);

	final_ret = terminate_request(ec_dev);

	spi_bus_unlock(ec_spi->spi->master);

	if (!ret)
		ret = final_ret;
	if (ret < 0)
		goto exit;

	ptr = ec_dev->din;

	 
	ec_msg->result = ptr[0];
	ret = cros_ec_check_result(ec_dev, ec_msg);
	if (ret)
		goto exit;

	len = ptr[1];
	sum = ptr[0] + ptr[1];
	if (len > ec_msg->insize) {
		dev_err(ec_dev->dev, "packet too long (%d bytes, expected %d)",
			len, ec_msg->insize);
		ret = -ENOSPC;
		goto exit;
	}

	 
	for (i = 0; i < len; i++) {
		sum += ptr[i + 2];
		if (ec_msg->insize)
			ec_msg->data[i] = ptr[i + 2];
	}
	sum &= 0xff;

	debug_packet(ec_dev->dev, "in", ptr, len + 3);

	if (sum != ptr[len + 2]) {
		dev_err(ec_dev->dev,
			"bad packet checksum, expected %02x, got %02x\n",
			sum, ptr[len + 2]);
		ret = -EBADMSG;
		goto exit;
	}

	ret = len;
exit:
	kfree(rx_buf);
	if (ec_msg->command == EC_CMD_REBOOT_EC)
		msleep(EC_REBOOT_DELAY_MS);

	return ret;
}

static void cros_ec_xfer_high_pri_work(struct kthread_work *work)
{
	struct cros_ec_xfer_work_params *params;

	params = container_of(work, struct cros_ec_xfer_work_params, work);
	params->ret = params->fn(params->ec_dev, params->ec_msg);
}

static int cros_ec_xfer_high_pri(struct cros_ec_device *ec_dev,
				 struct cros_ec_command *ec_msg,
				 cros_ec_xfer_fn_t fn)
{
	struct cros_ec_spi *ec_spi = ec_dev->priv;
	struct cros_ec_xfer_work_params params = {
		.work = KTHREAD_WORK_INIT(params.work,
					  cros_ec_xfer_high_pri_work),
		.ec_dev = ec_dev,
		.ec_msg = ec_msg,
		.fn = fn,
	};

	 
	kthread_queue_work(ec_spi->high_pri_worker, &params.work);
	kthread_flush_work(&params.work);

	return params.ret;
}

static int cros_ec_pkt_xfer_spi(struct cros_ec_device *ec_dev,
				struct cros_ec_command *ec_msg)
{
	return cros_ec_xfer_high_pri(ec_dev, ec_msg, do_cros_ec_pkt_xfer_spi);
}

static int cros_ec_cmd_xfer_spi(struct cros_ec_device *ec_dev,
				struct cros_ec_command *ec_msg)
{
	return cros_ec_xfer_high_pri(ec_dev, ec_msg, do_cros_ec_cmd_xfer_spi);
}

static void cros_ec_spi_dt_probe(struct cros_ec_spi *ec_spi, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;
	int ret;

	ret = of_property_read_u32(np, "google,cros-ec-spi-pre-delay", &val);
	if (!ret)
		ec_spi->start_of_msg_delay = val;

	ret = of_property_read_u32(np, "google,cros-ec-spi-msg-delay", &val);
	if (!ret)
		ec_spi->end_of_msg_delay = val;
}

static void cros_ec_spi_high_pri_release(void *worker)
{
	kthread_destroy_worker(worker);
}

static int cros_ec_spi_devm_high_pri_alloc(struct device *dev,
					   struct cros_ec_spi *ec_spi)
{
	int err;

	ec_spi->high_pri_worker =
		kthread_create_worker(0, "cros_ec_spi_high_pri");

	if (IS_ERR(ec_spi->high_pri_worker)) {
		err = PTR_ERR(ec_spi->high_pri_worker);
		dev_err(dev, "Can't create cros_ec high pri worker: %d\n", err);
		return err;
	}

	err = devm_add_action_or_reset(dev, cros_ec_spi_high_pri_release,
				       ec_spi->high_pri_worker);
	if (err)
		return err;

	sched_set_fifo(ec_spi->high_pri_worker->task);

	return 0;
}

static int cros_ec_spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct cros_ec_device *ec_dev;
	struct cros_ec_spi *ec_spi;
	int err;

	spi->rt = true;
	err = spi_setup(spi);
	if (err < 0)
		return err;

	ec_spi = devm_kzalloc(dev, sizeof(*ec_spi), GFP_KERNEL);
	if (ec_spi == NULL)
		return -ENOMEM;
	ec_spi->spi = spi;
	ec_dev = devm_kzalloc(dev, sizeof(*ec_dev), GFP_KERNEL);
	if (!ec_dev)
		return -ENOMEM;

	 
	cros_ec_spi_dt_probe(ec_spi, dev);

	spi_set_drvdata(spi, ec_dev);
	ec_dev->dev = dev;
	ec_dev->priv = ec_spi;
	ec_dev->irq = spi->irq;
	ec_dev->cmd_xfer = cros_ec_cmd_xfer_spi;
	ec_dev->pkt_xfer = cros_ec_pkt_xfer_spi;
	ec_dev->phys_name = dev_name(&ec_spi->spi->dev);
	ec_dev->din_size = EC_MSG_PREAMBLE_COUNT +
			   sizeof(struct ec_host_response) +
			   sizeof(struct ec_response_get_protocol_info);
	ec_dev->dout_size = sizeof(struct ec_host_request);

	ec_spi->last_transfer_ns = ktime_get_ns();

	err = cros_ec_spi_devm_high_pri_alloc(dev, ec_spi);
	if (err)
		return err;

	err = cros_ec_register(ec_dev);
	if (err) {
		dev_err(dev, "cannot register EC\n");
		return err;
	}

	device_init_wakeup(&spi->dev, true);

	return 0;
}

static void cros_ec_spi_remove(struct spi_device *spi)
{
	struct cros_ec_device *ec_dev = spi_get_drvdata(spi);

	cros_ec_unregister(ec_dev);
}

#ifdef CONFIG_PM_SLEEP
static int cros_ec_spi_suspend(struct device *dev)
{
	struct cros_ec_device *ec_dev = dev_get_drvdata(dev);

	return cros_ec_suspend(ec_dev);
}

static int cros_ec_spi_resume(struct device *dev)
{
	struct cros_ec_device *ec_dev = dev_get_drvdata(dev);

	return cros_ec_resume(ec_dev);
}
#endif

static SIMPLE_DEV_PM_OPS(cros_ec_spi_pm_ops, cros_ec_spi_suspend,
			 cros_ec_spi_resume);

static const struct of_device_id cros_ec_spi_of_match[] = {
	{ .compatible = "google,cros-ec-spi", },
	{   },
};
MODULE_DEVICE_TABLE(of, cros_ec_spi_of_match);

static const struct spi_device_id cros_ec_spi_id[] = {
	{ "cros-ec-spi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, cros_ec_spi_id);

static struct spi_driver cros_ec_driver_spi = {
	.driver	= {
		.name	= "cros-ec-spi",
		.of_match_table = cros_ec_spi_of_match,
		.pm	= &cros_ec_spi_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe		= cros_ec_spi_probe,
	.remove		= cros_ec_spi_remove,
	.id_table	= cros_ec_spi_id,
};

module_spi_driver(cros_ec_driver_spi);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SPI interface for ChromeOS Embedded Controller");
