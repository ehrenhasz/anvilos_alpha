 
 

#ifndef _CHARLCD_H
#define _CHARLCD_H

#define LCD_FLAG_B		0x0004	 
#define LCD_FLAG_C		0x0008	 
#define LCD_FLAG_D		0x0010	 
#define LCD_FLAG_F		0x0020	 
#define LCD_FLAG_N		0x0040	 
#define LCD_FLAG_L		0x0080	 

enum charlcd_onoff {
	CHARLCD_OFF = 0,
	CHARLCD_ON,
};

enum charlcd_shift_dir {
	CHARLCD_SHIFT_LEFT,
	CHARLCD_SHIFT_RIGHT,
};

enum charlcd_fontsize {
	CHARLCD_FONTSIZE_SMALL,
	CHARLCD_FONTSIZE_LARGE,
};

enum charlcd_lines {
	CHARLCD_LINES_1,
	CHARLCD_LINES_2,
};

struct charlcd {
	const struct charlcd_ops *ops;
	const unsigned char *char_conv;	 

	int height;
	int width;

	 
	struct {
		unsigned long x;
		unsigned long y;
	} addr;

	void *drvdata;
};

 
struct charlcd_ops {
	void (*backlight)(struct charlcd *lcd, enum charlcd_onoff on);
	int (*print)(struct charlcd *lcd, int c);
	int (*gotoxy)(struct charlcd *lcd, unsigned int x, unsigned int y);
	int (*home)(struct charlcd *lcd);
	int (*clear_display)(struct charlcd *lcd);
	int (*init_display)(struct charlcd *lcd);
	int (*shift_cursor)(struct charlcd *lcd, enum charlcd_shift_dir dir);
	int (*shift_display)(struct charlcd *lcd, enum charlcd_shift_dir dir);
	int (*display)(struct charlcd *lcd, enum charlcd_onoff on);
	int (*cursor)(struct charlcd *lcd, enum charlcd_onoff on);
	int (*blink)(struct charlcd *lcd, enum charlcd_onoff on);
	int (*fontsize)(struct charlcd *lcd, enum charlcd_fontsize size);
	int (*lines)(struct charlcd *lcd, enum charlcd_lines lines);
	int (*redefine_char)(struct charlcd *lcd, char *esc);
};

void charlcd_backlight(struct charlcd *lcd, enum charlcd_onoff on);
struct charlcd *charlcd_alloc(void);
void charlcd_free(struct charlcd *lcd);

int charlcd_register(struct charlcd *lcd);
int charlcd_unregister(struct charlcd *lcd);

void charlcd_poke(struct charlcd *lcd);

#endif  
