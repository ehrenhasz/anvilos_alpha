

#ifndef __NRF52_HAL
#define __NRF52_HAL

#include "py/mpconfig.h"
#include <nrfx.h>
#include "pin.h"
#include "nrf_gpio.h"
#include "nrfx_config.h"

typedef enum
{
    HAL_OK       = 0x00,
    HAL_ERROR    = 0x01,
    HAL_BUSY     = 0x02,
    HAL_TIMEOUT  = 0x03
} HAL_StatusTypeDef;

extern const unsigned char mp_hal_status_to_errno_table[4];

NORETURN void mp_hal_raise(HAL_StatusTypeDef status);
void mp_hal_set_interrupt_char(int c); 

int mp_hal_stdin_rx_chr(void);
void mp_hal_stdout_tx_str(const char *str);

void mp_hal_delay_ms(mp_uint_t ms);
void mp_hal_delay_us(mp_uint_t us);

const char *nrfx_error_code_lookup(uint32_t err_code);

void mp_nrf_start_lfclk(void);

#define MP_HAL_PIN_FMT           "%q"
#define mp_hal_pin_obj_t const pin_obj_t *
#define mp_hal_get_pin_obj(o)    pin_find(o)
#define mp_hal_pin_name(p)       ((p)->name)
#define mp_hal_pin_high(p)       nrf_gpio_pin_set(p->pin)
#define mp_hal_pin_low(p)        nrf_gpio_pin_clear(p->pin)
#define mp_hal_pin_read(p)       (nrf_gpio_pin_dir_get(p->pin) == NRF_GPIO_PIN_DIR_OUTPUT) ? nrf_gpio_pin_out_read(p->pin) : nrf_gpio_pin_read(p->pin)
#define mp_hal_pin_write(p, v)   ((v) ? mp_hal_pin_high(p) : mp_hal_pin_low(p))
#define mp_hal_pin_od_low(p)     mp_hal_pin_low(p)
#define mp_hal_pin_od_high(p)    mp_hal_pin_high(p)
#define mp_hal_pin_open_drain(p) nrf_gpio_cfg_input(p->pin, NRF_GPIO_PIN_NOPULL)

#if MICROPY_PY_TIME_TICKS
void rtc1_init_time_ticks();
#else
mp_uint_t mp_hal_ticks_ms(void);
#define mp_hal_ticks_us() (0)
#endif


#define mp_hal_delay_us_fast(p)
#define mp_hal_ticks_cpu() (0)

#endif
