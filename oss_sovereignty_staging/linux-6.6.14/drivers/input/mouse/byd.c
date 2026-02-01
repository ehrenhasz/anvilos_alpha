
 

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/libps2.h>
#include <linux/serio.h>
#include <linux/slab.h>

#include "psmouse.h"
#include "byd.h"

 
#define PS2_Y_OVERFLOW	BIT_MASK(7)
#define PS2_X_OVERFLOW	BIT_MASK(6)
#define PS2_Y_SIGN	BIT_MASK(5)
#define PS2_X_SIGN	BIT_MASK(4)
#define PS2_ALWAYS_1	BIT_MASK(3)
#define PS2_MIDDLE	BIT_MASK(2)
#define PS2_RIGHT	BIT_MASK(1)
#define PS2_LEFT	BIT_MASK(0)

 

 
#define BYD_PAD_WIDTH		11264
#define BYD_PAD_HEIGHT		6656
#define BYD_PAD_RESOLUTION	111

 
#define BYD_DT			11
 
#define BYD_TOUCH_TIMEOUT	msecs_to_jiffies(64)

 

 
#define BYD_CMD_SET_OFFSCREEN_SWIPE		0x10cc
 
#define BYD_CMD_SET_TAP_DRAG_DELAY_TIME		0x10cf
 
#define BYD_CMD_SET_PHYSICAL_BUTTONS		0x10d0
 
#define BYD_CMD_SET_ABSOLUTE_MODE		0x10d1
 
#define BYD_CMD_SET_TWO_FINGER_SCROLL		0x10d2
 
#define BYD_CMD_SET_HANDEDNESS			0x10d3
 
#define BYD_CMD_SET_TAP				0x10d4
 
#define BYD_CMD_SET_TAP_DRAG			0x10d5
 
#define BYD_CMD_SET_TOUCH_SENSITIVITY		0x10d6
 
#define BYD_CMD_SET_ONE_FINGER_SCROLL		0x10d7
 
#define BYD_CMD_SET_ONE_FINGER_SCROLL_FUNC	0x10d8
 
#define BYD_CMD_SET_SLIDING_SPEED		0x10da
 
#define BYD_CMD_SET_EDGE_MOTION			0x10db
 
#define BYD_CMD_SET_LEFT_EDGE_REGION		0x10dc
 
#define BYD_CMD_SET_TOP_EDGE_REGION		0x10dd
 
#define BYD_CMD_SET_PALM_CHECK			0x10de
 
#define BYD_CMD_SET_RIGHT_EDGE_REGION		0x10df
 
#define BYD_CMD_SET_BOTTOM_EDGE_REGION		0x10e1
 
#define BYD_CMD_SET_MULTITOUCH			0x10e3
 
#define BYD_CMD_SET_EDGE_MOTION_SPEED		0x10e4
 
#define BYD_CMD_SET_TWO_FINGER_SCROLL_FUNC	0x10e5

 
#define BYD_PACKET_ABSOLUTE			0xf8
#define BYD_PACKET_RELATIVE			0x00
 
#define BYD_PACKET_PINCH_IN			0xd8
#define BYD_PACKET_PINCH_OUT			0x28
#define BYD_PACKET_ROTATE_CLOCKWISE		0x29
#define BYD_PACKET_ROTATE_ANTICLOCKWISE		0xd7
#define BYD_PACKET_TWO_FINGER_SCROLL_RIGHT	0x2a
#define BYD_PACKET_TWO_FINGER_SCROLL_DOWN	0x2b
#define BYD_PACKET_TWO_FINGER_SCROLL_UP		0xd5
#define BYD_PACKET_TWO_FINGER_SCROLL_LEFT	0xd6
#define BYD_PACKET_THREE_FINGER_SWIPE_RIGHT	0x2c
#define BYD_PACKET_THREE_FINGER_SWIPE_DOWN	0x2d
#define BYD_PACKET_THREE_FINGER_SWIPE_UP	0xd3
#define BYD_PACKET_THREE_FINGER_SWIPE_LEFT	0xd4
#define BYD_PACKET_FOUR_FINGER_DOWN		0x33
#define BYD_PACKET_FOUR_FINGER_UP		0xcd
#define BYD_PACKET_REGION_SCROLL_RIGHT		0x35
#define BYD_PACKET_REGION_SCROLL_DOWN		0x36
#define BYD_PACKET_REGION_SCROLL_UP		0xca
#define BYD_PACKET_REGION_SCROLL_LEFT		0xcb
#define BYD_PACKET_RIGHT_CORNER_CLICK		0xd2
#define BYD_PACKET_LEFT_CORNER_CLICK		0x2e
#define BYD_PACKET_LEFT_AND_RIGHT_CORNER_CLICK	0x2f
#define BYD_PACKET_ONTO_PAD_SWIPE_RIGHT		0x37
#define BYD_PACKET_ONTO_PAD_SWIPE_DOWN		0x30
#define BYD_PACKET_ONTO_PAD_SWIPE_UP		0xd0
#define BYD_PACKET_ONTO_PAD_SWIPE_LEFT		0xc9

struct byd_data {
	struct timer_list timer;
	struct psmouse *psmouse;
	s32 abs_x;
	s32 abs_y;
	typeof(jiffies) last_touch_time;
	bool btn_left;
	bool btn_right;
	bool touch;
};

static void byd_report_input(struct psmouse *psmouse)
{
	struct byd_data *priv = psmouse->private;
	struct input_dev *dev = psmouse->dev;

	input_report_key(dev, BTN_TOUCH, priv->touch);
	input_report_key(dev, BTN_TOOL_FINGER, priv->touch);

	input_report_abs(dev, ABS_X, priv->abs_x);
	input_report_abs(dev, ABS_Y, priv->abs_y);
	input_report_key(dev, BTN_LEFT, priv->btn_left);
	input_report_key(dev, BTN_RIGHT, priv->btn_right);

	input_sync(dev);
}

static void byd_clear_touch(struct timer_list *t)
{
	struct byd_data *priv = from_timer(priv, t, timer);
	struct psmouse *psmouse = priv->psmouse;

	serio_pause_rx(psmouse->ps2dev.serio);
	priv->touch = false;

	byd_report_input(psmouse);

	serio_continue_rx(psmouse->ps2dev.serio);

	 
	priv->abs_x = BYD_PAD_WIDTH / 2;
	priv->abs_y = BYD_PAD_HEIGHT / 2;
}

static psmouse_ret_t byd_process_byte(struct psmouse *psmouse)
{
	struct byd_data *priv = psmouse->private;
	u8 *pkt = psmouse->packet;

	if (psmouse->pktcnt > 0 && !(pkt[0] & PS2_ALWAYS_1)) {
		psmouse_warn(psmouse, "Always_1 bit not 1. pkt[0] = %02x\n",
			     pkt[0]);
		return PSMOUSE_BAD_DATA;
	}

	if (psmouse->pktcnt < psmouse->pktsize)
		return PSMOUSE_GOOD_DATA;

	 
	switch (pkt[3]) {
	case BYD_PACKET_ABSOLUTE:
		 
		if (!priv->touch) {
			 
			typeof(jiffies) tap_time =
				priv->last_touch_time + BYD_TOUCH_TIMEOUT;
			priv->touch = time_after(jiffies, tap_time);

			 
			priv->abs_x = pkt[1] * (BYD_PAD_WIDTH / 256);
			priv->abs_y = (255 - pkt[2]) * (BYD_PAD_HEIGHT / 256);
		}
		break;
	case BYD_PACKET_RELATIVE: {
		 
		 
		u32 signx = pkt[0] & PS2_X_SIGN ? ~0xFF : 0;
		u32 signy = pkt[0] & PS2_Y_SIGN ? ~0xFF : 0;
		s32 dx = signx | (int) pkt[1];
		s32 dy = signy | (int) pkt[2];

		 
		priv->abs_x += dx * BYD_DT;
		priv->abs_y -= dy * BYD_DT;

		priv->touch = true;
		break;
	}
	default:
		psmouse_warn(psmouse,
			     "Unrecognized Z: pkt = %02x %02x %02x %02x\n",
			     psmouse->packet[0], psmouse->packet[1],
			     psmouse->packet[2], psmouse->packet[3]);
		return PSMOUSE_BAD_DATA;
	}

	priv->btn_left = pkt[0] & PS2_LEFT;
	priv->btn_right = pkt[0] & PS2_RIGHT;

	byd_report_input(psmouse);

	 
	if (priv->touch) {
		priv->last_touch_time = jiffies;
		mod_timer(&priv->timer, jiffies + BYD_TOUCH_TIMEOUT);
	}

	return PSMOUSE_FULL_PACKET;
}

static int byd_reset_touchpad(struct psmouse *psmouse)
{
	struct ps2dev *ps2dev = &psmouse->ps2dev;
	u8 param[4];
	size_t i;

	static const struct {
		u16 command;
		u8 arg;
	} seq[] = {
		 
		{ PSMOUSE_CMD_SETRATE, 0xC8 },
		{ PSMOUSE_CMD_SETRATE, 0x64 },
		{ PSMOUSE_CMD_SETRATE, 0x50 },
		{ PSMOUSE_CMD_GETID, 0 },
		{ PSMOUSE_CMD_ENABLE, 0 },
		 
		{ 0x10E2, 0x00 },
		{ 0x10E0, 0x02 },
		 
		{ 0x14E0, 0x01 },
		 
		{ BYD_CMD_SET_HANDEDNESS, 0x01 },
		{ BYD_CMD_SET_PHYSICAL_BUTTONS, 0x04 },
		{ BYD_CMD_SET_TAP, 0x02 },
		{ BYD_CMD_SET_ONE_FINGER_SCROLL, 0x04 },
		{ BYD_CMD_SET_ONE_FINGER_SCROLL_FUNC, 0x04 },
		{ BYD_CMD_SET_EDGE_MOTION, 0x01 },
		{ BYD_CMD_SET_PALM_CHECK, 0x00 },
		{ BYD_CMD_SET_MULTITOUCH, 0x02 },
		{ BYD_CMD_SET_TWO_FINGER_SCROLL, 0x04 },
		{ BYD_CMD_SET_TWO_FINGER_SCROLL_FUNC, 0x04 },
		{ BYD_CMD_SET_LEFT_EDGE_REGION, 0x00 },
		{ BYD_CMD_SET_TOP_EDGE_REGION, 0x00 },
		{ BYD_CMD_SET_RIGHT_EDGE_REGION, 0x00 },
		{ BYD_CMD_SET_BOTTOM_EDGE_REGION, 0x00 },
		{ BYD_CMD_SET_ABSOLUTE_MODE, 0x02 },
		 
		{ 0x10E0, 0x00 },
		{ 0x10E2, 0x01 },
	};

	for (i = 0; i < ARRAY_SIZE(seq); ++i) {
		memset(param, 0, sizeof(param));
		param[0] = seq[i].arg;
		if (ps2_command(ps2dev, param, seq[i].command))
			return -EIO;
	}

	psmouse_set_state(psmouse, PSMOUSE_ACTIVATED);
	return 0;
}

static int byd_reconnect(struct psmouse *psmouse)
{
	int retry = 0, error = 0;

	psmouse_dbg(psmouse, "Reconnect\n");
	do {
		psmouse_reset(psmouse);
		if (retry)
			ssleep(1);
		error = byd_detect(psmouse, 0);
	} while (error && ++retry < 3);

	if (error)
		return error;

	psmouse_dbg(psmouse, "Reconnected after %d attempts\n", retry);

	error = byd_reset_touchpad(psmouse);
	if (error) {
		psmouse_err(psmouse, "Unable to initialize device\n");
		return error;
	}

	return 0;
}

static void byd_disconnect(struct psmouse *psmouse)
{
	struct byd_data *priv = psmouse->private;

	if (priv) {
		del_timer(&priv->timer);
		kfree(psmouse->private);
		psmouse->private = NULL;
	}
}

int byd_detect(struct psmouse *psmouse, bool set_properties)
{
	struct ps2dev *ps2dev = &psmouse->ps2dev;
	u8 param[4] = {0x03, 0x00, 0x00, 0x00};

	if (ps2_command(ps2dev, param, PSMOUSE_CMD_SETRES))
		return -1;
	if (ps2_command(ps2dev, param, PSMOUSE_CMD_SETRES))
		return -1;
	if (ps2_command(ps2dev, param, PSMOUSE_CMD_SETRES))
		return -1;
	if (ps2_command(ps2dev, param, PSMOUSE_CMD_SETRES))
		return -1;
	if (ps2_command(ps2dev, param, PSMOUSE_CMD_GETINFO))
		return -1;

	if (param[1] != 0x03 || param[2] != 0x64)
		return -ENODEV;

	psmouse_dbg(psmouse, "BYD touchpad detected\n");

	if (set_properties) {
		psmouse->vendor = "BYD";
		psmouse->name = "TouchPad";
	}

	return 0;
}

int byd_init(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct byd_data *priv;

	if (psmouse_reset(psmouse))
		return -EIO;

	if (byd_reset_touchpad(psmouse))
		return -EIO;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->psmouse = psmouse;
	timer_setup(&priv->timer, byd_clear_touch, 0);

	psmouse->private = priv;
	psmouse->disconnect = byd_disconnect;
	psmouse->reconnect = byd_reconnect;
	psmouse->protocol_handler = byd_process_byte;
	psmouse->pktsize = 4;
	psmouse->resync_time = 0;

	__set_bit(INPUT_PROP_POINTER, dev->propbit);
	 
	__set_bit(BTN_TOUCH, dev->keybit);
	__set_bit(BTN_TOOL_FINGER, dev->keybit);
	 
	__set_bit(BTN_LEFT, dev->keybit);
	__set_bit(BTN_RIGHT, dev->keybit);
	__clear_bit(BTN_MIDDLE, dev->keybit);

	 
	__set_bit(EV_ABS, dev->evbit);
	input_set_abs_params(dev, ABS_X, 0, BYD_PAD_WIDTH, 0, 0);
	input_set_abs_params(dev, ABS_Y, 0, BYD_PAD_HEIGHT, 0, 0);
	input_abs_set_res(dev, ABS_X, BYD_PAD_RESOLUTION);
	input_abs_set_res(dev, ABS_Y, BYD_PAD_RESOLUTION);
	 
	__clear_bit(EV_REL, dev->evbit);
	__clear_bit(REL_X, dev->relbit);
	__clear_bit(REL_Y, dev->relbit);

	return 0;
}
