#include "boards/NUCLEO_H743ZI/mpconfigboard.h"

#undef MICROPY_HW_BOARD_NAME
#define MICROPY_HW_BOARD_NAME       "NUCLEO_H743ZI2"


#undef MICROPY_HW_RTC_USE_LSE
#define MICROPY_HW_RTC_USE_LSE      (1)




#undef MICROPY_HW_CLK_USE_BYPASS
#define MICROPY_HW_CLK_USE_BYPASS   (1)

#undef MICROPY_HW_LED2
#define MICROPY_HW_LED2             (pin_E1)    



#if defined(USE_MBOOT)
#define MBOOT_BOOTPIN_PIN (pin_C13)
#define MBOOT_BOOTPIN_PULL (MP_HAL_PIN_PULL_DOWN)
#define MBOOT_BOOTPIN_ACTIVE (1)
#endif
