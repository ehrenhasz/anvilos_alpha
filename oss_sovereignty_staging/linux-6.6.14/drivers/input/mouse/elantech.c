
 

#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/platform_device.h>
#include <linux/serio.h>
#include <linux/libps2.h>
#include <asm/unaligned.h>
#include "psmouse.h"
#include "elantech.h"
#include "elan_i2c.h"

#define elantech_debug(fmt, ...)					\
	do {								\
		if (etd->info.debug)					\
			psmouse_printk(KERN_DEBUG, psmouse,		\
					fmt, ##__VA_ARGS__);		\
	} while (0)

 
static int synaptics_send_cmd(struct psmouse *psmouse, unsigned char c,
				unsigned char *param)
{
	if (ps2_sliced_command(&psmouse->ps2dev, c) ||
	    ps2_command(&psmouse->ps2dev, param, PSMOUSE_CMD_GETINFO)) {
		psmouse_err(psmouse, "%s query 0x%02x failed.\n", __func__, c);
		return -1;
	}

	return 0;
}

 
static int elantech_send_cmd(struct psmouse *psmouse, unsigned char c,
				unsigned char *param)
{
	struct ps2dev *ps2dev = &psmouse->ps2dev;

	if (ps2_command(ps2dev, NULL, ETP_PS2_CUSTOM_COMMAND) ||
	    ps2_command(ps2dev, NULL, c) ||
	    ps2_command(ps2dev, param, PSMOUSE_CMD_GETINFO)) {
		psmouse_err(psmouse, "%s query 0x%02x failed.\n", __func__, c);
		return -1;
	}

	return 0;
}

 
static int elantech_ps2_command(struct psmouse *psmouse,
				unsigned char *param, int command)
{
	struct ps2dev *ps2dev = &psmouse->ps2dev;
	struct elantech_data *etd = psmouse->private;
	int rc;
	int tries = ETP_PS2_COMMAND_TRIES;

	do {
		rc = ps2_command(ps2dev, param, command);
		if (rc == 0)
			break;
		tries--;
		elantech_debug("retrying ps2 command 0x%02x (%d).\n",
				command, tries);
		msleep(ETP_PS2_COMMAND_DELAY);
	} while (tries > 0);

	if (rc)
		psmouse_err(psmouse, "ps2 command 0x%02x failed.\n", command);

	return rc;
}

 
static int elantech_read_reg_params(struct psmouse *psmouse, u8 reg, u8 *param)
{
	if (elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
	    elantech_ps2_command(psmouse, NULL, ETP_REGISTER_READWRITE) ||
	    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
	    elantech_ps2_command(psmouse, NULL, reg) ||
	    elantech_ps2_command(psmouse, param, PSMOUSE_CMD_GETINFO)) {
		psmouse_err(psmouse,
			    "failed to read register %#02x\n", reg);
		return -EIO;
	}

	return 0;
}

 
static int elantech_write_reg_params(struct psmouse *psmouse, u8 reg, u8 *param)
{
	if (elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
	    elantech_ps2_command(psmouse, NULL, ETP_REGISTER_READWRITE) ||
	    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
	    elantech_ps2_command(psmouse, NULL, reg) ||
	    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
	    elantech_ps2_command(psmouse, NULL, param[0]) ||
	    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
	    elantech_ps2_command(psmouse, NULL, param[1]) ||
	    elantech_ps2_command(psmouse, NULL, PSMOUSE_CMD_SETSCALE11)) {
		psmouse_err(psmouse,
			    "failed to write register %#02x with value %#02x%#02x\n",
			    reg, param[0], param[1]);
		return -EIO;
	}

	return 0;
}

 
static int elantech_read_reg(struct psmouse *psmouse, unsigned char reg,
				unsigned char *val)
{
	struct elantech_data *etd = psmouse->private;
	unsigned char param[3];
	int rc = 0;

	if (reg < 0x07 || reg > 0x26)
		return -1;

	if (reg > 0x11 && reg < 0x20)
		return -1;

	switch (etd->info.hw_version) {
	case 1:
		if (ps2_sliced_command(&psmouse->ps2dev, ETP_REGISTER_READ) ||
		    ps2_sliced_command(&psmouse->ps2dev, reg) ||
		    ps2_command(&psmouse->ps2dev, param, PSMOUSE_CMD_GETINFO)) {
			rc = -1;
		}
		break;

	case 2:
		if (elantech_ps2_command(psmouse,  NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse,  NULL, ETP_REGISTER_READ) ||
		    elantech_ps2_command(psmouse,  NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse,  NULL, reg) ||
		    elantech_ps2_command(psmouse, param, PSMOUSE_CMD_GETINFO)) {
			rc = -1;
		}
		break;

	case 3 ... 4:
		if (elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, ETP_REGISTER_READWRITE) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, reg) ||
		    elantech_ps2_command(psmouse, param, PSMOUSE_CMD_GETINFO)) {
			rc = -1;
		}
		break;
	}

	if (rc)
		psmouse_err(psmouse, "failed to read register 0x%02x.\n", reg);
	else if (etd->info.hw_version != 4)
		*val = param[0];
	else
		*val = param[1];

	return rc;
}

 
static int elantech_write_reg(struct psmouse *psmouse, unsigned char reg,
				unsigned char val)
{
	struct elantech_data *etd = psmouse->private;
	int rc = 0;

	if (reg < 0x07 || reg > 0x26)
		return -1;

	if (reg > 0x11 && reg < 0x20)
		return -1;

	switch (etd->info.hw_version) {
	case 1:
		if (ps2_sliced_command(&psmouse->ps2dev, ETP_REGISTER_WRITE) ||
		    ps2_sliced_command(&psmouse->ps2dev, reg) ||
		    ps2_sliced_command(&psmouse->ps2dev, val) ||
		    ps2_command(&psmouse->ps2dev, NULL, PSMOUSE_CMD_SETSCALE11)) {
			rc = -1;
		}
		break;

	case 2:
		if (elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, ETP_REGISTER_WRITE) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, reg) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, val) ||
		    elantech_ps2_command(psmouse, NULL, PSMOUSE_CMD_SETSCALE11)) {
			rc = -1;
		}
		break;

	case 3:
		if (elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, ETP_REGISTER_READWRITE) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, reg) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, val) ||
		    elantech_ps2_command(psmouse, NULL, PSMOUSE_CMD_SETSCALE11)) {
			rc = -1;
		}
		break;

	case 4:
		if (elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, ETP_REGISTER_READWRITE) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, reg) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, ETP_REGISTER_READWRITE) ||
		    elantech_ps2_command(psmouse, NULL, ETP_PS2_CUSTOM_COMMAND) ||
		    elantech_ps2_command(psmouse, NULL, val) ||
		    elantech_ps2_command(psmouse, NULL, PSMOUSE_CMD_SETSCALE11)) {
			rc = -1;
		}
		break;
	}

	if (rc)
		psmouse_err(psmouse,
			    "failed to write register 0x%02x with value 0x%02x.\n",
			    reg, val);

	return rc;
}

 
static void elantech_packet_dump(struct psmouse *psmouse)
{
	psmouse_printk(KERN_DEBUG, psmouse, "PS/2 packet [%*ph]\n",
		       psmouse->pktsize, psmouse->packet);
}

 
static inline int elantech_is_buttonpad(struct elantech_device_info *info)
{
	return info->fw_version & 0x001000;
}

 
static void elantech_report_absolute_v1(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	int fingers;

	if (etd->info.fw_version < 0x020000) {
		 
		fingers = ((packet[1] & 0x80) >> 7) +
				((packet[1] & 0x30) >> 4);
	} else {
		 
		fingers = (packet[0] & 0xc0) >> 6;
	}

	if (etd->info.jumpy_cursor) {
		if (fingers != 1) {
			etd->single_finger_reports = 0;
		} else if (etd->single_finger_reports < 2) {
			 
			etd->single_finger_reports++;
			elantech_debug("discarding packet\n");
			return;
		}
	}

	input_report_key(dev, BTN_TOUCH, fingers != 0);

	 
	if (fingers) {
		input_report_abs(dev, ABS_X,
			((packet[1] & 0x0c) << 6) | packet[2]);
		input_report_abs(dev, ABS_Y,
			etd->y_max - (((packet[1] & 0x03) << 8) | packet[3]));
	}

	input_report_key(dev, BTN_TOOL_FINGER, fingers == 1);
	input_report_key(dev, BTN_TOOL_DOUBLETAP, fingers == 2);
	input_report_key(dev, BTN_TOOL_TRIPLETAP, fingers == 3);

	psmouse_report_standard_buttons(dev, packet[0]);

	if (etd->info.fw_version < 0x020000 &&
	    (etd->info.capabilities[0] & ETP_CAP_HAS_ROCKER)) {
		 
		input_report_key(dev, BTN_FORWARD, packet[0] & 0x40);
		 
		input_report_key(dev, BTN_BACK, packet[0] & 0x80);
	}

	input_sync(dev);
}

static void elantech_set_slot(struct input_dev *dev, int slot, bool active,
			      unsigned int x, unsigned int y)
{
	input_mt_slot(dev, slot);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, active);
	if (active) {
		input_report_abs(dev, ABS_MT_POSITION_X, x);
		input_report_abs(dev, ABS_MT_POSITION_Y, y);
	}
}

 
static void elantech_report_semi_mt_data(struct input_dev *dev,
					 unsigned int num_fingers,
					 unsigned int x1, unsigned int y1,
					 unsigned int x2, unsigned int y2)
{
	elantech_set_slot(dev, 0, num_fingers != 0, x1, y1);
	elantech_set_slot(dev, 1, num_fingers >= 2, x2, y2);
}

 
static void elantech_report_absolute_v2(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;
	struct input_dev *dev = psmouse->dev;
	unsigned char *packet = psmouse->packet;
	unsigned int fingers, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	unsigned int width = 0, pres = 0;

	 
	fingers = (packet[0] & 0xc0) >> 6;

	switch (fingers) {
	case 3:
		 
		if (packet[3] & 0x80)
			fingers = 4;
		fallthrough;
	case 1:
		 
		x1 = ((packet[1] & 0x0f) << 8) | packet[2];
		 
		y1 = etd->y_max - (((packet[4] & 0x0f) << 8) | packet[5]);

		pres = (packet[1] & 0xf0) | ((packet[4] & 0xf0) >> 4);
		width = ((packet[0] & 0x30) >> 2) | ((packet[3] & 0x30) >> 4);
		break;

	case 2:
		 
		x1 = (((packet[0] & 0x10) << 4) | packet[1]) << 2;
		 
		y1 = etd->y_max -
			((((packet[0] & 0x20) << 3) | packet[2]) << 2);
		 
		x2 = (((packet[3] & 0x10) << 4) | packet[4]) << 2;
		 
		y2 = etd->y_max -
			((((packet[3] & 0x20) << 3) | packet[5]) << 2);

		 
		pres = 127;
		width = 7;
		break;
	}

	input_report_key(dev, BTN_TOUCH, fingers != 0);
	if (fingers != 0) {
		input_report_abs(dev, ABS_X, x1);
		input_report_abs(dev, ABS_Y, y1);
	}
	elantech_report_semi_mt_data(dev, fingers, x1, y1, x2, y2);
	input_report_key(dev, BTN_TOOL_FINGER, fingers == 1);
	input_report_key(dev, BTN_TOOL_DOUBLETAP, fingers == 2);
	input_report_key(dev, BTN_TOOL_TRIPLETAP, fingers == 3);
	input_report_key(dev, BTN_TOOL_QUADTAP, fingers == 4);
	psmouse_report_standard_buttons(dev, packet[0]);
	if (etd->info.reports_pressure) {
		input_report_abs(dev, ABS_PRESSURE, pres);
		input_report_abs(dev, ABS_TOOL_WIDTH, width);
	}

	input_sync(dev);
}

static void elantech_report_trackpoint(struct psmouse *psmouse,
				       int packet_type)
{
	 

	struct elantech_data *etd = psmouse->private;
	struct input_dev *tp_dev = etd->tp_dev;
	unsigned char *packet = psmouse->packet;
	int x, y;
	u32 t;

	t = get_unaligned_le32(&packet[0]);

	switch (t & ~7U) {
	case 0x06000030U:
	case 0x16008020U:
	case 0x26800010U:
	case 0x36808000U:

		 
		if (packet[4] == 0x80 || packet[5] == 0x80 ||
		    packet[1] >> 7 == packet[4] >> 7 ||
		    packet[2] >> 7 == packet[5] >> 7) {
			elantech_debug("discarding packet [%6ph]\n", packet);
			break;

		}
		x = packet[4] - (int)((packet[1]^0x80) << 1);
		y = (int)((packet[2]^0x80) << 1) - packet[5];

		psmouse_report_standard_buttons(tp_dev, packet[0]);

		input_report_rel(tp_dev, REL_X, x);
		input_report_rel(tp_dev, REL_Y, y);

		input_sync(tp_dev);

		break;

	default:
		 
		if (etd->info.debug == 1)
			elantech_packet_dump(psmouse);

		break;
	}
}

 
static void elantech_report_absolute_v3(struct psmouse *psmouse,
					int packet_type)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	unsigned int fingers = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	unsigned int width = 0, pres = 0;

	 
	fingers = (packet[0] & 0xc0) >> 6;

	switch (fingers) {
	case 3:
	case 1:
		 
		x1 = ((packet[1] & 0x0f) << 8) | packet[2];
		 
		y1 = etd->y_max - (((packet[4] & 0x0f) << 8) | packet[5]);
		break;

	case 2:
		if (packet_type == PACKET_V3_HEAD) {
			 
			etd->mt[0].x = ((packet[1] & 0x0f) << 8) | packet[2];
			 
			etd->mt[0].y = etd->y_max -
				(((packet[4] & 0x0f) << 8) | packet[5]);
			 
			return;
		}

		 
		x1 = etd->mt[0].x;
		y1 = etd->mt[0].y;
		x2 = ((packet[1] & 0x0f) << 8) | packet[2];
		y2 = etd->y_max - (((packet[4] & 0x0f) << 8) | packet[5]);
		break;
	}

	pres = (packet[1] & 0xf0) | ((packet[4] & 0xf0) >> 4);
	width = ((packet[0] & 0x30) >> 2) | ((packet[3] & 0x30) >> 4);

	input_report_key(dev, BTN_TOUCH, fingers != 0);
	if (fingers != 0) {
		input_report_abs(dev, ABS_X, x1);
		input_report_abs(dev, ABS_Y, y1);
	}
	elantech_report_semi_mt_data(dev, fingers, x1, y1, x2, y2);
	input_report_key(dev, BTN_TOOL_FINGER, fingers == 1);
	input_report_key(dev, BTN_TOOL_DOUBLETAP, fingers == 2);
	input_report_key(dev, BTN_TOOL_TRIPLETAP, fingers == 3);

	 
	if (elantech_is_buttonpad(&etd->info))
		input_report_key(dev, BTN_LEFT, packet[0] & 0x03);
	else
		psmouse_report_standard_buttons(dev, packet[0]);

	input_report_abs(dev, ABS_PRESSURE, pres);
	input_report_abs(dev, ABS_TOOL_WIDTH, width);

	input_sync(dev);
}

static void elantech_input_sync_v4(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;

	 
	if (elantech_is_buttonpad(&etd->info))
		input_report_key(dev, BTN_LEFT, packet[0] & 0x03);
	else
		psmouse_report_standard_buttons(dev, packet[0]);

	input_mt_report_pointer_emulation(dev, true);
	input_sync(dev);
}

static void process_packet_status_v4(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	unsigned char *packet = psmouse->packet;
	unsigned fingers;
	int i;

	 
	fingers = packet[1] & 0x1f;
	for (i = 0; i < ETP_MAX_FINGERS; i++) {
		if ((fingers & (1 << i)) == 0) {
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
		}
	}

	elantech_input_sync_v4(psmouse);
}

static void process_packet_head_v4(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	int id;
	int pres, traces;

	id = ((packet[3] & 0xe0) >> 5) - 1;
	if (id < 0 || id >= ETP_MAX_FINGERS)
		return;

	etd->mt[id].x = ((packet[1] & 0x0f) << 8) | packet[2];
	etd->mt[id].y = etd->y_max - (((packet[4] & 0x0f) << 8) | packet[5]);
	pres = (packet[1] & 0xf0) | ((packet[4] & 0xf0) >> 4);
	traces = (packet[0] & 0xf0) >> 4;

	input_mt_slot(dev, id);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);

	input_report_abs(dev, ABS_MT_POSITION_X, etd->mt[id].x);
	input_report_abs(dev, ABS_MT_POSITION_Y, etd->mt[id].y);
	input_report_abs(dev, ABS_MT_PRESSURE, pres);
	input_report_abs(dev, ABS_MT_TOUCH_MAJOR, traces * etd->width);
	 
	input_report_abs(dev, ABS_TOOL_WIDTH, traces);

	elantech_input_sync_v4(psmouse);
}

static void process_packet_motion_v4(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	int weight, delta_x1 = 0, delta_y1 = 0, delta_x2 = 0, delta_y2 = 0;
	int id, sid;

	id = ((packet[0] & 0xe0) >> 5) - 1;
	if (id < 0 || id >= ETP_MAX_FINGERS)
		return;

	sid = ((packet[3] & 0xe0) >> 5) - 1;
	weight = (packet[0] & 0x10) ? ETP_WEIGHT_VALUE : 1;
	 
	delta_x1 = (signed char)packet[1];
	delta_y1 = (signed char)packet[2];
	delta_x2 = (signed char)packet[4];
	delta_y2 = (signed char)packet[5];

	etd->mt[id].x += delta_x1 * weight;
	etd->mt[id].y -= delta_y1 * weight;
	input_mt_slot(dev, id);
	input_report_abs(dev, ABS_MT_POSITION_X, etd->mt[id].x);
	input_report_abs(dev, ABS_MT_POSITION_Y, etd->mt[id].y);

	if (sid >= 0 && sid < ETP_MAX_FINGERS) {
		etd->mt[sid].x += delta_x2 * weight;
		etd->mt[sid].y -= delta_y2 * weight;
		input_mt_slot(dev, sid);
		input_report_abs(dev, ABS_MT_POSITION_X, etd->mt[sid].x);
		input_report_abs(dev, ABS_MT_POSITION_Y, etd->mt[sid].y);
	}

	elantech_input_sync_v4(psmouse);
}

static void elantech_report_absolute_v4(struct psmouse *psmouse,
					int packet_type)
{
	switch (packet_type) {
	case PACKET_V4_STATUS:
		process_packet_status_v4(psmouse);
		break;

	case PACKET_V4_HEAD:
		process_packet_head_v4(psmouse);
		break;

	case PACKET_V4_MOTION:
		process_packet_motion_v4(psmouse);
		break;

	case PACKET_UNKNOWN:
	default:
		 
		break;
	}
}

static int elantech_packet_check_v1(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	unsigned char p1, p2, p3;

	 
	if (etd->info.fw_version < 0x020000) {
		 
		p1 = (packet[0] & 0x20) >> 5;
		p2 = (packet[0] & 0x10) >> 4;
	} else {
		 
		p1 = (packet[0] & 0x10) >> 4;
		p2 = (packet[0] & 0x20) >> 5;
	}

	p3 = (packet[0] & 0x04) >> 2;

	return etd->parity[packet[1]] == p1 &&
	       etd->parity[packet[2]] == p2 &&
	       etd->parity[packet[3]] == p3;
}

static int elantech_debounce_check_v2(struct psmouse *psmouse)
{
         
	static const u8 debounce_packet[] = {
		0x84, 0xff, 0xff, 0x02, 0xff, 0xff
	};
        unsigned char *packet = psmouse->packet;

        return !memcmp(packet, debounce_packet, sizeof(debounce_packet));
}

static int elantech_packet_check_v2(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;

	 
	if (etd->info.reports_pressure)
		return (packet[0] & 0x0c) == 0x04 &&
		       (packet[3] & 0x0f) == 0x02;

	if ((packet[0] & 0xc0) == 0x80)
		return (packet[0] & 0x0c) == 0x0c &&
		       (packet[3] & 0x0e) == 0x08;

	return (packet[0] & 0x3c) == 0x3c &&
	       (packet[1] & 0xf0) == 0x00 &&
	       (packet[3] & 0x3e) == 0x38 &&
	       (packet[4] & 0xf0) == 0x00;
}

 
static int elantech_packet_check_v3(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;
	static const u8 debounce_packet[] = {
		0xc4, 0xff, 0xff, 0x02, 0xff, 0xff
	};
	unsigned char *packet = psmouse->packet;

	 
	if (!memcmp(packet, debounce_packet, sizeof(debounce_packet)))
		return PACKET_DEBOUNCE;

	 
	if (etd->info.crc_enabled) {
		if ((packet[3] & 0x09) == 0x08)
			return PACKET_V3_HEAD;

		if ((packet[3] & 0x09) == 0x09)
			return PACKET_V3_TAIL;
	} else {
		if ((packet[0] & 0x0c) == 0x04 && (packet[3] & 0xcf) == 0x02)
			return PACKET_V3_HEAD;

		if ((packet[0] & 0x0c) == 0x0c && (packet[3] & 0xce) == 0x0c)
			return PACKET_V3_TAIL;
		if ((packet[3] & 0x0f) == 0x06)
			return PACKET_TRACKPOINT;
	}

	return PACKET_UNKNOWN;
}

static int elantech_packet_check_v4(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	unsigned char packet_type = packet[3] & 0x03;
	unsigned int ic_version;
	bool sanity_check;

	if (etd->tp_dev && (packet[3] & 0x0f) == 0x06)
		return PACKET_TRACKPOINT;

	 
	ic_version = (etd->info.fw_version & 0x0f0000) >> 16;

	 
	if (etd->info.crc_enabled)
		sanity_check = ((packet[3] & 0x08) == 0x00);
	else if (ic_version == 7 && etd->info.samples[1] == 0x2A)
		sanity_check = ((packet[3] & 0x1c) == 0x10);
	else
		sanity_check = ((packet[0] & 0x08) == 0x00 &&
				(packet[3] & 0x1c) == 0x10);

	if (!sanity_check)
		return PACKET_UNKNOWN;

	switch (packet_type) {
	case 0:
		return PACKET_V4_STATUS;

	case 1:
		return PACKET_V4_HEAD;

	case 2:
		return PACKET_V4_MOTION;
	}

	return PACKET_UNKNOWN;
}

 
static psmouse_ret_t elantech_process_byte(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;
	int packet_type;

	if (psmouse->pktcnt < psmouse->pktsize)
		return PSMOUSE_GOOD_DATA;

	if (etd->info.debug > 1)
		elantech_packet_dump(psmouse);

	switch (etd->info.hw_version) {
	case 1:
		if (etd->info.paritycheck && !elantech_packet_check_v1(psmouse))
			return PSMOUSE_BAD_DATA;

		elantech_report_absolute_v1(psmouse);
		break;

	case 2:
		 
		if (elantech_debounce_check_v2(psmouse))
			return PSMOUSE_FULL_PACKET;

		if (etd->info.paritycheck && !elantech_packet_check_v2(psmouse))
			return PSMOUSE_BAD_DATA;

		elantech_report_absolute_v2(psmouse);
		break;

	case 3:
		packet_type = elantech_packet_check_v3(psmouse);
		switch (packet_type) {
		case PACKET_UNKNOWN:
			return PSMOUSE_BAD_DATA;

		case PACKET_DEBOUNCE:
			 
			break;

		case PACKET_TRACKPOINT:
			elantech_report_trackpoint(psmouse, packet_type);
			break;

		default:
			elantech_report_absolute_v3(psmouse, packet_type);
			break;
		}

		break;

	case 4:
		packet_type = elantech_packet_check_v4(psmouse);
		switch (packet_type) {
		case PACKET_UNKNOWN:
			return PSMOUSE_BAD_DATA;

		case PACKET_TRACKPOINT:
			elantech_report_trackpoint(psmouse, packet_type);
			break;

		default:
			elantech_report_absolute_v4(psmouse, packet_type);
			break;
		}

		break;
	}

	return PSMOUSE_FULL_PACKET;
}

 
static void elantech_set_rate_restore_reg_07(struct psmouse *psmouse,
		unsigned int rate)
{
	struct elantech_data *etd = psmouse->private;

	etd->original_set_rate(psmouse, rate);
	if (elantech_write_reg(psmouse, 0x07, etd->reg_07))
		psmouse_err(psmouse, "restoring reg_07 failed\n");
}

 
static int elantech_set_absolute_mode(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;
	unsigned char val;
	int tries = ETP_READ_BACK_TRIES;
	int rc = 0;

	switch (etd->info.hw_version) {
	case 1:
		etd->reg_10 = 0x16;
		etd->reg_11 = 0x8f;
		if (elantech_write_reg(psmouse, 0x10, etd->reg_10) ||
		    elantech_write_reg(psmouse, 0x11, etd->reg_11)) {
			rc = -1;
		}
		break;

	case 2:
					 
		etd->reg_10 = 0x54;
		etd->reg_11 = 0x88;	 
		etd->reg_21 = 0x60;	 
		if (elantech_write_reg(psmouse, 0x10, etd->reg_10) ||
		    elantech_write_reg(psmouse, 0x11, etd->reg_11) ||
		    elantech_write_reg(psmouse, 0x21, etd->reg_21)) {
			rc = -1;
		}
		break;

	case 3:
		if (etd->info.set_hw_resolution)
			etd->reg_10 = 0x0b;
		else
			etd->reg_10 = 0x01;

		if (elantech_write_reg(psmouse, 0x10, etd->reg_10))
			rc = -1;

		break;

	case 4:
		etd->reg_07 = 0x01;
		if (elantech_write_reg(psmouse, 0x07, etd->reg_07))
			rc = -1;

		goto skip_readback_reg_10;  
	}

	if (rc == 0) {
		 
		do {
			rc = elantech_read_reg(psmouse, 0x10, &val);
			if (rc == 0)
				break;
			tries--;
			elantech_debug("retrying read (%d).\n", tries);
			msleep(ETP_READ_BACK_DELAY);
		} while (tries > 0);

		if (rc) {
			psmouse_err(psmouse,
				    "failed to read back register 0x10.\n");
		} else if (etd->info.hw_version == 1 &&
			   !(val & ETP_R10_ABSOLUTE_MODE)) {
			psmouse_err(psmouse,
				    "touchpad refuses to switch to absolute mode.\n");
			rc = -1;
		}
	}

 skip_readback_reg_10:
	if (rc)
		psmouse_err(psmouse, "failed to initialise registers.\n");

	return rc;
}

 
static unsigned int elantech_convert_res(unsigned int val)
{
	return (val * 10 + 790) * 10 / 254;
}

static int elantech_get_resolution_v4(struct psmouse *psmouse,
				      unsigned int *x_res,
				      unsigned int *y_res,
				      unsigned int *bus)
{
	unsigned char param[3];

	if (elantech_send_cmd(psmouse, ETP_RESOLUTION_QUERY, param))
		return -1;

	*x_res = elantech_convert_res(param[1] & 0x0f);
	*y_res = elantech_convert_res((param[1] & 0xf0) >> 4);
	*bus = param[2];

	return 0;
}

static void elantech_set_buttonpad_prop(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;

	if (elantech_is_buttonpad(&etd->info)) {
		__set_bit(INPUT_PROP_BUTTONPAD, dev->propbit);
		__clear_bit(BTN_RIGHT, dev->keybit);
	}
}

 
static const struct dmi_system_id elantech_dmi_has_middle_button[] = {
#if defined(CONFIG_DMI) && defined(CONFIG_X86)
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CELSIUS H730"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CELSIUS H760"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CELSIUS H780"),
		},
	},
#endif
	{ }
};

 
static int elantech_set_input_params(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	struct elantech_device_info *info = &etd->info;
	unsigned int x_min = info->x_min, y_min = info->y_min,
		     x_max = info->x_max, y_max = info->y_max,
		     width = info->width;

	__set_bit(INPUT_PROP_POINTER, dev->propbit);
	__set_bit(EV_KEY, dev->evbit);
	__set_bit(EV_ABS, dev->evbit);
	__clear_bit(EV_REL, dev->evbit);

	__set_bit(BTN_LEFT, dev->keybit);
	if (info->has_middle_button)
		__set_bit(BTN_MIDDLE, dev->keybit);
	__set_bit(BTN_RIGHT, dev->keybit);

	__set_bit(BTN_TOUCH, dev->keybit);
	__set_bit(BTN_TOOL_FINGER, dev->keybit);
	__set_bit(BTN_TOOL_DOUBLETAP, dev->keybit);
	__set_bit(BTN_TOOL_TRIPLETAP, dev->keybit);

	switch (info->hw_version) {
	case 1:
		 
		if (info->fw_version < 0x020000 &&
		    (info->capabilities[0] & ETP_CAP_HAS_ROCKER)) {
			__set_bit(BTN_FORWARD, dev->keybit);
			__set_bit(BTN_BACK, dev->keybit);
		}
		input_set_abs_params(dev, ABS_X, x_min, x_max, 0, 0);
		input_set_abs_params(dev, ABS_Y, y_min, y_max, 0, 0);
		break;

	case 2:
		__set_bit(BTN_TOOL_QUADTAP, dev->keybit);
		__set_bit(INPUT_PROP_SEMI_MT, dev->propbit);
		fallthrough;
	case 3:
		if (info->hw_version == 3)
			elantech_set_buttonpad_prop(psmouse);
		input_set_abs_params(dev, ABS_X, x_min, x_max, 0, 0);
		input_set_abs_params(dev, ABS_Y, y_min, y_max, 0, 0);
		if (info->reports_pressure) {
			input_set_abs_params(dev, ABS_PRESSURE, ETP_PMIN_V2,
					     ETP_PMAX_V2, 0, 0);
			input_set_abs_params(dev, ABS_TOOL_WIDTH, ETP_WMIN_V2,
					     ETP_WMAX_V2, 0, 0);
		}
		input_mt_init_slots(dev, 2, INPUT_MT_SEMI_MT);
		input_set_abs_params(dev, ABS_MT_POSITION_X, x_min, x_max, 0, 0);
		input_set_abs_params(dev, ABS_MT_POSITION_Y, y_min, y_max, 0, 0);
		break;

	case 4:
		elantech_set_buttonpad_prop(psmouse);
		__set_bit(BTN_TOOL_QUADTAP, dev->keybit);
		 
		input_set_abs_params(dev, ABS_X, x_min, x_max, 0, 0);
		input_set_abs_params(dev, ABS_Y, y_min, y_max, 0, 0);
		 
		input_set_abs_params(dev, ABS_PRESSURE, ETP_PMIN_V2,
				     ETP_PMAX_V2, 0, 0);
		input_set_abs_params(dev, ABS_TOOL_WIDTH, ETP_WMIN_V2,
				     ETP_WMAX_V2, 0, 0);
		 
		input_mt_init_slots(dev, ETP_MAX_FINGERS, 0);
		input_set_abs_params(dev, ABS_MT_POSITION_X, x_min, x_max, 0, 0);
		input_set_abs_params(dev, ABS_MT_POSITION_Y, y_min, y_max, 0, 0);
		input_set_abs_params(dev, ABS_MT_PRESSURE, ETP_PMIN_V2,
				     ETP_PMAX_V2, 0, 0);
		 
		input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR, 0,
				     ETP_WMAX_V2 * width, 0, 0);
		break;
	}

	input_abs_set_res(dev, ABS_X, info->x_res);
	input_abs_set_res(dev, ABS_Y, info->y_res);
	if (info->hw_version > 1) {
		input_abs_set_res(dev, ABS_MT_POSITION_X, info->x_res);
		input_abs_set_res(dev, ABS_MT_POSITION_Y, info->y_res);
	}

	etd->y_max = y_max;
	etd->width = width;

	return 0;
}

struct elantech_attr_data {
	size_t		field_offset;
	unsigned char	reg;
};

 
static ssize_t elantech_show_int_attr(struct psmouse *psmouse, void *data,
					char *buf)
{
	struct elantech_data *etd = psmouse->private;
	struct elantech_attr_data *attr = data;
	unsigned char *reg = (unsigned char *) etd + attr->field_offset;
	int rc = 0;

	if (attr->reg)
		rc = elantech_read_reg(psmouse, attr->reg, reg);

	return sprintf(buf, "0x%02x\n", (attr->reg && rc) ? -1 : *reg);
}

 
static ssize_t elantech_set_int_attr(struct psmouse *psmouse,
				     void *data, const char *buf, size_t count)
{
	struct elantech_data *etd = psmouse->private;
	struct elantech_attr_data *attr = data;
	unsigned char *reg = (unsigned char *) etd + attr->field_offset;
	unsigned char value;
	int err;

	err = kstrtou8(buf, 16, &value);
	if (err)
		return err;

	 
	if (etd->info.hw_version == 1) {
		if (attr->reg == 0x10)
			 
			value |= ETP_R10_ABSOLUTE_MODE;
		else if (attr->reg == 0x11)
			 
			value |= ETP_R11_4_BYTE_MODE;
	}

	if (!attr->reg || elantech_write_reg(psmouse, attr->reg, value) == 0)
		*reg = value;

	return count;
}

#define ELANTECH_INT_ATTR(_name, _register)				\
	static struct elantech_attr_data elantech_attr_##_name = {	\
		.field_offset = offsetof(struct elantech_data, _name),	\
		.reg = _register,					\
	};								\
	PSMOUSE_DEFINE_ATTR(_name, 0644,				\
			    &elantech_attr_##_name,			\
			    elantech_show_int_attr,			\
			    elantech_set_int_attr)

#define ELANTECH_INFO_ATTR(_name)					       \
	static struct elantech_attr_data elantech_attr_##_name = {	       \
		.field_offset = offsetof(struct elantech_data, info) +	       \
				offsetof(struct elantech_device_info, _name),  \
		.reg = 0,						       \
	};								       \
	PSMOUSE_DEFINE_ATTR(_name, 0644,				       \
			    &elantech_attr_##_name,			       \
			    elantech_show_int_attr,			       \
			    elantech_set_int_attr)

ELANTECH_INT_ATTR(reg_07, 0x07);
ELANTECH_INT_ATTR(reg_10, 0x10);
ELANTECH_INT_ATTR(reg_11, 0x11);
ELANTECH_INT_ATTR(reg_20, 0x20);
ELANTECH_INT_ATTR(reg_21, 0x21);
ELANTECH_INT_ATTR(reg_22, 0x22);
ELANTECH_INT_ATTR(reg_23, 0x23);
ELANTECH_INT_ATTR(reg_24, 0x24);
ELANTECH_INT_ATTR(reg_25, 0x25);
ELANTECH_INT_ATTR(reg_26, 0x26);
ELANTECH_INFO_ATTR(debug);
ELANTECH_INFO_ATTR(paritycheck);
ELANTECH_INFO_ATTR(crc_enabled);

static struct attribute *elantech_attrs[] = {
	&psmouse_attr_reg_07.dattr.attr,
	&psmouse_attr_reg_10.dattr.attr,
	&psmouse_attr_reg_11.dattr.attr,
	&psmouse_attr_reg_20.dattr.attr,
	&psmouse_attr_reg_21.dattr.attr,
	&psmouse_attr_reg_22.dattr.attr,
	&psmouse_attr_reg_23.dattr.attr,
	&psmouse_attr_reg_24.dattr.attr,
	&psmouse_attr_reg_25.dattr.attr,
	&psmouse_attr_reg_26.dattr.attr,
	&psmouse_attr_debug.dattr.attr,
	&psmouse_attr_paritycheck.dattr.attr,
	&psmouse_attr_crc_enabled.dattr.attr,
	NULL
};

static const struct attribute_group elantech_attr_group = {
	.attrs = elantech_attrs,
};

static bool elantech_is_signature_valid(const unsigned char *param)
{
	static const unsigned char rates[] = { 200, 100, 80, 60, 40, 20, 10 };
	int i;

	if (param[0] == 0)
		return false;

	if (param[1] == 0)
		return true;

	 
	if ((param[0] & 0x0f) >= 0x06 && (param[1] & 0xaf) == 0x0f &&
	    param[2] < 40)
		return true;

	for (i = 0; i < ARRAY_SIZE(rates); i++)
		if (param[2] == rates[i])
			return false;

	return true;
}

 
int elantech_detect(struct psmouse *psmouse, bool set_properties)
{
	struct ps2dev *ps2dev = &psmouse->ps2dev;
	unsigned char param[3];

	ps2_command(ps2dev, NULL, PSMOUSE_CMD_RESET_DIS);

	if (ps2_command(ps2dev,  NULL, PSMOUSE_CMD_DISABLE) ||
	    ps2_command(ps2dev,  NULL, PSMOUSE_CMD_SETSCALE11) ||
	    ps2_command(ps2dev,  NULL, PSMOUSE_CMD_SETSCALE11) ||
	    ps2_command(ps2dev,  NULL, PSMOUSE_CMD_SETSCALE11) ||
	    ps2_command(ps2dev, param, PSMOUSE_CMD_GETINFO)) {
		psmouse_dbg(psmouse, "sending Elantech magic knock failed.\n");
		return -1;
	}

	 
	if (param[0] != 0x3c || param[1] != 0x03 ||
	    (param[2] != 0xc8 && param[2] != 0x00)) {
		psmouse_dbg(psmouse,
			    "unexpected magic knock result 0x%02x, 0x%02x, 0x%02x.\n",
			    param[0], param[1], param[2]);
		return -1;
	}

	 
	if (synaptics_send_cmd(psmouse, ETP_FW_VERSION_QUERY, param)) {
		psmouse_dbg(psmouse, "failed to query firmware version.\n");
		return -1;
	}

	psmouse_dbg(psmouse,
		    "Elantech version query result 0x%02x, 0x%02x, 0x%02x.\n",
		    param[0], param[1], param[2]);

	if (!elantech_is_signature_valid(param)) {
		psmouse_dbg(psmouse,
			    "Probably not a real Elantech touchpad. Aborting.\n");
		return -1;
	}

	if (set_properties) {
		psmouse->vendor = "Elantech";
		psmouse->name = "Touchpad";
	}

	return 0;
}

 
static void elantech_disconnect(struct psmouse *psmouse)
{
	struct elantech_data *etd = psmouse->private;

	 
	psmouse_smbus_cleanup(psmouse);

	if (etd->tp_dev)
		input_unregister_device(etd->tp_dev);
	sysfs_remove_group(&psmouse->ps2dev.serio->dev.kobj,
			   &elantech_attr_group);
	kfree(psmouse->private);
	psmouse->private = NULL;
}

 
static int elantech_reconnect(struct psmouse *psmouse)
{
	psmouse_reset(psmouse);

	if (elantech_detect(psmouse, 0))
		return -1;

	if (elantech_set_absolute_mode(psmouse)) {
		psmouse_err(psmouse,
			    "failed to put touchpad back into absolute mode.\n");
		return -1;
	}

	return 0;
}

 
static const struct dmi_system_id elantech_dmi_force_crc_enabled[] = {
#if defined(CONFIG_DMI) && defined(CONFIG_X86)
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CELSIUS H730"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CELSIUS H760"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E544"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E546"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E547"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E554"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E556"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E557"),
		},
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK U745"),
		},
	},
#endif
	{ }
};

 
static const struct dmi_system_id no_hw_res_dmi_table[] = {
#if defined(CONFIG_DMI) && defined(CONFIG_X86)
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "U2442"),
		},
	},
#endif
	{ }
};

 
static int elantech_change_report_id(struct psmouse *psmouse)
{
	 
	unsigned char param[3] = { 0x10, 0x03 };

	if (elantech_write_reg_params(psmouse, 0x7, param) ||
	    elantech_read_reg_params(psmouse, 0x7, param) ||
	    param[0] != 0x10 || param[1] != 0x03) {
		psmouse_err(psmouse, "Unable to change report ID to 0x5f.\n");
		return -EIO;
	}

	return 0;
}
 
static int elantech_set_properties(struct elantech_device_info *info)
{
	 
	info->ic_version = (info->fw_version & 0x0f0000) >> 16;

	 
	if (info->fw_version < 0x020030 || info->fw_version == 0x020600)
		info->hw_version = 1;
	else {
		switch (info->ic_version) {
		case 2:
		case 4:
			info->hw_version = 2;
			break;
		case 5:
			info->hw_version = 3;
			break;
		case 6 ... 15:
			info->hw_version = 4;
			break;
		default:
			return -1;
		}
	}

	 
	info->pattern = 0x00;
	if (info->ic_version == 0x0f && (info->fw_version & 0xff) <= 0x02)
		info->pattern = info->fw_version & 0xff;

	 
	info->send_cmd = info->hw_version >= 3 ? elantech_send_cmd :
						 synaptics_send_cmd;

	 
	info->paritycheck = 1;

	 
	info->jumpy_cursor =
		(info->fw_version == 0x020022 || info->fw_version == 0x020600);

	if (info->hw_version > 1) {
		 
		info->debug = 1;

		if (info->fw_version >= 0x020800)
			info->reports_pressure = true;
	}

	 
	info->crc_enabled = (info->fw_version & 0x4000) == 0x4000 ||
			     dmi_check_system(elantech_dmi_force_crc_enabled);

	 
	info->set_hw_resolution = !dmi_check_system(no_hw_res_dmi_table);

	return 0;
}

static int elantech_query_info(struct psmouse *psmouse,
			       struct elantech_device_info *info)
{
	unsigned char param[3];
	unsigned char traces;
	unsigned char ic_body[3];

	memset(info, 0, sizeof(*info));

	 
	if (synaptics_send_cmd(psmouse, ETP_FW_VERSION_QUERY, param)) {
		psmouse_err(psmouse, "failed to query firmware version.\n");
		return -EINVAL;
	}
	info->fw_version = (param[0] << 16) | (param[1] << 8) | param[2];

	if (elantech_set_properties(info)) {
		psmouse_err(psmouse, "unknown hardware version, aborting...\n");
		return -EINVAL;
	}
	psmouse_info(psmouse,
		     "assuming hardware version %d (with firmware version 0x%02x%02x%02x)\n",
		     info->hw_version, param[0], param[1], param[2]);

	if (info->send_cmd(psmouse, ETP_CAPABILITIES_QUERY,
	    info->capabilities)) {
		psmouse_err(psmouse, "failed to query capabilities.\n");
		return -EINVAL;
	}
	psmouse_info(psmouse,
		     "Synaptics capabilities query result 0x%02x, 0x%02x, 0x%02x.\n",
		     info->capabilities[0], info->capabilities[1],
		     info->capabilities[2]);

	if (info->hw_version != 1) {
		if (info->send_cmd(psmouse, ETP_SAMPLE_QUERY, info->samples)) {
			psmouse_err(psmouse, "failed to query sample data\n");
			return -EINVAL;
		}
		psmouse_info(psmouse,
			     "Elan sample query result %02x, %02x, %02x\n",
			     info->samples[0],
			     info->samples[1],
			     info->samples[2]);
	}

	if (info->pattern > 0x00 && info->ic_version == 0xf) {
		if (info->send_cmd(psmouse, ETP_ICBODY_QUERY, ic_body)) {
			psmouse_err(psmouse, "failed to query ic body\n");
			return -EINVAL;
		}
		info->ic_version = be16_to_cpup((__be16 *)ic_body);
		psmouse_info(psmouse,
			     "Elan ic body: %#04x, current fw version: %#02x\n",
			     info->ic_version, ic_body[2]);
	}

	info->product_id = be16_to_cpup((__be16 *)info->samples);
	if (info->pattern == 0x00)
		info->product_id &= 0xff;

	if (info->samples[1] == 0x74 && info->hw_version == 0x03) {
		 
		psmouse_info(psmouse,
			     "absolute mode broken, forcing standard PS/2 protocol\n");
		return -ENODEV;
	}

	 
	info->has_trackpoint = (info->capabilities[0] & 0x80) == 0x80;

	if (info->has_trackpoint && info->ic_version == 0x0011 &&
	    (info->product_id == 0x08 || info->product_id == 0x09 ||
	     info->product_id == 0x0d || info->product_id == 0x0e)) {
		 
		if (elantech_change_report_id(psmouse)) {
			psmouse_info(psmouse,
				     "Trackpoint report is broken, forcing standard PS/2 protocol\n");
			return -ENODEV;
		}
	}

	info->x_res = 31;
	info->y_res = 31;
	if (info->hw_version == 4) {
		if (elantech_get_resolution_v4(psmouse,
					       &info->x_res,
					       &info->y_res,
					       &info->bus)) {
			psmouse_warn(psmouse,
				     "failed to query resolution data.\n");
		}
	}

	 
	switch (info->hw_version) {
	case 1:
		info->x_min = ETP_XMIN_V1;
		info->y_min = ETP_YMIN_V1;
		info->x_max = ETP_XMAX_V1;
		info->y_max = ETP_YMAX_V1;
		break;

	case 2:
		if (info->fw_version == 0x020800 ||
		    info->fw_version == 0x020b00 ||
		    info->fw_version == 0x020030) {
			info->x_min = ETP_XMIN_V2;
			info->y_min = ETP_YMIN_V2;
			info->x_max = ETP_XMAX_V2;
			info->y_max = ETP_YMAX_V2;
		} else {
			int i;
			int fixed_dpi;

			i = (info->fw_version > 0x020800 &&
			     info->fw_version < 0x020900) ? 1 : 2;

			if (info->send_cmd(psmouse, ETP_FW_ID_QUERY, param))
				return -EINVAL;

			fixed_dpi = param[1] & 0x10;

			if (((info->fw_version >> 16) == 0x14) && fixed_dpi) {
				if (info->send_cmd(psmouse, ETP_SAMPLE_QUERY, param))
					return -EINVAL;

				info->x_max = (info->capabilities[1] - i) * param[1] / 2;
				info->y_max = (info->capabilities[2] - i) * param[2] / 2;
			} else if (info->fw_version == 0x040216) {
				info->x_max = 819;
				info->y_max = 405;
			} else if (info->fw_version == 0x040219 || info->fw_version == 0x040215) {
				info->x_max = 900;
				info->y_max = 500;
			} else {
				info->x_max = (info->capabilities[1] - i) * 64;
				info->y_max = (info->capabilities[2] - i) * 64;
			}
		}
		break;

	case 3:
		if (info->send_cmd(psmouse, ETP_FW_ID_QUERY, param))
			return -EINVAL;

		info->x_max = (0x0f & param[0]) << 8 | param[1];
		info->y_max = (0xf0 & param[0]) << 4 | param[2];
		break;

	case 4:
		if (info->send_cmd(psmouse, ETP_FW_ID_QUERY, param))
			return -EINVAL;

		info->x_max = (0x0f & param[0]) << 8 | param[1];
		info->y_max = (0xf0 & param[0]) << 4 | param[2];
		traces = info->capabilities[1];
		if ((traces < 2) || (traces > info->x_max))
			return -EINVAL;

		info->width = info->x_max / (traces - 1);

		 
		info->x_traces = traces;

		 
		traces = info->capabilities[2];
		if ((traces >= 2) && (traces <= info->y_max))
			info->y_traces = traces;

		break;
	}

	 
	info->has_middle_button = dmi_check_system(elantech_dmi_has_middle_button) ||
				  (ETP_NEW_IC_SMBUS_HOST_NOTIFY(info->fw_version) &&
				   !elantech_is_buttonpad(info));

	return 0;
}

#if defined(CONFIG_MOUSE_PS2_ELANTECH_SMBUS)

 
enum {
	ELANTECH_SMBUS_NOT_SET = -1,
	ELANTECH_SMBUS_OFF,
	ELANTECH_SMBUS_ON,
};

static int elantech_smbus = IS_ENABLED(CONFIG_MOUSE_ELAN_I2C_SMBUS) ?
		ELANTECH_SMBUS_NOT_SET : ELANTECH_SMBUS_OFF;
module_param_named(elantech_smbus, elantech_smbus, int, 0644);
MODULE_PARM_DESC(elantech_smbus, "Use a secondary bus for the Elantech device.");

static const char * const i2c_blacklist_pnp_ids[] = {
	 
	NULL
};

static int elantech_create_smbus(struct psmouse *psmouse,
				 struct elantech_device_info *info,
				 bool leave_breadcrumbs)
{
	struct property_entry i2c_props[11] = {};
	struct i2c_board_info smbus_board = {
		I2C_BOARD_INFO("elan_i2c", 0x15),
		.flags = I2C_CLIENT_HOST_NOTIFY,
	};
	unsigned int idx = 0;

	i2c_props[idx++] = PROPERTY_ENTRY_U32("touchscreen-size-x",
						   info->x_max + 1);
	i2c_props[idx++] = PROPERTY_ENTRY_U32("touchscreen-size-y",
						   info->y_max + 1);
	i2c_props[idx++] = PROPERTY_ENTRY_U32("touchscreen-min-x",
						   info->x_min);
	i2c_props[idx++] = PROPERTY_ENTRY_U32("touchscreen-min-y",
						   info->y_min);
	if (info->x_res)
		i2c_props[idx++] = PROPERTY_ENTRY_U32("touchscreen-x-mm",
						      (info->x_max + 1) / info->x_res);
	if (info->y_res)
		i2c_props[idx++] = PROPERTY_ENTRY_U32("touchscreen-y-mm",
						      (info->y_max + 1) / info->y_res);

	if (info->has_trackpoint)
		i2c_props[idx++] = PROPERTY_ENTRY_BOOL("elan,trackpoint");

	if (info->has_middle_button)
		i2c_props[idx++] = PROPERTY_ENTRY_BOOL("elan,middle-button");

	if (info->x_traces)
		i2c_props[idx++] = PROPERTY_ENTRY_U32("elan,x_traces",
						      info->x_traces);
	if (info->y_traces)
		i2c_props[idx++] = PROPERTY_ENTRY_U32("elan,y_traces",
						      info->y_traces);

	if (elantech_is_buttonpad(info))
		i2c_props[idx++] = PROPERTY_ENTRY_BOOL("elan,clickpad");

	smbus_board.fwnode = fwnode_create_software_node(i2c_props, NULL);
	if (IS_ERR(smbus_board.fwnode))
		return PTR_ERR(smbus_board.fwnode);

	return psmouse_smbus_init(psmouse, &smbus_board, NULL, 0, false,
				  leave_breadcrumbs);
}

 
static int elantech_setup_smbus(struct psmouse *psmouse,
				struct elantech_device_info *info,
				bool leave_breadcrumbs)
{
	int error;

	if (elantech_smbus == ELANTECH_SMBUS_OFF)
		return -ENXIO;

	if (elantech_smbus == ELANTECH_SMBUS_NOT_SET) {
		 
		if (!ETP_NEW_IC_SMBUS_HOST_NOTIFY(info->fw_version) ||
		    psmouse_matches_pnp_id(psmouse, i2c_blacklist_pnp_ids))
			return -ENXIO;
	}

	psmouse_info(psmouse, "Trying to set up SMBus access\n");

	error = elantech_create_smbus(psmouse, info, leave_breadcrumbs);
	if (error) {
		if (error == -EAGAIN)
			psmouse_info(psmouse, "SMbus companion is not ready yet\n");
		else
			psmouse_err(psmouse, "unable to create intertouch device\n");

		return error;
	}

	return 0;
}

static bool elantech_use_host_notify(struct psmouse *psmouse,
				     struct elantech_device_info *info)
{
	if (ETP_NEW_IC_SMBUS_HOST_NOTIFY(info->fw_version))
		return true;

	switch (info->bus) {
	case ETP_BUS_PS2_ONLY:
		 
		break;
	case ETP_BUS_SMB_ALERT_ONLY:
	case ETP_BUS_PS2_SMB_ALERT:
		psmouse_dbg(psmouse, "Ignoring SMBus provider through alert protocol.\n");
		break;
	case ETP_BUS_SMB_HST_NTFY_ONLY:
	case ETP_BUS_PS2_SMB_HST_NTFY:
		return true;
	default:
		psmouse_dbg(psmouse,
			    "Ignoring SMBus bus provider %d.\n",
			    info->bus);
	}

	return false;
}

int elantech_init_smbus(struct psmouse *psmouse)
{
	struct elantech_device_info info;
	int error;

	psmouse_reset(psmouse);

	error = elantech_query_info(psmouse, &info);
	if (error)
		goto init_fail;

	if (info.hw_version < 4) {
		error = -ENXIO;
		goto init_fail;
	}

	return elantech_create_smbus(psmouse, &info, false);
 init_fail:
	psmouse_reset(psmouse);
	return error;
}
#endif  

 
static int elantech_setup_ps2(struct psmouse *psmouse,
			      struct elantech_device_info *info)
{
	struct elantech_data *etd;
	int i;
	int error = -EINVAL;
	struct input_dev *tp_dev;

	psmouse->private = etd = kzalloc(sizeof(*etd), GFP_KERNEL);
	if (!etd)
		return -ENOMEM;

	etd->info = *info;

	etd->parity[0] = 1;
	for (i = 1; i < 256; i++)
		etd->parity[i] = etd->parity[i & (i - 1)] ^ 1;

	if (elantech_set_absolute_mode(psmouse)) {
		psmouse_err(psmouse,
			    "failed to put touchpad into absolute mode.\n");
		goto init_fail;
	}

	if (info->fw_version == 0x381f17) {
		etd->original_set_rate = psmouse->set_rate;
		psmouse->set_rate = elantech_set_rate_restore_reg_07;
	}

	if (elantech_set_input_params(psmouse)) {
		psmouse_err(psmouse, "failed to query touchpad range.\n");
		goto init_fail;
	}

	error = sysfs_create_group(&psmouse->ps2dev.serio->dev.kobj,
				   &elantech_attr_group);
	if (error) {
		psmouse_err(psmouse,
			    "failed to create sysfs attributes, error: %d.\n",
			    error);
		goto init_fail;
	}

	if (info->has_trackpoint) {
		tp_dev = input_allocate_device();

		if (!tp_dev) {
			error = -ENOMEM;
			goto init_fail_tp_alloc;
		}

		etd->tp_dev = tp_dev;
		snprintf(etd->tp_phys, sizeof(etd->tp_phys), "%s/input1",
			psmouse->ps2dev.serio->phys);
		tp_dev->phys = etd->tp_phys;
		tp_dev->name = "ETPS/2 Elantech TrackPoint";
		tp_dev->id.bustype = BUS_I8042;
		tp_dev->id.vendor  = 0x0002;
		tp_dev->id.product = PSMOUSE_ELANTECH;
		tp_dev->id.version = 0x0000;
		tp_dev->dev.parent = &psmouse->ps2dev.serio->dev;
		tp_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
		tp_dev->relbit[BIT_WORD(REL_X)] =
			BIT_MASK(REL_X) | BIT_MASK(REL_Y);
		tp_dev->keybit[BIT_WORD(BTN_LEFT)] =
			BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_MIDDLE) |
			BIT_MASK(BTN_RIGHT);

		__set_bit(INPUT_PROP_POINTER, tp_dev->propbit);
		__set_bit(INPUT_PROP_POINTING_STICK, tp_dev->propbit);

		error = input_register_device(etd->tp_dev);
		if (error < 0)
			goto init_fail_tp_reg;
	}

	psmouse->protocol_handler = elantech_process_byte;
	psmouse->disconnect = elantech_disconnect;
	psmouse->reconnect = elantech_reconnect;
	psmouse->fast_reconnect = NULL;
	psmouse->pktsize = info->hw_version > 1 ? 6 : 4;

	return 0;
 init_fail_tp_reg:
	input_free_device(tp_dev);
 init_fail_tp_alloc:
	sysfs_remove_group(&psmouse->ps2dev.serio->dev.kobj,
			   &elantech_attr_group);
 init_fail:
	kfree(etd);
	return error;
}

int elantech_init_ps2(struct psmouse *psmouse)
{
	struct elantech_device_info info;
	int error;

	psmouse_reset(psmouse);

	error = elantech_query_info(psmouse, &info);
	if (error)
		goto init_fail;

	error = elantech_setup_ps2(psmouse, &info);
	if (error)
		goto init_fail;

	return 0;
 init_fail:
	psmouse_reset(psmouse);
	return error;
}

int elantech_init(struct psmouse *psmouse)
{
	struct elantech_device_info info;
	int error;

	psmouse_reset(psmouse);

	error = elantech_query_info(psmouse, &info);
	if (error)
		goto init_fail;

#if defined(CONFIG_MOUSE_PS2_ELANTECH_SMBUS)

	if (elantech_use_host_notify(psmouse, &info)) {
		if (!IS_ENABLED(CONFIG_MOUSE_ELAN_I2C_SMBUS) ||
		    !IS_ENABLED(CONFIG_MOUSE_PS2_ELANTECH_SMBUS)) {
			psmouse_warn(psmouse,
				     "The touchpad can support a better bus than the too old PS/2 protocol. "
				     "Make sure MOUSE_PS2_ELANTECH_SMBUS and MOUSE_ELAN_I2C_SMBUS are enabled to get a better touchpad experience.\n");
		}
		error = elantech_setup_smbus(psmouse, &info, true);
		if (!error)
			return PSMOUSE_ELANTECH_SMBUS;
	}

#endif  

	error = elantech_setup_ps2(psmouse, &info);
	if (error < 0) {
		 
		psmouse_smbus_cleanup(psmouse);
		goto init_fail;
	}

	return PSMOUSE_ELANTECH;
 init_fail:
	psmouse_reset(psmouse);
	return error;
}
