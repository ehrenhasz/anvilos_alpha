

#include "py/obj.h"

extern void mp_js_write(const char *str, mp_uint_t len);
extern int mp_js_ticks_ms(void);
extern void mp_js_hook(void);
extern double mp_js_time_ms(void);
extern uint32_t mp_js_random_u32(void);
