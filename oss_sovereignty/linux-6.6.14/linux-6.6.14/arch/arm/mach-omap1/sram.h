extern void omap_sram_reprogram_clock(u32 dpllctl, u32 ckctl);
int omap1_sram_init(void);
void *omap_sram_push(void *funcp, unsigned long size);
extern void omap1_sram_reprogram_clock(u32 ckctl, u32 dpllctl);
extern unsigned long omap1_sram_reprogram_clock_sz;
