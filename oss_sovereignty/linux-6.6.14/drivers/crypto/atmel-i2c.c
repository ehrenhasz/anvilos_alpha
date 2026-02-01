
 

#include <linux/bitrev.h>
#include <linux/crc16.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "atmel-i2c.h"

static const struct {
	u8 value;
	const char *error_text;
} error_list[] = {
	{ 0x01, "CheckMac or Verify miscompare" },
	{ 0x03, "Parse Error" },
	{ 0x05, "ECC Fault" },
	{ 0x0F, "Execution Error" },
	{ 0xEE, "Watchdog about to expire" },
	{ 0xFF, "CRC or other communication error" },
};

 
static void atmel_i2c_checksum(struct atmel_i2c_cmd *cmd)
{
	u8 *data = &cmd->count;
	size_t len = cmd->count - CRC_SIZE;
	__le16 *__crc16 = (__le16 *)(data + len);

	*__crc16 = cpu_to_le16(bitrev16(crc16(0, data, len)));
}

void atmel_i2c_init_read_cmd(struct atmel_i2c_cmd *cmd)
{
	cmd->word_addr = COMMAND;
	cmd->opcode = OPCODE_READ;
	 
	cmd->param1 = CONFIGURATION_ZONE;
	cmd->param2 = cpu_to_le16(DEVICE_LOCK_ADDR);
	cmd->count = READ_COUNT;

	atmel_i2c_checksum(cmd);

	cmd->msecs = MAX_EXEC_TIME_READ;
	cmd->rxsize = READ_RSP_SIZE;
}
EXPORT_SYMBOL(atmel_i2c_init_read_cmd);

void atmel_i2c_init_random_cmd(struct atmel_i2c_cmd *cmd)
{
	cmd->word_addr = COMMAND;
	cmd->opcode = OPCODE_RANDOM;
	cmd->param1 = 0;
	cmd->param2 = 0;
	cmd->count = RANDOM_COUNT;

	atmel_i2c_checksum(cmd);

	cmd->msecs = MAX_EXEC_TIME_RANDOM;
	cmd->rxsize = RANDOM_RSP_SIZE;
}
EXPORT_SYMBOL(atmel_i2c_init_random_cmd);

void atmel_i2c_init_genkey_cmd(struct atmel_i2c_cmd *cmd, u16 keyid)
{
	cmd->word_addr = COMMAND;
	cmd->count = GENKEY_COUNT;
	cmd->opcode = OPCODE_GENKEY;
	cmd->param1 = GENKEY_MODE_PRIVATE;
	 
	cmd->param2 = cpu_to_le16(keyid);

	atmel_i2c_checksum(cmd);

	cmd->msecs = MAX_EXEC_TIME_GENKEY;
	cmd->rxsize = GENKEY_RSP_SIZE;
}
EXPORT_SYMBOL(atmel_i2c_init_genkey_cmd);

int atmel_i2c_init_ecdh_cmd(struct atmel_i2c_cmd *cmd,
			    struct scatterlist *pubkey)
{
	size_t copied;

	cmd->word_addr = COMMAND;
	cmd->count = ECDH_COUNT;
	cmd->opcode = OPCODE_ECDH;
	cmd->param1 = ECDH_PREFIX_MODE;
	 
	cmd->param2 = cpu_to_le16(DATA_SLOT_2);

	 
	copied = sg_copy_to_buffer(pubkey,
				   sg_nents_for_len(pubkey,
						    ATMEL_ECC_PUBKEY_SIZE),
				   cmd->data, ATMEL_ECC_PUBKEY_SIZE);
	if (copied != ATMEL_ECC_PUBKEY_SIZE)
		return -EINVAL;

	atmel_i2c_checksum(cmd);

	cmd->msecs = MAX_EXEC_TIME_ECDH;
	cmd->rxsize = ECDH_RSP_SIZE;

	return 0;
}
EXPORT_SYMBOL(atmel_i2c_init_ecdh_cmd);

 
static int atmel_i2c_status(struct device *dev, u8 *status)
{
	size_t err_list_len = ARRAY_SIZE(error_list);
	int i;
	u8 err_id = status[1];

	if (*status != STATUS_SIZE)
		return 0;

	if (err_id == STATUS_WAKE_SUCCESSFUL || err_id == STATUS_NOERR)
		return 0;

	for (i = 0; i < err_list_len; i++)
		if (error_list[i].value == err_id)
			break;

	 
	if (i != err_list_len) {
		dev_err(dev, "%02x: %s:\n", err_id, error_list[i].error_text);
		return err_id;
	}

	return 0;
}

static int atmel_i2c_wakeup(struct i2c_client *client)
{
	struct atmel_i2c_client_priv *i2c_priv = i2c_get_clientdata(client);
	u8 status[STATUS_RSP_SIZE];
	int ret;

	 
	i2c_transfer_buffer_flags(client, i2c_priv->wake_token,
				i2c_priv->wake_token_sz, I2C_M_IGNORE_NAK);

	 
	usleep_range(TWHI_MIN, TWHI_MAX);

	ret = i2c_master_recv(client, status, STATUS_SIZE);
	if (ret < 0)
		return ret;

	return atmel_i2c_status(&client->dev, status);
}

static int atmel_i2c_sleep(struct i2c_client *client)
{
	u8 sleep = SLEEP_TOKEN;

	return i2c_master_send(client, &sleep, 1);
}

 
int atmel_i2c_send_receive(struct i2c_client *client, struct atmel_i2c_cmd *cmd)
{
	struct atmel_i2c_client_priv *i2c_priv = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&i2c_priv->lock);

	ret = atmel_i2c_wakeup(client);
	if (ret)
		goto err;

	 
	ret = i2c_master_send(client, (u8 *)cmd, cmd->count + WORD_ADDR_SIZE);
	if (ret < 0)
		goto err;

	 
	msleep(cmd->msecs);

	 
	ret = i2c_master_recv(client, cmd->data, cmd->rxsize);
	if (ret < 0)
		goto err;

	 
	ret = atmel_i2c_sleep(client);
	if (ret < 0)
		goto err;

	mutex_unlock(&i2c_priv->lock);
	return atmel_i2c_status(&client->dev, cmd->data);
err:
	mutex_unlock(&i2c_priv->lock);
	return ret;
}
EXPORT_SYMBOL(atmel_i2c_send_receive);

static void atmel_i2c_work_handler(struct work_struct *work)
{
	struct atmel_i2c_work_data *work_data =
			container_of(work, struct atmel_i2c_work_data, work);
	struct atmel_i2c_cmd *cmd = &work_data->cmd;
	struct i2c_client *client = work_data->client;
	int status;

	status = atmel_i2c_send_receive(client, cmd);
	work_data->cbk(work_data, work_data->areq, status);
}

static struct workqueue_struct *atmel_wq;

void atmel_i2c_enqueue(struct atmel_i2c_work_data *work_data,
		       void (*cbk)(struct atmel_i2c_work_data *work_data,
				   void *areq, int status),
		       void *areq)
{
	work_data->cbk = (void *)cbk;
	work_data->areq = areq;

	INIT_WORK(&work_data->work, atmel_i2c_work_handler);
	queue_work(atmel_wq, &work_data->work);
}
EXPORT_SYMBOL(atmel_i2c_enqueue);

void atmel_i2c_flush_queue(void)
{
	flush_workqueue(atmel_wq);
}
EXPORT_SYMBOL(atmel_i2c_flush_queue);

static inline size_t atmel_i2c_wake_token_sz(u32 bus_clk_rate)
{
	u32 no_of_bits = DIV_ROUND_UP(TWLO_USEC * bus_clk_rate, USEC_PER_SEC);

	 
	return DIV_ROUND_UP(no_of_bits, 8);
}

static int device_sanity_check(struct i2c_client *client)
{
	struct atmel_i2c_cmd *cmd;
	int ret;

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	atmel_i2c_init_read_cmd(cmd);

	ret = atmel_i2c_send_receive(client, cmd);
	if (ret)
		goto free_cmd;

	 
	if (cmd->data[LOCK_CONFIG_IDX] || cmd->data[LOCK_VALUE_IDX]) {
		dev_err(&client->dev, "Configuration or Data and OTP zones are unlocked!\n");
		ret = -ENOTSUPP;
	}

	 
free_cmd:
	kfree(cmd);
	return ret;
}

int atmel_i2c_probe(struct i2c_client *client)
{
	struct atmel_i2c_client_priv *i2c_priv;
	struct device *dev = &client->dev;
	int ret;
	u32 bus_clk_rate;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C_FUNC_I2C not supported\n");
		return -ENODEV;
	}

	bus_clk_rate = i2c_acpi_find_bus_speed(&client->adapter->dev);
	if (!bus_clk_rate) {
		ret = device_property_read_u32(&client->adapter->dev,
					       "clock-frequency", &bus_clk_rate);
		if (ret) {
			dev_err(dev, "failed to read clock-frequency property\n");
			return ret;
		}
	}

	if (bus_clk_rate > 1000000L) {
		dev_err(dev, "%u exceeds maximum supported clock frequency (1MHz)\n",
			bus_clk_rate);
		return -EINVAL;
	}

	i2c_priv = devm_kmalloc(dev, sizeof(*i2c_priv), GFP_KERNEL);
	if (!i2c_priv)
		return -ENOMEM;

	i2c_priv->client = client;
	mutex_init(&i2c_priv->lock);

	 
	i2c_priv->wake_token_sz = atmel_i2c_wake_token_sz(bus_clk_rate);

	memset(i2c_priv->wake_token, 0, sizeof(i2c_priv->wake_token));

	atomic_set(&i2c_priv->tfm_count, 0);

	i2c_set_clientdata(client, i2c_priv);

	return device_sanity_check(client);
}
EXPORT_SYMBOL(atmel_i2c_probe);

static int __init atmel_i2c_init(void)
{
	atmel_wq = alloc_workqueue("atmel_wq", 0, 0);
	return atmel_wq ? 0 : -ENOMEM;
}

static void __exit atmel_i2c_exit(void)
{
	destroy_workqueue(atmel_wq);
}

module_init(atmel_i2c_init);
module_exit(atmel_i2c_exit);

MODULE_AUTHOR("Tudor Ambarus");
MODULE_DESCRIPTION("Microchip / Atmel ECC (I2C) driver");
MODULE_LICENSE("GPL v2");
