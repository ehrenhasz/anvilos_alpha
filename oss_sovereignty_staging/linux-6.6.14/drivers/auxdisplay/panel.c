
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/parport.h>
#include <linux/list.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#include "charlcd.h"
#include "hd44780_common.h"

#define LCD_MAXBYTES		256	 

#define KEYPAD_BUFFER		64

 
#define INPUT_POLL_TIME		(HZ / 50)
 
#define KEYPAD_REP_START	(10)
 
#define KEYPAD_REP_DELAY	(2)

 
#define PNL_PINPUT(a)		((((unsigned char)(a)) ^ 0x7F) >> 3)

#define PNL_PBUSY		0x80	 
#define PNL_PACK		0x40	 
#define PNL_POUTPA		0x20	 
#define PNL_PSELECD		0x10	 
#define PNL_PERRORP		0x08	 

#define PNL_PBIDIR		0x20	 
 
#define PNL_PINTEN		0x10
#define PNL_PSELECP		0x08	 
#define PNL_PINITP		0x04	 
#define PNL_PAUTOLF		0x02	 
#define PNL_PSTROBE		0x01	 

#define PNL_PD0			0x01
#define PNL_PD1			0x02
#define PNL_PD2			0x04
#define PNL_PD3			0x08
#define PNL_PD4			0x10
#define PNL_PD5			0x20
#define PNL_PD6			0x40
#define PNL_PD7			0x80

#define PIN_NONE		0
#define PIN_STROBE		1
#define PIN_D0			2
#define PIN_D1			3
#define PIN_D2			4
#define PIN_D3			5
#define PIN_D4			6
#define PIN_D5			7
#define PIN_D6			8
#define PIN_D7			9
#define PIN_AUTOLF		14
#define PIN_INITP		16
#define PIN_SELECP		17
#define PIN_NOT_SET		127

#define NOT_SET			-1

 
#define r_ctr(x)        (parport_read_control((x)->port))
#define r_dtr(x)        (parport_read_data((x)->port))
#define r_str(x)        (parport_read_status((x)->port))
#define w_ctr(x, y)     (parport_write_control((x)->port, (y)))
#define w_dtr(x, y)     (parport_write_data((x)->port, (y)))

 
 
static __u8 scan_mask_o;
 
static __u8 scan_mask_i;

enum input_type {
	INPUT_TYPE_STD,
	INPUT_TYPE_KBD,
};

enum input_state {
	INPUT_ST_LOW,
	INPUT_ST_RISING,
	INPUT_ST_HIGH,
	INPUT_ST_FALLING,
};

struct logical_input {
	struct list_head list;
	__u64 mask;
	__u64 value;
	enum input_type type;
	enum input_state state;
	__u8 rise_time, fall_time;
	__u8 rise_timer, fall_timer, high_timer;

	union {
		struct {	 
			void (*press_fct)(int);
			void (*release_fct)(int);
			int press_data;
			int release_data;
		} std;
		struct {	 
			char press_str[sizeof(void *) + sizeof(int)] __nonstring;
			char repeat_str[sizeof(void *) + sizeof(int)] __nonstring;
			char release_str[sizeof(void *) + sizeof(int)] __nonstring;
		} kbd;
	} u;
};

static LIST_HEAD(logical_inputs);	 

 

 
static __u64 phys_read;
 
static __u64 phys_read_prev;
 
static __u64 phys_curr;
 
static __u64 phys_prev;
 
static char inputs_stable;

 
static struct {
	bool enabled;
} keypad;

static char keypad_buffer[KEYPAD_BUFFER];
static int keypad_buflen;
static int keypad_start;
static char keypressed;
static wait_queue_head_t keypad_read_wait;

 
static struct {
	bool enabled;
	bool initialized;

	int charset;
	int proto;

	 
	struct {
		int e;
		int rs;
		int rw;
		int cl;
		int da;
		int bl;
	} pins;

	struct charlcd *charlcd;
} lcd;

 
static int selected_lcd_type = NOT_SET;

 
#define BIT_CLR		0
#define BIT_SET		1
#define BIT_MSK		2
#define BIT_STATES	3
 
#define LCD_BIT_E	0
#define LCD_BIT_RS	1
#define LCD_BIT_RW	2
#define LCD_BIT_BL	3
#define LCD_BIT_CL	4
#define LCD_BIT_DA	5
#define LCD_BITS	6

 
#define LCD_PORT_C	0
#define LCD_PORT_D	1
#define LCD_PORTS	2

static unsigned char lcd_bits[LCD_PORTS][LCD_BITS][BIT_STATES];

 
#define LCD_PROTO_PARALLEL      0
#define LCD_PROTO_SERIAL        1
#define LCD_PROTO_TI_DA8XX_LCD	2

 
#define LCD_CHARSET_NORMAL      0
#define LCD_CHARSET_KS0074      1

 
#define LCD_TYPE_NONE		0
#define LCD_TYPE_CUSTOM		1
#define LCD_TYPE_OLD		2
#define LCD_TYPE_KS0074		3
#define LCD_TYPE_HANTRONIX	4
#define LCD_TYPE_NEXCOM		5

 
#define KEYPAD_TYPE_NONE	0
#define KEYPAD_TYPE_OLD		1
#define KEYPAD_TYPE_NEW		2
#define KEYPAD_TYPE_NEXCOM	3

 
#define PANEL_PROFILE_CUSTOM	0
#define PANEL_PROFILE_OLD	1
#define PANEL_PROFILE_NEW	2
#define PANEL_PROFILE_HANTRONIX	3
#define PANEL_PROFILE_NEXCOM	4
#define PANEL_PROFILE_LARGE	5

 
#define DEFAULT_PARPORT         0
#define DEFAULT_PROFILE         PANEL_PROFILE_LARGE
#define DEFAULT_KEYPAD_TYPE     KEYPAD_TYPE_OLD
#define DEFAULT_LCD_TYPE        LCD_TYPE_OLD
#define DEFAULT_LCD_HEIGHT      2
#define DEFAULT_LCD_WIDTH       40
#define DEFAULT_LCD_CHARSET     LCD_CHARSET_NORMAL
#define DEFAULT_LCD_PROTO       LCD_PROTO_PARALLEL

#define DEFAULT_LCD_PIN_E       PIN_AUTOLF
#define DEFAULT_LCD_PIN_RS      PIN_SELECP
#define DEFAULT_LCD_PIN_RW      PIN_INITP
#define DEFAULT_LCD_PIN_SCL     PIN_STROBE
#define DEFAULT_LCD_PIN_SDA     PIN_D0
#define DEFAULT_LCD_PIN_BL      PIN_NOT_SET

#ifdef CONFIG_PANEL_PARPORT
#undef DEFAULT_PARPORT
#define DEFAULT_PARPORT CONFIG_PANEL_PARPORT
#endif

#ifdef CONFIG_PANEL_PROFILE
#undef DEFAULT_PROFILE
#define DEFAULT_PROFILE CONFIG_PANEL_PROFILE
#endif

#if DEFAULT_PROFILE == 0	 
#ifdef CONFIG_PANEL_KEYPAD
#undef DEFAULT_KEYPAD_TYPE
#define DEFAULT_KEYPAD_TYPE CONFIG_PANEL_KEYPAD
#endif

#ifdef CONFIG_PANEL_LCD
#undef DEFAULT_LCD_TYPE
#define DEFAULT_LCD_TYPE CONFIG_PANEL_LCD
#endif

#ifdef CONFIG_PANEL_LCD_HEIGHT
#undef DEFAULT_LCD_HEIGHT
#define DEFAULT_LCD_HEIGHT CONFIG_PANEL_LCD_HEIGHT
#endif

#ifdef CONFIG_PANEL_LCD_WIDTH
#undef DEFAULT_LCD_WIDTH
#define DEFAULT_LCD_WIDTH CONFIG_PANEL_LCD_WIDTH
#endif

#ifdef CONFIG_PANEL_LCD_BWIDTH
#undef DEFAULT_LCD_BWIDTH
#define DEFAULT_LCD_BWIDTH CONFIG_PANEL_LCD_BWIDTH
#endif

#ifdef CONFIG_PANEL_LCD_HWIDTH
#undef DEFAULT_LCD_HWIDTH
#define DEFAULT_LCD_HWIDTH CONFIG_PANEL_LCD_HWIDTH
#endif

#ifdef CONFIG_PANEL_LCD_CHARSET
#undef DEFAULT_LCD_CHARSET
#define DEFAULT_LCD_CHARSET CONFIG_PANEL_LCD_CHARSET
#endif

#ifdef CONFIG_PANEL_LCD_PROTO
#undef DEFAULT_LCD_PROTO
#define DEFAULT_LCD_PROTO CONFIG_PANEL_LCD_PROTO
#endif

#ifdef CONFIG_PANEL_LCD_PIN_E
#undef DEFAULT_LCD_PIN_E
#define DEFAULT_LCD_PIN_E CONFIG_PANEL_LCD_PIN_E
#endif

#ifdef CONFIG_PANEL_LCD_PIN_RS
#undef DEFAULT_LCD_PIN_RS
#define DEFAULT_LCD_PIN_RS CONFIG_PANEL_LCD_PIN_RS
#endif

#ifdef CONFIG_PANEL_LCD_PIN_RW
#undef DEFAULT_LCD_PIN_RW
#define DEFAULT_LCD_PIN_RW CONFIG_PANEL_LCD_PIN_RW
#endif

#ifdef CONFIG_PANEL_LCD_PIN_SCL
#undef DEFAULT_LCD_PIN_SCL
#define DEFAULT_LCD_PIN_SCL CONFIG_PANEL_LCD_PIN_SCL
#endif

#ifdef CONFIG_PANEL_LCD_PIN_SDA
#undef DEFAULT_LCD_PIN_SDA
#define DEFAULT_LCD_PIN_SDA CONFIG_PANEL_LCD_PIN_SDA
#endif

#ifdef CONFIG_PANEL_LCD_PIN_BL
#undef DEFAULT_LCD_PIN_BL
#define DEFAULT_LCD_PIN_BL CONFIG_PANEL_LCD_PIN_BL
#endif

#endif  

 

 
static atomic_t keypad_available = ATOMIC_INIT(1);

static struct pardevice *pprt;

static int keypad_initialized;

static DEFINE_SPINLOCK(pprt_lock);
static struct timer_list scan_timer;

MODULE_DESCRIPTION("Generic parallel port LCD/Keypad driver");

static int parport = DEFAULT_PARPORT;
module_param(parport, int, 0000);
MODULE_PARM_DESC(parport, "Parallel port index (0=lpt1, 1=lpt2, ...)");

static int profile = DEFAULT_PROFILE;
module_param(profile, int, 0000);
MODULE_PARM_DESC(profile,
		 "1=16x2 old kp; 2=serial 16x2, new kp; 3=16x2 hantronix; "
		 "4=16x2 nexcom; default=40x2, old kp");

static int keypad_type = NOT_SET;
module_param(keypad_type, int, 0000);
MODULE_PARM_DESC(keypad_type,
		 "Keypad type: 0=none, 1=old 6 keys, 2=new 6+1 keys, 3=nexcom 4 keys");

static int lcd_type = NOT_SET;
module_param(lcd_type, int, 0000);
MODULE_PARM_DESC(lcd_type,
		 "LCD type: 0=none, 1=compiled-in, 2=old, 3=serial ks0074, 4=hantronix, 5=nexcom");

static int lcd_height = NOT_SET;
module_param(lcd_height, int, 0000);
MODULE_PARM_DESC(lcd_height, "Number of lines on the LCD");

static int lcd_width = NOT_SET;
module_param(lcd_width, int, 0000);
MODULE_PARM_DESC(lcd_width, "Number of columns on the LCD");

static int lcd_bwidth = NOT_SET;	 
module_param(lcd_bwidth, int, 0000);
MODULE_PARM_DESC(lcd_bwidth, "Internal LCD line width (40)");

static int lcd_hwidth = NOT_SET;	 
module_param(lcd_hwidth, int, 0000);
MODULE_PARM_DESC(lcd_hwidth, "LCD line hardware address (64)");

static int lcd_charset = NOT_SET;
module_param(lcd_charset, int, 0000);
MODULE_PARM_DESC(lcd_charset, "LCD character set: 0=standard, 1=KS0074");

static int lcd_proto = NOT_SET;
module_param(lcd_proto, int, 0000);
MODULE_PARM_DESC(lcd_proto,
		 "LCD communication: 0=parallel (//), 1=serial, 2=TI LCD Interface");

 

static int lcd_e_pin  = PIN_NOT_SET;
module_param(lcd_e_pin, int, 0000);
MODULE_PARM_DESC(lcd_e_pin,
		 "# of the // port pin connected to LCD 'E' signal, with polarity (-17..17)");

static int lcd_rs_pin = PIN_NOT_SET;
module_param(lcd_rs_pin, int, 0000);
MODULE_PARM_DESC(lcd_rs_pin,
		 "# of the // port pin connected to LCD 'RS' signal, with polarity (-17..17)");

static int lcd_rw_pin = PIN_NOT_SET;
module_param(lcd_rw_pin, int, 0000);
MODULE_PARM_DESC(lcd_rw_pin,
		 "# of the // port pin connected to LCD 'RW' signal, with polarity (-17..17)");

static int lcd_cl_pin = PIN_NOT_SET;
module_param(lcd_cl_pin, int, 0000);
MODULE_PARM_DESC(lcd_cl_pin,
		 "# of the // port pin connected to serial LCD 'SCL' signal, with polarity (-17..17)");

static int lcd_da_pin = PIN_NOT_SET;
module_param(lcd_da_pin, int, 0000);
MODULE_PARM_DESC(lcd_da_pin,
		 "# of the // port pin connected to serial LCD 'SDA' signal, with polarity (-17..17)");

static int lcd_bl_pin = PIN_NOT_SET;
module_param(lcd_bl_pin, int, 0000);
MODULE_PARM_DESC(lcd_bl_pin,
		 "# of the // port pin connected to LCD backlight, with polarity (-17..17)");

 

static int lcd_enabled = NOT_SET;
module_param(lcd_enabled, int, 0000);
MODULE_PARM_DESC(lcd_enabled, "Deprecated option, use lcd_type instead");

static int keypad_enabled = NOT_SET;
module_param(keypad_enabled, int, 0000);
MODULE_PARM_DESC(keypad_enabled, "Deprecated option, use keypad_type instead");

 
static const unsigned char lcd_char_conv_ks0074[256] = {
	 
	  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	  0x20, 0x21, 0x22, 0x23, 0xa2, 0x25, 0x26, 0x27,
	  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	  0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	  0xa0, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	  0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	  0x58, 0x59, 0x5a, 0xfa, 0xfb, 0xfc, 0x1d, 0xc4,
	  0x96, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	  0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	  0x78, 0x79, 0x7a, 0xfd, 0xfe, 0xff, 0xce, 0x20,
	  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	  0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	  0x20, 0x40, 0xb1, 0xa1, 0x24, 0xa3, 0xfe, 0x5f,
	  0x22, 0xc8, 0x61, 0x14, 0x97, 0x2d, 0xad, 0x96,
	  0x80, 0x8c, 0x82, 0x83, 0x27, 0x8f, 0x86, 0xdd,
	  0x2c, 0x81, 0x6f, 0x15, 0x8b, 0x8a, 0x84, 0x60,
	  0xe2, 0xe2, 0xe2, 0x5b, 0x5b, 0xae, 0xbc, 0xa9,
	  0xc5, 0xbf, 0xc6, 0xf1, 0xe3, 0xe3, 0xe3, 0xe3,
	  0x44, 0x5d, 0xa8, 0xe4, 0xec, 0xec, 0x5c, 0x78,
	  0xab, 0xa6, 0xe5, 0x5e, 0x5e, 0xe6, 0xaa, 0xbe,
	  0x7f, 0xe7, 0xaf, 0x7b, 0x7b, 0xaf, 0xbd, 0xc8,
	  0xa4, 0xa5, 0xc7, 0xf6, 0xa7, 0xe8, 0x69, 0x69,
	  0xed, 0x7d, 0xa8, 0xe4, 0xec, 0x5c, 0x5c, 0x25,
	  0xac, 0xa6, 0xea, 0xef, 0x7e, 0xeb, 0xb2, 0x79,
};

static const char old_keypad_profile[][4][9] = {
	{"S0", "Left\n", "Left\n", ""},
	{"S1", "Down\n", "Down\n", ""},
	{"S2", "Up\n", "Up\n", ""},
	{"S3", "Right\n", "Right\n", ""},
	{"S4", "Esc\n", "Esc\n", ""},
	{"S5", "Ret\n", "Ret\n", ""},
	{"", "", "", ""}
};

 
static const char new_keypad_profile[][4][9] = {
	{"S0", "Left\n", "Left\n", ""},
	{"S1", "Down\n", "Down\n", ""},
	{"S2", "Up\n", "Up\n", ""},
	{"S3", "Right\n", "Right\n", ""},
	{"S4s5", "", "Esc\n", "Esc\n"},
	{"s4S5", "", "Ret\n", "Ret\n"},
	{"S4S5", "Help\n", "", ""},
	 
	{"", "", "", ""}
};

 
static const char nexcom_keypad_profile[][4][9] = {
	{"a-p-e-", "Down\n", "Down\n", ""},
	{"a-p-E-", "Ret\n", "Ret\n", ""},
	{"a-P-E-", "Esc\n", "Esc\n", ""},
	{"a-P-e-", "Up\n", "Up\n", ""},
	 
	{"", "", "", ""}
};

static const char (*keypad_profile)[4][9] = old_keypad_profile;

static DECLARE_BITMAP(bits, LCD_BITS);

static void lcd_get_bits(unsigned int port, int *val)
{
	unsigned int bit, state;

	for (bit = 0; bit < LCD_BITS; bit++) {
		state = test_bit(bit, bits) ? BIT_SET : BIT_CLR;
		*val &= lcd_bits[port][bit][BIT_MSK];
		*val |= lcd_bits[port][bit][state];
	}
}

 
static int set_data_bits(void)
{
	int val;

	val = r_dtr(pprt);
	lcd_get_bits(LCD_PORT_D, &val);
	w_dtr(pprt, val);
	return val;
}

 
static int set_ctrl_bits(void)
{
	int val;

	val = r_ctr(pprt);
	lcd_get_bits(LCD_PORT_C, &val);
	w_ctr(pprt, val);
	return val;
}

 
static void panel_set_bits(void)
{
	set_data_bits();
	set_ctrl_bits();
}

 
static void pin_to_bits(int pin, unsigned char *d_val, unsigned char *c_val)
{
	int d_bit, c_bit, inv;

	d_val[0] = 0;
	c_val[0] = 0;
	d_val[1] = 0;
	c_val[1] = 0;
	d_val[2] = 0xFF;
	c_val[2] = 0xFF;

	if (pin == 0)
		return;

	inv = (pin < 0);
	if (inv)
		pin = -pin;

	d_bit = 0;
	c_bit = 0;

	switch (pin) {
	case PIN_STROBE:	 
		c_bit = PNL_PSTROBE;
		inv = !inv;
		break;
	case PIN_D0...PIN_D7:	 
		d_bit = 1 << (pin - 2);
		break;
	case PIN_AUTOLF:	 
		c_bit = PNL_PAUTOLF;
		inv = !inv;
		break;
	case PIN_INITP:		 
		c_bit = PNL_PINITP;
		break;
	case PIN_SELECP:	 
		c_bit = PNL_PSELECP;
		inv = !inv;
		break;
	default:		 
		break;
	}

	if (c_bit) {
		c_val[2] &= ~c_bit;
		c_val[!inv] = c_bit;
	} else if (d_bit) {
		d_val[2] &= ~d_bit;
		d_val[!inv] = d_bit;
	}
}

 
static void lcd_send_serial(int byte)
{
	int bit;

	 
	for (bit = 0; bit < 8; bit++) {
		clear_bit(LCD_BIT_CL, bits);	 
		panel_set_bits();
		if (byte & 1) {
			set_bit(LCD_BIT_DA, bits);
		} else {
			clear_bit(LCD_BIT_DA, bits);
		}

		panel_set_bits();
		udelay(2);   
		set_bit(LCD_BIT_CL, bits);	 
		panel_set_bits();
		udelay(1);   
		byte >>= 1;
	}
}

 
static void lcd_backlight(struct charlcd *charlcd, enum charlcd_onoff on)
{
	if (lcd.pins.bl == PIN_NONE)
		return;

	 
	spin_lock_irq(&pprt_lock);
	if (on)
		set_bit(LCD_BIT_BL, bits);
	else
		clear_bit(LCD_BIT_BL, bits);
	panel_set_bits();
	spin_unlock_irq(&pprt_lock);
}

 
static void lcd_write_cmd_s(struct hd44780_common *hdc, int cmd)
{
	spin_lock_irq(&pprt_lock);
	lcd_send_serial(0x1F);	 
	lcd_send_serial(cmd & 0x0F);
	lcd_send_serial((cmd >> 4) & 0x0F);
	udelay(40);		 
	spin_unlock_irq(&pprt_lock);
}

 
static void lcd_write_data_s(struct hd44780_common *hdc, int data)
{
	spin_lock_irq(&pprt_lock);
	lcd_send_serial(0x5F);	 
	lcd_send_serial(data & 0x0F);
	lcd_send_serial((data >> 4) & 0x0F);
	udelay(40);		 
	spin_unlock_irq(&pprt_lock);
}

 
static void lcd_write_cmd_p8(struct hd44780_common *hdc, int cmd)
{
	spin_lock_irq(&pprt_lock);
	 
	w_dtr(pprt, cmd);
	udelay(20);	 

	set_bit(LCD_BIT_E, bits);
	clear_bit(LCD_BIT_RS, bits);
	clear_bit(LCD_BIT_RW, bits);
	set_ctrl_bits();

	udelay(40);	 

	clear_bit(LCD_BIT_E, bits);
	set_ctrl_bits();

	udelay(120);	 
	spin_unlock_irq(&pprt_lock);
}

 
static void lcd_write_data_p8(struct hd44780_common *hdc, int data)
{
	spin_lock_irq(&pprt_lock);
	 
	w_dtr(pprt, data);
	udelay(20);	 

	set_bit(LCD_BIT_E, bits);
	set_bit(LCD_BIT_RS, bits);
	clear_bit(LCD_BIT_RW, bits);
	set_ctrl_bits();

	udelay(40);	 

	clear_bit(LCD_BIT_E, bits);
	set_ctrl_bits();

	udelay(45);	 
	spin_unlock_irq(&pprt_lock);
}

 
static void lcd_write_cmd_tilcd(struct hd44780_common *hdc, int cmd)
{
	spin_lock_irq(&pprt_lock);
	 
	w_ctr(pprt, cmd);
	udelay(60);
	spin_unlock_irq(&pprt_lock);
}

 
static void lcd_write_data_tilcd(struct hd44780_common *hdc, int data)
{
	spin_lock_irq(&pprt_lock);
	 
	w_dtr(pprt, data);
	udelay(60);
	spin_unlock_irq(&pprt_lock);
}

static const struct charlcd_ops charlcd_ops = {
	.backlight	= lcd_backlight,
	.print		= hd44780_common_print,
	.gotoxy		= hd44780_common_gotoxy,
	.home		= hd44780_common_home,
	.clear_display	= hd44780_common_clear_display,
	.init_display	= hd44780_common_init_display,
	.shift_cursor	= hd44780_common_shift_cursor,
	.shift_display	= hd44780_common_shift_display,
	.display	= hd44780_common_display,
	.cursor		= hd44780_common_cursor,
	.blink		= hd44780_common_blink,
	.fontsize	= hd44780_common_fontsize,
	.lines		= hd44780_common_lines,
	.redefine_char	= hd44780_common_redefine_char,
};

 
static void lcd_init(void)
{
	struct charlcd *charlcd;
	struct hd44780_common *hdc;

	hdc = hd44780_common_alloc();
	if (!hdc)
		return;

	charlcd = charlcd_alloc();
	if (!charlcd) {
		kfree(hdc);
		return;
	}

	hdc->hd44780 = &lcd;
	charlcd->drvdata = hdc;

	 
	charlcd->height = lcd_height;
	charlcd->width = lcd_width;
	hdc->bwidth = lcd_bwidth;
	hdc->hwidth = lcd_hwidth;

	switch (selected_lcd_type) {
	case LCD_TYPE_OLD:
		 
		lcd.proto = LCD_PROTO_PARALLEL;
		lcd.charset = LCD_CHARSET_NORMAL;
		lcd.pins.e = PIN_STROBE;
		lcd.pins.rs = PIN_AUTOLF;

		charlcd->width = 40;
		hdc->bwidth = 40;
		hdc->hwidth = 64;
		charlcd->height = 2;
		break;
	case LCD_TYPE_KS0074:
		 
		lcd.proto = LCD_PROTO_SERIAL;
		lcd.charset = LCD_CHARSET_KS0074;
		lcd.pins.bl = PIN_AUTOLF;
		lcd.pins.cl = PIN_STROBE;
		lcd.pins.da = PIN_D0;

		charlcd->width = 16;
		hdc->bwidth = 40;
		hdc->hwidth = 16;
		charlcd->height = 2;
		break;
	case LCD_TYPE_NEXCOM:
		 
		lcd.proto = LCD_PROTO_PARALLEL;
		lcd.charset = LCD_CHARSET_NORMAL;
		lcd.pins.e = PIN_AUTOLF;
		lcd.pins.rs = PIN_SELECP;
		lcd.pins.rw = PIN_INITP;

		charlcd->width = 16;
		hdc->bwidth = 40;
		hdc->hwidth = 64;
		charlcd->height = 2;
		break;
	case LCD_TYPE_CUSTOM:
		 
		lcd.proto = DEFAULT_LCD_PROTO;
		lcd.charset = DEFAULT_LCD_CHARSET;
		 
		break;
	case LCD_TYPE_HANTRONIX:
		 
	default:
		lcd.proto = LCD_PROTO_PARALLEL;
		lcd.charset = LCD_CHARSET_NORMAL;
		lcd.pins.e = PIN_STROBE;
		lcd.pins.rs = PIN_SELECP;

		charlcd->width = 16;
		hdc->bwidth = 40;
		hdc->hwidth = 64;
		charlcd->height = 2;
		break;
	}

	 
	if (lcd_height != NOT_SET)
		charlcd->height = lcd_height;
	if (lcd_width != NOT_SET)
		charlcd->width = lcd_width;
	if (lcd_bwidth != NOT_SET)
		hdc->bwidth = lcd_bwidth;
	if (lcd_hwidth != NOT_SET)
		hdc->hwidth = lcd_hwidth;
	if (lcd_charset != NOT_SET)
		lcd.charset = lcd_charset;
	if (lcd_proto != NOT_SET)
		lcd.proto = lcd_proto;
	if (lcd_e_pin != PIN_NOT_SET)
		lcd.pins.e = lcd_e_pin;
	if (lcd_rs_pin != PIN_NOT_SET)
		lcd.pins.rs = lcd_rs_pin;
	if (lcd_rw_pin != PIN_NOT_SET)
		lcd.pins.rw = lcd_rw_pin;
	if (lcd_cl_pin != PIN_NOT_SET)
		lcd.pins.cl = lcd_cl_pin;
	if (lcd_da_pin != PIN_NOT_SET)
		lcd.pins.da = lcd_da_pin;
	if (lcd_bl_pin != PIN_NOT_SET)
		lcd.pins.bl = lcd_bl_pin;

	 
	if (charlcd->width <= 0)
		charlcd->width = DEFAULT_LCD_WIDTH;
	if (hdc->bwidth <= 0)
		hdc->bwidth = DEFAULT_LCD_BWIDTH;
	if (hdc->hwidth <= 0)
		hdc->hwidth = DEFAULT_LCD_HWIDTH;
	if (charlcd->height <= 0)
		charlcd->height = DEFAULT_LCD_HEIGHT;

	if (lcd.proto == LCD_PROTO_SERIAL) {	 
		charlcd->ops = &charlcd_ops;
		hdc->write_data = lcd_write_data_s;
		hdc->write_cmd = lcd_write_cmd_s;

		if (lcd.pins.cl == PIN_NOT_SET)
			lcd.pins.cl = DEFAULT_LCD_PIN_SCL;
		if (lcd.pins.da == PIN_NOT_SET)
			lcd.pins.da = DEFAULT_LCD_PIN_SDA;

	} else if (lcd.proto == LCD_PROTO_PARALLEL) {	 
		charlcd->ops = &charlcd_ops;
		hdc->write_data = lcd_write_data_p8;
		hdc->write_cmd = lcd_write_cmd_p8;

		if (lcd.pins.e == PIN_NOT_SET)
			lcd.pins.e = DEFAULT_LCD_PIN_E;
		if (lcd.pins.rs == PIN_NOT_SET)
			lcd.pins.rs = DEFAULT_LCD_PIN_RS;
		if (lcd.pins.rw == PIN_NOT_SET)
			lcd.pins.rw = DEFAULT_LCD_PIN_RW;
	} else {
		charlcd->ops = &charlcd_ops;
		hdc->write_data = lcd_write_data_tilcd;
		hdc->write_cmd = lcd_write_cmd_tilcd;
	}

	if (lcd.pins.bl == PIN_NOT_SET)
		lcd.pins.bl = DEFAULT_LCD_PIN_BL;

	if (lcd.pins.e == PIN_NOT_SET)
		lcd.pins.e = PIN_NONE;
	if (lcd.pins.rs == PIN_NOT_SET)
		lcd.pins.rs = PIN_NONE;
	if (lcd.pins.rw == PIN_NOT_SET)
		lcd.pins.rw = PIN_NONE;
	if (lcd.pins.bl == PIN_NOT_SET)
		lcd.pins.bl = PIN_NONE;
	if (lcd.pins.cl == PIN_NOT_SET)
		lcd.pins.cl = PIN_NONE;
	if (lcd.pins.da == PIN_NOT_SET)
		lcd.pins.da = PIN_NONE;

	if (lcd.charset == NOT_SET)
		lcd.charset = DEFAULT_LCD_CHARSET;

	if (lcd.charset == LCD_CHARSET_KS0074)
		charlcd->char_conv = lcd_char_conv_ks0074;
	else
		charlcd->char_conv = NULL;

	pin_to_bits(lcd.pins.e, lcd_bits[LCD_PORT_D][LCD_BIT_E],
		    lcd_bits[LCD_PORT_C][LCD_BIT_E]);
	pin_to_bits(lcd.pins.rs, lcd_bits[LCD_PORT_D][LCD_BIT_RS],
		    lcd_bits[LCD_PORT_C][LCD_BIT_RS]);
	pin_to_bits(lcd.pins.rw, lcd_bits[LCD_PORT_D][LCD_BIT_RW],
		    lcd_bits[LCD_PORT_C][LCD_BIT_RW]);
	pin_to_bits(lcd.pins.bl, lcd_bits[LCD_PORT_D][LCD_BIT_BL],
		    lcd_bits[LCD_PORT_C][LCD_BIT_BL]);
	pin_to_bits(lcd.pins.cl, lcd_bits[LCD_PORT_D][LCD_BIT_CL],
		    lcd_bits[LCD_PORT_C][LCD_BIT_CL]);
	pin_to_bits(lcd.pins.da, lcd_bits[LCD_PORT_D][LCD_BIT_DA],
		    lcd_bits[LCD_PORT_C][LCD_BIT_DA]);

	lcd.charlcd = charlcd;
	lcd.initialized = true;
}

 

static ssize_t keypad_read(struct file *file,
			   char __user *buf, size_t count, loff_t *ppos)
{
	unsigned i = *ppos;
	char __user *tmp = buf;

	if (keypad_buflen == 0) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(keypad_read_wait,
					     keypad_buflen != 0))
			return -EINTR;
	}

	for (; count-- > 0 && (keypad_buflen > 0);
	     ++i, ++tmp, --keypad_buflen) {
		put_user(keypad_buffer[keypad_start], tmp);
		keypad_start = (keypad_start + 1) % KEYPAD_BUFFER;
	}
	*ppos = i;

	return tmp - buf;
}

static int keypad_open(struct inode *inode, struct file *file)
{
	int ret;

	ret = -EBUSY;
	if (!atomic_dec_and_test(&keypad_available))
		goto fail;	 

	ret = -EPERM;
	if (file->f_mode & FMODE_WRITE)	 
		goto fail;

	keypad_buflen = 0;	 
	return 0;
 fail:
	atomic_inc(&keypad_available);
	return ret;
}

static int keypad_release(struct inode *inode, struct file *file)
{
	atomic_inc(&keypad_available);
	return 0;
}

static const struct file_operations keypad_fops = {
	.read    = keypad_read,		 
	.open    = keypad_open,		 
	.release = keypad_release,	 
	.llseek  = default_llseek,
};

static struct miscdevice keypad_dev = {
	.minor	= KEYPAD_MINOR,
	.name	= "keypad",
	.fops	= &keypad_fops,
};

static void keypad_send_key(const char *string, int max_len)
{
	 
	if (!atomic_read(&keypad_available)) {
		while (max_len-- && keypad_buflen < KEYPAD_BUFFER && *string) {
			keypad_buffer[(keypad_start + keypad_buflen++) %
				      KEYPAD_BUFFER] = *string++;
		}
		wake_up_interruptible(&keypad_read_wait);
	}
}

 
static void phys_scan_contacts(void)
{
	int bit, bitval;
	char oldval;
	char bitmask;
	char gndmask;

	phys_prev = phys_curr;
	phys_read_prev = phys_read;
	phys_read = 0;		 

	 
	oldval = r_dtr(pprt) | scan_mask_o;
	 
	w_dtr(pprt, oldval & ~scan_mask_o);

	 
	bitmask = PNL_PINPUT(r_str(pprt)) & scan_mask_i;
	 
	w_dtr(pprt, oldval);

	 

	 
	gndmask = PNL_PINPUT(r_str(pprt)) & scan_mask_i;

	 
	phys_read |= (__u64)gndmask << 40;

	if (bitmask != gndmask) {
		 
		for (bit = 0; bit < 8; bit++) {
			bitval = BIT(bit);

			if (!(scan_mask_o & bitval))	 
				continue;

			w_dtr(pprt, oldval & ~bitval);	 
			bitmask = PNL_PINPUT(r_str(pprt)) & ~gndmask;
			phys_read |= (__u64)bitmask << (5 * bit);
		}
		w_dtr(pprt, oldval);	 
	}
	 
	phys_curr = (phys_prev & (phys_read ^ phys_read_prev)) |
		    (phys_read & ~(phys_read ^ phys_read_prev));
}

static inline int input_state_high(struct logical_input *input)
{
#if 0
	 

	 
	if (((phys_prev & input->mask) == input->value) &&
	    ((phys_curr & input->mask) >  input->value)) {
		input->state = INPUT_ST_LOW;  
		return 1;
	}
#endif

	if ((phys_curr & input->mask) == input->value) {
		if ((input->type == INPUT_TYPE_STD) &&
		    (input->high_timer == 0)) {
			input->high_timer++;
			if (input->u.std.press_fct)
				input->u.std.press_fct(input->u.std.press_data);
		} else if (input->type == INPUT_TYPE_KBD) {
			 
			keypressed = 1;

			if (input->high_timer == 0) {
				char *press_str = input->u.kbd.press_str;

				if (press_str[0]) {
					int s = sizeof(input->u.kbd.press_str);

					keypad_send_key(press_str, s);
				}
			}

			if (input->u.kbd.repeat_str[0]) {
				char *repeat_str = input->u.kbd.repeat_str;

				if (input->high_timer >= KEYPAD_REP_START) {
					int s = sizeof(input->u.kbd.repeat_str);

					input->high_timer -= KEYPAD_REP_DELAY;
					keypad_send_key(repeat_str, s);
				}
				 
				inputs_stable = 0;
			}

			if (input->high_timer < 255)
				input->high_timer++;
		}
		return 1;
	}

	 
	input->state = INPUT_ST_FALLING;
	input->fall_timer = 0;

	return 0;
}

static inline void input_state_falling(struct logical_input *input)
{
#if 0
	 
	if (((phys_prev & input->mask) == input->value) &&
	    ((phys_curr & input->mask) >  input->value)) {
		input->state = INPUT_ST_LOW;	 
		return;
	}
#endif

	if ((phys_curr & input->mask) == input->value) {
		if (input->type == INPUT_TYPE_KBD) {
			 
			keypressed = 1;

			if (input->u.kbd.repeat_str[0]) {
				char *repeat_str = input->u.kbd.repeat_str;

				if (input->high_timer >= KEYPAD_REP_START) {
					int s = sizeof(input->u.kbd.repeat_str);

					input->high_timer -= KEYPAD_REP_DELAY;
					keypad_send_key(repeat_str, s);
				}
				 
				inputs_stable = 0;
			}

			if (input->high_timer < 255)
				input->high_timer++;
		}
		input->state = INPUT_ST_HIGH;
	} else if (input->fall_timer >= input->fall_time) {
		 
		if (input->type == INPUT_TYPE_STD) {
			void (*release_fct)(int) = input->u.std.release_fct;

			if (release_fct)
				release_fct(input->u.std.release_data);
		} else if (input->type == INPUT_TYPE_KBD) {
			char *release_str = input->u.kbd.release_str;

			if (release_str[0]) {
				int s = sizeof(input->u.kbd.release_str);

				keypad_send_key(release_str, s);
			}
		}

		input->state = INPUT_ST_LOW;
	} else {
		input->fall_timer++;
		inputs_stable = 0;
	}
}

static void panel_process_inputs(void)
{
	struct logical_input *input;

	keypressed = 0;
	inputs_stable = 1;
	list_for_each_entry(input, &logical_inputs, list) {
		switch (input->state) {
		case INPUT_ST_LOW:
			if ((phys_curr & input->mask) != input->value)
				break;
			 
			if ((phys_prev & input->mask) == input->value)
				break;
			input->rise_timer = 0;
			input->state = INPUT_ST_RISING;
			fallthrough;
		case INPUT_ST_RISING:
			if ((phys_curr & input->mask) != input->value) {
				input->state = INPUT_ST_LOW;
				break;
			}
			if (input->rise_timer < input->rise_time) {
				inputs_stable = 0;
				input->rise_timer++;
				break;
			}
			input->high_timer = 0;
			input->state = INPUT_ST_HIGH;
			fallthrough;
		case INPUT_ST_HIGH:
			if (input_state_high(input))
				break;
			fallthrough;
		case INPUT_ST_FALLING:
			input_state_falling(input);
		}
	}
}

static void panel_scan_timer(struct timer_list *unused)
{
	if (keypad.enabled && keypad_initialized) {
		if (spin_trylock_irq(&pprt_lock)) {
			phys_scan_contacts();

			 
			spin_unlock_irq(&pprt_lock);
		}

		if (!inputs_stable || phys_curr != phys_prev)
			panel_process_inputs();
	}

	if (keypressed && lcd.enabled && lcd.initialized)
		charlcd_poke(lcd.charlcd);

	mod_timer(&scan_timer, jiffies + INPUT_POLL_TIME);
}

static void init_scan_timer(void)
{
	if (scan_timer.function)
		return;		 

	timer_setup(&scan_timer, panel_scan_timer, 0);
	scan_timer.expires = jiffies + INPUT_POLL_TIME;
	add_timer(&scan_timer);
}

 
static u8 input_name2mask(const char *name, __u64 *mask, __u64 *value,
			  u8 *imask, u8 *omask)
{
	const char sigtab[] = "EeSsPpAaBb";
	u8 im, om;
	__u64 m, v;

	om = 0;
	im = 0;
	m = 0ULL;
	v = 0ULL;
	while (*name) {
		int in, out, bit, neg;
		const char *idx;

		idx = strchr(sigtab, *name);
		if (!idx)
			return 0;	 

		in = idx - sigtab;
		neg = (in & 1);	 
		in >>= 1;
		im |= BIT(in);

		name++;
		if (*name >= '0' && *name <= '7') {
			out = *name - '0';
			om |= BIT(out);
		} else if (*name == '-') {
			out = 8;
		} else {
			return 0;	 
		}

		bit = (out * 5) + in;

		m |= 1ULL << bit;
		if (!neg)
			v |= 1ULL << bit;
		name++;
	}
	*mask = m;
	*value = v;
	if (imask)
		*imask |= im;
	if (omask)
		*omask |= om;
	return 1;
}

 
static struct logical_input *panel_bind_key(const char *name, const char *press,
					    const char *repeat,
					    const char *release)
{
	struct logical_input *key;

	key = kzalloc(sizeof(*key), GFP_KERNEL);
	if (!key)
		return NULL;

	if (!input_name2mask(name, &key->mask, &key->value, &scan_mask_i,
			     &scan_mask_o)) {
		kfree(key);
		return NULL;
	}

	key->type = INPUT_TYPE_KBD;
	key->state = INPUT_ST_LOW;
	key->rise_time = 1;
	key->fall_time = 1;

	strncpy(key->u.kbd.press_str, press, sizeof(key->u.kbd.press_str));
	strncpy(key->u.kbd.repeat_str, repeat, sizeof(key->u.kbd.repeat_str));
	strncpy(key->u.kbd.release_str, release,
		sizeof(key->u.kbd.release_str));
	list_add(&key->list, &logical_inputs);
	return key;
}

#if 0
 
static struct logical_input *panel_bind_callback(char *name,
						 void (*press_fct)(int),
						 int press_data,
						 void (*release_fct)(int),
						 int release_data)
{
	struct logical_input *callback;

	callback = kmalloc(sizeof(*callback), GFP_KERNEL);
	if (!callback)
		return NULL;

	memset(callback, 0, sizeof(struct logical_input));
	if (!input_name2mask(name, &callback->mask, &callback->value,
			     &scan_mask_i, &scan_mask_o))
		return NULL;

	callback->type = INPUT_TYPE_STD;
	callback->state = INPUT_ST_LOW;
	callback->rise_time = 1;
	callback->fall_time = 1;
	callback->u.std.press_fct = press_fct;
	callback->u.std.press_data = press_data;
	callback->u.std.release_fct = release_fct;
	callback->u.std.release_data = release_data;
	list_add(&callback->list, &logical_inputs);
	return callback;
}
#endif

static void keypad_init(void)
{
	int keynum;

	init_waitqueue_head(&keypad_read_wait);
	keypad_buflen = 0;	 

	 

	for (keynum = 0; keypad_profile[keynum][0][0]; keynum++) {
		panel_bind_key(keypad_profile[keynum][0],
			       keypad_profile[keynum][1],
			       keypad_profile[keynum][2],
			       keypad_profile[keynum][3]);
	}

	init_scan_timer();
	keypad_initialized = 1;
}

 
 
 

static void panel_attach(struct parport *port)
{
	struct pardev_cb panel_cb;

	if (port->number != parport)
		return;

	if (pprt) {
		pr_err("%s: port->number=%d parport=%d, already registered!\n",
		       __func__, port->number, parport);
		return;
	}

	memset(&panel_cb, 0, sizeof(panel_cb));
	panel_cb.private = &pprt;
	 

	pprt = parport_register_dev_model(port, "panel", &panel_cb, 0);
	if (!pprt) {
		pr_err("%s: port->number=%d parport=%d, parport_register_device() failed\n",
		       __func__, port->number, parport);
		return;
	}

	if (parport_claim(pprt)) {
		pr_err("could not claim access to parport%d. Aborting.\n",
		       parport);
		goto err_unreg_device;
	}

	 
	if (lcd.enabled) {
		lcd_init();
		if (!lcd.charlcd || charlcd_register(lcd.charlcd))
			goto err_unreg_device;
	}

	if (keypad.enabled) {
		keypad_init();
		if (misc_register(&keypad_dev))
			goto err_lcd_unreg;
	}
	return;

err_lcd_unreg:
	if (scan_timer.function)
		del_timer_sync(&scan_timer);
	if (lcd.enabled)
		charlcd_unregister(lcd.charlcd);
err_unreg_device:
	kfree(lcd.charlcd);
	lcd.charlcd = NULL;
	parport_unregister_device(pprt);
	pprt = NULL;
}

static void panel_detach(struct parport *port)
{
	if (port->number != parport)
		return;

	if (!pprt) {
		pr_err("%s: port->number=%d parport=%d, nothing to unregister.\n",
		       __func__, port->number, parport);
		return;
	}
	if (scan_timer.function)
		del_timer_sync(&scan_timer);

	if (keypad.enabled) {
		misc_deregister(&keypad_dev);
		keypad_initialized = 0;
	}

	if (lcd.enabled) {
		charlcd_unregister(lcd.charlcd);
		lcd.initialized = false;
		kfree(lcd.charlcd->drvdata);
		kfree(lcd.charlcd);
		lcd.charlcd = NULL;
	}

	 
	parport_release(pprt);
	parport_unregister_device(pprt);
	pprt = NULL;
}

static struct parport_driver panel_driver = {
	.name = "panel",
	.match_port = panel_attach,
	.detach = panel_detach,
	.devmodel = true,
};

 
static int __init panel_init_module(void)
{
	int selected_keypad_type = NOT_SET, err;

	 
	switch (profile) {
	case PANEL_PROFILE_CUSTOM:
		 
		selected_keypad_type = DEFAULT_KEYPAD_TYPE;
		selected_lcd_type = DEFAULT_LCD_TYPE;
		break;
	case PANEL_PROFILE_OLD:
		 
		selected_keypad_type = KEYPAD_TYPE_OLD;
		selected_lcd_type = LCD_TYPE_OLD;

		 
		if (lcd_width == NOT_SET)
			lcd_width = 16;
		if (lcd_hwidth == NOT_SET)
			lcd_hwidth = 16;
		break;
	case PANEL_PROFILE_NEW:
		 
		selected_keypad_type = KEYPAD_TYPE_NEW;
		selected_lcd_type = LCD_TYPE_KS0074;
		break;
	case PANEL_PROFILE_HANTRONIX:
		 
		selected_keypad_type = KEYPAD_TYPE_NONE;
		selected_lcd_type = LCD_TYPE_HANTRONIX;
		break;
	case PANEL_PROFILE_NEXCOM:
		 
		selected_keypad_type = KEYPAD_TYPE_NEXCOM;
		selected_lcd_type = LCD_TYPE_NEXCOM;
		break;
	case PANEL_PROFILE_LARGE:
		 
		selected_keypad_type = KEYPAD_TYPE_OLD;
		selected_lcd_type = LCD_TYPE_OLD;
		break;
	}

	 
	if (keypad_enabled != NOT_SET)
		selected_keypad_type = keypad_enabled;
	if (keypad_type != NOT_SET)
		selected_keypad_type = keypad_type;

	keypad.enabled = (selected_keypad_type > 0);

	if (lcd_enabled != NOT_SET)
		selected_lcd_type = lcd_enabled;
	if (lcd_type != NOT_SET)
		selected_lcd_type = lcd_type;

	lcd.enabled = (selected_lcd_type > 0);

	if (lcd.enabled) {
		 
		lcd.charset = lcd_charset;
		lcd.proto = lcd_proto;
		lcd.pins.e = lcd_e_pin;
		lcd.pins.rs = lcd_rs_pin;
		lcd.pins.rw = lcd_rw_pin;
		lcd.pins.cl = lcd_cl_pin;
		lcd.pins.da = lcd_da_pin;
		lcd.pins.bl = lcd_bl_pin;
	}

	switch (selected_keypad_type) {
	case KEYPAD_TYPE_OLD:
		keypad_profile = old_keypad_profile;
		break;
	case KEYPAD_TYPE_NEW:
		keypad_profile = new_keypad_profile;
		break;
	case KEYPAD_TYPE_NEXCOM:
		keypad_profile = nexcom_keypad_profile;
		break;
	default:
		keypad_profile = NULL;
		break;
	}

	if (!lcd.enabled && !keypad.enabled) {
		 
		pr_err("panel driver disabled.\n");
		return -ENODEV;
	}

	err = parport_register_driver(&panel_driver);
	if (err) {
		pr_err("could not register with parport. Aborting.\n");
		return err;
	}

	if (pprt)
		pr_info("panel driver registered on parport%d (io=0x%lx).\n",
			parport, pprt->port->base);
	else
		pr_info("panel driver not yet registered\n");
	return 0;
}

static void __exit panel_cleanup_module(void)
{
	parport_unregister_driver(&panel_driver);
}

module_init(panel_init_module);
module_exit(panel_cleanup_module);
MODULE_AUTHOR("Willy Tarreau");
MODULE_LICENSE("GPL");
