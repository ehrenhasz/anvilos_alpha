 

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>
#include <linux/vt_buffer.h>
#include <linux/selection.h>
#include <linux/spinlock.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/vga.h>

static DEFINE_SPINLOCK(mda_lock);

 

static u16		*mda_vram_base;		 
static unsigned long	mda_vram_len;		 
static unsigned int	mda_num_columns;	 
static unsigned int	mda_num_lines;		 

static unsigned int	mda_index_port;		 
static unsigned int	mda_value_port;		 
static unsigned int	mda_mode_port;		 
static unsigned int	mda_status_port;	 
static unsigned int	mda_gfx_port;		 

 

static int	mda_cursor_loc=-1;
static int	mda_cursor_size_from=-1;
static int	mda_cursor_size_to=-1;

static enum { TYPE_MDA, TYPE_HERC, TYPE_HERCPLUS, TYPE_HERCCOLOR } mda_type;
static char *mda_type_name;

 

static int	mda_first_vc = 13;
static int	mda_last_vc  = 16;

static struct vc_data	*mda_display_fg = NULL;

module_param(mda_first_vc, int, 0);
MODULE_PARM_DESC(mda_first_vc, "First virtual console. Default: 13");
module_param(mda_last_vc, int, 0);
MODULE_PARM_DESC(mda_last_vc, "Last virtual console. Default: 16");

 

#define MDA_CURSOR_BLINKING	0x00
#define MDA_CURSOR_OFF		0x20
#define MDA_CURSOR_SLOWBLINK	0x60

#define MDA_MODE_GRAPHICS	0x02
#define MDA_MODE_VIDEO_EN	0x08
#define MDA_MODE_BLINK_EN	0x20
#define MDA_MODE_GFX_PAGE1	0x80

#define MDA_STATUS_HSYNC	0x01
#define MDA_STATUS_VSYNC	0x80
#define MDA_STATUS_VIDEO	0x08

#define MDA_CONFIG_COL132	0x08
#define MDA_GFX_MODE_EN		0x01
#define MDA_GFX_PAGE_EN		0x02


 

static void write_mda_b(unsigned int val, unsigned char reg)
{
	unsigned long flags;

	spin_lock_irqsave(&mda_lock, flags);	

	outb_p(reg, mda_index_port); 
	outb_p(val, mda_value_port);

	spin_unlock_irqrestore(&mda_lock, flags);
}

static void write_mda_w(unsigned int val, unsigned char reg)
{
	unsigned long flags;

	spin_lock_irqsave(&mda_lock, flags);

	outb_p(reg,   mda_index_port); outb_p(val >> 8,   mda_value_port);
	outb_p(reg+1, mda_index_port); outb_p(val & 0xff, mda_value_port);

	spin_unlock_irqrestore(&mda_lock, flags);
}

#ifdef TEST_MDA_B
static int test_mda_b(unsigned char val, unsigned char reg)
{
	unsigned long flags;

	spin_lock_irqsave(&mda_lock, flags);

	outb_p(reg, mda_index_port); 
	outb  (val, mda_value_port);

	udelay(20); val = (inb_p(mda_value_port) == val);

	spin_unlock_irqrestore(&mda_lock, flags);
	return val;
}
#endif

static inline void mda_set_cursor(unsigned int location) 
{
	if (mda_cursor_loc == location)
		return;

	write_mda_w(location >> 1, 0x0e);

	mda_cursor_loc = location;
}

static inline void mda_set_cursor_size(int from, int to)
{
	if (mda_cursor_size_from==from && mda_cursor_size_to==to)
		return;
	
	if (from > to) {
		write_mda_b(MDA_CURSOR_OFF, 0x0a);	 
	} else {
		write_mda_b(from, 0x0a);	 
		write_mda_b(to,   0x0b);	 
	}

	mda_cursor_size_from = from;
	mda_cursor_size_to   = to;
}


#ifndef MODULE
static int __init mdacon_setup(char *str)
{
	 

	int ints[3];

	str = get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] < 2)
		return 0;

	if (ints[1] < 1 || ints[1] > MAX_NR_CONSOLES || 
	    ints[2] < 1 || ints[2] > MAX_NR_CONSOLES)
		return 0;

	mda_first_vc = ints[1];
	mda_last_vc  = ints[2];
	return 1;
}

__setup("mdacon=", mdacon_setup);
#endif

static int mda_detect(void)
{
	int count=0;
	u16 *p, p_save;
	u16 *q, q_save;

	 

	p = mda_vram_base;
	q = mda_vram_base + 0x01000 / 2;

	p_save = scr_readw(p);
	q_save = scr_readw(q);

	scr_writew(0xAA55, p);
	if (scr_readw(p) == 0xAA55)
		count++;

	scr_writew(0x55AA, p);
	if (scr_readw(p) == 0x55AA)
		count++;

	scr_writew(p_save, p);

	if (count != 2) {
		return 0;
	}

	 

	scr_writew(0xA55A, q);
	scr_writew(0x0000, p);
	if (scr_readw(q) == 0xA55A)
		count++;
	
	scr_writew(0x5AA5, q);
	scr_writew(0x0000, p);
	if (scr_readw(q) == 0x5AA5)
		count++;

	scr_writew(p_save, p);
	scr_writew(q_save, q);
	
	if (count == 4) {
		mda_vram_len = 0x02000;
	}
	
	 

#ifdef TEST_MDA_B
	 

	 
	if (!test_mda_b(0x66, 0x0f))
		return 0;

	 
	if (!test_mda_b(0x99, 0x0f))
		return 0;
#endif

	 
	
	p_save = q_save = inb_p(mda_status_port) & MDA_STATUS_VSYNC;

	for (count = 0; count < 50000 && p_save == q_save; count++) {
		q_save = inb(mda_status_port) & MDA_STATUS_VSYNC;
		udelay(2);
	}

	if (p_save != q_save) {
		switch (inb_p(mda_status_port) & 0x70) {
		case 0x10:
			mda_type = TYPE_HERCPLUS;
			mda_type_name = "HerculesPlus";
			break;
		case 0x50:
			mda_type = TYPE_HERCCOLOR;
			mda_type_name = "HerculesColor";
			break;
		default:
			mda_type = TYPE_HERC;
			mda_type_name = "Hercules";
			break;
		}
	}

	return 1;
}

static void mda_initialize(void)
{
	write_mda_b(97, 0x00);		 
	write_mda_b(80, 0x01);		 
	write_mda_b(82, 0x02);		 
	write_mda_b(15, 0x03);		 

	write_mda_b(25, 0x04);		 
	write_mda_b(6,  0x05);		 
	write_mda_b(25, 0x06);		 
	write_mda_b(25, 0x07);		 

	write_mda_b(2,  0x08);		 
	write_mda_b(13, 0x09);		 
	write_mda_b(12, 0x0a);		 
	write_mda_b(13, 0x0b);		 

	write_mda_w(0x0000, 0x0c);	 
	write_mda_w(0x0000, 0x0e);	 

	outb_p(MDA_MODE_VIDEO_EN | MDA_MODE_BLINK_EN, mda_mode_port);
	outb_p(0x00, mda_status_port);
	outb_p(0x00, mda_gfx_port);
}

static const char *mdacon_startup(void)
{
	mda_num_columns = 80;
	mda_num_lines   = 25;

	mda_vram_len  = 0x01000;
	mda_vram_base = (u16 *)VGA_MAP_MEM(0xb0000, mda_vram_len);

	mda_index_port  = 0x3b4;
	mda_value_port  = 0x3b5;
	mda_mode_port   = 0x3b8;
	mda_status_port = 0x3ba;
	mda_gfx_port    = 0x3bf;

	mda_type = TYPE_MDA;
	mda_type_name = "MDA";

	if (! mda_detect()) {
		printk("mdacon: MDA card not detected.\n");
		return NULL;
	}

	if (mda_type != TYPE_MDA) {
		mda_initialize();
	}

	 
	mda_set_cursor(mda_vram_len - 1);

	printk("mdacon: %s with %ldK of memory detected.\n",
		mda_type_name, mda_vram_len/1024);

	return "MDA-2";
}

static void mdacon_init(struct vc_data *c, int init)
{
	c->vc_complement_mask = 0x0800;	  
	c->vc_display_fg = &mda_display_fg;

	if (init) {
		c->vc_cols = mda_num_columns;
		c->vc_rows = mda_num_lines;
	} else
		vc_resize(c, mda_num_columns, mda_num_lines);

	 

	if (mda_display_fg == NULL)
		mda_display_fg = c;
}

static void mdacon_deinit(struct vc_data *c)
{
	 

	if (mda_display_fg == c)
		mda_display_fg = NULL;
}

static inline u16 mda_convert_attr(u16 ch)
{
	u16 attr = 0x0700;

	 

	if (ch & 0x0800)	attr = 0x7000;	 
	else if (ch & 0x0400)	attr = 0x0100;	 

	return ((ch & 0x0200) << 2) | 		  
		(ch & 0x8000) |			  
		(ch & 0x00ff) | attr;
}

static u8 mdacon_build_attr(struct vc_data *c, u8 color,
			    enum vc_intensity intensity,
			    bool blink, bool underline, bool reverse,
			    bool italic)
{
	 

	return (intensity & VCI_MASK) |
		(underline << 2) |
		(reverse << 3) |
		(italic << 4) |
		(blink << 7);
}

static void mdacon_invert_region(struct vc_data *c, u16 *p, int count)
{
	for (; count > 0; count--) {
		scr_writew(scr_readw(p) ^ 0x0800, p);
		p++;
	}
}

static inline u16 *mda_addr(unsigned int x, unsigned int y)
{
	return mda_vram_base + y * mda_num_columns + x;
}

static void mdacon_putc(struct vc_data *c, int ch, int y, int x)
{
	scr_writew(mda_convert_attr(ch), mda_addr(x, y));
}

static void mdacon_putcs(struct vc_data *c, const unsigned short *s,
		         int count, int y, int x)
{
	u16 *dest = mda_addr(x, y);

	for (; count > 0; count--) {
		scr_writew(mda_convert_attr(scr_readw(s++)), dest++);
	}
}

static void mdacon_clear(struct vc_data *c, int y, int x, 
			  int height, int width)
{
	u16 *dest = mda_addr(x, y);
	u16 eattr = mda_convert_attr(c->vc_video_erase_char);

	if (width <= 0 || height <= 0)
		return;

	if (x==0 && width==mda_num_columns) {
		scr_memsetw(dest, eattr, height*width*2);
	} else {
		for (; height > 0; height--, dest+=mda_num_columns)
			scr_memsetw(dest, eattr, width*2);
	}
}
                        
static int mdacon_switch(struct vc_data *c)
{
	return 1;	 
}

static int mdacon_blank(struct vc_data *c, int blank, int mode_switch)
{
	if (mda_type == TYPE_MDA) {
		if (blank) 
			scr_memsetw(mda_vram_base,
				mda_convert_attr(c->vc_video_erase_char),
				c->vc_screenbuf_size);
		 
		return 1;
	} else {
		if (blank)
			outb_p(0x00, mda_mode_port);	 
		else
			outb_p(MDA_MODE_VIDEO_EN | MDA_MODE_BLINK_EN, 
				mda_mode_port);
		return 0;
	}
}

static void mdacon_cursor(struct vc_data *c, int mode)
{
	if (mode == CM_ERASE) {
		mda_set_cursor(mda_vram_len - 1);
		return;
	}

	mda_set_cursor(c->state.y * mda_num_columns * 2 + c->state.x * 2);

	switch (CUR_SIZE(c->vc_cursor_type)) {

		case CUR_LOWER_THIRD:	mda_set_cursor_size(10, 13); break;
		case CUR_LOWER_HALF:	mda_set_cursor_size(7,  13); break;
		case CUR_TWO_THIRDS:	mda_set_cursor_size(4,  13); break;
		case CUR_BLOCK:		mda_set_cursor_size(1,  13); break;
		case CUR_NONE:		mda_set_cursor_size(14, 13); break;
		default:		mda_set_cursor_size(12, 13); break;
	}
}

static bool mdacon_scroll(struct vc_data *c, unsigned int t, unsigned int b,
		enum con_scroll dir, unsigned int lines)
{
	u16 eattr = mda_convert_attr(c->vc_video_erase_char);

	if (!lines)
		return false;

	if (lines > c->vc_rows)    
		lines = c->vc_rows;

	switch (dir) {

	case SM_UP:
		scr_memmovew(mda_addr(0, t), mda_addr(0, t + lines),
				(b-t-lines)*mda_num_columns*2);
		scr_memsetw(mda_addr(0, b - lines), eattr,
				lines*mda_num_columns*2);
		break;

	case SM_DOWN:
		scr_memmovew(mda_addr(0, t + lines), mda_addr(0, t),
				(b-t-lines)*mda_num_columns*2);
		scr_memsetw(mda_addr(0, t), eattr, lines*mda_num_columns*2);
		break;
	}

	return false;
}


 

static const struct consw mda_con = {
	.owner =		THIS_MODULE,
	.con_startup =		mdacon_startup,
	.con_init =		mdacon_init,
	.con_deinit =		mdacon_deinit,
	.con_clear =		mdacon_clear,
	.con_putc =		mdacon_putc,
	.con_putcs =		mdacon_putcs,
	.con_cursor =		mdacon_cursor,
	.con_scroll =		mdacon_scroll,
	.con_switch =		mdacon_switch,
	.con_blank =		mdacon_blank,
	.con_build_attr =	mdacon_build_attr,
	.con_invert_region =	mdacon_invert_region,
};

int __init mda_console_init(void)
{
	int err;

	if (mda_first_vc > mda_last_vc)
		return 1;
	console_lock();
	err = do_take_over_console(&mda_con, mda_first_vc-1, mda_last_vc-1, 0);
	console_unlock();
	return err;
}

static void __exit mda_console_exit(void)
{
	give_up_console(&mda_con);
}

module_init(mda_console_init);
module_exit(mda_console_exit);

MODULE_LICENSE("GPL");

