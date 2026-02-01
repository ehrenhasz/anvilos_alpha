




#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include "rc-core-priv.h"

#define IMON_UNIT		416  
#define IMON_BITS		30
#define IMON_CHKBITS		(BIT(30) | BIT(25) | BIT(24) | BIT(22) | \
				 BIT(21) | BIT(20) | BIT(19) | BIT(18) | \
				 BIT(17) | BIT(16) | BIT(14) | BIT(13) | \
				 BIT(12) | BIT(11) | BIT(10) | BIT(9))

 
enum imon_state {
	STATE_INACTIVE,
	STATE_BIT_CHK,
	STATE_BIT_START,
	STATE_FINISHED,
	STATE_ERROR,
};

static void ir_imon_decode_scancode(struct rc_dev *dev)
{
	struct imon_dec *imon = &dev->raw->imon;

	 
	if (imon->bits == 0x299115b7)
		imon->stick_keyboard = !imon->stick_keyboard;

	if ((imon->bits & 0xfc0000ff) == 0x680000b7) {
		int rel_x, rel_y;
		u8 buf;

		buf = imon->bits >> 16;
		rel_x = (buf & 0x08) | (buf & 0x10) >> 2 |
			(buf & 0x20) >> 4 | (buf & 0x40) >> 6;
		if (imon->bits & 0x02000000)
			rel_x |= ~0x0f;
		buf = imon->bits >> 8;
		rel_y = (buf & 0x08) | (buf & 0x10) >> 2 |
			(buf & 0x20) >> 4 | (buf & 0x40) >> 6;
		if (imon->bits & 0x01000000)
			rel_y |= ~0x0f;

		if (rel_x && rel_y && imon->stick_keyboard) {
			if (abs(rel_y) > abs(rel_x))
				imon->bits = rel_y > 0 ?
					0x289515b7 :  
					0x2aa515b7;   
			else
				imon->bits = rel_x > 0 ?
					0x2ba515b7 :  
					0x29a515b7;   
		}

		if (!imon->stick_keyboard) {
			input_report_rel(dev->input_dev, REL_X, rel_x);
			input_report_rel(dev->input_dev, REL_Y, rel_y);

			input_report_key(dev->input_dev, BTN_LEFT,
					 (imon->bits & 0x00010000) != 0);
			input_report_key(dev->input_dev, BTN_RIGHT,
					 (imon->bits & 0x00040000) != 0);
		}
	}

	rc_keydown(dev, RC_PROTO_IMON, imon->bits, 0);
}

 
static int ir_imon_decode(struct rc_dev *dev, struct ir_raw_event ev)
{
	struct imon_dec *data = &dev->raw->imon;

	if (!is_timing_event(ev)) {
		if (ev.overflow)
			data->state = STATE_INACTIVE;
		return 0;
	}

	dev_dbg(&dev->dev,
		"iMON decode started at state %d bitno %d (%uus %s)\n",
		data->state, data->count, ev.duration, TO_STR(ev.pulse));

	 
	if (data->state == STATE_ERROR) {
		if (!ev.pulse && ev.duration > MS_TO_US(10))
			data->state = STATE_INACTIVE;
		return 0;
	}

	for (;;) {
		if (!geq_margin(ev.duration, IMON_UNIT, IMON_UNIT / 2))
			return 0;

		decrease_duration(&ev, IMON_UNIT);

		switch (data->state) {
		case STATE_INACTIVE:
			if (ev.pulse) {
				data->state = STATE_BIT_CHK;
				data->bits = 0;
				data->count = IMON_BITS;
			}
			break;
		case STATE_BIT_CHK:
			if (IMON_CHKBITS & BIT(data->count))
				data->last_chk = ev.pulse;
			else if (ev.pulse)
				goto err_out;
			data->state = STATE_BIT_START;
			break;
		case STATE_BIT_START:
			data->bits <<= 1;
			if (!ev.pulse)
				data->bits |= 1;

			if (IMON_CHKBITS & BIT(data->count)) {
				if (data->last_chk != !(data->bits & 3))
					goto err_out;
			}

			if (!data->count--)
				data->state = STATE_FINISHED;
			else
				data->state = STATE_BIT_CHK;
			break;
		case STATE_FINISHED:
			if (ev.pulse)
				goto err_out;
			ir_imon_decode_scancode(dev);
			data->state = STATE_INACTIVE;
			break;
		}
	}

err_out:
	dev_dbg(&dev->dev,
		"iMON decode failed at state %d bitno %d (%uus %s)\n",
		data->state, data->count, ev.duration, TO_STR(ev.pulse));

	data->state = STATE_ERROR;

	return -EINVAL;
}

 
static int ir_imon_encode(enum rc_proto protocol, u32 scancode,
			  struct ir_raw_event *events, unsigned int max)
{
	struct ir_raw_event *e = events;
	int i, pulse;

	if (!max--)
		return -ENOBUFS;
	init_ir_raw_event_duration(e, 1, IMON_UNIT);

	for (i = IMON_BITS; i >= 0; i--) {
		if (BIT(i) & IMON_CHKBITS)
			pulse = !(scancode & (BIT(i) | BIT(i + 1)));
		else
			pulse = 0;

		if (pulse == e->pulse) {
			e->duration += IMON_UNIT;
		} else {
			if (!max--)
				return -ENOBUFS;
			init_ir_raw_event_duration(++e, pulse, IMON_UNIT);
		}

		pulse = !(scancode & BIT(i));

		if (pulse == e->pulse) {
			e->duration += IMON_UNIT;
		} else {
			if (!max--)
				return -ENOBUFS;
			init_ir_raw_event_duration(++e, pulse, IMON_UNIT);
		}
	}

	if (e->pulse)
		e++;

	return e - events;
}

static int ir_imon_register(struct rc_dev *dev)
{
	struct imon_dec *imon = &dev->raw->imon;

	imon->stick_keyboard = false;

	return 0;
}

static struct ir_raw_handler imon_handler = {
	.protocols	= RC_PROTO_BIT_IMON,
	.decode		= ir_imon_decode,
	.encode		= ir_imon_encode,
	.carrier	= 38000,
	.raw_register	= ir_imon_register,
	.min_timeout	= IMON_UNIT * IMON_BITS * 2,
};

static int __init ir_imon_decode_init(void)
{
	ir_raw_handler_register(&imon_handler);

	pr_info("IR iMON protocol handler initialized\n");
	return 0;
}

static void __exit ir_imon_decode_exit(void)
{
	ir_raw_handler_unregister(&imon_handler);
}

module_init(ir_imon_decode_init);
module_exit(ir_imon_decode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sean Young <sean@mess.org>");
MODULE_DESCRIPTION("iMON IR protocol decoder");
