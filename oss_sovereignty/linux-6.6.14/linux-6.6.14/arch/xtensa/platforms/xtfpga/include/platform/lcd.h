#ifndef __XTENSA_XTAVNET_LCD_H
#define __XTENSA_XTAVNET_LCD_H
#ifdef CONFIG_XTFPGA_LCD
void lcd_disp_at_pos(char *str, unsigned char pos);
void lcd_shiftleft(void);
void lcd_shiftright(void);
#else
static inline void lcd_disp_at_pos(char *str, unsigned char pos)
{
}
static inline void lcd_shiftleft(void)
{
}
static inline void lcd_shiftright(void)
{
}
#endif
#endif
