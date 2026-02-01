 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#if defined(__mc68000__)
#include <asm/setup.h>
#endif
#include <linux/font.h>

static const struct font_desc *fonts[] = {
#ifdef CONFIG_FONT_8x8
	&font_vga_8x8,
#endif
#ifdef CONFIG_FONT_8x16
	&font_vga_8x16,
#endif
#ifdef CONFIG_FONT_6x11
	&font_vga_6x11,
#endif
#ifdef CONFIG_FONT_7x14
	&font_7x14,
#endif
#ifdef CONFIG_FONT_SUN8x16
	&font_sun_8x16,
#endif
#ifdef CONFIG_FONT_SUN12x22
	&font_sun_12x22,
#endif
#ifdef CONFIG_FONT_10x18
	&font_10x18,
#endif
#ifdef CONFIG_FONT_ACORN_8x8
	&font_acorn_8x8,
#endif
#ifdef CONFIG_FONT_PEARL_8x8
	&font_pearl_8x8,
#endif
#ifdef CONFIG_FONT_MINI_4x6
	&font_mini_4x6,
#endif
#ifdef CONFIG_FONT_6x10
	&font_6x10,
#endif
#ifdef CONFIG_FONT_TER16x32
	&font_ter_16x32,
#endif
#ifdef CONFIG_FONT_6x8
	&font_6x8,
#endif
};

#define num_fonts ARRAY_SIZE(fonts)

#ifdef NO_FONTS
#error No fonts configured.
#endif


 
const struct font_desc *find_font(const char *name)
{
	unsigned int i;

	BUILD_BUG_ON(!num_fonts);
	for (i = 0; i < num_fonts; i++)
		if (!strcmp(fonts[i]->name, name))
			return fonts[i];
	return NULL;
}
EXPORT_SYMBOL(find_font);


 
const struct font_desc *get_default_font(int xres, int yres, u32 font_w,
					 u32 font_h)
{
	int i, c, cc, res;
	const struct font_desc *f, *g;

	g = NULL;
	cc = -10000;
	for (i = 0; i < num_fonts; i++) {
		f = fonts[i];
		c = f->pref;
#if defined(__mc68000__)
#ifdef CONFIG_FONT_PEARL_8x8
		if (MACH_IS_AMIGA && f->idx == PEARL8x8_IDX)
			c = 100;
#endif
#ifdef CONFIG_FONT_6x11
		if (MACH_IS_MAC && xres < 640 && f->idx == VGA6x11_IDX)
			c = 100;
#endif
#endif
		if ((yres < 400) == (f->height <= 8))
			c += 1000;

		 
		res = (xres / f->width) * (yres / f->height) / 1000;
		if (res > 20)
			c += 20 - res;

		if ((font_w & (1U << (f->width - 1))) &&
		    (font_h & (1U << (f->height - 1))))
			c += 1000;

		if (c > cc) {
			cc = c;
			g = f;
		}
	}
	return g;
}
EXPORT_SYMBOL(get_default_font);

MODULE_AUTHOR("James Simmons <jsimmons@users.sf.net>");
MODULE_DESCRIPTION("Console Fonts");
MODULE_LICENSE("GPL");
