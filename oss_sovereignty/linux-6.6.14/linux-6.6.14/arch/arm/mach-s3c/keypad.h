#ifndef __PLAT_SAMSUNG_KEYPAD_H
#define __PLAT_SAMSUNG_KEYPAD_H
#include <linux/input/samsung-keypad.h>
extern void samsung_keypad_set_platdata(struct samsung_keypad_platdata *pd);
extern void samsung_keypad_cfg_gpio(unsigned int rows, unsigned int cols);
#endif  
