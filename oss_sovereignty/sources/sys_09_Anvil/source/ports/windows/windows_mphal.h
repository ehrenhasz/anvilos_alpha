

#include "sleep.h"
#include "ports/unix/mphalport.h"


#undef MICROPY_INTERNAL_WFE

#if MICROPY_ENABLE_SCHEDULER


#define MICROPY_INTERNAL_WFE(TIMEOUT_MS) msec_sleep(MAX(1.0, (double)(TIMEOUT_MS)))
#else
#define MICROPY_INTERNAL_WFE(TIMEOUT_MS) 
#endif

#define MICROPY_HAL_HAS_VT100 (0)

void mp_hal_move_cursor_back(unsigned int pos);
void mp_hal_erase_line_from_cursor(unsigned int n_chars_to_erase);

#undef mp_hal_ticks_cpu
mp_uint_t mp_hal_ticks_cpu(void);
