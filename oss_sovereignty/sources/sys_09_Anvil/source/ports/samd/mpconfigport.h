




#include "mpconfigboard.h"

#include "mpconfigmcu.h"


#define MICROPY_GC_STACK_ENTRY_TYPE         uint16_t
#define MICROPY_GC_ALLOC_THRESHOLD          (0)
#define MICROPY_ALLOC_PATH_MAX              (256)


#define MICROPY_PERSISTENT_CODE_LOAD        (1)


#define MICROPY_ENABLE_GC                   (1)
#define MICROPY_LONGINT_IMPL                (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_FLOAT_IMPL                  (MICROPY_FLOAT_IMPL_FLOAT)
#ifndef MICROPY_PY_BUILTINS_COMPLEX
#define MICROPY_PY_BUILTINS_COMPLEX         (0)
#endif
#define MICROPY_ERROR_REPORTING             (MICROPY_ERROR_REPORTING_TERSE)
#define MICROPY_PY_BUILTINS_HELP_TEXT       samd_help_text
#define MICROPY_USE_INTERNAL_ERRNO          (1)
#define MICROPY_SCHEDULER_STATIC_NODES      (1)

#define MICROPY_HW_ENABLE_USBDEV            (1)
#define MICROPY_HW_USB_CDC_1200BPS_TOUCH    (1)

#if MICROPY_HW_ENABLE_USBDEV

#ifndef MICROPY_HW_USB_CDC
#define MICROPY_HW_USB_CDC (1)
#endif

#ifndef MICROPY_HW_USB_DESC_STR_MAX
#define MICROPY_HW_USB_DESC_STR_MAX (32)
#endif

#ifndef MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE
#define MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE    (1)
#endif

#endif 

#define MICROPY_PY_SYS_PLATFORM             "samd"


#define MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME (1)
#define MICROPY_PY_TIME_TIME_TIME_NS        (1)
#define MICROPY_PY_TIME_INCLUDEFILE         "ports/samd/modtime.c"
#define MICROPY_PY_MACHINE                  (1)
#define MICROPY_PY_MACHINE_INCLUDEFILE      "ports/samd/modmachine.c"
#define MICROPY_PY_MACHINE_RESET            (1)
#define MICROPY_PY_MACHINE_BARE_METAL_FUNCS (1)
#define MICROPY_PY_MACHINE_BOOTLOADER       (1)
#define MICROPY_PY_MACHINE_DISABLE_IRQ_ENABLE_IRQ (1)
#define MICROPY_PY_OS_INCLUDEFILE           "ports/samd/modos.c"
#define MICROPY_READER_VFS                  (1)
#define MICROPY_VFS                         (1)
#ifndef MICROPY_PY_MACHINE_ADC
#define MICROPY_PY_MACHINE_ADC              (1)
#endif
#define MICROPY_PY_MACHINE_ADC_INCLUDEFILE  "ports/samd/machine_adc.c"
#define MICROPY_PY_MACHINE_ADC_DEINIT       (1)
#ifndef MICROPY_PY_MACHINE_DAC
#define MICROPY_PY_MACHINE_DAC              (1)
#endif
#ifndef MICROPY_PY_MACHINE_I2C
#define MICROPY_PY_MACHINE_I2C              (1)
#endif
#ifndef MICROPY_PY_MACHINE_SPI
#define MICROPY_PY_MACHINE_SPI              (1)
#endif
#ifndef MICROPY_PY_MACHINE_SOFTI2C
#define MICROPY_PY_MACHINE_SOFTI2C          (1)
#endif
#ifndef MICROPY_PY_MACHINE_SOFTSPI
#define MICROPY_PY_MACHINE_SOFTSPI          (1)
#endif
#ifndef MICROPY_PY_MACHINE_UART
#define MICROPY_PY_MACHINE_UART             (1)
#endif
#define MICROPY_PY_MACHINE_UART_INCLUDEFILE "ports/samd/machine_uart.c"
#define MICROPY_PY_MACHINE_UART_SENDBREAK   (1)
#define MICROPY_PY_MACHINE_TIMER            (1)
#define MICROPY_SOFT_TIMER_TICKS_MS         systick_ms
#define MICROPY_PY_OS_DUPTERM               (3)
#define MICROPY_PY_MACHINE_BITSTREAM        (1)
#ifndef MICROPY_PY_MACHINE_PULSE
#define MICROPY_PY_MACHINE_PULSE            (1)
#endif
#ifndef MICROPY_PY_MACHINE_PWM
#define MICROPY_PY_MACHINE_PWM              (1)
#define MICROPY_PY_MACHINE_PWM_INCLUDEFILE  "ports/samd/machine_pwm.c"
#endif
#define MICROPY_PY_MACHINE_PIN_MAKE_NEW     mp_pin_make_new
#define MICROPY_PY_MACHINE_DHT_READINTO     (1)
#define MICROPY_PY_MACHINE_WDT              (1)
#define MICROPY_PY_MACHINE_WDT_INCLUDEFILE  "ports/samd/machine_wdt.c"
#define MICROPY_PY_MACHINE_WDT_TIMEOUT_MS   (1)
#define MICROPY_PLATFORM_VERSION            "ASF4"

#define MP_STATE_PORT MP_STATE_VM



#ifndef MICROPY_HW_USB_VID
#define MICROPY_HW_USB_VID (0xf055)
#endif
#ifndef MICROPY_HW_USB_PID
#define MICROPY_HW_USB_PID (0x9802)
#endif


#ifndef MICROPY_BOARD_PENDSV_ENTRIES
#define MICROPY_BOARD_PENDSV_ENTRIES
#endif


#if !defined(MICROPY_HW_MCUFLASH) && !defined(MICROPY_HW_QSPIFLASH) && !(defined(MICROPY_HW_SPIFLASH) && defined(MICROPY_HW_SPIFLASH_ID))
#define MICROPY_HW_MCUFLASH                 (1)
#endif  



#define MICROPY_EVENT_POLL_HOOK \
    do { \
        extern void mp_handle_pending(bool); \
        mp_handle_pending(true); \
        __WFE(); \
    } while (0);

#define MICROPY_MAKE_POINTER_CALLABLE(p) ((void *)((mp_uint_t)(p) | 1))

#define MP_SSIZE_MAX (0x7fffffff)
typedef int mp_int_t; 
typedef unsigned mp_uint_t; 
typedef long mp_off_t;


#define MP_NEED_LOG2 (1)


#include <alloca.h>
