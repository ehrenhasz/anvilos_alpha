#ifndef _MIPS_SPRAM_H
#define _MIPS_SPRAM_H
#if defined(CONFIG_MIPS_SPRAM)
extern __init void spram_config(void);
#else
static inline void spram_config(void) { }
#endif  
#endif  
