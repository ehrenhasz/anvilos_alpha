
 

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_iop.h>
#include <asm/adb_iop.h>
#include <asm/unaligned.h>

#include <linux/adb.h>

static struct adb_request *current_req;
static struct adb_request *last_req;
static unsigned int autopoll_devs;
static u8 autopoll_addr;

static enum adb_iop_state {
	idle,
	sending,
	awaiting_reply
} adb_iop_state;

static void adb_iop_start(void);
static int adb_iop_probe(void);
static int adb_iop_init(void);
static int adb_iop_send_request(struct adb_request *, int);
static int adb_iop_write(struct adb_request *);
static int adb_iop_autopoll(int);
static void adb_iop_poll(void);
static int adb_iop_reset_bus(void);

 
#define ADDR_MASK       0xF0
#define OP_MASK         0x0C
#define TALK            0x0C

struct adb_driver adb_iop_driver = {
	.name         = "ISM IOP",
	.probe        = adb_iop_probe,
	.init         = adb_iop_init,
	.send_request = adb_iop_send_request,
	.autopoll     = adb_iop_autopoll,
	.poll         = adb_iop_poll,
	.reset_bus    = adb_iop_reset_bus
};

static void adb_iop_done(void)
{
	struct adb_request *req = current_req;

	adb_iop_state = idle;

	req->complete = 1;
	current_req = req->next;
	if (req->done)
		(*req->done)(req);

	if (adb_iop_state == idle)
		adb_iop_start();
}

 

static void adb_iop_complete(struct iop_msg *msg)
{
	unsigned long flags;

	local_irq_save(flags);

	adb_iop_state = awaiting_reply;

	local_irq_restore(flags);
}

 

static void adb_iop_listen(struct iop_msg *msg)
{
	struct adb_iopmsg *amsg = (struct adb_iopmsg *)msg->message;
	u8 addr = (amsg->cmd & ADDR_MASK) >> 4;
	u8 op = amsg->cmd & OP_MASK;
	unsigned long flags;
	bool req_done = false;

	local_irq_save(flags);

	 
	if (op == TALK && ((1 << addr) & autopoll_devs))
		autopoll_addr = addr;

	switch (amsg->flags & (ADB_IOP_EXPLICIT |
			       ADB_IOP_AUTOPOLL |
			       ADB_IOP_TIMEOUT)) {
	case ADB_IOP_EXPLICIT:
	case ADB_IOP_EXPLICIT | ADB_IOP_TIMEOUT:
		if (adb_iop_state == awaiting_reply) {
			struct adb_request *req = current_req;

			if (req->reply_expected) {
				req->reply_len = amsg->count + 1;
				memcpy(req->reply, &amsg->cmd, req->reply_len);
			}

			req_done = true;
		}
		break;
	case ADB_IOP_AUTOPOLL:
		if (((1 << addr) & autopoll_devs) &&
		    amsg->cmd == ADB_READREG(addr, 0))
			adb_input(&amsg->cmd, amsg->count + 1, 1);
		break;
	}
	msg->reply[0] = autopoll_addr ? ADB_IOP_AUTOPOLL : 0;
	msg->reply[1] = 0;
	msg->reply[2] = autopoll_addr ? ADB_READREG(autopoll_addr, 0) : 0;
	iop_complete_message(msg);

	if (req_done)
		adb_iop_done();

	local_irq_restore(flags);
}

 

static void adb_iop_start(void)
{
	struct adb_request *req;
	struct adb_iopmsg amsg;

	 
	req = current_req;
	if (!req)
		return;

	 
	amsg.flags = ADB_IOP_EXPLICIT;
	amsg.count = req->nbytes - 2;

	 
	memcpy(&amsg.cmd, req->data + 1, req->nbytes - 1);

	req->sent = 1;
	adb_iop_state = sending;

	 
	iop_send_message(ADB_IOP, ADB_CHAN, req, sizeof(amsg), (__u8 *)&amsg,
			 adb_iop_complete);
}

static int adb_iop_probe(void)
{
	if (!iop_ism_present)
		return -ENODEV;
	return 0;
}

static int adb_iop_init(void)
{
	pr_info("adb: IOP ISM driver v0.4 for Unified ADB\n");
	iop_listen(ADB_IOP, ADB_CHAN, adb_iop_listen, "ADB");
	return 0;
}

static int adb_iop_send_request(struct adb_request *req, int sync)
{
	int err;

	err = adb_iop_write(req);
	if (err)
		return err;

	if (sync) {
		while (!req->complete)
			adb_iop_poll();
	}
	return 0;
}

static int adb_iop_write(struct adb_request *req)
{
	unsigned long flags;

	if ((req->nbytes < 2) || (req->data[0] != ADB_PACKET)) {
		req->complete = 1;
		return -EINVAL;
	}

	req->next = NULL;
	req->sent = 0;
	req->complete = 0;
	req->reply_len = 0;

	local_irq_save(flags);

	if (current_req) {
		last_req->next = req;
		last_req = req;
	} else {
		current_req = req;
		last_req = req;
	}

	if (adb_iop_state == idle)
		adb_iop_start();

	local_irq_restore(flags);

	return 0;
}

static void adb_iop_set_ap_complete(struct iop_msg *msg)
{
	struct adb_iopmsg *amsg = (struct adb_iopmsg *)msg->message;

	autopoll_devs = get_unaligned_be16(amsg->data);
	if (autopoll_devs & (1 << autopoll_addr))
		return;
	autopoll_addr = autopoll_devs ? (ffs(autopoll_devs) - 1) : 0;
}

static int adb_iop_autopoll(int devs)
{
	struct adb_iopmsg amsg;
	unsigned long flags;
	unsigned int mask = (unsigned int)devs & 0xFFFE;

	local_irq_save(flags);

	amsg.flags = ADB_IOP_SET_AUTOPOLL | (mask ? ADB_IOP_AUTOPOLL : 0);
	amsg.count = 2;
	amsg.cmd = 0;
	put_unaligned_be16(mask, amsg.data);

	iop_send_message(ADB_IOP, ADB_CHAN, NULL, sizeof(amsg), (__u8 *)&amsg,
			 adb_iop_set_ap_complete);

	local_irq_restore(flags);

	return 0;
}

static void adb_iop_poll(void)
{
	iop_ism_irq_poll(ADB_IOP);
}

static int adb_iop_reset_bus(void)
{
	struct adb_request req;

	 
	adb_request(&req, NULL, ADBREQ_NOSEND, 1, ADB_BUSRESET);
	adb_iop_send_request(&req, 1);

	 
	mdelay(3);

	return 0;
}
