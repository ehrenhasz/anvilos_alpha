
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "charlcd.h"
#include "hd44780_common.h"

 
#define LCD_CMD_DISPLAY_CLEAR	0x01	 

#define LCD_CMD_ENTRY_MODE	0x04	 
#define LCD_CMD_CURSOR_INC	0x02	 

#define LCD_CMD_DISPLAY_CTRL	0x08	 
#define LCD_CMD_DISPLAY_ON	0x04	 
#define LCD_CMD_CURSOR_ON	0x02	 
#define LCD_CMD_BLINK_ON	0x01	 

#define LCD_CMD_SHIFT		0x10	 
#define LCD_CMD_DISPLAY_SHIFT	0x08	 
#define LCD_CMD_SHIFT_RIGHT	0x04	 

#define LCD_CMD_FUNCTION_SET	0x20	 
#define LCD_CMD_DATA_LEN_8BITS	0x10	 
#define LCD_CMD_TWO_LINES	0x08	 
#define LCD_CMD_FONT_5X10_DOTS	0x04	 

#define LCD_CMD_SET_CGRAM_ADDR	0x40	 

#define LCD_CMD_SET_DDRAM_ADDR	0x80	 

 
static void long_sleep(int ms)
{
	schedule_timeout_interruptible(msecs_to_jiffies(ms));
}

int hd44780_common_print(struct charlcd *lcd, int c)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (lcd->addr.x < hdc->bwidth) {
		hdc->write_data(hdc, c);
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL_GPL(hd44780_common_print);

int hd44780_common_gotoxy(struct charlcd *lcd, unsigned int x, unsigned int y)
{
	struct hd44780_common *hdc = lcd->drvdata;
	unsigned int addr;

	 
	addr = x < hdc->bwidth ? x & (hdc->hwidth - 1) : hdc->bwidth - 1;
	if (y & 1)
		addr += hdc->hwidth;
	if (y & 2)
		addr += hdc->bwidth;
	hdc->write_cmd(hdc, LCD_CMD_SET_DDRAM_ADDR | addr);
	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_gotoxy);

int hd44780_common_home(struct charlcd *lcd)
{
	return hd44780_common_gotoxy(lcd, 0, 0);
}
EXPORT_SYMBOL_GPL(hd44780_common_home);

 
int hd44780_common_clear_display(struct charlcd *lcd)
{
	struct hd44780_common *hdc = lcd->drvdata;

	hdc->write_cmd(hdc, LCD_CMD_DISPLAY_CLEAR);
	 
	long_sleep(2);

	 
	return hd44780_common_home(lcd);
}
EXPORT_SYMBOL_GPL(hd44780_common_clear_display);

int hd44780_common_init_display(struct charlcd *lcd)
{
	struct hd44780_common *hdc = lcd->drvdata;

	void (*write_cmd_raw)(struct hd44780_common *hdc, int cmd);
	u8 init;

	if (hdc->ifwidth != 4 && hdc->ifwidth != 8)
		return -EINVAL;

	hdc->hd44780_common_flags = ((lcd->height > 1) ? LCD_FLAG_N : 0) |
		LCD_FLAG_D | LCD_FLAG_C | LCD_FLAG_B;

	long_sleep(20);		 

	 
	init = LCD_CMD_FUNCTION_SET | LCD_CMD_DATA_LEN_8BITS;
	if (hdc->ifwidth == 4) {
		init >>= 4;
		write_cmd_raw = hdc->write_cmd_raw4;
	} else {
		write_cmd_raw = hdc->write_cmd;
	}
	write_cmd_raw(hdc, init);
	long_sleep(10);
	write_cmd_raw(hdc, init);
	long_sleep(10);
	write_cmd_raw(hdc, init);
	long_sleep(10);

	if (hdc->ifwidth == 4) {
		 
		hdc->write_cmd_raw4(hdc, LCD_CMD_FUNCTION_SET >> 4);
		long_sleep(10);
	}

	 
	hdc->write_cmd(hdc,
		LCD_CMD_FUNCTION_SET |
		((hdc->ifwidth == 8) ? LCD_CMD_DATA_LEN_8BITS : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_F) ?
			LCD_CMD_FONT_5X10_DOTS : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_N) ?
			LCD_CMD_TWO_LINES : 0));
	long_sleep(10);

	 
	hdc->write_cmd(hdc, LCD_CMD_DISPLAY_CTRL);
	long_sleep(10);

	hdc->write_cmd(hdc,
		LCD_CMD_DISPLAY_CTRL |	 
		((hdc->hd44780_common_flags & LCD_FLAG_D) ?
			LCD_CMD_DISPLAY_ON : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_C) ?
			LCD_CMD_CURSOR_ON : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_B) ?
			LCD_CMD_BLINK_ON : 0));

	charlcd_backlight(lcd,
			(hdc->hd44780_common_flags & LCD_FLAG_L) ? 1 : 0);

	long_sleep(10);

	 
	hdc->write_cmd(hdc, LCD_CMD_ENTRY_MODE | LCD_CMD_CURSOR_INC);

	hd44780_common_clear_display(lcd);
	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_init_display);

int hd44780_common_shift_cursor(struct charlcd *lcd, enum charlcd_shift_dir dir)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (dir == CHARLCD_SHIFT_LEFT) {
		 
		if (lcd->addr.x < hdc->bwidth)
			hdc->write_cmd(hdc, LCD_CMD_SHIFT);
	} else if (dir == CHARLCD_SHIFT_RIGHT) {
		 
		if (lcd->addr.x < (hdc->bwidth - 1))
			hdc->write_cmd(hdc,
					LCD_CMD_SHIFT | LCD_CMD_SHIFT_RIGHT);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_shift_cursor);

int hd44780_common_shift_display(struct charlcd *lcd,
		enum charlcd_shift_dir dir)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (dir == CHARLCD_SHIFT_LEFT)
		hdc->write_cmd(hdc, LCD_CMD_SHIFT | LCD_CMD_DISPLAY_SHIFT);
	else if (dir == CHARLCD_SHIFT_RIGHT)
		hdc->write_cmd(hdc, LCD_CMD_SHIFT | LCD_CMD_DISPLAY_SHIFT |
			LCD_CMD_SHIFT_RIGHT);

	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_shift_display);

static void hd44780_common_set_mode(struct hd44780_common *hdc)
{
	hdc->write_cmd(hdc,
		LCD_CMD_DISPLAY_CTRL |
		((hdc->hd44780_common_flags & LCD_FLAG_D) ?
			LCD_CMD_DISPLAY_ON : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_C) ?
			LCD_CMD_CURSOR_ON : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_B) ?
			LCD_CMD_BLINK_ON : 0));
}

int hd44780_common_display(struct charlcd *lcd, enum charlcd_onoff on)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (on == CHARLCD_ON)
		hdc->hd44780_common_flags |= LCD_FLAG_D;
	else
		hdc->hd44780_common_flags &= ~LCD_FLAG_D;

	hd44780_common_set_mode(hdc);
	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_display);

int hd44780_common_cursor(struct charlcd *lcd, enum charlcd_onoff on)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (on == CHARLCD_ON)
		hdc->hd44780_common_flags |= LCD_FLAG_C;
	else
		hdc->hd44780_common_flags &= ~LCD_FLAG_C;

	hd44780_common_set_mode(hdc);
	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_cursor);

int hd44780_common_blink(struct charlcd *lcd, enum charlcd_onoff on)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (on == CHARLCD_ON)
		hdc->hd44780_common_flags |= LCD_FLAG_B;
	else
		hdc->hd44780_common_flags &= ~LCD_FLAG_B;

	hd44780_common_set_mode(hdc);
	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_blink);

static void hd44780_common_set_function(struct hd44780_common *hdc)
{
	hdc->write_cmd(hdc,
		LCD_CMD_FUNCTION_SET |
		((hdc->ifwidth == 8) ? LCD_CMD_DATA_LEN_8BITS : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_F) ?
			LCD_CMD_FONT_5X10_DOTS : 0) |
		((hdc->hd44780_common_flags & LCD_FLAG_N) ?
			LCD_CMD_TWO_LINES : 0));
}

int hd44780_common_fontsize(struct charlcd *lcd, enum charlcd_fontsize size)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (size == CHARLCD_FONTSIZE_LARGE)
		hdc->hd44780_common_flags |= LCD_FLAG_F;
	else
		hdc->hd44780_common_flags &= ~LCD_FLAG_F;

	hd44780_common_set_function(hdc);
	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_fontsize);

int hd44780_common_lines(struct charlcd *lcd, enum charlcd_lines lines)
{
	struct hd44780_common *hdc = lcd->drvdata;

	if (lines == CHARLCD_LINES_2)
		hdc->hd44780_common_flags |= LCD_FLAG_N;
	else
		hdc->hd44780_common_flags &= ~LCD_FLAG_N;

	hd44780_common_set_function(hdc);
	return 0;
}
EXPORT_SYMBOL_GPL(hd44780_common_lines);

int hd44780_common_redefine_char(struct charlcd *lcd, char *esc)
{
	 

	struct hd44780_common *hdc = lcd->drvdata;
	unsigned char cgbytes[8];
	unsigned char cgaddr;
	int cgoffset;
	int shift;
	char value;
	int addr;

	if (!strchr(esc, ';'))
		return 0;

	esc++;

	cgaddr = *(esc++) - '0';
	if (cgaddr > 7)
		return 1;

	cgoffset = 0;
	shift = 0;
	value = 0;
	while (*esc && cgoffset < 8) {
		int half;

		shift ^= 4;
		half = hex_to_bin(*esc++);
		if (half < 0)
			continue;

		value |= half << shift;
		if (shift == 0) {
			cgbytes[cgoffset++] = value;
			value = 0;
		}
	}

	hdc->write_cmd(hdc, LCD_CMD_SET_CGRAM_ADDR | (cgaddr * 8));
	for (addr = 0; addr < cgoffset; addr++)
		hdc->write_data(hdc, cgbytes[addr]);

	 
	lcd->ops->gotoxy(lcd, lcd->addr.x, lcd->addr.y);
	return 1;
}
EXPORT_SYMBOL_GPL(hd44780_common_redefine_char);

struct hd44780_common *hd44780_common_alloc(void)
{
	struct hd44780_common *hd;

	hd = kzalloc(sizeof(*hd), GFP_KERNEL);
	if (!hd)
		return NULL;

	hd->ifwidth = 8;
	hd->bwidth = DEFAULT_LCD_BWIDTH;
	hd->hwidth = DEFAULT_LCD_HWIDTH;
	return hd;
}
EXPORT_SYMBOL_GPL(hd44780_common_alloc);

MODULE_LICENSE("GPL");
