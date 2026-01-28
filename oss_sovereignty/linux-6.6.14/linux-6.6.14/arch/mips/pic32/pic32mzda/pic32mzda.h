#ifndef PIC32MZDA_COMMON_H
#define PIC32MZDA_COMMON_H
u32 pic32_get_pbclk(int bus);
u32 pic32_get_sysclk(void);
void __init pic32_config_init(void);
int pic32_set_lcd_mode(int mode);
int pic32_set_sdhci_adma_fifo_threshold(u32 rthrs, u32 wthrs);
u32 pic32_get_boot_status(void);
int pic32_disable_lcd(void);
int pic32_enable_lcd(void);
#endif
