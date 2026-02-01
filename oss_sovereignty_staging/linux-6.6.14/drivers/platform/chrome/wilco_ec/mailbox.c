
 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_data/wilco-ec.h>
#include <linux/platform_device.h>

#include "../cros_ec_lpc_mec.h"

 
#define EC_MAILBOX_VERSION		0

 
#define EC_MAILBOX_START_COMMAND	0xda

 
#define EC_MAILBOX_PROTO_VERSION	3

 
#define EC_MAILBOX_DATA_EXTRA		2

 
#define EC_MAILBOX_TIMEOUT		HZ

 
#define EC_CMDR_DATA		BIT(0)	 
#define EC_CMDR_PENDING		BIT(1)	 
#define EC_CMDR_BUSY		BIT(2)	 
#define EC_CMDR_CMD		BIT(3)	 

 
static bool wilco_ec_response_timed_out(struct wilco_ec_device *ec)
{
	unsigned long timeout = jiffies + EC_MAILBOX_TIMEOUT;

	do {
		if (!(inb(ec->io_command->start) &
		      (EC_CMDR_PENDING | EC_CMDR_BUSY)))
			return false;
		usleep_range(100, 200);
	} while (time_before(jiffies, timeout));

	return true;
}

 
static u8 wilco_ec_checksum(const void *data, size_t size)
{
	u8 *data_bytes = (u8 *)data;
	u8 checksum = 0;
	size_t i;

	for (i = 0; i < size; i++)
		checksum += data_bytes[i];

	return checksum;
}

 
static void wilco_ec_prepare(struct wilco_ec_message *msg,
			     struct wilco_ec_request *rq)
{
	memset(rq, 0, sizeof(*rq));
	rq->struct_version = EC_MAILBOX_PROTO_VERSION;
	rq->mailbox_id = msg->type;
	rq->mailbox_version = EC_MAILBOX_VERSION;
	rq->data_size = msg->request_size;

	 
	rq->checksum = wilco_ec_checksum(rq, sizeof(*rq));
	rq->checksum += wilco_ec_checksum(msg->request_data, msg->request_size);
	rq->checksum = -rq->checksum;
}

 
static int wilco_ec_transfer(struct wilco_ec_device *ec,
			     struct wilco_ec_message *msg,
			     struct wilco_ec_request *rq)
{
	struct wilco_ec_response *rs;
	u8 checksum;
	u8 flag;

	 
	cros_ec_lpc_io_bytes_mec(MEC_IO_WRITE, 0, sizeof(*rq), (u8 *)rq);
	cros_ec_lpc_io_bytes_mec(MEC_IO_WRITE, sizeof(*rq), msg->request_size,
				 msg->request_data);

	 
	outb(EC_MAILBOX_START_COMMAND, ec->io_command->start);

	 
	if (msg->flags & WILCO_EC_FLAG_NO_RESPONSE) {
		dev_dbg(ec->dev, "EC does not respond to this command\n");
		return 0;
	}

	 
	if (wilco_ec_response_timed_out(ec)) {
		dev_dbg(ec->dev, "response timed out\n");
		return -ETIMEDOUT;
	}

	 
	flag = inb(ec->io_data->start);
	if (flag) {
		dev_dbg(ec->dev, "bad response: 0x%02x\n", flag);
		return -EIO;
	}

	 
	rs = ec->data_buffer;
	checksum = cros_ec_lpc_io_bytes_mec(MEC_IO_READ, 0,
					    sizeof(*rs) + EC_MAILBOX_DATA_SIZE,
					    (u8 *)rs);
	if (checksum) {
		dev_dbg(ec->dev, "bad packet checksum 0x%02x\n", rs->checksum);
		return -EBADMSG;
	}

	if (rs->result) {
		dev_dbg(ec->dev, "EC reported failure: 0x%02x\n", rs->result);
		return -EBADMSG;
	}

	if (rs->data_size != EC_MAILBOX_DATA_SIZE) {
		dev_dbg(ec->dev, "unexpected packet size (%u != %u)\n",
			rs->data_size, EC_MAILBOX_DATA_SIZE);
		return -EMSGSIZE;
	}

	if (rs->data_size < msg->response_size) {
		dev_dbg(ec->dev, "EC didn't return enough data (%u < %zu)\n",
			rs->data_size, msg->response_size);
		return -EMSGSIZE;
	}

	memcpy(msg->response_data, rs->data, msg->response_size);

	return rs->data_size;
}

 
int wilco_ec_mailbox(struct wilco_ec_device *ec, struct wilco_ec_message *msg)
{
	struct wilco_ec_request *rq;
	int ret;

	dev_dbg(ec->dev, "type=%04x flags=%02x rslen=%zu rqlen=%zu\n",
		msg->type, msg->flags, msg->response_size, msg->request_size);

	mutex_lock(&ec->mailbox_lock);
	 
	rq = ec->data_buffer;
	wilco_ec_prepare(msg, rq);

	ret = wilco_ec_transfer(ec, msg, rq);
	mutex_unlock(&ec->mailbox_lock);

	return ret;

}
EXPORT_SYMBOL_GPL(wilco_ec_mailbox);
