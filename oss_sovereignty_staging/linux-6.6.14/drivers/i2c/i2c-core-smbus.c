
 
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-smbus.h>
#include <linux/property.h>
#include <linux/slab.h>

#include "i2c-core.h"

#define CREATE_TRACE_POINTS
#include <trace/events/smbus.h>


 

#define POLY    (0x1070U << 3)
static u8 crc8(u16 data)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (data & 0x8000)
			data = data ^ POLY;
		data = data << 1;
	}
	return (u8)(data >> 8);
}

 
u8 i2c_smbus_pec(u8 crc, u8 *p, size_t count)
{
	int i;

	for (i = 0; i < count; i++)
		crc = crc8((crc ^ p[i]) << 8);
	return crc;
}
EXPORT_SYMBOL(i2c_smbus_pec);

 
static u8 i2c_smbus_msg_pec(u8 pec, struct i2c_msg *msg)
{
	 
	u8 addr = i2c_8bit_addr_from_msg(msg);
	pec = i2c_smbus_pec(pec, &addr, 1);

	 
	return i2c_smbus_pec(pec, msg->buf, msg->len);
}

 
static inline void i2c_smbus_add_pec(struct i2c_msg *msg)
{
	msg->buf[msg->len] = i2c_smbus_msg_pec(0, msg);
	msg->len++;
}

 
static int i2c_smbus_check_pec(u8 cpec, struct i2c_msg *msg)
{
	u8 rpec = msg->buf[--msg->len];
	cpec = i2c_smbus_msg_pec(cpec, msg);

	if (rpec != cpec) {
		pr_debug("Bad PEC 0x%02x vs. 0x%02x\n",
			rpec, cpec);
		return -EBADMSG;
	}
	return 0;
}

 
s32 i2c_smbus_read_byte(const struct i2c_client *client)
{
	union i2c_smbus_data data;
	int status;

	status = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
				I2C_SMBUS_READ, 0,
				I2C_SMBUS_BYTE, &data);
	return (status < 0) ? status : data.byte;
}
EXPORT_SYMBOL(i2c_smbus_read_byte);

 
s32 i2c_smbus_write_byte(const struct i2c_client *client, u8 value)
{
	return i2c_smbus_xfer(client->adapter, client->addr, client->flags,
	                      I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}
EXPORT_SYMBOL(i2c_smbus_write_byte);

 
s32 i2c_smbus_read_byte_data(const struct i2c_client *client, u8 command)
{
	union i2c_smbus_data data;
	int status;

	status = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
				I2C_SMBUS_READ, command,
				I2C_SMBUS_BYTE_DATA, &data);
	return (status < 0) ? status : data.byte;
}
EXPORT_SYMBOL(i2c_smbus_read_byte_data);

 
s32 i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command,
			      u8 value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_xfer(client->adapter, client->addr, client->flags,
			      I2C_SMBUS_WRITE, command,
			      I2C_SMBUS_BYTE_DATA, &data);
}
EXPORT_SYMBOL(i2c_smbus_write_byte_data);

 
s32 i2c_smbus_read_word_data(const struct i2c_client *client, u8 command)
{
	union i2c_smbus_data data;
	int status;

	status = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
				I2C_SMBUS_READ, command,
				I2C_SMBUS_WORD_DATA, &data);
	return (status < 0) ? status : data.word;
}
EXPORT_SYMBOL(i2c_smbus_read_word_data);

 
s32 i2c_smbus_write_word_data(const struct i2c_client *client, u8 command,
			      u16 value)
{
	union i2c_smbus_data data;
	data.word = value;
	return i2c_smbus_xfer(client->adapter, client->addr, client->flags,
			      I2C_SMBUS_WRITE, command,
			      I2C_SMBUS_WORD_DATA, &data);
}
EXPORT_SYMBOL(i2c_smbus_write_word_data);

 
s32 i2c_smbus_read_block_data(const struct i2c_client *client, u8 command,
			      u8 *values)
{
	union i2c_smbus_data data;
	int status;

	status = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
				I2C_SMBUS_READ, command,
				I2C_SMBUS_BLOCK_DATA, &data);
	if (status)
		return status;

	memcpy(values, &data.block[1], data.block[0]);
	return data.block[0];
}
EXPORT_SYMBOL(i2c_smbus_read_block_data);

 
s32 i2c_smbus_write_block_data(const struct i2c_client *client, u8 command,
			       u8 length, const u8 *values)
{
	union i2c_smbus_data data;

	if (length > I2C_SMBUS_BLOCK_MAX)
		length = I2C_SMBUS_BLOCK_MAX;
	data.block[0] = length;
	memcpy(&data.block[1], values, length);
	return i2c_smbus_xfer(client->adapter, client->addr, client->flags,
			      I2C_SMBUS_WRITE, command,
			      I2C_SMBUS_BLOCK_DATA, &data);
}
EXPORT_SYMBOL(i2c_smbus_write_block_data);

 
s32 i2c_smbus_read_i2c_block_data(const struct i2c_client *client, u8 command,
				  u8 length, u8 *values)
{
	union i2c_smbus_data data;
	int status;

	if (length > I2C_SMBUS_BLOCK_MAX)
		length = I2C_SMBUS_BLOCK_MAX;
	data.block[0] = length;
	status = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
				I2C_SMBUS_READ, command,
				I2C_SMBUS_I2C_BLOCK_DATA, &data);
	if (status < 0)
		return status;

	memcpy(values, &data.block[1], data.block[0]);
	return data.block[0];
}
EXPORT_SYMBOL(i2c_smbus_read_i2c_block_data);

s32 i2c_smbus_write_i2c_block_data(const struct i2c_client *client, u8 command,
				   u8 length, const u8 *values)
{
	union i2c_smbus_data data;

	if (length > I2C_SMBUS_BLOCK_MAX)
		length = I2C_SMBUS_BLOCK_MAX;
	data.block[0] = length;
	memcpy(data.block + 1, values, length);
	return i2c_smbus_xfer(client->adapter, client->addr, client->flags,
			      I2C_SMBUS_WRITE, command,
			      I2C_SMBUS_I2C_BLOCK_DATA, &data);
}
EXPORT_SYMBOL(i2c_smbus_write_i2c_block_data);

static void i2c_smbus_try_get_dmabuf(struct i2c_msg *msg, u8 init_val)
{
	bool is_read = msg->flags & I2C_M_RD;
	unsigned char *dma_buf;

	dma_buf = kzalloc(I2C_SMBUS_BLOCK_MAX + (is_read ? 2 : 3), GFP_KERNEL);
	if (!dma_buf)
		return;

	msg->buf = dma_buf;
	msg->flags |= I2C_M_DMA_SAFE;

	if (init_val)
		msg->buf[0] = init_val;
}

 
static s32 i2c_smbus_xfer_emulated(struct i2c_adapter *adapter, u16 addr,
				   unsigned short flags,
				   char read_write, u8 command, int size,
				   union i2c_smbus_data *data)
{
	 
	unsigned char msgbuf0[I2C_SMBUS_BLOCK_MAX+3];
	unsigned char msgbuf1[I2C_SMBUS_BLOCK_MAX+2];
	int nmsgs = read_write == I2C_SMBUS_READ ? 2 : 1;
	u8 partial_pec = 0;
	int status;
	struct i2c_msg msg[2] = {
		{
			.addr = addr,
			.flags = flags,
			.len = 1,
			.buf = msgbuf0,
		}, {
			.addr = addr,
			.flags = flags | I2C_M_RD,
			.len = 0,
			.buf = msgbuf1,
		},
	};
	bool wants_pec = ((flags & I2C_CLIENT_PEC) && size != I2C_SMBUS_QUICK
			  && size != I2C_SMBUS_I2C_BLOCK_DATA);

	msgbuf0[0] = command;
	switch (size) {
	case I2C_SMBUS_QUICK:
		msg[0].len = 0;
		 
		msg[0].flags = flags | (read_write == I2C_SMBUS_READ ?
					I2C_M_RD : 0);
		nmsgs = 1;
		break;
	case I2C_SMBUS_BYTE:
		if (read_write == I2C_SMBUS_READ) {
			 
			msg[0].flags = I2C_M_RD | flags;
			nmsgs = 1;
		}
		break;
	case I2C_SMBUS_BYTE_DATA:
		if (read_write == I2C_SMBUS_READ)
			msg[1].len = 1;
		else {
			msg[0].len = 2;
			msgbuf0[1] = data->byte;
		}
		break;
	case I2C_SMBUS_WORD_DATA:
		if (read_write == I2C_SMBUS_READ)
			msg[1].len = 2;
		else {
			msg[0].len = 3;
			msgbuf0[1] = data->word & 0xff;
			msgbuf0[2] = data->word >> 8;
		}
		break;
	case I2C_SMBUS_PROC_CALL:
		nmsgs = 2;  
		read_write = I2C_SMBUS_READ;
		msg[0].len = 3;
		msg[1].len = 2;
		msgbuf0[1] = data->word & 0xff;
		msgbuf0[2] = data->word >> 8;
		break;
	case I2C_SMBUS_BLOCK_DATA:
		if (read_write == I2C_SMBUS_READ) {
			msg[1].flags |= I2C_M_RECV_LEN;
			msg[1].len = 1;  
			i2c_smbus_try_get_dmabuf(&msg[1], 0);
		} else {
			msg[0].len = data->block[0] + 2;
			if (msg[0].len > I2C_SMBUS_BLOCK_MAX + 2) {
				dev_err(&adapter->dev,
					"Invalid block write size %d\n",
					data->block[0]);
				return -EINVAL;
			}

			i2c_smbus_try_get_dmabuf(&msg[0], command);
			memcpy(msg[0].buf + 1, data->block, msg[0].len - 1);
		}
		break;
	case I2C_SMBUS_BLOCK_PROC_CALL:
		nmsgs = 2;  
		read_write = I2C_SMBUS_READ;
		if (data->block[0] > I2C_SMBUS_BLOCK_MAX) {
			dev_err(&adapter->dev,
				"Invalid block write size %d\n",
				data->block[0]);
			return -EINVAL;
		}

		msg[0].len = data->block[0] + 2;
		i2c_smbus_try_get_dmabuf(&msg[0], command);
		memcpy(msg[0].buf + 1, data->block, msg[0].len - 1);

		msg[1].flags |= I2C_M_RECV_LEN;
		msg[1].len = 1;  
		i2c_smbus_try_get_dmabuf(&msg[1], 0);
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA:
		if (data->block[0] > I2C_SMBUS_BLOCK_MAX) {
			dev_err(&adapter->dev, "Invalid block %s size %d\n",
				read_write == I2C_SMBUS_READ ? "read" : "write",
				data->block[0]);
			return -EINVAL;
		}

		if (read_write == I2C_SMBUS_READ) {
			msg[1].len = data->block[0];
			i2c_smbus_try_get_dmabuf(&msg[1], 0);
		} else {
			msg[0].len = data->block[0] + 1;

			i2c_smbus_try_get_dmabuf(&msg[0], command);
			memcpy(msg[0].buf + 1, data->block + 1, data->block[0]);
		}
		break;
	default:
		dev_err(&adapter->dev, "Unsupported transaction %d\n", size);
		return -EOPNOTSUPP;
	}

	if (wants_pec) {
		 
		if (!(msg[0].flags & I2C_M_RD)) {
			if (nmsgs == 1)  
				i2c_smbus_add_pec(&msg[0]);
			else  
				partial_pec = i2c_smbus_msg_pec(0, &msg[0]);
		}
		 
		if (msg[nmsgs - 1].flags & I2C_M_RD)
			msg[nmsgs - 1].len++;
	}

	status = __i2c_transfer(adapter, msg, nmsgs);
	if (status < 0)
		goto cleanup;
	if (status != nmsgs) {
		status = -EIO;
		goto cleanup;
	}
	status = 0;

	 
	if (wants_pec && (msg[nmsgs - 1].flags & I2C_M_RD)) {
		status = i2c_smbus_check_pec(partial_pec, &msg[nmsgs - 1]);
		if (status < 0)
			goto cleanup;
	}

	if (read_write == I2C_SMBUS_READ)
		switch (size) {
		case I2C_SMBUS_BYTE:
			data->byte = msgbuf0[0];
			break;
		case I2C_SMBUS_BYTE_DATA:
			data->byte = msgbuf1[0];
			break;
		case I2C_SMBUS_WORD_DATA:
		case I2C_SMBUS_PROC_CALL:
			data->word = msgbuf1[0] | (msgbuf1[1] << 8);
			break;
		case I2C_SMBUS_I2C_BLOCK_DATA:
			memcpy(data->block + 1, msg[1].buf, data->block[0]);
			break;
		case I2C_SMBUS_BLOCK_DATA:
		case I2C_SMBUS_BLOCK_PROC_CALL:
			if (msg[1].buf[0] > I2C_SMBUS_BLOCK_MAX) {
				dev_err(&adapter->dev,
					"Invalid block size returned: %d\n",
					msg[1].buf[0]);
				status = -EPROTO;
				goto cleanup;
			}
			memcpy(data->block, msg[1].buf, msg[1].buf[0] + 1);
			break;
		}

cleanup:
	if (msg[0].flags & I2C_M_DMA_SAFE)
		kfree(msg[0].buf);
	if (msg[1].flags & I2C_M_DMA_SAFE)
		kfree(msg[1].buf);

	return status;
}

 
s32 i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int protocol, union i2c_smbus_data *data)
{
	s32 res;

	res = __i2c_lock_bus_helper(adapter);
	if (res)
		return res;

	res = __i2c_smbus_xfer(adapter, addr, flags, read_write,
			       command, protocol, data);
	i2c_unlock_bus(adapter, I2C_LOCK_SEGMENT);

	return res;
}
EXPORT_SYMBOL(i2c_smbus_xfer);

s32 __i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr,
		     unsigned short flags, char read_write,
		     u8 command, int protocol, union i2c_smbus_data *data)
{
	int (*xfer_func)(struct i2c_adapter *adap, u16 addr,
			 unsigned short flags, char read_write,
			 u8 command, int size, union i2c_smbus_data *data);
	unsigned long orig_jiffies;
	int try;
	s32 res;

	res = __i2c_check_suspended(adapter);
	if (res)
		return res;

	 
	trace_smbus_write(adapter, addr, flags, read_write,
			  command, protocol, data);
	trace_smbus_read(adapter, addr, flags, read_write,
			 command, protocol);

	flags &= I2C_M_TEN | I2C_CLIENT_PEC | I2C_CLIENT_SCCB;

	xfer_func = adapter->algo->smbus_xfer;
	if (i2c_in_atomic_xfer_mode()) {
		if (adapter->algo->smbus_xfer_atomic)
			xfer_func = adapter->algo->smbus_xfer_atomic;
		else if (adapter->algo->master_xfer_atomic)
			xfer_func = NULL;  
	}

	if (xfer_func) {
		 
		orig_jiffies = jiffies;
		for (res = 0, try = 0; try <= adapter->retries; try++) {
			res = xfer_func(adapter, addr, flags, read_write,
					command, protocol, data);
			if (res != -EAGAIN)
				break;
			if (time_after(jiffies,
				       orig_jiffies + adapter->timeout))
				break;
		}

		if (res != -EOPNOTSUPP || !adapter->algo->master_xfer)
			goto trace;
		 
	}

	res = i2c_smbus_xfer_emulated(adapter, addr, flags, read_write,
				      command, protocol, data);

trace:
	 
	trace_smbus_reply(adapter, addr, flags, read_write,
			  command, protocol, data, res);
	trace_smbus_result(adapter, addr, flags, read_write,
			   command, protocol, res);

	return res;
}
EXPORT_SYMBOL(__i2c_smbus_xfer);

 
s32 i2c_smbus_read_i2c_block_data_or_emulated(const struct i2c_client *client,
					      u8 command, u8 length, u8 *values)
{
	u8 i = 0;
	int status;

	if (length > I2C_SMBUS_BLOCK_MAX)
		length = I2C_SMBUS_BLOCK_MAX;

	if (i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return i2c_smbus_read_i2c_block_data(client, command, length, values);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_BYTE_DATA))
		return -EOPNOTSUPP;

	if (i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)) {
		while ((i + 2) <= length) {
			status = i2c_smbus_read_word_data(client, command + i);
			if (status < 0)
				return status;
			values[i] = status & 0xff;
			values[i + 1] = status >> 8;
			i += 2;
		}
	}

	while (i < length) {
		status = i2c_smbus_read_byte_data(client, command + i);
		if (status < 0)
			return status;
		values[i] = status;
		i++;
	}

	return i;
}
EXPORT_SYMBOL(i2c_smbus_read_i2c_block_data_or_emulated);

 
struct i2c_client *i2c_new_smbus_alert_device(struct i2c_adapter *adapter,
					      struct i2c_smbus_alert_setup *setup)
{
	struct i2c_board_info ara_board_info = {
		I2C_BOARD_INFO("smbus_alert", 0x0c),
		.platform_data = setup,
	};

	return i2c_new_client_device(adapter, &ara_board_info);
}
EXPORT_SYMBOL_GPL(i2c_new_smbus_alert_device);

#if IS_ENABLED(CONFIG_I2C_SMBUS)
int i2c_setup_smbus_alert(struct i2c_adapter *adapter)
{
	struct device *parent = adapter->dev.parent;
	int irq;

	 
	if (!parent)
		return 0;

	irq = device_property_match_string(parent, "interrupt-names", "smbus_alert");
	if (irq == -EINVAL || irq == -ENODATA)
		return 0;
	else if (irq < 0)
		return irq;

	return PTR_ERR_OR_ZERO(i2c_new_smbus_alert_device(adapter, NULL));
}
#endif
