
 
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/adb.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_via.h>

static volatile unsigned char *via;

 
#define RS		0x200		 
#define B		0		 
#define A		RS		 
#define DIRB		(2*RS)		 
#define DIRA		(3*RS)		 
#define T1CL		(4*RS)		 
#define T1CH		(5*RS)		 
#define T1LL		(6*RS)		 
#define T1LH		(7*RS)		 
#define T2CL		(8*RS)		 
#define T2CH		(9*RS)		 
#define SR		(10*RS)		 
#define ACR		(11*RS)		 
#define PCR		(12*RS)		 
#define IFR		(13*RS)		 
#define IER		(14*RS)		 
#define ANH		(15*RS)		 

 
#define CTLR_IRQ	0x08		 
#define ST_MASK		0x30		 

 
#define SR_CTRL		0x1c		 
#define SR_EXT		0x0c		 
#define SR_OUT		0x10		 

 
#define IER_SET		0x80		 
#define IER_CLR		0		 
#define SR_INT		0x04		 

 
#define ST_CMD		0x00		 
#define ST_EVEN		0x10		 
#define ST_ODD		0x20		 
#define ST_IDLE		0x30		 

 
#define ADDR_MASK	0xF0
#define CMD_MASK	0x0F
#define OP_MASK		0x0C
#define TALK		0x0C

static int macii_init_via(void);
static void macii_start(void);
static irqreturn_t macii_interrupt(int irq, void *arg);
static void macii_queue_poll(void);

static int macii_probe(void);
static int macii_init(void);
static int macii_send_request(struct adb_request *req, int sync);
static int macii_write(struct adb_request *req);
static int macii_autopoll(int devs);
static void macii_poll(void);
static int macii_reset_bus(void);

struct adb_driver via_macii_driver = {
	.name         = "Mac II",
	.probe        = macii_probe,
	.init         = macii_init,
	.send_request = macii_send_request,
	.autopoll     = macii_autopoll,
	.poll         = macii_poll,
	.reset_bus    = macii_reset_bus,
};

static enum macii_state {
	idle,
	sending,
	reading,
} macii_state;

static struct adb_request *current_req;  
static struct adb_request *last_req;      
static unsigned char reply_buf[16];         
static unsigned char *reply_ptr;      
static bool reading_reply;        
static int data_index;       
static int reply_len;  
static int status;           
static bool bus_timeout;                    
static bool srq_asserted;     
static u8 last_cmd;               
static u8 last_talk_cmd;     
static u8 last_poll_cmd;  
static unsigned int autopoll_devs;   

 
static int macii_probe(void)
{
	if (macintosh_config->adb_type != MAC_ADB_II)
		return -ENODEV;

	via = via1;

	pr_info("adb: Mac II ADB Driver v1.0 for Unified ADB\n");
	return 0;
}

 
static int macii_init(void)
{
	unsigned long flags;
	int err;

	local_irq_save(flags);

	err = macii_init_via();
	if (err)
		goto out;

	err = request_irq(IRQ_MAC_ADB, macii_interrupt, 0, "ADB",
			  macii_interrupt);
	if (err)
		goto out;

	macii_state = idle;
out:
	local_irq_restore(flags);
	return err;
}

 
static int macii_init_via(void)
{
	unsigned char x;

	 
	via[DIRB] = (via[DIRB] | ST_EVEN | ST_ODD) & ~CTLR_IRQ;

	 
	via[B] |= ST_IDLE;

	 
	via[ACR] = (via[ACR] & ~SR_CTRL) | SR_EXT;

	 
	x = via[SR];

	return 0;
}

 
static void macii_queue_poll(void)
{
	static struct adb_request req;
	unsigned char poll_command;
	unsigned int poll_addr;

	 
	if (!autopoll_devs)
		return;

	 
	poll_addr = (last_poll_cmd & ADDR_MASK) >> 4;
	if ((srq_asserted && last_cmd == last_poll_cmd) ||
	    !(autopoll_devs & (1 << poll_addr))) {
		unsigned int higher_devs;

		higher_devs = autopoll_devs & -(1 << (poll_addr + 1));
		poll_addr = ffs(higher_devs ? higher_devs : autopoll_devs) - 1;
	}

	 
	poll_command = ADB_READREG(poll_addr, 0);

	 
	if (poll_command == last_cmd)
		return;

	adb_request(&req, NULL, ADBREQ_NOSEND, 1, poll_command);

	req.sent = 0;
	req.complete = 0;
	req.reply_len = 0;
	req.next = current_req;

	if (WARN_ON(current_req)) {
		current_req = &req;
	} else {
		current_req = &req;
		last_req = &req;
	}
}

 
static int macii_send_request(struct adb_request *req, int sync)
{
	int err;

	err = macii_write(req);
	if (err)
		return err;

	if (sync)
		while (!req->complete)
			macii_poll();

	return 0;
}

 
static int macii_write(struct adb_request *req)
{
	unsigned long flags;

	if (req->nbytes < 2 || req->data[0] != ADB_PACKET || req->nbytes > 15) {
		req->complete = 1;
		return -EINVAL;
	}

	req->next = NULL;
	req->sent = 0;
	req->complete = 0;
	req->reply_len = 0;

	local_irq_save(flags);

	if (current_req != NULL) {
		last_req->next = req;
		last_req = req;
	} else {
		current_req = req;
		last_req = req;
		if (macii_state == idle)
			macii_start();
	}

	local_irq_restore(flags);

	return 0;
}

 
static int macii_autopoll(int devs)
{
	unsigned long flags;

	local_irq_save(flags);

	 
	autopoll_devs = (unsigned int)devs & 0xFFFE;

	if (!current_req) {
		macii_queue_poll();
		if (current_req && macii_state == idle)
			macii_start();
	}

	local_irq_restore(flags);

	return 0;
}

 
static void macii_poll(void)
{
	macii_interrupt(0, NULL);
}

 
static int macii_reset_bus(void)
{
	struct adb_request req;

	 
	adb_request(&req, NULL, ADBREQ_NOSEND, 1, ADB_BUSRESET);
	macii_send_request(&req, 1);

	 
	udelay(3000);

	return 0;
}

 
static void macii_start(void)
{
	struct adb_request *req;

	req = current_req;

	 

	 
	via[ACR] |= SR_OUT;
	 
	via[SR] = req->data[1];
	 
	via[B] = (via[B] & ~ST_MASK) | ST_CMD;

	macii_state = sending;
	data_index = 2;

	bus_timeout = false;
	srq_asserted = false;
}

 
static irqreturn_t macii_interrupt(int irq, void *arg)
{
	int x;
	struct adb_request *req;
	unsigned long flags;

	local_irq_save(flags);

	if (!arg) {
		 
		if (via[IFR] & SR_INT)
			via[IFR] = SR_INT;
		else {
			local_irq_restore(flags);
			return IRQ_NONE;
		}
	}

	status = via[B] & (ST_MASK | CTLR_IRQ);

	switch (macii_state) {
	case idle:
		WARN_ON((status & ST_MASK) != ST_IDLE);

		reply_ptr = reply_buf;
		reading_reply = false;

		bus_timeout = false;
		srq_asserted = false;

		x = via[SR];

		if (!(status & CTLR_IRQ)) {
			 
			macii_state = reading;
			*reply_ptr = x;
			reply_len = 1;
		} else {
			 
			reply_len = 0;
			break;
		}

		 
		via[B] = (via[B] & ~ST_MASK) | ST_EVEN;
		break;

	case sending:
		req = current_req;

		if (status == (ST_CMD | CTLR_IRQ)) {
			 

			 
			last_cmd = req->data[1];
			if ((last_cmd & OP_MASK) == TALK) {
				last_talk_cmd = last_cmd;
				if ((last_cmd & CMD_MASK) == ADB_READREG(0, 0))
					last_poll_cmd = last_cmd;
			}
		}

		if (status == ST_CMD) {
			 
			macii_state = reading;

			reading_reply = false;
			reply_ptr = reply_buf;
			*reply_ptr = last_talk_cmd;
			reply_len = 1;

			 
			via[ACR] &= ~SR_OUT;
			x = via[SR];
		} else if (data_index >= req->nbytes) {
			req->sent = 1;

			if (req->reply_expected) {
				macii_state = reading;

				reading_reply = true;
				reply_ptr = req->reply;
				*reply_ptr = req->data[1];
				reply_len = 1;

				via[ACR] &= ~SR_OUT;
				x = via[SR];
			} else if ((req->data[1] & OP_MASK) == TALK) {
				macii_state = reading;

				reading_reply = false;
				reply_ptr = reply_buf;
				*reply_ptr = req->data[1];
				reply_len = 1;

				via[ACR] &= ~SR_OUT;
				x = via[SR];

				req->complete = 1;
				current_req = req->next;
				if (req->done)
					(*req->done)(req);
			} else {
				macii_state = idle;

				req->complete = 1;
				current_req = req->next;
				if (req->done)
					(*req->done)(req);
				break;
			}
		} else {
			via[SR] = req->data[data_index++];
		}

		if ((via[B] & ST_MASK) == ST_CMD) {
			 
			via[B] = (via[B] & ~ST_MASK) | ST_EVEN;
		} else {
			 
			via[B] ^= ST_MASK;
		}
		break;

	case reading:
		x = via[SR];
		WARN_ON((status & ST_MASK) == ST_CMD ||
			(status & ST_MASK) == ST_IDLE);

		if (!(status & CTLR_IRQ)) {
			if (status == ST_EVEN && reply_len == 1) {
				bus_timeout = true;
			} else if (status == ST_ODD && reply_len == 2) {
				srq_asserted = true;
			} else {
				macii_state = idle;

				if (bus_timeout)
					reply_len = 0;

				if (reading_reply) {
					struct adb_request *req = current_req;

					req->reply_len = reply_len;

					req->complete = 1;
					current_req = req->next;
					if (req->done)
						(*req->done)(req);
				} else if (reply_len && autopoll_devs &&
					   reply_buf[0] == last_poll_cmd) {
					adb_input(reply_buf, reply_len, 1);
				}
				break;
			}
		}

		if (reply_len < ARRAY_SIZE(reply_buf)) {
			reply_ptr++;
			*reply_ptr = x;
			reply_len++;
		}

		 
		via[B] ^= ST_MASK;
		break;

	default:
		break;
	}

	if (macii_state == idle) {
		if (!current_req)
			macii_queue_poll();

		if (current_req)
			macii_start();

		if (macii_state == idle) {
			via[ACR] &= ~SR_OUT;
			x = via[SR];
			via[B] = (via[B] & ~ST_MASK) | ST_IDLE;
		}
	}

	local_irq_restore(flags);
	return IRQ_HANDLED;
}
