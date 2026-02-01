
 

#include <linux/atomic.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <generated/utsrelease.h>

#include "charlcd.h"

 
#define LCD_BL_TEMPO_PERIOD	4

#define LCD_ESCAPE_LEN		24	 
#define LCD_ESCAPE_CHAR		27	 

struct charlcd_priv {
	struct charlcd lcd;

	struct delayed_work bl_work;
	struct mutex bl_tempo_lock;	 
	bool bl_tempo;

	bool must_clear;

	 
	unsigned long flags;

	 
	struct {
		char buf[LCD_ESCAPE_LEN + 1];
		int len;
	} esc_seq;

	unsigned long long drvdata[];
};

#define charlcd_to_priv(p)	container_of(p, struct charlcd_priv, lcd)

 
static atomic_t charlcd_available = ATOMIC_INIT(1);

 
void charlcd_backlight(struct charlcd *lcd, enum charlcd_onoff on)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	if (!lcd->ops->backlight)
		return;

	mutex_lock(&priv->bl_tempo_lock);
	if (!priv->bl_tempo)
		lcd->ops->backlight(lcd, on);
	mutex_unlock(&priv->bl_tempo_lock);
}
EXPORT_SYMBOL_GPL(charlcd_backlight);

static void charlcd_bl_off(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charlcd_priv *priv =
		container_of(dwork, struct charlcd_priv, bl_work);

	mutex_lock(&priv->bl_tempo_lock);
	if (priv->bl_tempo) {
		priv->bl_tempo = false;
		if (!(priv->flags & LCD_FLAG_L))
			priv->lcd.ops->backlight(&priv->lcd, CHARLCD_OFF);
	}
	mutex_unlock(&priv->bl_tempo_lock);
}

 
void charlcd_poke(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	if (!lcd->ops->backlight)
		return;

	cancel_delayed_work_sync(&priv->bl_work);

	mutex_lock(&priv->bl_tempo_lock);
	if (!priv->bl_tempo && !(priv->flags & LCD_FLAG_L))
		lcd->ops->backlight(lcd, CHARLCD_ON);
	priv->bl_tempo = true;
	schedule_delayed_work(&priv->bl_work, LCD_BL_TEMPO_PERIOD * HZ);
	mutex_unlock(&priv->bl_tempo_lock);
}
EXPORT_SYMBOL_GPL(charlcd_poke);

static void charlcd_home(struct charlcd *lcd)
{
	lcd->addr.x = 0;
	lcd->addr.y = 0;
	lcd->ops->home(lcd);
}

static void charlcd_print(struct charlcd *lcd, char c)
{
	if (lcd->addr.x >= lcd->width)
		return;

	if (lcd->char_conv)
		c = lcd->char_conv[(unsigned char)c];

	if (!lcd->ops->print(lcd, c))
		lcd->addr.x++;

	 
	if (lcd->addr.x == lcd->width)
		lcd->ops->gotoxy(lcd, lcd->addr.x - 1, lcd->addr.y);
}

static void charlcd_clear_display(struct charlcd *lcd)
{
	lcd->ops->clear_display(lcd);
	lcd->addr.x = 0;
	lcd->addr.y = 0;
}

 
static bool parse_xy(const char *s, unsigned long *x, unsigned long *y)
{
	unsigned long new_x = *x;
	unsigned long new_y = *y;
	char *p;

	for (;;) {
		if (!*s)
			return false;

		if (*s == ';')
			break;

		if (*s == 'x') {
			new_x = simple_strtoul(s + 1, &p, 10);
			if (p == s + 1)
				return false;
			s = p;
		} else if (*s == 'y') {
			new_y = simple_strtoul(s + 1, &p, 10);
			if (p == s + 1)
				return false;
			s = p;
		} else {
			return false;
		}
	}

	*x = new_x;
	*y = new_y;
	return true;
}

 

static inline int handle_lcd_special_code(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	 

	int processed = 0;

	char *esc = priv->esc_seq.buf + 2;
	int oldflags = priv->flags;

	 
	switch (*esc) {
	case 'D':	 
		priv->flags |= LCD_FLAG_D;
		if (priv->flags != oldflags)
			lcd->ops->display(lcd, CHARLCD_ON);

		processed = 1;
		break;
	case 'd':	 
		priv->flags &= ~LCD_FLAG_D;
		if (priv->flags != oldflags)
			lcd->ops->display(lcd, CHARLCD_OFF);

		processed = 1;
		break;
	case 'C':	 
		priv->flags |= LCD_FLAG_C;
		if (priv->flags != oldflags)
			lcd->ops->cursor(lcd, CHARLCD_ON);

		processed = 1;
		break;
	case 'c':	 
		priv->flags &= ~LCD_FLAG_C;
		if (priv->flags != oldflags)
			lcd->ops->cursor(lcd, CHARLCD_OFF);

		processed = 1;
		break;
	case 'B':	 
		priv->flags |= LCD_FLAG_B;
		if (priv->flags != oldflags)
			lcd->ops->blink(lcd, CHARLCD_ON);

		processed = 1;
		break;
	case 'b':	 
		priv->flags &= ~LCD_FLAG_B;
		if (priv->flags != oldflags)
			lcd->ops->blink(lcd, CHARLCD_OFF);

		processed = 1;
		break;
	case '+':	 
		priv->flags |= LCD_FLAG_L;
		if (priv->flags != oldflags)
			charlcd_backlight(lcd, CHARLCD_ON);

		processed = 1;
		break;
	case '-':	 
		priv->flags &= ~LCD_FLAG_L;
		if (priv->flags != oldflags)
			charlcd_backlight(lcd, CHARLCD_OFF);

		processed = 1;
		break;
	case '*':	 
		charlcd_poke(lcd);
		processed = 1;
		break;
	case 'f':	 
		priv->flags &= ~LCD_FLAG_F;
		if (priv->flags != oldflags)
			lcd->ops->fontsize(lcd, CHARLCD_FONTSIZE_SMALL);

		processed = 1;
		break;
	case 'F':	 
		priv->flags |= LCD_FLAG_F;
		if (priv->flags != oldflags)
			lcd->ops->fontsize(lcd, CHARLCD_FONTSIZE_LARGE);

		processed = 1;
		break;
	case 'n':	 
		priv->flags &= ~LCD_FLAG_N;
		if (priv->flags != oldflags)
			lcd->ops->lines(lcd, CHARLCD_LINES_1);

		processed = 1;
		break;
	case 'N':	 
		priv->flags |= LCD_FLAG_N;
		if (priv->flags != oldflags)
			lcd->ops->lines(lcd, CHARLCD_LINES_2);

		processed = 1;
		break;
	case 'l':	 
		if (lcd->addr.x > 0) {
			if (!lcd->ops->shift_cursor(lcd, CHARLCD_SHIFT_LEFT))
				lcd->addr.x--;
		}

		processed = 1;
		break;
	case 'r':	 
		if (lcd->addr.x < lcd->width) {
			if (!lcd->ops->shift_cursor(lcd, CHARLCD_SHIFT_RIGHT))
				lcd->addr.x++;
		}

		processed = 1;
		break;
	case 'L':	 
		lcd->ops->shift_display(lcd, CHARLCD_SHIFT_LEFT);
		processed = 1;
		break;
	case 'R':	 
		lcd->ops->shift_display(lcd, CHARLCD_SHIFT_RIGHT);
		processed = 1;
		break;
	case 'k': {	 
		int x, xs, ys;

		xs = lcd->addr.x;
		ys = lcd->addr.y;
		for (x = lcd->addr.x; x < lcd->width; x++)
			lcd->ops->print(lcd, ' ');

		 
		lcd->addr.x = xs;
		lcd->addr.y = ys;
		lcd->ops->gotoxy(lcd, lcd->addr.x, lcd->addr.y);
		processed = 1;
		break;
	}
	case 'I':	 
		lcd->ops->init_display(lcd);
		priv->flags = ((lcd->height > 1) ? LCD_FLAG_N : 0) | LCD_FLAG_D |
			LCD_FLAG_C | LCD_FLAG_B;
		processed = 1;
		break;
	case 'G':
		if (lcd->ops->redefine_char)
			processed = lcd->ops->redefine_char(lcd, esc);
		else
			processed = 1;
		break;

	case 'x':	 
	case 'y':	 
		if (priv->esc_seq.buf[priv->esc_seq.len - 1] != ';')
			break;

		 
		if (parse_xy(esc, &lcd->addr.x, &lcd->addr.y))
			lcd->ops->gotoxy(lcd, lcd->addr.x, lcd->addr.y);

		 
		processed = 1;
		break;
	}

	return processed;
}

static void charlcd_write_char(struct charlcd *lcd, char c)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	 
	if ((c != '\n') && priv->esc_seq.len >= 0) {
		 
		priv->esc_seq.buf[priv->esc_seq.len++] = c;
		priv->esc_seq.buf[priv->esc_seq.len] = '\0';
	} else {
		 
		priv->esc_seq.len = -1;

		switch (c) {
		case LCD_ESCAPE_CHAR:
			 
			priv->esc_seq.len = 0;
			priv->esc_seq.buf[priv->esc_seq.len] = '\0';
			break;
		case '\b':
			 
			if (lcd->addr.x > 0) {
				 
				if (!lcd->ops->shift_cursor(lcd,
							CHARLCD_SHIFT_LEFT))
					lcd->addr.x--;
			}
			 
			charlcd_print(lcd, ' ');
			 
			if (!lcd->ops->shift_cursor(lcd, CHARLCD_SHIFT_LEFT))
				lcd->addr.x--;

			break;
		case '\f':
			 
			charlcd_clear_display(lcd);
			break;
		case '\n':
			 
			for (; lcd->addr.x < lcd->width; lcd->addr.x++)
				lcd->ops->print(lcd, ' ');

			lcd->addr.x = 0;
			lcd->addr.y = (lcd->addr.y + 1) % lcd->height;
			lcd->ops->gotoxy(lcd, lcd->addr.x, lcd->addr.y);
			break;
		case '\r':
			 
			lcd->addr.x = 0;
			lcd->ops->gotoxy(lcd, lcd->addr.x, lcd->addr.y);
			break;
		case '\t':
			 
			charlcd_print(lcd, ' ');
			break;
		default:
			 
			charlcd_print(lcd, c);
			break;
		}
	}

	 
	if (priv->esc_seq.len >= 2) {
		int processed = 0;

		if (!strcmp(priv->esc_seq.buf, "[2J")) {
			 
			charlcd_clear_display(lcd);
			processed = 1;
		} else if (!strcmp(priv->esc_seq.buf, "[H")) {
			 
			charlcd_home(lcd);
			processed = 1;
		}
		 
		else if ((priv->esc_seq.len >= 3) &&
			 (priv->esc_seq.buf[0] == '[') &&
			 (priv->esc_seq.buf[1] == 'L')) {
			processed = handle_lcd_special_code(lcd);
		}

		 
		 
		if (processed || (priv->esc_seq.len >= LCD_ESCAPE_LEN))
			priv->esc_seq.len = -1;
	}  
}

static struct charlcd *the_charlcd;

static ssize_t charlcd_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	const char __user *tmp = buf;
	char c;

	for (; count-- > 0; (*ppos)++, tmp++) {
		if (((count + 1) & 0x1f) == 0) {
			 
			cond_resched();
		}

		if (get_user(c, tmp))
			return -EFAULT;

		charlcd_write_char(the_charlcd, c);
	}

	return tmp - buf;
}

static int charlcd_open(struct inode *inode, struct file *file)
{
	struct charlcd_priv *priv = charlcd_to_priv(the_charlcd);
	int ret;

	ret = -EBUSY;
	if (!atomic_dec_and_test(&charlcd_available))
		goto fail;	 

	ret = -EPERM;
	if (file->f_mode & FMODE_READ)	 
		goto fail;

	if (priv->must_clear) {
		priv->lcd.ops->clear_display(&priv->lcd);
		priv->must_clear = false;
		priv->lcd.addr.x = 0;
		priv->lcd.addr.y = 0;
	}
	return nonseekable_open(inode, file);

 fail:
	atomic_inc(&charlcd_available);
	return ret;
}

static int charlcd_release(struct inode *inode, struct file *file)
{
	atomic_inc(&charlcd_available);
	return 0;
}

static const struct file_operations charlcd_fops = {
	.write   = charlcd_write,
	.open    = charlcd_open,
	.release = charlcd_release,
	.llseek  = no_llseek,
};

static struct miscdevice charlcd_dev = {
	.minor	= LCD_MINOR,
	.name	= "lcd",
	.fops	= &charlcd_fops,
};

static void charlcd_puts(struct charlcd *lcd, const char *s)
{
	const char *tmp = s;
	int count = strlen(s);

	for (; count-- > 0; tmp++) {
		if (((count + 1) & 0x1f) == 0)
			cond_resched();

		charlcd_write_char(lcd, *tmp);
	}
}

#ifdef CONFIG_PANEL_BOOT_MESSAGE
#define LCD_INIT_TEXT CONFIG_PANEL_BOOT_MESSAGE
#else
#define LCD_INIT_TEXT "Linux-" UTS_RELEASE "\n"
#endif

#ifdef CONFIG_CHARLCD_BL_ON
#define LCD_INIT_BL "\x1b[L+"
#elif defined(CONFIG_CHARLCD_BL_FLASH)
#define LCD_INIT_BL "\x1b[L*"
#else
#define LCD_INIT_BL "\x1b[L-"
#endif

 
static int charlcd_init(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);
	int ret;

	priv->flags = ((lcd->height > 1) ? LCD_FLAG_N : 0) | LCD_FLAG_D |
		      LCD_FLAG_C | LCD_FLAG_B;
	if (lcd->ops->backlight) {
		mutex_init(&priv->bl_tempo_lock);
		INIT_DELAYED_WORK(&priv->bl_work, charlcd_bl_off);
	}

	 
	if (WARN_ON(!lcd->ops->init_display))
		return -EINVAL;

	ret = lcd->ops->init_display(lcd);
	if (ret)
		return ret;

	 
	charlcd_puts(lcd, "\x1b[Lc\x1b[Lb" LCD_INIT_BL LCD_INIT_TEXT);

	 
	priv->must_clear = true;
	charlcd_home(lcd);
	return 0;
}

struct charlcd *charlcd_alloc(void)
{
	struct charlcd_priv *priv;
	struct charlcd *lcd;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return NULL;

	priv->esc_seq.len = -1;

	lcd = &priv->lcd;

	return lcd;
}
EXPORT_SYMBOL_GPL(charlcd_alloc);

void charlcd_free(struct charlcd *lcd)
{
	kfree(charlcd_to_priv(lcd));
}
EXPORT_SYMBOL_GPL(charlcd_free);

static int panel_notify_sys(struct notifier_block *this, unsigned long code,
			    void *unused)
{
	struct charlcd *lcd = the_charlcd;

	switch (code) {
	case SYS_DOWN:
		charlcd_puts(lcd,
			     "\x0cReloading\nSystem...\x1b[Lc\x1b[Lb\x1b[L+");
		break;
	case SYS_HALT:
		charlcd_puts(lcd, "\x0cSystem Halted.\x1b[Lc\x1b[Lb\x1b[L+");
		break;
	case SYS_POWER_OFF:
		charlcd_puts(lcd, "\x0cPower off.\x1b[Lc\x1b[Lb\x1b[L+");
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block panel_notifier = {
	.notifier_call = panel_notify_sys,
};

int charlcd_register(struct charlcd *lcd)
{
	int ret;

	ret = charlcd_init(lcd);
	if (ret)
		return ret;

	ret = misc_register(&charlcd_dev);
	if (ret)
		return ret;

	the_charlcd = lcd;
	register_reboot_notifier(&panel_notifier);
	return 0;
}
EXPORT_SYMBOL_GPL(charlcd_register);

int charlcd_unregister(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	unregister_reboot_notifier(&panel_notifier);
	charlcd_puts(lcd, "\x0cLCD driver unloaded.\x1b[Lc\x1b[Lb\x1b[L-");
	misc_deregister(&charlcd_dev);
	the_charlcd = NULL;
	if (lcd->ops->backlight) {
		cancel_delayed_work_sync(&priv->bl_work);
		priv->lcd.ops->backlight(&priv->lcd, CHARLCD_OFF);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(charlcd_unregister);

MODULE_LICENSE("GPL");
